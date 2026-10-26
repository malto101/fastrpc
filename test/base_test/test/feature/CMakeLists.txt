# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Regular feature-test category.  Suite registration is provided by each
# test/feature/<suite>/all_tests.c manifest; do not maintain a central suite list.

include("${CMAKE_SOURCE_DIR}/cmake/regular_test_category.cmake")

base_test_add_category(
    CATEGORY             feature
    CATEGORY_ROOT        "${CMAKE_CURRENT_SOURCE_DIR}"
    SUITE_TARGET_PREFIX  feature
    SUITE_ENTRY_SUFFIX   feature_tests
    DISPATCH_FUNCTION    run_feature_suite_tests
    DISPATCH_SOURCE_VAR  FEATURE_CATEGORY_DISPATCH_SOURCE
    SUITE_LIBS_VAR       FEATURE_SUITE_LIBS
)

set(ALL_SUITE_LIBS ${ALL_SUITE_LIBS} ${FEATURE_SUITE_LIBS} PARENT_SCOPE)
set(REGULAR_CATEGORY_DISPATCH_SOURCES
    ${REGULAR_CATEGORY_DISPATCH_SOURCES}
    "${FEATURE_CATEGORY_DISPATCH_SOURCE}"
    PARENT_SCOPE)
