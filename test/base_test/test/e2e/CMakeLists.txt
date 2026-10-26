# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Calculator and HAP sample tests use the common fastrpc_test IDL and skeleton.
# Their test cases are ordinary base-test suites; no separate QAIC or DSP build
# path is required.

set(_e2e_suites calculator hap_example)
set(_e2e_suite_libs)

add_library(e2e_support STATIC
    common/e2e_session.c
)
target_include_directories(e2e_support PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/utils"
    "${CMAKE_SOURCE_DIR}/utils/fastrpc_utils"
    "${FASTRPC_TEST_IDL_INC_DIR}"
    "${FASTRPC_INC_DIR}"
)
target_link_libraries(e2e_support PRIVATE
    common_test_dep
)

foreach(_suite IN LISTS _e2e_suites)
    set(_suite_dir "${CMAKE_CURRENT_SOURCE_DIR}/${_suite}")
    file(GLOB _suite_sources CONFIGURE_DEPENDS "${_suite_dir}/*.c")

    add_library("e2e_${_suite}_all" STATIC ${_suite_sources})
    target_include_directories("e2e_${_suite}_all" PRIVATE
        "${_suite_dir}"
        "${CMAKE_CURRENT_SOURCE_DIR}/common")
    target_link_libraries("e2e_${_suite}_all" PRIVATE
        common_test_dep
        e2e_support)
    list(APPEND _e2e_suite_libs "e2e_${_suite}_all")
endforeach()

set(ALL_SUITE_LIBS ${ALL_SUITE_LIBS} ${_e2e_suite_libs} PARENT_SCOPE)

# The suites use the generic libfastrpc_test_skel.so generated from
# idl/fastrpc_test.idl.  Deploy that one artifact for all base-test cases.
