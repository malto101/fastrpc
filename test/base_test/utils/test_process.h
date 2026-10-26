// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BASE_TEST_PROCESS_H
#define BASE_TEST_PROCESS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEST_PROCESS_PROTOCOL_MAGIC 0x42545052u /* "BTPR" */
#define TEST_PROCESS_PROTOCOL_VERSION 1u
#define TEST_PROCESS_MAX_MESSAGE 256u
#define TEST_PROCESS_MAX_FILE 256u
#define TEST_PROCESS_MAX_ERROR_CODES 32u
#define TEST_PROCESS_MAX_OUTPUT (64u * 1024u)

typedef enum {
    TEST_PROCESS_PASS = 0,
    TEST_PROCESS_FAIL = 1,
    TEST_PROCESS_IGNORE = 2,
    TEST_PROCESS_TIMEOUT = 3,
    TEST_PROCESS_LAUNCH_FAILURE = 4,
    TEST_PROCESS_MALFORMED = 5,
    TEST_PROCESS_SUPERVISOR_FAILURE = 6,
} test_process_status_t;

enum {
    TEST_PROCESS_RESULT_FLAG_OUTPUT_TRUNCATED = 1u << 0,
    TEST_PROCESS_RESULT_FLAG_RESULT_TRUNCATED = 1u << 1,
};

#define TEST_PROCESS_RESULT_KNOWN_FLAGS \
    (TEST_PROCESS_RESULT_FLAG_OUTPUT_TRUNCATED | TEST_PROCESS_RESULT_FLAG_RESULT_TRUNCATED)
#define TEST_PROCESS_RESULT_MAX_WORKER_STATUS TEST_PROCESS_IGNORE

/* Result frames use only worker-owned statuses; supervisor-only statuses are
 * synthesized after the frame has been read. */



typedef enum {
    TEST_PROCESS_EXPECTED_SIGNAL_NONE = 0,
    TEST_PROCESS_EXPECTED_SIGNAL_PASS = 1,
    TEST_PROCESS_EXPECTED_SIGNAL_FAIL = 2,
} test_process_expected_signal_policy_t;

/* This frame is deliberately pointer-free and small enough for one pipe write. */
typedef struct {
    uint32_t magic;
    uint32_t protocol_version;
    uint32_t status;
    int32_t exit_code;
    int32_t signal_number;
    uint32_t failure_line;
    uint32_t error_count;
    uint32_t flags;
    char file[TEST_PROCESS_MAX_FILE];
    char message[TEST_PROCESS_MAX_MESSAGE];
    int32_t error_codes[TEST_PROCESS_MAX_ERROR_CODES];
} test_process_result_t;

typedef struct {
    const char *worker_path;
    int domain_id;
    int unsigned_pd;
    const char *group;
    const char *name;
    unsigned timeout_ms;
    int expected_signal;
    test_process_expected_signal_policy_t expected_signal_policy;
} test_process_request_t;

typedef struct {
    test_process_result_t result;
    char *output;
    size_t output_length;
    int output_truncated;
} test_process_outcome_t;

void test_process_result_init(test_process_result_t *result);
int test_process_result_valid(const test_process_result_t *result);
int test_process_worker_write_result(int fd, const test_process_result_t *result);

/* Returns FASTRPC_TEST_WORKER when set, otherwise the current executable path.
 * The current executable is re-execed with private worker arguments. */
int test_process_default_worker_path(char *path, size_t path_size);

/* Runs exactly one group/name case in a fresh exec'ed process. */
int test_process_run(const test_process_request_t *request,
                     test_process_outcome_t *outcome);
void test_process_outcome_free(test_process_outcome_t *outcome);

/* Worker mode is process-local and is set only by the worker entry point. */
void test_process_set_worker_mode(int enabled);
int test_process_is_worker(void);

#ifdef __cplusplus
}
#endif

#endif /* BASE_TEST_PROCESS_H */
