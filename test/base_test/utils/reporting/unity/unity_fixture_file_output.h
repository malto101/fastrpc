// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

/**
 * @file unity_fixture_file_output.h
 * @brief Unity Fixture integration for formatted output and test metadata.
 */

#ifndef UNITY_FIXTURE_FILE_OUTPUT_H
#define UNITY_FIXTURE_FILE_OUTPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "unity_fixture.h"

#ifdef UnityTestRunner
#undef UnityTestRunner
#endif
#define UnityTestRunner UnityTestRunnerWithFileOutput

int UnityFixtureFileOutputBegin(const char *output_dir, const char *base_name);
int UnityFixtureFileOutputEnd(void);

void UnityTestRunnerWithFileOutput(unityfunction *setup, unityfunction *test_body,
                                   unityfunction *teardown, const char *printable_name,
                                   const char *group, const char *name, const char *file,
                                   unsigned int line);

int UnityMainWithFileOutput(int argc, const char *argv[], void (*run_all_tests)(void));
void unity_test_case_set_summary(unsigned int total, unsigned int failures,
                                 unsigned int ignored);
void unity_test_case_print_domain_begin(const char *domain_name, int domain_id);
void unity_test_case_print_domain_summary(const char *domain_name, int domain_id,
                                          unsigned int total, unsigned int failures,
                                          unsigned int ignored);
void unity_test_case_print_domain_end(void);

#if defined(__GNUC__) || defined(__clang__)
#define UNITY_METADATA_CONSTRUCTOR __attribute__((constructor))
#define UNITY_METADATA_USED __attribute__((used))
#else
#error "base_test metadata registration requires a constructor-capable compiler"
#endif

typedef enum {
    TEST_SIGNAL_FAIL = 0,
    TEST_SIGNAL_PASS = 1,
} test_signal_policy_t;

/*
 * Canonical metadata for one (group, name) pair. Descriptor storage supplied to
 * the registration API must have static lifetime. The declaration macro is
 * intended for file-scope C11 declarations.
 */
typedef struct unity_test_case_metadata {
    const char *group;
    const char *name;
    const char *const *tags;
    size_t tag_count;
    const int *domains;
    size_t domain_count;
    int signal_number;
    int pass_on_signal;
    unsigned has_tags : 1;
    unsigned has_domains : 1;
    unsigned has_expected_signal : 1;
    unsigned invalid : 1;
} unity_test_case_metadata_t;

void unity_test_case_metadata_register(unity_test_case_metadata_t *entry);
const unity_test_case_metadata_t *unity_test_case_metadata_find(const char *group,
                                                                 const char *name);
int unity_test_case_metadata_validate(void);
int unity_test_case_domain_supported(const char *group, const char *name, int domain_id);
int unity_test_case_group_has_selected(const char *group);

/* Records cases dispatched by parent-side group runners. */
void unity_test_case_dispatch_observe(const char *group, const char *name);
void unity_test_case_dispatch_reset(void);
int unity_test_case_dispatch_validate(void);

/* Returns non-zero when a case passes Unity, tag, and domain selection. */
int unity_test_case_selected(const char *group, const char *name);

/* Apply metadata-aware selection to an explicit Unity ignore declaration. */
void unity_test_case_ignore(const char *printable_name, const char *group,
                           const char *name);

/* Replace Unity's dispatch macro without modifying vendored Unity. RUN_TEST_CASE
 * records dispatches, including IGNORE_TEST cases, before invoking the wrapper. */
#ifdef RUN_TEST_CASE
#undef RUN_TEST_CASE
#endif
#define RUN_TEST_CASE(group, name) \
    { void TEST_##group##_##name##_run(void); \
      unity_test_case_dispatch_observe(#group, #name); \
      TEST_##group##_##name##_run(); }

#ifdef IGNORE_TEST
#undef IGNORE_TEST
#endif
#define IGNORE_TEST(group, name) \
    void TEST_##group##_##name##_(void); \
    void TEST_##group##_##name##_run(void); \
    void TEST_##group##_##name##_run(void) \
    { \
        unity_test_case_ignore("IGNORE_TEST(" #group ", " #name ")", \
                              TEST_GROUP_##group, #name); \
    } \
    void TEST_##group##_##name##_(void)

/*
 * Option fragments for TEST_CASE_METADATA. Each supplied list must contain at
 * least one value. Tags are string literals and domains are numeric FastRPC
 * domain IDs. Omitting metadata or a domain option means all known domains.
 * These are C11 file-scope compound literals; do not use them for local or
 * dynamically constructed metadata. Omitting all option fragments is valid and
 * means no tags, all domains, and no expected signal.
 */
#define TEST_METADATA_TAGS(...)                                                        \
    , .tags = (const char *const[]){ __VA_ARGS__, NULL },                            \
    .tag_count = sizeof((const char *const[]){ __VA_ARGS__, NULL }) /                 \
                     sizeof(const char *) - 1,                                       \
    .has_tags = 1

#define TEST_METADATA_DOMAINS(...)                                                     \
    , .domains = (const int[]){ __VA_ARGS__ },                                       \
    .domain_count = sizeof((const int[]){ __VA_ARGS__ }) / sizeof(int),              \
    .has_domains = 1

#define TEST_METADATA_EXPECTED_SIGNAL(signal_value, pass_value)                       \
    , .signal_number = (signal_value),                                                 \
    .pass_on_signal = (pass_value),                                                    \
    .has_expected_signal = 1

#define TEST_CASE_METADATA(metadata_group, metadata_name, ...)                         \
    static unity_test_case_metadata_t _utmd_entry_##metadata_group##_##metadata_name = \
        { .group = #metadata_group, .name = #metadata_name __VA_ARGS__ };             \
    UNITY_METADATA_CONSTRUCTOR UNITY_METADATA_USED static void                         \
    _utmd_register_##metadata_group##_##metadata_name(void)                            \
    {                                                                                  \
        unity_test_case_metadata_register(                                             \
            &_utmd_entry_##metadata_group##_##metadata_name);                         \
    }

int unity_test_case_tag_filter_passes(const char *group, const char *name);
int unity_test_case_registry_print_tests(void);
int unity_test_case_registry_print_groups(void);
int unity_test_case_registry_print_tags(void);

#ifdef __cplusplus
}
#endif

#endif /* UNITY_FIXTURE_FILE_OUTPUT_H */
