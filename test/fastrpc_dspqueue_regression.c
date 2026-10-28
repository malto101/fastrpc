// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/*
 * Device-side regression driver for the PR #399 fuzz findings.
 *
 * Potentially blocking queue teardown runs in a child process.  The parent
 * supervises that child with a monotonic deadline and never tries to repair
 * a queue from a second thread after a timeout.
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
#define DEFAULT_MODULE_URI \
    "file:///libcalculator_skel.so?calculator_skel_handle_invoke&_modver=1.0"
#define PATH_TEMPLATE "/tmp/fpXXXXXX"
#define MAX_QUEUES 64
#define MAX_LIBRARY_FILES 10000
#define RESULT_SKIP 77

struct options {
    int domain;
    int iterations;
    int queues;
    int timeout_sec;
    int unsigned_pd;
    int callbacks;
    int library_count;
    const char *path_mode;
    const char *module_uri;
};

static void usage(const char *program)
{
    printf("Usage: %s [options]\n"
           "  -d, --domain N          DSP domain (default %d)\n"
           "  -i, --iterations N      create/close rounds (default %d)\n"
           "  -q, --queues N          queues per round, 1-%d (default %d)\n"
           "  -t, --timeout SEC       parent watchdog (default %d)\n"
           "  -u, --unsigned 0|1      enable unsigned PD (default 1)\n"
           "      --callbacks         use a no-op packet callback on alternating queues\n"
           "      --path-mode MODE    normal, empty, nomatch, or many (default normal)\n"
           "      --library-count N   .so names for many mode (default 16)\n"
           "      --module-uri URI    URI used to create the DSP process domain\n"
           "  -h, --help              show this help\n\n"
           "The module and dspqueue skeletons must be in the device search path.\n",
           program, DEFAULT_DOMAIN, DEFAULT_ITERATIONS, MAX_QUEUES,
           DEFAULT_QUEUES, DEFAULT_TIMEOUT_SEC);
}

static int parse_positive(const char *text, int *value, int maximum)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < 1 ||
        parsed > maximum) {
        return -1;
    }
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
        {"path-mode", required_argument, NULL, 2},
        {"library-count", required_argument, NULL, 3},
        {"module-uri", required_argument, NULL, 4},
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
    options->library_count = 16;
    options->path_mode = "normal";
    options->module_uri = DEFAULT_MODULE_URI;

    while ((option = getopt_long(argc, argv, "d:i:q:t:u:h", long_options,
                                 NULL)) != -1) {
        switch (option) {
        case 'd':
            options->domain = atoi(optarg);
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
            options->path_mode = optarg;
            break;
        case 3:
            if (parse_positive(optarg, &options->library_count,
                               MAX_LIBRARY_FILES) != 0)
                return -1;
            break;
        case 4:
            options->module_uri = optarg;
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
        fprintf(stderr, "WARN unsigned-PD control failed: 0x%x\n", ret);
}

static int run_child(const struct options *options, const char *dsp_path)
{
    remote_handle64 handle = 0;
    dspqueue_t queues[MAX_QUEUES] = {0};
    int ret;
    int round;
    int i;
    int unsafe_cleanup = 0;
    int first_failure = AEE_SUCCESS;

    if (dsp_path != NULL && setenv("DSP_LIBRARY_PATH", dsp_path, 1) != 0) {
        fprintf(stderr, "setenv DSP_LIBRARY_PATH failed: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    printf("CHILD path_mode=%s DSP_LIBRARY_PATH=%s domain=%d iterations=%d "
           "queues=%d\n", options->path_mode,
           getenv("DSP_LIBRARY_PATH") ? getenv("DSP_LIBRARY_PATH") : "<unset>",
           options->domain, options->iterations, options->queues);
    fflush(stdout);

    enable_unsigned_pd(options);
    ret = remote_handle64_open(options->module_uri, &handle);
    if (ret != AEE_SUCCESS) {
        fprintf(stderr, "OPEN failed: 0x%x uri=%s\n", ret,
                options->module_uri);
        return is_device_unavailable(ret) ? RESULT_SKIP : EXIT_FAILURE;
    }

    for (round = 0; round < options->iterations; ++round) {
        memset(queues, 0, sizeof(queues));
        printf("ROUND begin=%d\n", round);
        fflush(stdout);

        for (i = 0; i < options->queues; ++i) {
            uint64_t signaling_perf = 0;
            dspqueue_callback_t callback =
                options->callbacks && (i & 1) ? noop_callback : NULL;

            ret = dspqueue_create(options->domain, 0, 4096, 4096, callback,
                                  NULL, NULL, &queues[i]);
            printf("CREATE round=%d queue=%d ret=0x%x\n", round, i, ret);
            fflush(stdout);
            if (ret != AEE_SUCCESS) {
                first_failure = ret;
                /* Leave all teardown to process exit after a failed create;
                 * the library may have partially initialized domain state. */
                unsafe_cleanup = 1;
                goto child_done;
            }

            ret = dspqueue_get_stat(queues[i], DSPQUEUE_STAT_SIGNALING_PERF,
                                    &signaling_perf);
            if (ret == AEE_SUCCESS)
                printf("SIGNALING round=%d queue=%d perf=%llu\n", round, i,
                       (unsigned long long)signaling_perf);
            else
                printf("SIGNALING round=%d queue=%d ret=0x%x\n", round, i,
                       ret);
            fflush(stdout);
        }

        /* Alternate close order to exercise the shared domain list. */
        for (i = (round & 1) ? 0 : options->queues - 1;
             (round & 1) ? i < options->queues : i >= 0;
             (round & 1) ? ++i : --i) {
            ret = dspqueue_close(queues[i]);
            printf("CLOSE round=%d queue=%d ret=0x%x\n", round, i, ret);
            fflush(stdout);
            queues[i] = NULL;
            if (ret != AEE_SUCCESS) {
                /* Never retry close after a partial teardown. */
                first_failure = ret;
                unsafe_cleanup = 1;
                goto child_done;
            }
        }
    }

child_done:
    /* A failed close may have partially freed internal state.  Do not call
     * it again; process exit safely reclaims anything left in this child. */
    if (!unsafe_cleanup) {
        ret = remote_handle64_close(handle);
        printf("HANDLE_CLOSE ret=0x%x\n", ret);
        fflush(stdout);
        if (first_failure == AEE_SUCCESS && ret != AEE_SUCCESS)
            first_failure = ret;
    } else {
        printf("HANDLE_CLOSE skipped=partial_queue_teardown\n");
        fflush(stdout);
    }

    if (first_failure == AEE_SUCCESS) {
        printf("CHILD_RESULT PASS\n");
        return EXIT_SUCCESS;
    }
    printf("CHILD_RESULT FAIL ret=0x%x\n", first_failure);
    return is_device_unavailable(first_failure) ? RESULT_SKIP : EXIT_FAILURE;
}

static int create_library_path(const struct options *options, char *path,
                               size_t path_size)
{
    char template_path[] = PATH_TEMPLATE;
    int fd;
    int i;

    if (strcmp(options->path_mode, "normal") == 0 ||
        strcmp(options->path_mode, "empty") == 0) {
        path[0] = '\0';
        return 0;
    }

    if (mkdtemp(template_path) == NULL) {
        fprintf(stderr, "mkdtemp failed: %s\n", strerror(errno));
        return -1;
    }
    if (strlen(template_path) + 1 > path_size) {
        rmdir(template_path);
        return -1;
    }
    strcpy(path, template_path);

    if (strcmp(options->path_mode, "many") != 0)
        return 0;

    for (i = 0; i < options->library_count; ++i) {
        char file_path[PATH_MAX];
        int length = snprintf(file_path, sizeof(file_path), "%s/x%04d.so",
                              path, i);
        if (length < 0 || (size_t)length >= sizeof(file_path))
            return -1;
        fd = open(file_path, O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
        if (fd < 0) {
            fprintf(stderr, "create %s failed: %s\n", file_path,
                    strerror(errno));
            return -1;
        }
        close(fd);
    }
    return 0;
}

static void remove_library_path(const struct options *options,
                                const char *path)
{
    DIR *directory;
    struct dirent *entry;

    if (path == NULL || path[0] == '\0' ||
        strcmp(options->path_mode, "empty") == 0 ||
        strcmp(options->path_mode, "normal") == 0)
        return;

    directory = opendir(path);
    if (directory != NULL) {
        while ((entry = readdir(directory)) != NULL) {
            char file_path[PATH_MAX];
            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(file_path, sizeof(file_path), "%s/%s", path,
                     entry->d_name);
            unlink(file_path);
        }
        closedir(directory);
    }
    rmdir(path);
}

static int wait_for_child(pid_t child, int timeout_sec)
{
    int status;
    double deadline = monotonic_seconds() + timeout_sec;

    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            break;
        if (waited < 0) {
            perror("waitpid");
            return EXIT_FAILURE;
        }
        if (monotonic_seconds() >= deadline) {
            fprintf(stderr, "WATCHDOG_TIMEOUT pid=%ld\n", (long)child);
            kill(-child, SIGTERM);
            sleep_milliseconds(500);
            if (waitpid(child, &status, WNOHANG) == 0) {
                fprintf(stderr, "WATCHDOG_KILL pid=%ld\n", (long)child);
                kill(-child, SIGKILL);
                waitpid(child, &status, 0);
            }
            return EXIT_FAILURE;
        }
        sleep_microseconds(100000);
    }

    if (WIFEXITED(status)) {
        printf("SUPERVISOR child=%ld exit=%d\n", (long)child,
               WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
        fprintf(stderr, "SUPERVISOR child=%ld signal=%d\n", (long)child,
                WTERMSIG(status));
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    struct options options;
    char dsp_path[PATH_MAX] = {0};
    const char *child_path = NULL;
    pid_t child;
    int ret;

    if (parse_options(argc, argv, &options) != 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(options.path_mode, "empty") == 0) {
        child_path = "";
    } else if (strcmp(options.path_mode, "nomatch") == 0 ||
               strcmp(options.path_mode, "many") == 0) {
        if (create_library_path(&options, dsp_path, sizeof(dsp_path)) != 0)
            return EXIT_FAILURE;
        child_path = dsp_path;
    }

    printf("SUPERVISOR domain=%d path_mode=%s iterations=%d queues=%d "
           "timeout_sec=%d library_count=%d\n", options.domain,
           options.path_mode, options.iterations, options.queues,
           options.timeout_sec, options.library_count);
    fflush(stdout);

    child = fork();
    if (child < 0) {
        perror("fork");
        remove_library_path(&options, dsp_path);
        return EXIT_FAILURE;
    }
    if (child == 0) {
        (void)setpgid(0, 0);
        ret = run_child(&options, child_path);
        _exit(ret);
    }
    (void)setpgid(child, child);

    ret = wait_for_child(child, options.timeout_sec);
    remove_library_path(&options, dsp_path);
    return ret;
}
