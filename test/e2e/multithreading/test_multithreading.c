// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "e2e_test_utils.h"
#include "multithreading.h"
#include "unity_fixture.h"

TEST_GROUP(MultithreadingE2E);

static remote_handle64 multithreading_handle;

TEST_SETUP(MultithreadingE2E)
{
    int ret = e2e_open(multithreading_URI, g_test_config.domain_id,
                       g_test_config.unsigned_pd, multithreading_open,
                       &multithreading_handle);
    if (ret != AEE_SUCCESS)
        TEST_FAIL_MESSAGE("multithreading_open failed");
}

TEST_TEAR_DOWN(MultithreadingE2E)
{
    int ret = e2e_close(multithreading_handle, multithreading_close);
    TEST_ASSERT_EQUAL_HEX32(AEE_SUCCESS, ret);
    multithreading_handle = (remote_handle64)-1;
}

TEST(MultithreadingE2E, ParallelSum)
{
    int ret = multithreading_parallel_sum(multithreading_handle);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "multithreading_parallel_sum");
}
TEST_CASE_TAGS(MultithreadingE2E, ParallelSum, "Multithreading", "e2e", "positive");

TEST(MultithreadingE2E, BarrierSynchronization)
{
    int ret = multithreading_barriers(multithreading_handle);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "multithreading_barriers");
}
TEST_CASE_TAGS(MultithreadingE2E, BarrierSynchronization,
               "Multithreading", "e2e", "positive", "QuRT");

TEST(MultithreadingE2E, MutexSynchronization)
{
    int ret = multithreading_mutexes(multithreading_handle);
    REPORT_ERROR_CODE(ret);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(AEE_SUCCESS, ret, "multithreading_mutexes");
}
TEST_CASE_TAGS(MultithreadingE2E, MutexSynchronization,
               "Multithreading", "e2e", "positive", "QuRT");

TEST_GROUP_RUNNER(MultithreadingE2E)
{
    RUN_TEST_CASE(MultithreadingE2E, ParallelSum);
    RUN_TEST_CASE(MultithreadingE2E, BarrierSynchronization);
    RUN_TEST_CASE(MultithreadingE2E, MutexSynchronization);
}
