// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "unity_fixture.h"
#include "test_utils.h"

void TEST_MultithreadingE2E_GROUP_RUNNER(void);

void run_multithreading_e2e_tests(void)
{
    if (test_suite_setup("multithreading-e2e") != 0)
        return;
    RUN_TEST_GROUP(MultithreadingE2E);
}
