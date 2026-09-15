// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file root_all_tests.c
 * @brief Root dispatcher — owns main() and calls every suite runner.
 *
 * To add a new unit suite:
 *   1. Create test/unit/<name>/all_tests.c defining
 *      void run_<name>_tests(void).
 *   2. Add the extern declaration and call in run_all_tests() below.
 *   3. Add the suite name to _unit_suites in test/unit/CMakeLists.txt.
 *
 * To add a new feature suite:
 *   1. Create test/feature/<name>/all_tests.c defining
 *      void run_<name>_feature_tests(void).
 *   2. Add the extern declaration and call in run_all_tests() below.
 *   3. Add the suite name to _feat_suites in test/feature/CMakeLists.txt.
 *
 * The --fuzz flag (built only with -DENABLE_FUZZ_TESTS=ON) hands argv to
 * libFuzzer's driver instead of Unity — see test/fuzz/dspqueue/README.md.
 * With no other args it runs every suite in fuzz_suites[] below against its
 * own default corpus; --tags/--any-tags/--all-tags (the same flags used to
 * filter Unity test cases, parsed by test_config_init() and matched via
 * unity_tag_list_matches_filters()) narrow which suite(s) run, and an
 * explicit positional corpus path overrides the default (only valid once
 * narrowed to exactly one suite).
 */

#include "reporting/unity/unity_fixture_file_output.h"
#include "test_utils.h"
#include "unity_fixture.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Unit suite declarations -------------------------------------------- */
void run_dspqueue_tests(void);

/* ---- Feature suite declarations ----------------------------------------- */
void run_dspqueue_feature_tests(void);
void run_remote_heap_feature_tests(void);

#ifdef ENABLE_FUZZ_TESTS
/*
 * Fuzz suite entry points + libFuzzer's own driver entry point (no public C
 * header ships this; see https://llvm.org/docs/LibFuzzer.html#using-libfuzzer-as-a-library).
 * Register each entry point's selection tag(s), default corpus dir, and
 * function pointer in fuzz_suites[] below — that's the only wiring a new
 * suite needs here (see test/fuzz/CMakeLists.txt for the build-side step).
 */
int fuzz_dspqueue_create_run(const uint8_t *data, size_t size);
extern int LLVMFuzzerRunDriver(int *argc, char ***argv,
                                int (*UserCb)(const uint8_t *Data, size_t Size));

struct fuzz_suite {
    const char *name;            /* selects this suite via --tags/--any-tags/--all-tags */
    const char *const *tags;     /* NULL-terminated; matched like TEST_CASE_TAGS() */
    const char *default_corpus;  /* used when argv has no positional corpus path; relative to CWD */
    int (*run)(const uint8_t *data, size_t size);
};

static const char *const dspqueue_create_tags[] = { "dspqueue_create", "dspqueue", NULL };

static const struct fuzz_suite fuzz_suites[] = {
    { "dspqueue_create", dspqueue_create_tags, "corpus/dspqueue_create", fuzz_dspqueue_create_run },
};
#define FUZZ_SUITE_COUNT ((int)(sizeof(fuzz_suites) / sizeof(fuzz_suites[0])))

static int run_fuzz_tests(int argc, const char *argv[]);
#endif

/* ---- Root dispatcher ---------------------------------------------------- */

static void run_all_tests(void)
{
    /* Unit suites */
    run_dspqueue_tests();

    /* Feature suites */
    run_dspqueue_feature_tests();
    run_remote_heap_feature_tests();
}

static int run_base_tests(int argc, const char *argv[])
{
    int filtered_argc;
    const char **filtered_argv;
    int result = 0;

    if (test_config_init(argc, argv, &filtered_argc, &filtered_argv) != 0)
        return 2;

    if (g_test_config.list_mode != TEST_LIST_NONE) {
        if (UnityGetCommandLineOptions(filtered_argc, filtered_argv) != 0) {
            free((void *)filtered_argv);
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

        free((void *)filtered_argv);
        return result == 0 ? 0 : 2;
    }

    if (g_test_config.fuzz_mode) {
#ifdef ENABLE_FUZZ_TESTS
        result = run_fuzz_tests(filtered_argc, filtered_argv);
#else
        fprintf(stderr,
                "This test-fastrpc build has no fuzz support "
                "(rebuild with -DENABLE_FUZZ_TESTS=ON).\n");
        result = 2;
#endif
        free((void *)filtered_argv);
        return result;
    }

    for (int i = 0; i < g_test_config.domain_count; i++) {
        int domain_result;

        g_test_config.domain_id = g_test_config.domain_ids[i];
        printf("\n[test_config] running tests on %s domain (%d)\n",
               test_utils_domain_name(), g_test_config.domain_id);

        if (UnityFixtureFileOutputBegin(NULL, NULL) != 0)
            fprintf(stderr, "Warning: Failed to initialize custom Unity output\n");

        domain_result = UnityMain(filtered_argc, filtered_argv, run_all_tests);
        UnityFixtureFileOutputEnd();

        result += domain_result;
    }

    free((void *)filtered_argv);

    return result;
}

#ifdef ENABLE_FUZZ_TESTS
/* Returns the first positional (non-flag) arg after argv[0], or NULL —
 * that's libFuzzer's own convention for an input/corpus path. */
static const char *fuzz_argv_corpus_path(int argc, const char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-')
            return argv[i];
    }
    return NULL;
}

/*
 * Runs one suite. argv/argc must already exclude --fuzz and any tag flags
 * (test_config_init() strips those before this is ever called). Appends
 * suite->default_corpus when argv has no positional corpus path of its own.
 */
static int run_one_fuzz_suite(const struct fuzz_suite *suite, int argc, const char *argv[])
{
    const char *corpus = fuzz_argv_corpus_path(argc, argv);
    int fuzz_argc = 0;
    char *fuzz_argv[argc + 1];

    for (int i = 0; i < argc; i++)
        fuzz_argv[fuzz_argc++] = (char *)argv[i];
    if (!corpus) {
        corpus = suite->default_corpus;
        fuzz_argv[fuzz_argc++] = (char *)corpus;
    }

    printf("[fuzz] running suite '%s' (corpus: %s)\n", suite->name, corpus);

    char **fuzz_argv_ptr = fuzz_argv;
    return LLVMFuzzerRunDriver(&fuzz_argc, &fuzz_argv_ptr, suite->run);
}

/*
 * Dispatches --fuzz to every registered suite whose tags pass the
 * --tags/--any-tags/--all-tags filters (every suite runs when no filter is
 * given). argc/argv are the post-test_config_init() filtered args: no
 * --fuzz, no tag flags — just libFuzzer's own flags (-max_len=, -runs=, ...)
 * and an optional positional corpus path.
 *
 * An explicit corpus path only makes sense for a single suite, so it's
 * rejected once more than one suite matches — narrow with --tags/--any-tags
 * first. Running more than one *unbounded* suite in one invocation means
 * only the first ever returns; pass e.g. -max_total_time=<seconds> to bound
 * each run when selecting more than one.
 */
static int run_fuzz_tests(int argc, const char *argv[])
{
    const struct fuzz_suite *matched[FUZZ_SUITE_COUNT];
    int matched_count = 0;

    for (int i = 0; i < FUZZ_SUITE_COUNT; i++) {
        if (unity_tag_list_matches_filters(fuzz_suites[i].tags))
            matched[matched_count++] = &fuzz_suites[i];
    }

    if (matched_count == 0) {
        fprintf(stderr, "[fuzz] no registered suite matches the given tag filter\n");
        return 2;
    }

    if (matched_count > 1 && fuzz_argv_corpus_path(argc, argv) != NULL) {
        fprintf(stderr,
                "[fuzz] an explicit corpus path requires selecting exactly one suite "
                "(narrow with --tags/--any-tags); %d suites matched\n",
                matched_count);
        return 2;
    }

    int result = 0;

    for (int i = 0; i < matched_count; i++)
        result += run_one_fuzz_suite(matched[i], argc, argv);

    return result;
}
#endif

#ifdef BASE_TEST_PLUGIN
/* Legacy fastrpc_test plugin ABI; keep in sync with test/fastrpc_test.c. */
int run_test(int domain_id, bool is_unsignedpd_enabled)
{
    char domain_arg[12];
    char unsigned_pd_arg[2];
    const char *argv[] = {
        "base_test",
        "-d",
        domain_arg,
        "-u",
        unsigned_pd_arg,
    };

    snprintf(domain_arg, sizeof(domain_arg), "%d", domain_id);
    snprintf(unsigned_pd_arg, sizeof(unsigned_pd_arg), "%d", is_unsignedpd_enabled ? 1 : 0);

    return run_base_tests((int)(sizeof(argv) / sizeof(argv[0])), argv);
}
#else
int main(int argc, const char *argv[])
{
    return run_base_tests(argc, argv);
}
#endif
