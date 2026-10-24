// Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "fastrpc_test.h"
#include "test_utils.h"
#include "unity_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TEST_GROUP(TestUtils);

TEST_SETUP(TestUtils)
{
}

TEST_TEAR_DOWN(TestUtils)
{
}

TEST(TestUtils, UnknownDomainDoesNotFallBackToCdsp)
{
    char uri[256] = "sentinel";

    TEST_ASSERT_NULL(test_utils_domain_name_for(99));
    TEST_ASSERT_EQUAL_INT(-1, test_utils_domain_uri_for(99, uri, sizeof(uri)));
    TEST_ASSERT_EQUAL_STRING("", uri);
}

TEST(TestUtils, DomainUrisUseTheirCanonicalNames)
{
    char uri[256];

    TEST_ASSERT_EQUAL_INT(0, test_utils_domain_uri_for(GDSP0_DOMAIN_ID, uri, sizeof(uri)));
    TEST_ASSERT_NOT_NULL(strstr(uri, "&_dom=gdsp0"));
    TEST_ASSERT_EQUAL_STRING("gdsp0", test_utils_domain_name_for(GDSP0_DOMAIN_ID));

    TEST_ASSERT_EQUAL_INT(0, test_utils_domain_uri_for(CDSP1_DOMAIN_ID, uri, sizeof(uri)));
    TEST_ASSERT_NOT_NULL(strstr(uri, "&_dom=cdsp1"));
    TEST_ASSERT_EQUAL_STRING("cdsp1", test_utils_domain_name_for(CDSP1_DOMAIN_ID));
}

TEST(TestUtils, LongExistingDspLibraryPathIsPreserved)
{
    const size_t existing_len = 1024;
    char *existing = malloc(existing_len + 1);
    char *expected = malloc(existing_len + 1 + strlen(SKEL_SEARCH_PATH) + 1);

    TEST_ASSERT_NOT_NULL(existing);
    TEST_ASSERT_NOT_NULL(expected);

    memset(existing, 'x', existing_len);
    existing[existing_len] = '\0';
    snprintf(expected, existing_len + 1 + strlen(SKEL_SEARCH_PATH) + 1,
             "%s;%s", existing, SKEL_SEARCH_PATH);
    TEST_ASSERT_EQUAL_INT(0, setenv("DSP_LIBRARY_PATH", existing, 1));

    TEST_ASSERT_EQUAL_INT(0, test_utils_setup_dsp_lib_path());
    TEST_ASSERT_EQUAL_STRING(expected, getenv("DSP_LIBRARY_PATH"));

    free(expected);
    free(existing);
}

TEST_GROUP_RUNNER(TestUtils)
{
    RUN_TEST_CASE(TestUtils, UnknownDomainDoesNotFallBackToCdsp);
    RUN_TEST_CASE(TestUtils, DomainUrisUseTheirCanonicalNames);
    RUN_TEST_CASE(TestUtils, LongExistingDspLibraryPathIsPreserved);
}
