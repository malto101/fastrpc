// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "AEEStdErr.h"
#include "e2e_test_utils.h"
#include "hap_example.h"
#include "unity_fixture.h"

#ifndef hap_example_URI
#define hap_example_URI "file:///libhap_example_skel.so?hap_example_skel_handle_invoke&_modver=1.0"
#endif

TEST_GROUP(HapExampleE2E);

static remote_handle64 hap_handle = (remote_handle64)-1;

TEST_SETUP(HapExampleE2E)
{
    int ret = e2e_open(hap_example_URI, g_test_config.domain_id,
                       g_test_config.unsigned_pd, hap_example_open, &hap_handle);
    REPORT_ERROR_CODE(ret);
    if (ret != AEE_SUCCESS)
        TEST_IGNORE_MESSAGE("HAP example DSP service is unavailable");
}

TEST_TEAR_DOWN(HapExampleE2E)
{
    int ret = e2e_close(hap_handle, hap_example_close);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32(AEE_SUCCESS, ret);
    hap_handle = (remote_handle64)-1;
}

TEST(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured)
{
    int ret = hap_example_farf_runtime(hap_handle, 0);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "disable FARF runtime logging");

    ret = hap_example_farf_runtime(hap_handle, 1);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "enable FARF runtime logging");
}
TEST_CASE_TAGS(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured,
               "HapExample", "e2e", "positive", "FARF");

TEST_GROUP_RUNNER(HapExampleE2E)
{
    RUN_TEST_CASE(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured);
}
