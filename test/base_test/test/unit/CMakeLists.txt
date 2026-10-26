# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Regular unit-test category.  Suite registration is provided by each
# test/unit/<suite>/all_tests.c manifest; do not maintain a central suite list.

include("${CMAKE_SOURCE_DIR}/cmake/regular_test_category.cmake")

base_test_add_category(
    CATEGORY             unit
    CATEGORY_ROOT        "${CMAKE_CURRENT_SOURCE_DIR}"
    SUITE_TARGET_PREFIX  unit
    SUITE_ENTRY_SUFFIX   tests
    DISPATCH_FUNCTION    run_unit_suite_tests
    DISPATCH_SOURCE_VAR  UNIT_CATEGORY_DISPATCH_SOURCE
    SUITE_LIBS_VAR       UNIT_SUITE_LIBS
)

set(ALL_SUITE_LIBS ${ALL_SUITE_LIBS} ${UNIT_SUITE_LIBS} PARENT_SCOPE)
set(REGULAR_CATEGORY_DISPATCH_SOURCES
    ${REGULAR_CATEGORY_DISPATCH_SOURCES}
    "${UNIT_CATEGORY_DISPATCH_SOURCE}"
    PARENT_SCOPE)
