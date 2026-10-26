// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include "AEEStdErr.h"
#include "fastrpc_common.h"
#include "remote.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#ifndef NUM_DOMAINS
#define NUM_DOMAINS 8
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Override at compile time: -DSKEL_SEARCH_PATH=\"/my/path\"
 * fastrpc uses ';' as the path delimiter (not ':'). */
#ifndef SKEL_SEARCH_PATH
#define SKEL_SEARCH_PATH "/usr/lib/rfsa/adsp;/usr/share;/usr/local/share"
#endif

/* Buffer size for the generated FastRPC URI plus its domain suffix. */
#define TEST_UTILS_DOMAIN_URI_BUFFER_SIZE 512U

/* Maximum number of tag values accepted per tag flag on the command line. */
#define TEST_CONFIG_MAX_TAGS 32

/* Maximum number of error codes retained for one test case. */
#define TEST_UTILS_MAX_ERROR_CODES 32
#define TEST_UTILS_ERROR_CODES_TRUNCATED 1
#define TEST_UTILS_ERROR_CODES_COMPLETE 0
#define TEST_UTILS_INVALID_ERROR_CODE INT_MIN

/* domain_ids[] has room for every FastRPC domain ID.  The set of named
 * domains supported by this test is defined by the metadata table in
 * test_utils.c. */


typedef enum {
    TEST_LIST_NONE = 0,
    TEST_LIST_TESTS,
    TEST_LIST_GROUPS,
    TEST_LIST_TAGS,
} test_list_mode_t;

/* Process-wide settings populated by main() from CLI args.
 *
 *   domain_id   : DSP domain for the current test run.
 *
 *   domain_ids[]: Domains selected for execution. With no -d argument this
 *                 is populated by probing running remoteproc instances and
 *                 their FastRPC device nodes. Repeat -d to select one or
 *                 more domains explicitly.
 *
 *   unsigned_pd : Call remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE)
 *                 before opening handles.  Default 1; pass -u 0 to skip for
 *                 signed skels.
 *
 *   list_mode   : Metadata-only listing requested by -l or --list-*. When
 *                 active, DSP discovery and test execution are skipped.
 *
 *   any_tags[]   : OR-mode inclusion filter.  Populated by --any-tags <tag>
 *                 (repeatable, up to TEST_CONFIG_MAX_TAGS values).
 *
 *                 When any_tag_count > 0, a test passes this filter if its
 *                 test metadata annotation contains AT LEAST ONE of the
 *                 listed tags.
 *
 *   all_tags[]   : AND-mode inclusion filter.  Populated by --all-tags <tag>
 *                 (repeatable, up to TEST_CONFIG_MAX_TAGS values).
 *
 *                 When all_tag_count > 0, a test passes this filter only if
 *                 its test metadata annotation contains ALL of the listed
 *                 tags simultaneously.
 *
 *   Filter activation rules:
 *     - Both filters inactive (counts == 0): every test runs.
 *     - Only --any-tags active: test runs if it matches ANY listed tag.
 *     - Only --all-tags active: test runs if it matches ALL listed tags.
 *     - Both active: test must pass BOTH filters independently.
 *
 *   Tags are compared case-sensitively (strcmp).
 *
 *   Recommended tag vocabulary:
 *     Functionality  — "Remote", "RpcMem", "Profiling"
 *     Classification — "unit", "feature"
 *     Polarity       — "positive", "negative"
 */
typedef struct {
    int domain_id;
    int domain_ids[NUM_DOMAINS];
    int domain_count;
    int unsigned_pd;
    int silent_mode; /**< --silent; suppress per-test output for passing tests */
    int isolation_process; /**< 1 for exec-isolated cases, 0 for debug in-process mode */
    unsigned timeout_ms; /**< worker timeout; zero selects the default */
    test_list_mode_t list_mode;

    /* Worker-only selector state. These options are consumed before Unity. */
    int worker_mode;
    int worker_result_fd;
    int worker_domain_id;
    int worker_unsigned_pd;
    const char *worker_group;
    const char *worker_name;
    const char *worker_executable;
    const char *any_tags[TEST_CONFIG_MAX_TAGS]; /**< --any-tags values (OR filter) */
    int any_tag_count;                          /**< number of active --any-tags entries       */
    const char *all_tags[TEST_CONFIG_MAX_TAGS]; /**< --all-tags values (AND filter)            */
    int all_tag_count;                          /**< number of active --all-tags entries       */
} test_config_t;

extern test_config_t g_test_config;

/* Parses FastRPC-specific arguments, writes them into g_test_config, and
 * returns a filtered (argc, argv) pair for Unity. With no -d, discovers all
 * available DSP domains unless a metadata listing was requested. Returns 0
 * on success or -1 for invalid arguments/discovery failure. */
int test_config_init(int argc, const char **argv, int *out_argc, const char ***out_argv);

/* Builds a fully-qualified fastrpc_test URI for g_test_config.domain_id into buf.
 * Returns 0 on success, or -1 for an unknown domain, invalid buffer, or a
 * buffer too small for the URI.  On failure, buf is cleared when possible. */
int test_utils_domain_uri(char *buf, size_t buflen);
int test_utils_domain_uri_for(int domain_id, char *buf, size_t buflen);

/* Returns the plain domain name string (e.g. "cdsp", "adsp") for
 * g_test_config.domain_id, or NULL for an unknown ID. */
const char *test_utils_domain_name(void);
const char *test_utils_domain_name_for(int domain_id);

/* Sets DSP_LIBRARY_PATH, appending SKEL_SEARCH_PATH to any existing value.
 * Must be called before any fastrpc API. Returns 0 on success, -1 on failure. */
int test_utils_setup_dsp_lib_path(void);

void test_config_free_argv(const char **argv);

/* Calls remote_session_control(DSPRPC_CONTROL_UNSIGNED_MODULE) for the given domain.
 * Must be called before remote_handle_open when the skel is unsigned. */
int test_utils_enable_unsigned_pd(int domain);

/* Returns a human-readable string for a fastrpc/AEE error code. */
const char *test_utils_err_str(int err);

/* ---- Error code reporting ---- */

/* Call after any FastRPC/DSP API call to store the code for the test footer.
 * The runner collects all codes and prints them consolidated after the test body. */
#define REPORT_ERROR_CODE(ret)                                                                     \
    do {                                                                                           \
        test_utils_set_last_error_code(ret);                                                       \
        test_utils_accumulate_error_code(ret);                                                     \
    } while (0)

void test_utils_set_last_error_code(int err);
int test_utils_get_last_error_code(void); /* Returns TEST_UTILS_INVALID_ERROR_CODE if none set */
void test_utils_reset_last_error_code(void);

void test_utils_accumulate_error_code(int err);
const int *test_utils_get_accumulated_error_codes(int *count);
int test_utils_error_codes_truncated(void);
void test_utils_reset_accumulated_error_codes(void);

/* Per-test setup helper: sets DSP_LIBRARY_PATH and optionally enables
 * unsigned PD. Returns the first setup error, or 0 on success. */
int test_utils_setup_test(const char *tag);

#ifdef __cplusplus
}
#endif

#endif /* TEST_UTILS_H */
