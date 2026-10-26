// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "test_process.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TEST_PROCESS_DEFAULT_TIMEOUT_MS 120000u
#define TEST_PROCESS_TERM_GRACE_MS 200u
#define TEST_PROCESS_ARG_SIZE 32

static int s_worker_mode;

static int64_t monotonic_ms(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void sleep_ms(unsigned milliseconds)
{
    struct timespec delay = {
        .tv_sec = (time_t)(milliseconds / 1000),
        .tv_nsec = (long)(milliseconds % 1000) * 1000000L,
    };

    while (nanosleep(&delay, &delay) != 0 && errno == EINTR)
        ;
}

static int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0)
        return -1;
    return 0;
}

static int set_close_on_exec(int fd)
{
    int flags = fcntl(fd, F_GETFD, 0);

    if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) != 0)
        return -1;
    return 0;
}

static int move_fd_above_stdio(int *fd)
{
    int duplicate;

    if (!fd || *fd < 0)
        return -1;
    if (*fd > STDERR_FILENO)
        return 0;

    duplicate = fcntl(*fd, F_DUPFD, STDERR_FILENO + 1);
    if (duplicate < 0)
        return -1;
    close(*fd);
    *fd = duplicate;
    return 0;
}

static void close_child_fds_except(int keep_one, int keep_two, int keep_three)
{
    long maximum = sysconf(_SC_OPEN_MAX);

    if (maximum < STDERR_FILENO + 1)
        maximum = 65536;
    for (int fd = STDERR_FILENO + 1; fd < maximum; fd++) {
        if (fd != keep_one && fd != keep_two && fd != keep_three)
            close(fd);
    }
}

static int write_all(int fd, const void *data, size_t size)
{
    const unsigned char *bytes = data;

    while (size > 0) {
        ssize_t written = write(fd, bytes, size);

        if (written > 0) {
            bytes += written;
            size -= (size_t)written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            return -1;
        }
    }
    return 0;
}

static int read_pipe(int fd, unsigned char *buffer, size_t capacity, size_t *length,
                     int *truncated)
{
    for (;;) {
        unsigned char scratch[4096];
        ssize_t count = read(fd, scratch, sizeof(scratch));

        if (count > 0) {
            size_t available = capacity > *length ? capacity - *length : 0;
            size_t copied = (size_t)count < available ? (size_t)count : available;

            if (copied != 0) {
                memcpy(buffer + *length, scratch, copied);
                *length += copied;
            }
            if (copied != (size_t)count)
                *truncated = 1;
            continue;
        }
        if (count == 0)
            return 1;
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }
}

void test_process_result_init(test_process_result_t *result)
{
    if (!result)
        return;
    memset(result, 0, sizeof(*result));
    result->magic = TEST_PROCESS_PROTOCOL_MAGIC;
    result->protocol_version = TEST_PROCESS_PROTOCOL_VERSION;
    result->status = TEST_PROCESS_MALFORMED;
}

static int has_terminator(const char *value, size_t size)
{
    return memchr(value, '\0', size) != NULL;
}

int test_process_result_valid(const test_process_result_t *result)
{
    if (!result || result->magic != TEST_PROCESS_PROTOCOL_MAGIC ||
        result->protocol_version != TEST_PROCESS_PROTOCOL_VERSION ||
        result->status > TEST_PROCESS_RESULT_MAX_WORKER_STATUS ||
        result->error_count > TEST_PROCESS_MAX_ERROR_CODES ||
        (result->flags & ~TEST_PROCESS_RESULT_KNOWN_FLAGS) != 0 ||
        (result->flags & TEST_PROCESS_RESULT_FLAG_RESULT_TRUNCATED) != 0 ||
        !has_terminator(result->file, sizeof(result->file)) ||
        !has_terminator(result->message, sizeof(result->message)) ||
        result->signal_number != 0)
        return 0;

    /* A worker frame describes a normal worker return.  The supervisor
     * validates its wait status separately before accepting the frame. */
    if (result->status == TEST_PROCESS_PASS || result->status == TEST_PROCESS_IGNORE)
        return result->exit_code == 0;
    if (result->status == TEST_PROCESS_FAIL)
        return result->exit_code != 0;
    return 0;
}

int test_process_worker_write_result(int fd, const test_process_result_t *result)
{
    if (fd < 0 || !result)
        return -1;
    return write_all(fd, result, sizeof(*result));
}

int test_process_default_worker_path(char *path, size_t path_size)
{
    const char *configured = getenv("FASTRPC_TEST_WORKER");
    ssize_t length;

    if (!path || path_size == 0)
        return -1;
    path[0] = '\0';

    if (configured && configured[0] != '\0') {
        if (snprintf(path, path_size, "%s", configured) >= (int)path_size)
            return -1;
        return 0;
    }

    length = readlink("/proc/self/exe", path, path_size - 1);
    if (length <= 0 || (size_t)length >= path_size)
        return -1;
    path[length] = '\0';

    /* The parent and worker are the same executable.  The worker mode is
     * selected by private arguments after exec(), so no sibling binary is
     * required at deployment time. */
    return 0;
}

static void classify_wait_status(test_process_result_t *result, int wait_status,
                                 const test_process_request_t *request)
{
    if (WIFSIGNALED(wait_status)) {
        result->signal_number = WTERMSIG(wait_status);
        result->status = TEST_PROCESS_FAIL;
        if (result->status != TEST_PROCESS_IGNORE &&
            request->expected_signal != 0 &&
            result->signal_number == request->expected_signal &&
            request->expected_signal_policy == TEST_PROCESS_EXPECTED_SIGNAL_PASS)
            result->status = TEST_PROCESS_PASS;
        snprintf(result->message, sizeof(result->message),
                 "worker terminated by signal %d", result->signal_number);
    } else if (WIFEXITED(wait_status)) {
        result->exit_code = WEXITSTATUS(wait_status);
        if (request->expected_signal != 0) {
            result->status = TEST_PROCESS_FAIL;
            snprintf(result->message, sizeof(result->message),
                     "worker returned normally; expected signal %d",
                     request->expected_signal);
        } else if (result->exit_code != 0) {
            result->status = TEST_PROCESS_FAIL;
            if (result->message[0] == '\0')
                snprintf(result->message, sizeof(result->message),
                         "worker exited with status %d", result->exit_code);
        }
    } else {
        result->status = TEST_PROCESS_SUPERVISOR_FAILURE;
        snprintf(result->message, sizeof(result->message),
                 "unable to classify worker wait status");
    }
}

static void reap_after_termination(pid_t pid, int *status, int *waited,
                                   int *status_valid, int wait_forever)
{
    unsigned attempts = 0;
    int64_t deadline = monotonic_ms();

    if (wait_forever) {
        while (!*waited) {
            pid_t child = waitpid(pid, status, 0);

            if (child == pid) {
                *waited = 1;
                *status_valid = 1;
                return;
            }
            if (child < 0 && errno == EINTR)
                continue;
            if (child < 0 && errno == ECHILD) {
                *waited = 1;
                return;
            }
            return;
        }
        return;
    }

    if (deadline >= 0)
        deadline += TEST_PROCESS_TERM_GRACE_MS;
    while (!*waited && attempts++ < TEST_PROCESS_TERM_GRACE_MS / 10 &&
           (deadline < 0 || monotonic_ms() < deadline)) {
        pid_t child = waitpid(pid, status, WNOHANG);

        if (child == pid) {
            *waited = 1;
            *status_valid = 1;
            return;
        }
        if (child < 0 && errno == EINTR)
            continue;
        if (child < 0 && errno == ECHILD) {
            *waited = 1;
            return;
        }
        if (child < 0)
            return;
        sleep_ms(10);
    }
}

static void signal_worker(pid_t pid, int group_established, int signal_number,
                          int signal_direct_worker)
{
    if (pid <= 0)
        return;

    if (group_established)
        (void)kill(-pid, signal_number);
    if (signal_direct_worker)
        (void)kill(pid, signal_number);
}

static void terminate_group(pid_t pid, int group_established, int *status, int *waited,
                            int *status_valid)
{
    int64_t deadline;
    unsigned attempts = 0;

    signal_worker(pid, group_established, SIGTERM, !*waited);

    deadline = monotonic_ms();
    if (deadline >= 0)
        deadline += TEST_PROCESS_TERM_GRACE_MS;

    while (!*waited && attempts++ < TEST_PROCESS_TERM_GRACE_MS / 10 &&
           (deadline < 0 || monotonic_ms() < deadline)) {
        pid_t child = waitpid(pid, status, WNOHANG);

        if (child == pid) {
            *waited = 1;
            *status_valid = 1;
            break;
        }
        if (child < 0 && errno == EINTR)
            continue;
        if (child < 0 && errno == ECHILD) {
            *waited = 1;
            break;
        }
        if (child < 0)
            break;
        sleep_ms(10);
    }

    /* Descendants may still hold inherited pipe descriptors after the direct
     * child exits. Kill the private group and then reap the direct child. */
    signal_worker(pid, group_established, SIGKILL, !*waited);
    reap_after_termination(pid, status, waited, status_valid, 1);
}

static void child_report_error(int fd, int error_number)
{
    /* This function runs only in the post-fork child and uses no allocation. */
    (void)write_all(fd, &error_number, sizeof(error_number));
}

int test_process_run(const test_process_request_t *request,
                     test_process_outcome_t *outcome)
{
    int result_pipe[2] = { -1, -1 };
    int output_pipe[2] = { -1, -1 };
    int error_pipe[2] = { -1, -1 };
    pid_t pid = -1;
    int status = 0;
    int waited = 0;
    int status_valid = 0;
    int group_established = 0;
    int timed_out = 0;
    int launch_error = 0;
    int supervisor_error = 0;
    int result_closed = 0;
    int result_eof = 0;
    int output_closed = 0;
    int error_closed = 0;
    int result_overflow = 0;
    size_t result_bytes = 0;
    size_t error_bytes = 0;
    unsigned char result_data[sizeof(test_process_result_t) + 1];
    unsigned char error_data[sizeof(int)];
    unsigned char output_data[TEST_PROCESS_MAX_OUTPUT];
    size_t output_length = 0;
    int output_truncated = 0;
    int error_truncated = 0;
    int64_t deadline;
    char domain_arg[TEST_PROCESS_ARG_SIZE];
    char unsigned_arg[TEST_PROCESS_ARG_SIZE];
    char result_fd_arg[TEST_PROCESS_ARG_SIZE];
    char *child_argv[TEST_PROCESS_ARG_SIZE];
    unsigned int child_argc = 0;

    if (!request || !outcome || !request->worker_path || !request->group || !request->name)
        return -1;

    memset(outcome, 0, sizeof(*outcome));
    test_process_result_init(&outcome->result);

    snprintf(domain_arg, sizeof(domain_arg), "%d", request->domain_id);
    snprintf(unsigned_arg, sizeof(unsigned_arg), "%d", request->unsigned_pd ? 1 : 0);
    child_argv[child_argc++] = (char *)request->worker_path;
    child_argv[child_argc++] = (char *)"--base-test-worker";
    child_argv[child_argc++] = (char *)"--base-test-worker-domain";
    child_argv[child_argc++] = domain_arg;
    child_argv[child_argc++] = (char *)"--base-test-worker-unsigned";
    child_argv[child_argc++] = unsigned_arg;
    child_argv[child_argc++] = (char *)"--base-test-worker-group";
    child_argv[child_argc++] = (char *)request->group;
    child_argv[child_argc++] = (char *)"--base-test-worker-name";
    child_argv[child_argc++] = (char *)request->name;
    child_argv[child_argc++] = (char *)"--base-test-worker-result-fd";
    child_argv[child_argc++] = result_fd_arg;
    child_argv[child_argc] = NULL;

    if (pipe(result_pipe) != 0 || pipe(output_pipe) != 0 || pipe(error_pipe) != 0 ||
        move_fd_above_stdio(&result_pipe[0]) != 0 ||
        move_fd_above_stdio(&result_pipe[1]) != 0 ||
        move_fd_above_stdio(&output_pipe[0]) != 0 ||
        move_fd_above_stdio(&output_pipe[1]) != 0 ||
        move_fd_above_stdio(&error_pipe[0]) != 0 ||
        move_fd_above_stdio(&error_pipe[1]) != 0 ||
        set_close_on_exec(result_pipe[0]) != 0 ||
        set_close_on_exec(output_pipe[0]) != 0 ||
        set_close_on_exec(error_pipe[0]) != 0 ||
        set_close_on_exec(error_pipe[1]) != 0)
        goto launch_failure;

    snprintf(result_fd_arg, sizeof(result_fd_arg), "%d", result_pipe[1]);
    pid = fork();
    if (pid < 0)
        goto launch_failure;
    if (pid == 0) {
        int child_error;

        if (setpgid(0, 0) != 0) {
            child_error = errno;
            child_report_error(error_pipe[1], child_error);
            _exit(126);
        }
        close(result_pipe[0]);
        close(output_pipe[0]);
        close(error_pipe[0]);
        close_child_fds_except(result_pipe[1], output_pipe[1], error_pipe[1]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0) {
            child_error = errno;
            child_report_error(error_pipe[1], child_error);
            _exit(126);
        }
        if (dup2(output_pipe[1], STDERR_FILENO) < 0) {
            child_error = errno;
            child_report_error(error_pipe[1], child_error);
            _exit(126);
        }
        close(output_pipe[1]);
        execv(request->worker_path, child_argv);
        child_error = errno;
        child_report_error(error_pipe[1], child_error);
        _exit(127);
    }

    if (setpgid(pid, pid) == 0) {
        group_established = 1;
    } else if (errno == EACCES) {
        int group_probe = kill(-pid, 0);

        if (group_probe == 0 || errno == EPERM)
            group_established = 1; /* The child established its group before exec. */
        else
            supervisor_error = 1;
    } else {
        supervisor_error = 1;
    }
    close(result_pipe[1]);
    result_pipe[1] = -1;
    close(output_pipe[1]);
    output_pipe[1] = -1;
    close(error_pipe[1]);
    error_pipe[1] = -1;

    if (set_nonblocking(result_pipe[0]) != 0 || set_nonblocking(output_pipe[0]) != 0 ||
        set_nonblocking(error_pipe[0]) != 0) {
        supervisor_error = 1;
        terminate_group(pid, group_established, &status, &waited, &status_valid);
        goto collect_done;
    }

    deadline = monotonic_ms();
    if (deadline < 0) {
        supervisor_error = 1;
        terminate_group(pid, group_established, &status, &waited, &status_valid);
        goto collect_done;
    }
    deadline += request->timeout_ms ? request->timeout_ms : TEST_PROCESS_DEFAULT_TIMEOUT_MS;

    while (!waited || !result_closed || !output_closed || !error_closed) {
        struct pollfd fds[3];
        nfds_t nfds = 0;
        int poll_timeout = 50;
        int64_t now = monotonic_ms();

        if (now < 0) {
            supervisor_error = 1;
            terminate_group(pid, group_established, &status, &waited, &status_valid);
            break;
        }
        if (now >= deadline) {
            timed_out = 1;
            terminate_group(pid, group_established, &status, &waited, &status_valid);
            break;
        }
        if (deadline - now < poll_timeout)
            poll_timeout = (int)(deadline - now);

        if (!waited) {
            pid_t child = waitpid(pid, &status, WNOHANG);

            if (child == pid) {
                waited = 1;
                status_valid = 1;
            } else if (child < 0 && errno != EINTR) {
                supervisor_error = 1;
                if (errno == ECHILD)
                    waited = 1;
                break;
            }
        }

        if (!result_closed) {
            fds[nfds].fd = result_pipe[0];
            fds[nfds].events = POLLIN;
            fds[nfds++].revents = 0;
        }
        if (!output_closed) {
            fds[nfds].fd = output_pipe[0];
            fds[nfds].events = POLLIN;
            fds[nfds++].revents = 0;
        }
        if (!error_closed) {
            fds[nfds].fd = error_pipe[0];
            fds[nfds].events = POLLIN;
            fds[nfds++].revents = 0;
        }

        if (nfds != 0) {
            int poll_result = poll(fds, nfds, poll_timeout);
            if (poll_result < 0 && errno != EINTR) {
                supervisor_error = 1;
                terminate_group(pid, group_established, &status, &waited, &status_valid);
                break;
            }
        } else {
            (void)poll(NULL, 0, poll_timeout);
        }

        {
            nfds_t index = 0;

            if (!result_closed) {
                if (fds[index].revents & (POLLIN | POLLHUP | POLLERR)) {
                    unsigned char scratch[4096];
                    ssize_t count = read(result_pipe[0], scratch, sizeof(scratch));

                    if (count > 0) {
                        size_t available = sizeof(result_data) - result_bytes;
                        size_t copied = (size_t)count < available ? (size_t)count : available;

                        if (copied != 0) {
                            memcpy(result_data + result_bytes, scratch, copied);
                            result_bytes += copied;
                        }
                        if (copied != (size_t)count || result_bytes > sizeof(test_process_result_t)) {
                            result_overflow = 1;
                            close(result_pipe[0]);
                            result_pipe[0] = -1;
                            result_closed = 1;
                        }
                    } else if (count == 0) {
                        close(result_pipe[0]);
                        result_pipe[0] = -1;
                        result_closed = 1;
                        result_eof = 1;
                    } else if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                        supervisor_error = 1;
                        close(result_pipe[0]);
                        result_pipe[0] = -1;
                        result_closed = 1;
                    }
                }
                index++;
            }
            if (!output_closed) {
                if (fds[index].revents & (POLLIN | POLLHUP | POLLERR)) {
                    int read_result = read_pipe(output_pipe[0], output_data,
                                                sizeof(output_data), &output_length,
                                                &output_truncated);
                    if (read_result < 0) {
                        supervisor_error = 1;
                        close(output_pipe[0]);
                        output_pipe[0] = -1;
                        output_closed = 1;
                    } else if (read_result != 0) {
                        close(output_pipe[0]);
                        output_pipe[0] = -1;
                        output_closed = 1;
                    }
                }
                index++;
            }
            if (!error_closed) {
                if (fds[index].revents & (POLLIN | POLLHUP | POLLERR)) {
                    int read_result = read_pipe(error_pipe[0], error_data, sizeof(error_data),
                                                &error_bytes, &error_truncated);
                    if (read_result < 0) {
                        supervisor_error = 1;
                        close(error_pipe[0]);
                        error_pipe[0] = -1;
                        error_closed = 1;
                    }
                    if (error_bytes != 0)
                        launch_error = 1;
                    if (read_result != 0) {
                        close(error_pipe[0]);
                        error_pipe[0] = -1;
                        error_closed = 1;
                    }
                }
            }
        }

        if (supervisor_error) {
            terminate_group(pid, group_established, &status, &waited, &status_valid);
            break;
        }
    }

collect_done:
    if (!waited && !timed_out && !supervisor_error)
        reap_after_termination(pid, &status, &waited, &status_valid, 0);

    if (result_pipe[0] >= 0)
        close(result_pipe[0]);
    if (output_pipe[0] >= 0)
        close(output_pipe[0]);
    if (error_pipe[0] >= 0)
        close(error_pipe[0]);

    outcome->output = malloc(output_length + 1);
    if (!outcome->output) {
        outcome->result.status = TEST_PROCESS_SUPERVISOR_FAILURE;
        snprintf(outcome->result.message, sizeof(outcome->result.message),
                 "unable to allocate worker output buffer");
        return 0;
    }
    memcpy(outcome->output, output_data, output_length);
    outcome->output[output_length] = '\0';
    outcome->output_length = output_length;
    outcome->output_truncated = output_truncated;

    if (timed_out) {
        unsigned timeout_ms = request->timeout_ms ? request->timeout_ms
                                                   : TEST_PROCESS_DEFAULT_TIMEOUT_MS;
        outcome->result.status = TEST_PROCESS_TIMEOUT;
        snprintf(outcome->result.message, sizeof(outcome->result.message),
                 "worker timed out after %u ms", timeout_ms);
    } else if (supervisor_error) {
        outcome->result.status = TEST_PROCESS_SUPERVISOR_FAILURE;
        snprintf(outcome->result.message, sizeof(outcome->result.message),
                 "worker supervisor I/O or wait failure");
    } else if (launch_error || error_truncated) {
        int error_number = EIO;

        if (error_bytes >= sizeof(error_number))
            memcpy(&error_number, error_data, sizeof(error_number));
        outcome->result.status = TEST_PROCESS_LAUNCH_FAILURE;
        snprintf(outcome->result.message, sizeof(outcome->result.message),
                 "worker launch failed: %s", strerror(error_number));
    } else if (result_bytes == 0 && status_valid && WIFSIGNALED(status)) {
        /* A signal normally prevents the worker from writing its frame.  An
         * empty pipe is therefore the expected representation of a signal
         * outcome; partial or invalid bytes below remain malformed. */
        test_process_result_init(&outcome->result);
        classify_wait_status(&outcome->result, status, request);
    } else {
        test_process_result_t candidate;

        memset(&candidate, 0, sizeof(candidate));
        if (result_bytes == sizeof(candidate) && !result_overflow) {
            memcpy(&candidate, result_data, sizeof(candidate));
        }
        if (result_bytes == sizeof(candidate) && result_eof && !result_overflow &&
            test_process_result_valid(&candidate)) {
            outcome->result = candidate;
            if (!status_valid) {
                outcome->result.status = TEST_PROCESS_SUPERVISOR_FAILURE;
                snprintf(outcome->result.message, sizeof(outcome->result.message),
                         "worker wait status was unavailable");
            } else if (!WIFEXITED(status) ||
                       outcome->result.exit_code != WEXITSTATUS(status) ||
                       ((outcome->result.status == TEST_PROCESS_PASS ||
                         outcome->result.status == TEST_PROCESS_IGNORE) &&
                        WEXITSTATUS(status) != 0) ||
                       (outcome->result.status == TEST_PROCESS_FAIL &&
                        WEXITSTATUS(status) == 0)) {
                outcome->result.status = TEST_PROCESS_MALFORMED;
                snprintf(outcome->result.message, sizeof(outcome->result.message),
                         "worker result frame disagreed with wait status");
            } else {
                /* Apply expected-signal policy even when a normal worker frame
                 * was emitted; returning normally cannot satisfy that policy. */
                classify_wait_status(&outcome->result, status, request);
            }
        } else {
            outcome->result.status = TEST_PROCESS_MALFORMED;
            outcome->result.flags |= result_overflow ? TEST_PROCESS_RESULT_FLAG_RESULT_TRUNCATED : 0;
            snprintf(outcome->result.message, sizeof(outcome->result.message),
                     "worker returned an incomplete or invalid result frame");
        }
    }
    if (output_truncated)
        outcome->result.flags |= TEST_PROCESS_RESULT_FLAG_OUTPUT_TRUNCATED;
    return 0;

launch_failure:
    if (pid > 0)
        terminate_group(pid, group_established, &status, &waited, &status_valid);
    if (result_pipe[0] >= 0)
        close(result_pipe[0]);
    if (result_pipe[1] >= 0)
        close(result_pipe[1]);
    if (output_pipe[0] >= 0)
        close(output_pipe[0]);
    if (output_pipe[1] >= 0)
        close(output_pipe[1]);
    if (error_pipe[0] >= 0)
        close(error_pipe[0]);
    if (error_pipe[1] >= 0)
        close(error_pipe[1]);
    test_process_result_init(&outcome->result);
    outcome->result.status = TEST_PROCESS_LAUNCH_FAILURE;
    snprintf(outcome->result.message, sizeof(outcome->result.message),
             "unable to launch worker '%s'", request && request->worker_path
                                                   ? request->worker_path : "(null)");
    return 0;
}

void test_process_outcome_free(test_process_outcome_t *outcome)
{
    if (!outcome)
        return;
    free(outcome->output);
    memset(outcome, 0, sizeof(*outcome));
}

void test_process_set_worker_mode(int enabled)
{
    s_worker_mode = enabled ? 1 : 0;
}

int test_process_is_worker(void)
{
    return s_worker_mode;
}
