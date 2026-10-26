// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "AEEStdErr.h"
#include "e2e_session.h"
#include "fastrpc_test.h"
#include "test_utils.h"
#include "unity_fixture.h"

TEST_GROUP(HapExampleE2E);

static test_e2e_session_t hap_session;

static int hap_example_open(void)
{
    return test_e2e_session_open(&hap_session);
}

static int hap_example_close(void)
{
    return test_e2e_session_close(&hap_session);
}

static remote_handle64 hap_example_handle(void)
{
    return hap_session.handle;
}

TEST_SETUP(HapExampleE2E)
{
    int ret = test_utils_setup_test("hap-example");
    if (ret != AEE_SUCCESS)
        TEST_IGNORE_MESSAGE("FastRPC test setup failed");

    ret = hap_example_open();
    if (ret != AEE_SUCCESS)
        TEST_IGNORE_MESSAGE("HAP example DSP service is unavailable");
}

TEST_TEAR_DOWN(HapExampleE2E)
{
    int ret = hap_example_close();

    TEST_ASSERT_EQUAL_HEX32(AEE_SUCCESS, ret);
}

TEST(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured)
{
    int ret = fastrpc_test_hap_example_farf_runtime(hap_example_handle(), 0);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "disable FARF runtime logging");

    ret = fastrpc_test_hap_example_farf_runtime(hap_example_handle(), 1);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "enable FARF runtime logging");
}
TEST_CASE_METADATA(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured,
                   TEST_METADATA_TAGS("HapExample", "e2e", "positive", "Remote", "FARF"));

TEST_GROUP_RUNNER(HapExampleE2E)
{
    RUN_TEST_CASE(HapExampleE2E, FarfRuntimeLoggingCanBeConfigured);
}
