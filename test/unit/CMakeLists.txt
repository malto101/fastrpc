# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Build one static library for each unit suite.

set(_unit_suites dspqueue)
set(_new_suite_libs "")

foreach(_suite IN LISTS _unit_suites)
    set(_suite_dir "${CMAKE_CURRENT_SOURCE_DIR}/${_suite}")
    file(GLOB _suite_srcs CONFIGURE_DEPENDS "${_suite_dir}/*.c")
    if(NOT _suite_srcs)
        message(WARNING "${_suite_dir}: no .c files found — skipping suite")
        continue()
    endif()

    add_library("unit_${_suite}_all" STATIC ${_suite_srcs})
    set_target_properties("unit_${_suite}_all" PROPERTIES
        POSITION_INDEPENDENT_CODE ON)
    target_include_directories("unit_${_suite}_all" PRIVATE "${_suite_dir}")
    target_link_libraries("unit_${_suite}_all" PRIVATE common_test_dep)
    list(APPEND _new_suite_libs "unit_${_suite}_all")
endforeach()

set(ALL_SUITE_LIBS ${ALL_SUITE_LIBS} ${_new_suite_libs} PARENT_SCOPE)
