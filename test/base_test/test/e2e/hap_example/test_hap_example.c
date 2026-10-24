// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "AEEStdErr.h"
#include "fastrpc_test.h"
#include "test_utils.h"
#include "unity_fixture.h"

TEST_GROUP(HapExampleE2E);

static remote_handle64 hap_handle = (remote_handle64)-1;

static int hap_example_open(void)
{
    char uri[512];

    hap_handle = (remote_handle64)-1;
    if (test_utils_domain_uri(uri, sizeof(uri)) != 0)
        return AEE_EBADDOMAIN;

    return fastrpc_test_open(uri, &hap_handle);
}

static int hap_example_close(void)
{
    int ret = AEE_SUCCESS;

    if (hap_handle != (remote_handle64)-1)
        ret = fastrpc_test_close(hap_handle);
    hap_handle = (remote_handle64)-1;
    return ret;
}

TEST_SETUP(HapExampleE2E)
{
    int ret = hap_example_open();
    REPORT_ERROR_CODE(ret);
    if (ret != AEE_SUCCESS)
        TEST_IGNORE_MESSAGE("HAP example DSP service is unavailable");
}

TEST_TEAR_DOWN(HapExampleE2E)
{
    int ret = hap_example_close();
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32(AEE_SUCCESS, ret);
}

TEST(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured)
{
    int ret = fastrpc_test_hap_example_farf_runtime(hap_handle, 0);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "disable FARF runtime logging");

    ret = fastrpc_test_hap_example_farf_runtime(hap_handle, 1);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "enable FARF runtime logging");
}
TEST_CASE_TAGS(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured,
               "HapExample", "e2e", "positive", "FARF");

TEST_GROUP_RUNNER(HapExampleE2E)
{
    RUN_TEST_CASE(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured);
}
