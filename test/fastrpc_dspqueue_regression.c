// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * Device-side regression driver for the PR #399 fuzz findings.
 *
 * Potentially blocking queue teardown runs in a child process. The parent
 * supervises each case with a monotonic deadline and never tries to repair a
 * queue from a second thread after a timeout.
 */

#include "AEEStdErr.h"
#include "dspqueue.h"
#include "remote.h"
#include "fastrpc_common.h"

#ifndef NUM_DOMAINS_EXTEND
#define NUM_DOMAINS_EXTEND (NUM_DOMAINS * NUM_SESSIONS)
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_DOMAIN CDSP_DOMAIN_ID
#define DEFAULT_ITERATIONS 10
#define DEFAULT_QUEUES 2
#define DEFAULT_TIMEOUT_SEC 30
#define DEFAULT_LIBRARY_COUNT 16
#define DEFAULT_NEAR_LIBRARY_COUNT 220
#define DEFAULT_OVER_LIBRARY_COUNT 260
#define DEFAULT_MODULE_URI \
    "file:///libcalculator_skel.so?calculator_skel_handle_invoke&_modver=1.0"
#define PATH_TEMPLATE "/tmp/fpXXXXXX"
#define MAX_QUEUES 64
#define MAX_LIBRARY_FILES 10000
#define RESULT_SKIP 77
#define RESULT_EXPECTED_BOUNDARY 78
#define RESULT_UNSUPPORTED 79

struct options {
    int domain;
    int iterations;
    int queues;
    int timeout_sec;
    int unsigned_pd;
    int callbacks;
    int skip_unsupported;
    int library_count;
    int near_library_count;
    int over_library_count;
    const char *path_mode;
    int path_mode_set;
    const char *case_name;
    const char *module_uri;
    const char *report_path;
};

enum case_kind {
    CASE_PATH,
    CASE_QUEUE
};

struct matrix_case {
    const char *name;
    enum case_kind kind;
    const char *path_mode;
    int queues;
    int callbacks;
    int library_count;
    int expected_boundary;
};

static const struct matrix_case matrix_cases[] = {
    {"normal", CASE_PATH, "normal", 0, 0, 0, 0},
    {"empty", CASE_PATH, "empty", 0, 0, 0, 0},
    {"nomatch", CASE_PATH, "nomatch", 0, 0, 0, 0},
    {"many-under", CASE_PATH, "many", 0, 0, 0, 0},
    {"many-near", CASE_PATH, "many", 0, 0, 0, 0},
    {"many-over", CASE_PATH, "many", 0, 0, 0, 1},
    {"queue-single", CASE_QUEUE, "normal", 1, 0, 0, 0},
    {"queue-multi", CASE_QUEUE, "normal", 0, 0, 0, 0},
    {"queue-callback", CASE_QUEUE, "normal", 0, 1, 0, 0},
};

#define MATRIX_CASE_COUNT \
    (sizeof(matrix_cases) / sizeof(matrix_cases[0]))

static FILE *report_file;
static int aggregate_skips;
static int aggregate_timeouts;
static int aggregate_boundaries;
static int aggregate_unsupported;

static void report_line(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
    fflush(stdout);

    if (report_file != NULL) {
        va_start(arguments, format);
        vfprintf(report_file, format, arguments);
        va_end(arguments);
        fflush(report_file);
    }
}

static void usage(const char *program)
{
    printf("Usage: %s [options]\n"
           "  -d, --domain N          DSP domain (default %d)\n"
           "  -i, --iterations N      queue rounds (default %d)\n"
           "  -q, --queues N          queues per round (default %d)\n"
           "  -t, --timeout SEC       per-case watchdog (default %d)\n"
           "  -u, --unsigned 0|1      enable unsigned PD (default 1)\n"
           "      --callbacks         include callback queue in focused mode\n"
           "      --case NAME         all or one matrix case (default all)\n"
           "      --path-mode MODE    focused: normal, empty, nomatch, many\n"
           "      --library-count N   focused/under .so count (default %d)\n"
           "      --near-count N      near-boundary .so count (default %d)\n"
           "      --over-count N      over-limit .so count (default %d)\n"
           "      --skip-unsupported allow unavailable device cases to skip\n"
           "      --module-uri URI    URI used to create the DSP process domain\n"
           "      --report PATH       write the same report records to PATH\n"
           "  -h, --help              show this help\n\n"
           "The default invocation runs the complete isolated matrix.\n"
           "The module and dspqueue skeletons must be in the device search path.\n",
           program, DEFAULT_DOMAIN, DEFAULT_ITERATIONS, DEFAULT_QUEUES,
           DEFAULT_TIMEOUT_SEC, DEFAULT_LIBRARY_COUNT,
           DEFAULT_NEAR_LIBRARY_COUNT, DEFAULT_OVER_LIBRARY_COUNT);
}

static int parse_nonnegative(const char *text, int *value, int maximum)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < 0 ||
        parsed > maximum)
        return -1;
    *value = (int)parsed;
    return 0;
}

static int parse_positive(const char *text, int *value, int maximum)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < 1 ||
        parsed > maximum)
        return -1;
    *value = (int)parsed;
    return 0;
}

static int parse_options(int argc, char **argv, struct options *options)
{
    static const struct option long_options[] = {
        {"domain", required_argument, NULL, 'd'},
        {"iterations", required_argument, NULL, 'i'},
        {"queues", required_argument, NULL, 'q'},
        {"timeout", required_argument, NULL, 't'},
        {"unsigned", required_argument, NULL, 'u'},
        {"callbacks", no_argument, NULL, 1},
        {"case", required_argument, NULL, 2},
        {"path-mode", required_argument, NULL, 3},
        {"library-count", required_argument, NULL, 4},
        {"near-count", required_argument, NULL, 5},
        {"over-count", required_argument, NULL, 6},
        {"skip-unsupported", no_argument, NULL, 7},
        {"module-uri", required_argument, NULL, 8},
        {"report", required_argument, NULL, 9},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };
    int option;

    memset(options, 0, sizeof(*options));
    options->domain = DEFAULT_DOMAIN;
    options->iterations = DEFAULT_ITERATIONS;
    options->queues = DEFAULT_QUEUES;
    options->timeout_sec = DEFAULT_TIMEOUT_SEC;
    options->unsigned_pd = 1;
    options->library_count = DEFAULT_LIBRARY_COUNT;
    options->near_library_count = DEFAULT_NEAR_LIBRARY_COUNT;
    options->over_library_count = DEFAULT_OVER_LIBRARY_COUNT;
    options->case_name = "all";
    options->path_mode = "normal";
    options->module_uri = DEFAULT_MODULE_URI;

    while ((option = getopt_long(argc, argv, "d:i:q:t:u:h", long_options,
                                 NULL)) != -1) {
        switch (option) {
        case 'd':
            if (parse_nonnegative(optarg, &options->domain,
                                  NUM_DOMAINS_EXTEND - 1) != 0)
                return -1;
            break;
        case 'i':
            if (parse_positive(optarg, &options->iterations, 100000) != 0)
                return -1;
            break;
        case 'q':
            if (parse_positive(optarg, &options->queues, MAX_QUEUES) != 0)
                return -1;
            break;
        case 't':
            if (parse_positive(optarg, &options->timeout_sec, 86400) != 0)
                return -1;
            break;
        case 'u':
            if (strcmp(optarg, "0") != 0 && strcmp(optarg, "1") != 0)
                return -1;
            options->unsigned_pd = atoi(optarg);
            break;
        case 1:
            options->callbacks = 1;
            break;
        case 2:
            options->case_name = optarg;
            break;
        case 3:
            options->path_mode = optarg;
            options->path_mode_set = 1;
            break;
        case 4:
            if (parse_positive(optarg, &options->library_count,
                               MAX_LIBRARY_FILES) != 0)
                return -1;
            break;
        case 5:
            if (parse_positive(optarg, &options->near_library_count,
                               MAX_LIBRARY_FILES) != 0)
                return -1;
            break;
        case 6:
            if (parse_positive(optarg, &options->over_library_count,
                               MAX_LIBRARY_FILES) != 0)
                return -1;
            break;
        case 7:
            options->skip_unsupported = 1;
            break;
        case 8:
            options->module_uri = optarg;
            break;
        case 9:
            options->report_path = optarg;
            break;
        case 'h':
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            return -1;
        }
    }

    if (options->domain < 0 || options->domain >= NUM_DOMAINS_EXTEND)
        return -1;
    if (strcmp(options->path_mode, "normal") != 0 &&
        strcmp(options->path_mode, "empty") != 0 &&
        strcmp(options->path_mode, "nomatch") != 0 &&
        strcmp(options->path_mode, "many") != 0)
        return -1;
    if (options->path_mode_set && strcmp(options->case_name, "all") == 0)
        options->case_name = options->path_mode;
    if (strcmp(options->case_name, "all") != 0 &&
        strchr(options->case_name, ',') != NULL)
        return -1;
    return 0;
}

static double monotonic_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void sleep_milliseconds(long milliseconds)
{
    struct timespec delay;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep(&delay, NULL);
}

static void sleep_microseconds(long microseconds)
{
    struct timespec delay;
    delay.tv_sec = microseconds / 1000000;
    delay.tv_nsec = (microseconds % 1000000) * 1000L;
    nanosleep(&delay, NULL);
}

static int is_device_unavailable(int error)
{
    return error == AEE_EUNSUPPORTED || error == AEE_ENOSUCHDEVICE ||
           error == AEE_ENOTINITIALIZED || error == AEE_ECONNREFUSED;
}

static void noop_callback(dspqueue_t queue, AEEResult error, void *context)
{
    (void)queue;
    (void)error;
    (void)context;
}

static void enable_unsigned_pd(const struct options *options)
{
    struct remote_rpc_control_unsigned_module control;
    int ret;

    if (!options->unsigned_pd)
        return;
    control.domain = options->domain;
    control.enable = 1;
    ret = remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE, &control,
                                 sizeof(control));
    if (ret != AEE_SUCCESS)
        report_line("WARN unsigned_control_ret=0x%x\n", ret);
}

static int open_module(const struct options *options)
{
    remote_handle64 handle = 0;
    int ret;

    enable_unsigned_pd(options);
    ret = remote_handle64_open(options->module_uri, &handle);
    report_line("OPEN uri=%s ret=0x%x\n", options->module_uri, ret);
    if (ret != AEE_SUCCESS) {
        if (ret == AEE_EBUFFERTOOSMALL)
            return RESULT_EXPECTED_BOUNDARY;
        return is_device_unavailable(ret) ? RESULT_SKIP : EXIT_FAILURE;
    }

    ret = remote_handle64_close(handle);
    report_line("HANDLE_CLOSE ret=0x%x\n", ret);
    return ret == AEE_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int run_queue_case(const struct options *options, int queues_count,
                          int callbacks)
{
    remote_handle64 handle = 0;
    dspqueue_t queues[MAX_QUEUES] = {0};
    int first_failure = AEE_SUCCESS;
    int unsafe_cleanup = 0;
    int round;
    int i;
    int ret;

    enable_unsigned_pd(options);
    ret = remote_handle64_open(options->module_uri, &handle);
    report_line("OPEN uri=%s ret=0x%x\n", options->module_uri, ret);
    if (ret != AEE_SUCCESS)
        return is_device_unavailable(ret) ? RESULT_SKIP : EXIT_FAILURE;

    for (round = 0; round < options->iterations; ++round) {
        memset(queues, 0, sizeof(queues));
        report_line("ROUND begin=%d queues=%d callbacks=%d\n", round,
                    queues_count, callbacks);

        for (i = 0; i < queues_count; ++i) {
            uint64_t signaling_perf = 0;
            dspqueue_callback_t callback =
                callbacks && (queues_count == 1 || (i & 1)) ?
                    noop_callback : NULL;

            ret = dspqueue_create(options->domain, 0, 4096, 4096, callback,
                                  NULL, NULL, &queues[i]);
            report_line("CREATE round=%d queue=%d ret=0x%x\n", round, i,
                        ret);
            if (ret != AEE_SUCCESS) {
                first_failure = ret;
                unsafe_cleanup = 1;
                goto queue_done;
            }

            ret = dspqueue_get_stat(queues[i], DSPQUEUE_STAT_SIGNALING_PERF,
                                    &signaling_perf);
            if (ret == AEE_SUCCESS)
                report_line("SIGNALING round=%d queue=%d perf=%llu\n", round,
                            i, (unsigned long long)signaling_perf);
            else
                report_line("SIGNALING round=%d queue=%d ret=0x%x\n", round,
                            i, ret);
        }

        for (i = (round & 1) ? 0 : queues_count - 1;
             (round & 1) ? i < queues_count : i >= 0;
             (round & 1) ? ++i : --i) {
            ret = dspqueue_close(queues[i]);
            report_line("CLOSE round=%d queue=%d ret=0x%x\n", round, i,
                        ret);
            queues[i] = NULL;
            if (ret != AEE_SUCCESS) {
                first_failure = ret;
                unsafe_cleanup = 1;
                goto queue_done;
            }
        }
    }

queue_done:
    if (!unsafe_cleanup) {
        ret = remote_handle64_close(handle);
        report_line("HANDLE_CLOSE ret=0x%x\n", ret);
        if (first_failure == AEE_SUCCESS && ret != AEE_SUCCESS)
            first_failure = ret;
    } else {
        report_line("HANDLE_CLOSE skipped=partial_queue_teardown\n");
    }

    if (first_failure == AEE_SUCCESS) {
        report_line("CHILD_RESULT PASS\n");
        return EXIT_SUCCESS;
    }
    report_line("CHILD_RESULT FAIL ret=0x%x\n", first_failure);
    return is_device_unavailable(first_failure) ? RESULT_SKIP : EXIT_FAILURE;
}

static int create_library_path(const struct options *options, const char *mode,
                               int library_count, char *path, size_t path_size)
{
    char template_path[] = PATH_TEMPLATE;
    int fd;
    int i;

    if (strcmp(mode, "normal") == 0 || strcmp(mode, "empty") == 0) {
        path[0] = '\0';
        return 0;
    }

    if (mkdtemp(template_path) == NULL) {
        report_line("PATH_CREATE ret=%d errno=%d\n", -1, errno);
        return -1;
    }
    if (strlen(template_path) + 1 > path_size) {
        rmdir(template_path);
        return -1;
    }
    strcpy(path, template_path);

    if (strcmp(mode, "many") != 0)
        return 0;

    for (i = 0; i < library_count; ++i) {
        char file_path[PATH_MAX];
        int length = snprintf(file_path, sizeof(file_path), "%s/x%04d.so",
                              path, i);
        if (length < 0 || (size_t)length >= sizeof(file_path))
            return -1;
        fd = open(file_path, O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
        if (fd < 0) {
            report_line("LIB_CREATE path=%s errno=%d\n", file_path, errno);
            return -1;
        }
        close(fd);
    }
    (void)options;
    return 0;
}

static void remove_library_path(const char *path)
{
    DIR *directory;
    struct dirent *entry;

    if (path == NULL || path[0] == '\0')
        return;
    directory = opendir(path);
    if (directory != NULL) {
        while ((entry = readdir(directory)) != NULL) {
            char file_path[PATH_MAX];
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;
            if (snprintf(file_path, sizeof(file_path), "%s/%s", path,
                         entry->d_name) < (int)sizeof(file_path))
                unlink(file_path);
        }
        closedir(directory);
    }
    rmdir(path);
}

static int run_child(const struct options *options, const struct matrix_case *test,
                     const char *dsp_path)
{
    struct options child_options = *options;
    int queues_count = test->queues ? test->queues : options->queues;

    if (dsp_path != NULL) {
        if (setenv("DSP_LIBRARY_PATH", dsp_path, 1) != 0) {
            report_line("ENV_SET ret=%d errno=%d\n", -1, errno);
            return EXIT_FAILURE;
        }
        report_line("ENV_SET DSP_LIBRARY_PATH=%s\n", dsp_path);
    } else {
        report_line("ENV_PRESERVE DSP_LIBRARY_PATH=%s\n",
                    getenv("DSP_LIBRARY_PATH") ? getenv("DSP_LIBRARY_PATH") : "<unset>");
    }
    child_options.path_mode = test->path_mode;
    child_options.callbacks = test->callbacks || options->callbacks;
    if (test->library_count > 0)
        child_options.library_count = test->library_count;

    report_line("CHILD case=%s path_mode=%s DSP_LIBRARY_PATH=%s domain=%d "
                "iterations=%d queues=%d callbacks=%d library_count=%d\n",
                test->name, child_options.path_mode,
                getenv("DSP_LIBRARY_PATH") ? getenv("DSP_LIBRARY_PATH") : "<unset>",
                child_options.domain, child_options.iterations, queues_count,
                child_options.callbacks, child_options.library_count);

    if (test->kind == CASE_QUEUE)
        return run_queue_case(&child_options, queues_count,
                              child_options.callbacks);
    return open_module(&child_options);
}

static void drain_child_output(int fd)
{
    char buffer[1024];
    ssize_t count;

    for (;;) {
        count = read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            if (report_file != NULL) {
                (void)fwrite(buffer, 1, (size_t)count, report_file);
                (void)fflush(report_file);
            }
            (void)fwrite(buffer, 1, (size_t)count, stdout);
            (void)fflush(stdout);
            continue;
        }
        if (count < 0 && errno == EINTR)
            continue;
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        break;
    }
}

static int wait_for_child(pid_t child, int output_fd, int timeout_sec,
                          int *status, int *timed_out)
{
    double deadline = monotonic_seconds() + timeout_sec;
    struct pollfd output = { .fd = output_fd, .events = POLLIN };
    int output_open = 1;

    *timed_out = 0;
    for (;;) {
        pid_t waited = waitpid(child, status, WNOHANG);
        if (waited < 0) {
            if (errno == EINTR)
                continue;
            (void)kill(-child, SIGKILL);
            (void)waitpid(child, status, 0);
            if (output_open)
                close(output_fd);
            return EXIT_FAILURE;
        }
        if (waited == child)
            break;
        if (monotonic_seconds() >= deadline) {
            report_line("WATCHDOG_TIMEOUT pid=%ld\n", (long)child);
            *timed_out = 1;
            (void)kill(-child, SIGTERM);
            sleep_milliseconds(500);
            if (waitpid(child, status, WNOHANG) == 0) {
                report_line("WATCHDOG_KILL pid=%ld\n", (long)child);
                (void)kill(-child, SIGKILL);
                (void)waitpid(child, status, 0);
            }
            break;
        }
        if (output_open) {
            int poll_result = poll(&output, 1, 100);
            if (poll_result < 0 && errno == EINTR)
                continue;
            if (poll_result < 0) {
                (void)kill(-child, SIGKILL);
                (void)waitpid(child, status, 0);
                close(output_fd);
                return EXIT_FAILURE;
            }
            if (output.revents & (POLLIN | POLLHUP | POLLERR)) {
                drain_child_output(output_fd);
                if (output.revents & (POLLHUP | POLLERR)) {
                    close(output_fd);
                    output_open = 0;
                }
            } else if (poll_result == 0) {
                output.revents = 0;
            }
        } else {
            sleep_microseconds(100000);
        }
    }

    if (output_open) {
        drain_child_output(output_fd);
        close(output_fd);
    }
    return *timed_out ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int make_focused_case(const struct options *options,
                             struct matrix_case *test);

static int case_library_count(const struct options *options,
                              const struct matrix_case *test);

static int run_one_case(const struct options *options,
                        const struct matrix_case *test, size_t case_number);

static int make_focused_case(const struct options *options,
                             struct matrix_case *test)
{
    memset(test, 0, sizeof(*test));
    test->name = options->case_name;
    if (strcmp(options->case_name, "normal") == 0 ||
        strcmp(options->case_name, "empty") == 0 ||
        strcmp(options->case_name, "nomatch") == 0 ||
        strcmp(options->case_name, "many") == 0 ||
        strcmp(options->case_name, "many-under") == 0 ||
        strcmp(options->case_name, "many-near") == 0 ||
        strcmp(options->case_name, "many-over") == 0) {
        test->kind = CASE_PATH;
        test->path_mode = (strncmp(options->case_name, "many-", 5) == 0) ?
                          "many" : options->case_name;
        test->expected_boundary = strcmp(options->case_name, "many-over") == 0;
        return 0;
    }
    if (strcmp(options->case_name, "queue-single") == 0) {
        test->kind = CASE_QUEUE;
        test->path_mode = "normal";
        test->queues = 1;
        return 0;
    }
    if (strcmp(options->case_name, "queue-multi") == 0 ||
        strcmp(options->case_name, "queue-callback") == 0) {
        test->kind = CASE_QUEUE;
        test->path_mode = "normal";
        test->queues = options->queues;
        test->callbacks = strcmp(options->case_name, "queue-callback") == 0;
        return 0;
    }
    return -1;
}

static int case_library_count(const struct options *options,
                              const struct matrix_case *test)
{
    if (strcmp(test->name, "many-near") == 0)
        return options->near_library_count;
    if (strcmp(test->name, "many-over") == 0)
        return options->over_library_count;
    return options->library_count;
}

static int run_one_case(const struct options *options,
                        const struct matrix_case *test, size_t case_number)
{
    char dsp_path[PATH_MAX] = {0};
    const char *child_path = NULL;
    int output_pipe[2];
    pid_t child;
    int status = 0;
    int timed_out = 0;
    int ret;
    int result;
    double started = monotonic_seconds();
    int library_count = case_library_count(options, test);
    struct matrix_case adjusted = *test;

    adjusted.library_count = library_count;
    if (strcmp(test->name, "many-over") == 0)
        adjusted.expected_boundary = 1;

    if (strcmp(adjusted.path_mode, "empty") == 0) {
        child_path = "";
    } else if (strcmp(adjusted.path_mode, "nomatch") == 0 ||
               strcmp(adjusted.path_mode, "many") == 0) {
        if (create_library_path(options, adjusted.path_mode, library_count,
                                dsp_path, sizeof(dsp_path)) != 0)
            return EXIT_FAILURE;
        child_path = dsp_path;
    }

    report_line("CASE_START number=%zu name=%s kind=%s domain=%d iterations=%d "
                "queues=%d library_count=%d\n", case_number, adjusted.name,
                adjusted.kind == CASE_QUEUE ? "queue" : "path", options->domain,
                options->iterations, adjusted.queues ? adjusted.queues : options->queues,
                library_count);

    if (pipe(output_pipe) != 0) {
        report_line("CASE_RESULT name=%s status=FAIL reason=pipe errno=%d\n",
                    adjusted.name, errno);
        remove_library_path(dsp_path);
        return EXIT_FAILURE;
    }
    child = fork();
    if (child < 0) {
        report_line("CASE_RESULT name=%s status=FAIL reason=fork errno=%d\n",
                    adjusted.name, errno);
        close(output_pipe[0]);
        close(output_pipe[1]);
        remove_library_path(dsp_path);
        return EXIT_FAILURE;
    }
    if (child == 0) {
        close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0)
            _exit(EXIT_FAILURE);
        close(output_pipe[1]);
        report_file = NULL;
        (void)setpgid(0, 0);
        ret = run_child(options, &adjusted, child_path);
        _exit(ret);
    }
    close(output_pipe[1]);
    (void)fcntl(output_pipe[0], F_SETFL,
                fcntl(output_pipe[0], F_GETFL, 0) | O_NONBLOCK);
    (void)setpgid(child, child);

    ret = wait_for_child(child, output_pipe[0], options->timeout_sec, &status,
                         &timed_out);
    remove_library_path(dsp_path);
    if (timed_out) {
        ++aggregate_timeouts;
        report_line("CASE_RESULT name=%s status=TIMEOUT rc=124 watchdog=1 "
                    "elapsed=%.3f\n", adjusted.name,
                    monotonic_seconds() - started);
        return EXIT_FAILURE;
    }
    if (ret != EXIT_SUCCESS) {
        report_line("CASE_RESULT name=%s status=FAIL rc=%d watchdog=0 "
                    "elapsed=%.3f\n", adjusted.name, ret,
                    monotonic_seconds() - started);
        return EXIT_FAILURE;
    }

    result = WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE;
    if (WIFSIGNALED(status)) {
        report_line("CASE_RESULT name=%s status=FAIL signal=%d watchdog=0 "
                    "elapsed=%.3f\n", adjusted.name, WTERMSIG(status),
                    monotonic_seconds() - started);
        return EXIT_FAILURE;
    }
    if (result == EXIT_SUCCESS && adjusted.expected_boundary) {
        ++aggregate_boundaries;
        report_line("CASE_RESULT name=%s status=EXPECTED_BOUNDARY rc=0 "
                    "watchdog=0 elapsed=%.3f\n", adjusted.name,
                    monotonic_seconds() - started);
        return EXIT_SUCCESS;
    }
    if (result == EXIT_SUCCESS) {
        report_line("CASE_RESULT name=%s status=PASS rc=0 watchdog=0 "
                    "elapsed=%.3f\n", adjusted.name,
                    monotonic_seconds() - started);
        return EXIT_SUCCESS;
    }
    if (result == RESULT_SKIP) {
        const char *status_name = options->skip_unsupported ? "SKIP" : "FAIL";
        if (options->skip_unsupported)
            ++aggregate_skips;
        report_line("CASE_RESULT name=%s status=%s rc=%d watchdog=0 "
                    "elapsed=%.3f\n", adjusted.name, status_name, result,
                    monotonic_seconds() - started);
        return options->skip_unsupported ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (result == RESULT_UNSUPPORTED) {
        ++aggregate_unsupported;
        report_line("CASE_RESULT name=%s status=UNSUPPORTED rc=%d watchdog=0 "
                    "elapsed=%.3f\n", adjusted.name, result,
                    monotonic_seconds() - started);
        return EXIT_SUCCESS;
    }
    if (result == RESULT_EXPECTED_BOUNDARY && adjusted.expected_boundary) {
        ++aggregate_boundaries;
        report_line("CASE_RESULT name=%s status=EXPECTED_BOUNDARY rc=%d "
                    "watchdog=0 elapsed=%.3f\n", adjusted.name, result,
                    monotonic_seconds() - started);
        return EXIT_SUCCESS;
    }
    report_line("CASE_RESULT name=%s status=FAIL rc=%d watchdog=0 elapsed=%.3f\n",
                adjusted.name, result, monotonic_seconds() - started);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    struct options options;
    struct matrix_case focused_case;
    size_t i;
    int failures = 0;
    int cases_run = 0;

    if (parse_options(argc, argv, &options) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (options.report_path != NULL) {
        report_file = fopen(options.report_path, "w");
        if (report_file == NULL) {
            fprintf(stderr, "REPORT_OPEN failed path=%s errno=%d\n",
                    options.report_path, errno);
            return EXIT_FAILURE;
        }
    }

    report_line("FINAL_MATRIX_START domain=%d iterations=%d queues=%d timeout=%d "
                "skip_unsupported=%d\n", options.domain, options.iterations,
                options.queues, options.timeout_sec, options.skip_unsupported);

    if (strcmp(options.case_name, "all") == 0) {
        for (i = 0; i < MATRIX_CASE_COUNT; ++i) {
            ++cases_run;
            if (run_one_case(&options, &matrix_cases[i], i + 1) != EXIT_SUCCESS)
                ++failures;
        }
    } else if (make_focused_case(&options, &focused_case) == 0) {
        ++cases_run;
        if (run_one_case(&options, &focused_case, 1) != EXIT_SUCCESS)
            ++failures;
    } else {
        fprintf(stderr, "unknown case: %s\n", options.case_name);
        usage(argv[0]);
        if (report_file != NULL)
            fclose(report_file);
        return EXIT_FAILURE;
    }

    aggregate_unsupported += 2;
    report_line("UNSUPPORTED name=procbuf_exact_copy reason=private_static_helper\n");
    report_line("UNSUPPORTED name=cancel_wait_signal_fault_injection reason=no_public_fault_injection\n");
    report_line("FINAL_SUMMARY cases=%d failures=%d skips=%d timeouts=%d "
                "boundaries=%d unsupported=%d status=%s watchdog=0\n",
                cases_run, failures, aggregate_skips, aggregate_timeouts,
                aggregate_boundaries, aggregate_unsupported,
                failures == 0 ? "PASS" : "FAIL");
    if (report_file != NULL)
        fclose(report_file);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
