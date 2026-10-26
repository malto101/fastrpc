// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file root_all_tests.c
 * @brief Root dispatcher for the standalone base-test binary.
 */

#include "reporting/unity/unity_fixture_file_output.h"
#include "test_process.h"
#include "test_utils.h"
#include "unity_fixture.h"
#include "unity_internals.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void run_e2e_suite_tests(void);

static void run_all_tests(void)
{
    run_e2e_suite_tests();
}

static int parse_worker_value(const char **argv, int argc, int *index,
                              const char **value)
{
    if (!argv || !index || !value || *index + 1 >= argc)
        return -1;
    *value = argv[++*index];
    if (!*value || (*value)[0] == '\0' || (*value)[0] == '-')
        return -1;
    return 0;
}

static int parse_worker_flag(const char *value, int *seen)
{
    if (!value || !seen || *seen)
        return -1;
    *seen = 1;
    return 0;
}

static int parse_worker_arguments(int argc, const char *argv[],
                                  const char **domain, const char **unsigned_pd,
                                  const char **group, const char **name,
                                  const char **result_fd)
{
    int seen_domain = 0;
    int seen_unsigned = 0;
    int seen_group = 0;
    int seen_name = 0;
    int seen_result_fd = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--base-test-worker") == 0)
            continue;
        if (strcmp(argv[i], "--base-test-worker-domain") == 0) {
            if (parse_worker_flag(argv[i], &seen_domain) != 0 ||
                parse_worker_value(argv, argc, &i, domain) != 0)
                return -1;
        } else if (strcmp(argv[i], "--base-test-worker-unsigned") == 0) {
            if (parse_worker_flag(argv[i], &seen_unsigned) != 0 ||
                parse_worker_value(argv, argc, &i, unsigned_pd) != 0)
                return -1;
        } else if (strcmp(argv[i], "--base-test-worker-group") == 0) {
            if (parse_worker_flag(argv[i], &seen_group) != 0 ||
                parse_worker_value(argv, argc, &i, group) != 0)
                return -1;
        } else if (strcmp(argv[i], "--base-test-worker-name") == 0) {
            if (parse_worker_flag(argv[i], &seen_name) != 0 ||
                parse_worker_value(argv, argc, &i, name) != 0)
                return -1;
        } else if (strcmp(argv[i], "--base-test-worker-result-fd") == 0) {
            if (parse_worker_flag(argv[i], &seen_result_fd) != 0 ||
                parse_worker_value(argv, argc, &i, result_fd) != 0)
                return -1;
        } else {
            return -1;
        }
    }
    return seen_domain && seen_unsigned && seen_group && seen_name && seen_result_fd ? 0 : -1;
}

static int run_worker(int argc, const char *argv[])
{
    const char *domain = NULL;
    const char *unsigned_pd = NULL;
    const char *group = NULL;
    const char *name = NULL;
    const char *result_fd_text = NULL;
    const char *worker_argv[11];
    int worker_argc = 0;
    int result_fd;
    char *result_fd_end = NULL;
    char *domain_end = NULL;
    char *unsigned_end = NULL;
    long result_fd_value;
    long domain_id;
    long unsigned_value;

    int filtered_argc;
    const char **filtered_argv;
    int unity_result;
    test_process_result_t result;
    int tests_run;
    int test_ignores;

    if (parse_worker_arguments(argc, argv, &domain, &unsigned_pd, &group, &name,
                               &result_fd_text) != 0)
        return 2;

    if (!domain || !unsigned_pd || !group || !name || !result_fd_text)
        return 2;

    errno = 0;
    result_fd_value = strtol(result_fd_text, &result_fd_end, 10);
    if (errno != 0 || !result_fd_end || *result_fd_end != '\0' ||
        result_fd_value < 0 || result_fd_value > INT_MAX)
        return 2;
    result_fd = (int)result_fd_value;

    errno = 0;
    domain_id = strtol(domain, &domain_end, 10);
    if (errno != 0 || !domain_end || *domain_end != '\0' ||
        domain_id < INT_MIN || domain_id > INT_MAX)
        return 2;

    errno = 0;
    unsigned_value = strtol(unsigned_pd, &unsigned_end, 10);
    if (errno != 0 || !unsigned_end || *unsigned_end != '\0' ||
        (unsigned_value != 0 && unsigned_value != 1))
        return 2;

    if (unity_test_case_metadata_validate() != 0)
        return 2;

    worker_argv[worker_argc++] = "fastrpc_test";
    worker_argv[worker_argc++] = "-d";
    worker_argv[worker_argc++] = domain;
    worker_argv[worker_argc++] = "-u";
    worker_argv[worker_argc++] = unsigned_pd;
    worker_argv[worker_argc++] = "-G";
    worker_argv[worker_argc++] = group;
    worker_argv[worker_argc++] = "-N";
    worker_argv[worker_argc++] = name;
    worker_argv[worker_argc] = NULL;

    test_process_set_worker_mode(1);
    test_process_result_init(&result);
    if (test_config_init(worker_argc, worker_argv, &filtered_argc, &filtered_argv) != 0) {
        result.status = TEST_PROCESS_FAIL;
        result.exit_code = 1;
        snprintf(result.message, sizeof(result.message), "worker configuration failed");

        (void)test_process_worker_write_result(result_fd, &result);
        close(result_fd);
        return 1;
    }
    /* test_config_init() stores explicit domains in domain_ids[]. The worker
     * has exactly one selected domain, so make it current before suite setup
     * and test code consult g_test_config.domain_id. */
    g_test_config.domain_id = (int)domain_id;

    /* The worker deliberately does not enable the custom parent runner. */
    unity_result = UnityMain(filtered_argc, filtered_argv, run_all_tests);
    tests_run = Unity.NumberOfTests;
    test_ignores = Unity.TestIgnores;
    if (unity_result == 0 && tests_run == 0) {
        /* A selected worker that never reached its test body indicates suite
         * setup/configuration failure, not a passing case. */
        unity_result = 1;
        snprintf(result.message, sizeof(result.message),
                 "worker suite setup failed before test execution");
    }
    result.status = unity_result != 0 ? TEST_PROCESS_FAIL :
                    (test_ignores != 0 ? TEST_PROCESS_IGNORE : TEST_PROCESS_PASS);
    result.exit_code = result.status == TEST_PROCESS_FAIL ? 1 : 0;
    result.failure_line = Unity.CurrentTestLineNumber;
    if (Unity.TestFile)
        snprintf(result.file, sizeof(result.file), "%s", Unity.TestFile);
    if (Unity.CurrentDetail1 && result.message[0] == '\0')
        snprintf(result.message, sizeof(result.message), "%s", Unity.CurrentDetail1);
    {
        int code_count = 0;
        const int *codes = test_utils_get_accumulated_error_codes(&code_count);
        if (code_count > (int)TEST_PROCESS_MAX_ERROR_CODES)
            code_count = (int)TEST_PROCESS_MAX_ERROR_CODES;
        result.error_count = (uint32_t)code_count;
        for (int i = 0; i < code_count; i++)
            result.error_codes[i] = codes[i];
        if (test_utils_error_codes_truncated())
            result.flags |= TEST_PROCESS_RESULT_FLAG_ERROR_CODES_TRUNCATED;
    }
    (void)test_process_worker_write_result(result_fd, &result);
    close(result_fd);
    test_config_free_argv(filtered_argv);
    return result.status == TEST_PROCESS_PASS || result.status == TEST_PROCESS_IGNORE ? 0 : 1;
}

static int is_worker_invocation(int argc, const char *argv[])
{
    return argc > 1 && strcmp(argv[1], "--base-test-worker") == 0;
}

static int arguments_request_help(int argc, const char *argv[])
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            return 1;
    }
    return 0;
}

static int parse_timeout(const char *value, unsigned *timeout_ms)
{
    char *end = NULL;
    unsigned long parsed;

    if (!value || !timeout_ms || value[0] == '\0')
        return -1;
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || !end || *end != '\0' || parsed == 0 || parsed > UINT_MAX)
        return -1;
    *timeout_ms = (unsigned)parsed;
    return 0;
}

static int strip_process_options(int argc, const char *argv[], int *out_argc,
                                 const char ***out_argv)
{
    const char **filtered;
    int count = 0;

    if (!out_argc || !out_argv || argc < 1)
        return -1;
    filtered = malloc(((size_t)argc + 1U) * sizeof(*filtered));
    if (!filtered)
        return -1;

    g_test_config.isolation_process = 1;
    g_test_config.timeout_ms = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--isolation") == 0) {
            const char *mode;

            if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                free(filtered);
                return -1;
            }
            mode = argv[++i];
            if (strcmp(mode, "process") == 0)
                g_test_config.isolation_process = 1;
            else if (strcmp(mode, "in-process") == 0)
                g_test_config.isolation_process = 0;
            else {
                free(filtered);
                return -1;
            }
        } else if (strcmp(argv[i], "--timeout-ms") == 0) {
            if (i + 1 >= argc ||
                parse_timeout(argv[++i], &g_test_config.timeout_ms) != 0) {
                free(filtered);
                return -1;
            }
        } else {
            filtered[count++] = argv[i];
        }
    }
    filtered[count] = NULL;
    *out_argc = count;
    *out_argv = filtered;
    return 0;
}

static int run_base_tests(int argc, const char *argv[])
{
    int process_argc;
    const char **process_argv;
    int result = 0;
    unsigned int summary_total = 0;
    unsigned int summary_failures = 0;
    unsigned int summary_ignored = 0;
    int filtered_argc;
    const char **filtered_argv;

    if (arguments_request_help(argc, argv)) {
        fprintf(stdout,
                "FastRPC base tests\n\n"
                "  -d DOMAIN         select a DSP domain (repeatable)\n"
                "  -u 0|1            disable/enable unsigned PD\n"
                "  --isolation MODE  process or in-process\n"
                "  --timeout-ms MS   bound isolated tests\n"
                "  --silent          suppress passing-test output\n"
                "  --any-tags TAG    select tests matching any tag\n"
                "  --all-tags TAG    select tests matching all tags\n"
                "  -l MODE           list tests, groups, or tags\n"
                "  -r COUNT          repeat selected tests\n");
        return 0;
    }
    if (unity_test_case_metadata_validate() != 0) {
        fprintf(stderr, "[metadata] invalid test metadata; refusing to run\n");
        return 2;
    }
    if (strip_process_options(argc, argv, &process_argc, &process_argv) != 0)
        return 2;
    if (test_config_init(process_argc, process_argv, &filtered_argc, &filtered_argv) != 0) {
        free((void *)process_argv);
        return 2;
    }
    free((void *)process_argv);

    if (g_test_config.list_mode != TEST_LIST_NONE) {
        if (UnityGetCommandLineOptions(filtered_argc, filtered_argv) != 0) {
            test_config_free_argv(filtered_argv);
            return 2;
        }
        switch (g_test_config.list_mode) {
        case TEST_LIST_TESTS:
            result = unity_test_case_registry_print_tests();
            break;
        case TEST_LIST_GROUPS:
            result = unity_test_case_registry_print_groups();
            break;
        case TEST_LIST_TAGS:
            result = unity_test_case_registry_print_tags();
            break;
        case TEST_LIST_NONE:
            break;
        }
        test_config_free_argv(filtered_argv);
        return result == 0 ? 0 : 2;
    }

    if (UnityFixtureFileOutputBegin(NULL, NULL) != 0)
        fprintf(stderr, "Warning: Failed to initialize custom Unity output\n");

    for (int i = 0; i < g_test_config.domain_count; i++) {
        int domain_result;
        const char *domain_name;

        g_test_config.domain_id = g_test_config.domain_ids[i];
        domain_name = test_utils_domain_name();
        printf("\n[test_config] running tests on %s domain (%d)\n",
               domain_name ? domain_name : "<invalid>", g_test_config.domain_id);
        unity_test_case_dispatch_reset();
        domain_result = UnityMainWithFileOutput(filtered_argc, filtered_argv, run_all_tests);
        summary_total += Unity.NumberOfTests;
        summary_failures += Unity.TestFailures;
        summary_ignored += Unity.TestIgnores;
        unity_test_case_dispatch_reset();
        result += domain_result;
    }

    unity_test_case_set_summary(summary_total, summary_failures, summary_ignored);
    UnityFixtureFileOutputEnd();
    test_config_free_argv(filtered_argv);
    return result;
}

int main(int argc, const char *argv[])
{
    if (is_worker_invocation(argc, argv))
        return run_worker(argc, argv);
    return run_base_tests(argc, argv);
}
