// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "unity_fixture.h"
#include "test_utils.h"

void TEST_HapExampleE2E_GROUP_RUNNER(void);

void run_hap_example_e2e_tests(void)
{
    if (test_suite_setup("hap-example-e2e") != 0)
        return;
    RUN_TEST_GROUP(HapExampleE2E);
}
