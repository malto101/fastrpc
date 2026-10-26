// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "AEEStdErr.h"
#include "fastrpc_test.h"
#include "test_utils.h"
#include "unity_fixture.h"

#include <stdint.h>

TEST_GROUP(CalculatorE2E);

static remote_handle64 calculator_handle = (remote_handle64)-1;

static int calculator_open(void)
{
    char uri[512];

    calculator_handle = (remote_handle64)-1;
    if (test_utils_domain_uri(uri, sizeof(uri)) != 0)
        return AEE_EBADDOMAIN;

    return fastrpc_test_open(uri, &calculator_handle);
}

static int calculator_close(void)
{
    int ret = AEE_SUCCESS;

    if (calculator_handle != (remote_handle64)-1)
        ret = fastrpc_test_close(calculator_handle);
    calculator_handle = (remote_handle64)-1;
    return ret;
}

TEST_SETUP(CalculatorE2E)
{
    int ret = test_utils_setup_test("calculator");
    REPORT_ERROR_CODE(ret);
    if (ret != AEE_SUCCESS)
        TEST_IGNORE_MESSAGE("FastRPC test setup failed");

    ret = calculator_open();
    REPORT_ERROR_CODE(ret);
    if (ret != AEE_SUCCESS)
        TEST_IGNORE_MESSAGE("calculator DSP service is unavailable");
}

TEST_TEAR_DOWN(CalculatorE2E)
{
    int ret = calculator_close();
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32(AEE_SUCCESS, ret);
}

TEST(CalculatorE2E, SumAndMaximum)
{
    const int values[] = {-7, 0, 4, 9, 2};
    int64_t sum = 0;
    int maximum = 0;

    int ret = fastrpc_test_calculator_sum(calculator_handle, values,
                                          (int)(sizeof(values) / sizeof(values[0])), &sum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "calculator_sum");
    TEST_ASSERT_EQUAL_INT64(8, sum);

    ret = fastrpc_test_calculator_max(calculator_handle, values,
                                      (int)(sizeof(values) / sizeof(values[0])), &maximum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "calculator_max");
    TEST_ASSERT_EQUAL_INT(9, maximum);
}
TEST_CASE_METADATA(CalculatorE2E, SumAndMaximum,
                   TEST_METADATA_TAGS("Calculator", "e2e", "positive"));

TEST(CalculatorE2E, EmptyInputsAreRejected)
{
    int64_t sum = 0;
    int maximum = 0;
    int ret = fastrpc_test_calculator_sum(calculator_handle, NULL, 1, &sum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_NOT_EQUAL(AEE_SUCCESS, ret);

    ret = fastrpc_test_calculator_max(calculator_handle, NULL, 0, &maximum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_NOT_EQUAL(AEE_SUCCESS, ret);
}
TEST_CASE_METADATA(CalculatorE2E, EmptyInputsAreRejected,
                   TEST_METADATA_TAGS("Calculator", "e2e", "negative"));

TEST_GROUP_RUNNER(CalculatorE2E)
{
    RUN_TEST_CASE(CalculatorE2E, SumAndMaximum);
    RUN_TEST_CASE(CalculatorE2E, EmptyInputsAreRejected);
}
