// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "calculator.h"
#include "e2e_test_utils.h"
#include "unity_fixture.h"

#include <stdint.h>

TEST_GROUP(CalculatorE2E);

static remote_handle64 calculator_handle;

TEST_SETUP(CalculatorE2E)
{
    int ret = e2e_open(calculator_URI, g_test_config.domain_id,
                       g_test_config.unsigned_pd, calculator_open,
                       &calculator_handle);
    if (ret != AEE_SUCCESS)
        TEST_FAIL_MESSAGE("calculator_open failed");
}

TEST_TEAR_DOWN(CalculatorE2E)
{
    int ret = e2e_close(calculator_handle, calculator_close);
    TEST_ASSERT_EQUAL_HEX32(AEE_SUCCESS, ret);
    calculator_handle = (remote_handle64)-1;
}

TEST(CalculatorE2E, SumAndMaximum)
{
    const int values[] = {-7, 0, 4, 9, 2};
    int64_t sum = 0;
    int maximum = 0;

    int ret = calculator_sum(calculator_handle, values,
                             (int)(sizeof(values) / sizeof(values[0])), &sum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "calculator_sum");
    TEST_ASSERT_EQUAL_INT64(8, sum);

    ret = calculator_max(calculator_handle, values,
                         (int)(sizeof(values) / sizeof(values[0])), &maximum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "calculator_max");
    TEST_ASSERT_EQUAL_INT(9, maximum);
}
TEST_CASE_TAGS(CalculatorE2E, SumAndMaximum, "Calculator", "e2e", "positive");

TEST(CalculatorE2E, EmptyInputsAreRejected)
{
    int64_t sum = 0;
    int maximum = 0;
    int ret = calculator_sum(calculator_handle, NULL, 1, &sum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_NOT_EQUAL(AEE_SUCCESS, ret);

    ret = calculator_max(calculator_handle, NULL, 0, &maximum);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_NOT_EQUAL(AEE_SUCCESS, ret);
}
TEST_CASE_TAGS(CalculatorE2E, EmptyInputsAreRejected, "Calculator", "e2e", "negative");

TEST_GROUP_RUNNER(CalculatorE2E)
{
    RUN_TEST_CASE(CalculatorE2E, SumAndMaximum);
    RUN_TEST_CASE(CalculatorE2E, EmptyInputsAreRejected);
}
