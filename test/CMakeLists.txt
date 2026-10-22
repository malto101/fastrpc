# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Unified FastRPC CMake/Unity test tree.
#
# Configure from the repository root with:
#   cmake -S test -B build-test -DQAIC_EXECUTABLE=/path/to/qaic
#
# FASTRPC_ROOT points at a source checkout. FASTRPC_PREFIX can instead point at
# a prefix populated by `make install`; FASTRPC_IDL_DIR may still be needed in
# that mode because the Autotools install does not package QAIC input files.

cmake_minimum_required(VERSION 3.21)

set(FASTRPC_TEST_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# When this project is configured directly, the submodule path is registered by
# the repository's .gitmodules file.  Keep the moved layout self-contained.
set(FASTRPC_TEST_UNITY_DIR "${FASTRPC_TEST_SOURCE_DIR}/vendor/unity")

set(TARGET_PLATFORM "linux" CACHE STRING
    "Target platform to cross-compile for (linux or android)")
set(_valid_platforms linux android)
if(NOT TARGET_PLATFORM IN_LIST _valid_platforms)
    message(FATAL_ERROR
        "TARGET_PLATFORM='${TARGET_PLATFORM}' is not recognised. "
        "Valid choices: linux, android")
endif()
include("${FASTRPC_TEST_SOURCE_DIR}/cmake/platform/${TARGET_PLATFORM}.cmake")

project(fastrpc_tests
    VERSION 1.0.0
    LANGUAGES C
    DESCRIPTION "FastRPC unit, feature, and end-to-end tests")

include(GNUInstallDirs)
include(CTest)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(FASTRPC_ROOT "" CACHE PATH
    "FastRPC source checkout containing inc/, idl/, and optionally src/.libs/")
set(FASTRPC_PREFIX "" CACHE PATH
    "Installed FastRPC prefix (for example a DESTDIR staging prefix)")
set(FASTRPC_INCLUDE_DIR "" CACHE PATH
    "FastRPC include directory override")
set(FASTRPC_LIBRARY "" CACHE FILEPATH
    "FastRPC libcdsprpc shared library override")
set(FASTRPC_IDL_DIR "" CACHE PATH
    "Directory containing AEEStdDef.idl and remote.idl")
set(QAIC_EXECUTABLE "" CACHE FILEPATH
    "Path to the QAIC compiler used to generate test IDL sources")
set(ANDROID_NDK_HOME "" CACHE PATH
    "Android NDK root when TARGET_PLATFORM=android")
set(DSP_ARCH "73" CACHE STRING
    "Hexagon DSP architecture version for the base skeleton")
set(DSP_ARCHES "68;75" CACHE STRING
    "Hexagon DSP architectures for e2e skeletons")
set(HEXAGON_SDK_ROOT "" CACHE PATH
    "Optional Hexagon SDK root for QuRT-based DSP sources")

if(FASTRPC_ROOT STREQUAL "")
    get_filename_component(_default_root "${FASTRPC_TEST_SOURCE_DIR}/.." ABSOLUTE)
    set(FASTRPC_ROOT "${_default_root}" CACHE PATH
        "FastRPC source checkout containing inc/, idl/, and optionally src/.libs/"
        FORCE)
endif()

# Resolve headers, IDL inputs, and the host shared library independently so a
# test build can consume a staged `make install` without an uninstalled .libs.
if(FASTRPC_INCLUDE_DIR STREQUAL "")
    if(NOT FASTRPC_PREFIX STREQUAL "" AND
       EXISTS "${FASTRPC_PREFIX}/include/fastrpc")
        set(FASTRPC_INCLUDE_DIR "${FASTRPC_PREFIX}/include/fastrpc"
            CACHE PATH "FastRPC include directory" FORCE)
    elseif(EXISTS "${FASTRPC_ROOT}/inc")
        set(FASTRPC_INCLUDE_DIR "${FASTRPC_ROOT}/inc"
            CACHE PATH "FastRPC include directory" FORCE)
    endif()
endif()
if(FASTRPC_INCLUDE_DIR STREQUAL "" OR
   NOT EXISTS "${FASTRPC_INCLUDE_DIR}")
    message(FATAL_ERROR
        "FastRPC headers were not found. Set FASTRPC_ROOT, "
        "FASTRPC_PREFIX, or FASTRPC_INCLUDE_DIR.")
endif()
if(EXISTS "${FASTRPC_ROOT}/inc")
    # The test sources use a few internal headers that make install does not
    # publish. Keep the installed include directory available through the
    # imported target, while compiling test sources against the checkout's
    # complete header set.
    set(FASTRPC_INC_DIR "${FASTRPC_ROOT}/inc")
else()
    set(FASTRPC_INC_DIR "${FASTRPC_INCLUDE_DIR}")
endif()
set(_fastrpc_include_dirs "${FASTRPC_INCLUDE_DIR}")
if(NOT "${FASTRPC_INC_DIR}" STREQUAL "${FASTRPC_INCLUDE_DIR}")
    list(APPEND _fastrpc_include_dirs "${FASTRPC_INC_DIR}")
endif()

if(FASTRPC_IDL_DIR STREQUAL "")
    if(EXISTS "${FASTRPC_ROOT}/idl/AEEStdDef.idl")
        set(FASTRPC_IDL_DIR "${FASTRPC_ROOT}/idl"
            CACHE PATH "Directory containing FastRPC QAIC IDLs" FORCE)
    elseif(NOT FASTRPC_PREFIX STREQUAL "" AND
           EXISTS "${FASTRPC_PREFIX}/share/fastrpc/idl/AEEStdDef.idl")
        set(FASTRPC_IDL_DIR "${FASTRPC_PREFIX}/share/fastrpc/idl"
            CACHE PATH "Directory containing FastRPC QAIC IDLs" FORCE)
    endif()
endif()
if(FASTRPC_IDL_DIR STREQUAL "" OR
   NOT EXISTS "${FASTRPC_IDL_DIR}/AEEStdDef.idl" OR
   NOT EXISTS "${FASTRPC_IDL_DIR}/remote.idl")
    message(FATAL_ERROR
        "FastRPC QAIC IDLs were not found. Set FASTRPC_IDL_DIR to a directory "
        "containing AEEStdDef.idl and remote.idl.")
endif()

if(QAIC_EXECUTABLE STREQUAL "")
    find_program(QAIC_EXECUTABLE NAMES qaic)
endif()
if(QAIC_EXECUTABLE STREQUAL "" OR NOT EXISTS "${QAIC_EXECUTABLE}")
    message(FATAL_ERROR
        "QAIC compiler not found. Set -DQAIC_EXECUTABLE=/path/to/qaic.")
endif()

set(_fastrpc_library_paths)
if(NOT FASTRPC_PREFIX STREQUAL "")
    list(APPEND _fastrpc_library_paths
        "${FASTRPC_PREFIX}/lib"
        "${FASTRPC_PREFIX}/lib64"
        "${FASTRPC_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
endif()
if(EXISTS "${FASTRPC_ROOT}/src/.libs")
    list(APPEND _fastrpc_library_paths "${FASTRPC_ROOT}/src/.libs")
endif()

if(FASTRPC_LIBRARY STREQUAL "")
    find_library(FASTRPC_LIBRARY
        NAMES cdsprpc libcdsprpc.so.1 libcdsprpc
        PATHS ${_fastrpc_library_paths}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH)
    if(NOT FASTRPC_LIBRARY)
        find_library(FASTRPC_LIBRARY NAMES cdsprpc)
    endif()
endif()
if(FASTRPC_LIBRARY STREQUAL "FASTRPC_LIBRARY-NOTFOUND" OR
   FASTRPC_LIBRARY STREQUAL "" OR NOT EXISTS "${FASTRPC_LIBRARY}")
    message(FATAL_ERROR
        "libcdsprpc was not found. Set FASTRPC_LIBRARY, FASTRPC_PREFIX, "
        "or FASTRPC_ROOT after building FastRPC.")
endif()

message(STATUS "FastRPC include directory: ${FASTRPC_INCLUDE_DIR}")
message(STATUS "FastRPC library: ${FASTRPC_LIBRARY}")
message(STATUS "FastRPC QAIC IDL directory: ${FASTRPC_IDL_DIR}")

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(IS_ANDROID TRUE)
else()
    set(IS_ANDROID FALSE)
endif()
set(PLATFORM_C_DEFS _POSIX_C_SOURCE=200112L)
if(IS_ANDROID)
    list(APPEND PLATFORM_C_DEFS ANDROID)
    set(PLATFORM_LINK_FLAGS -pie)
else()
    set(PLATFORM_LINK_FLAGS
        -Wl,--dynamic-linker=/lib/ld-linux-aarch64.so.1)
endif()

add_compile_options(-Wall -Wextra -Werror)

set(UNITY_EXTENSION_FIXTURE ON CACHE BOOL
    "Build Unity with the fixture extension" FORCE)
set(UNITY_EXTENSION_MEMORY ON CACHE BOOL
    "Build Unity with the memory extension" FORCE)
add_subdirectory(vendor/unity EXCLUDE_FROM_ALL)
set(UNITY_COMPILE_DEFS UNITY_OUTPUT_COLOR UNITY_INCLUDE_PRINT_FORMATTED)
target_compile_definitions(unity PUBLIC ${UNITY_COMPILE_DEFS})
set_target_properties(unity PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(unity_dep INTERFACE)
target_link_libraries(unity_dep INTERFACE unity)

add_subdirectory(idl)
add_subdirectory(utils)

add_library(cdsprpc SHARED IMPORTED GLOBAL)
set_target_properties(cdsprpc PROPERTIES
    IMPORTED_LOCATION "${FASTRPC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FASTRPC_INCLUDE_DIR}")
add_library(fastrpc_dep INTERFACE)
target_include_directories(fastrpc_dep INTERFACE "${FASTRPC_INCLUDE_DIR}")
target_link_libraries(fastrpc_dep INTERFACE cdsprpc)

find_library(LIBM_PATH m)
add_library(libm_dep INTERFACE)
if(LIBM_PATH)
    target_link_libraries(libm_dep INTERFACE "${LIBM_PATH}")
endif()

add_library(common_test_iface INTERFACE)
add_dependencies(common_test_iface fastrpc_test_idl)
target_include_directories(common_test_iface INTERFACE
    "${FASTRPC_TEST_SOURCE_DIR}"
    "${FASTRPC_TEST_SOURCE_DIR}/feature"
    "${FASTRPC_TEST_SOURCE_DIR}/unit"
    "${FASTRPC_TEST_SOURCE_DIR}/e2e"
    "${FASTRPC_TEST_IDL_INC_DIR}")
target_compile_definitions(common_test_iface INTERFACE
    ${PLATFORM_C_DEFS} ${UNITY_COMPILE_DEFS})
target_compile_options(common_test_iface INTERFACE
    "-include${FASTRPC_TEST_SOURCE_DIR}/utils/reporting/unity/unity_fixture_file_output.h")

add_library(common_test_dep INTERFACE)
target_link_libraries(common_test_dep INTERFACE
    common_test_iface unity_dep utils_dep fastrpc_dep fastrpc_test_stub libm_dep)

set(ALL_SUITE_LIBS "")
add_subdirectory(unit)
add_subdirectory(feature)
add_subdirectory(e2e)
add_subdirectory(bin)

if(BUILD_TESTING)
    add_test(NAME fastrpc_metadata
        COMMAND "$<TARGET_FILE:fastrpc_test>" --list-tests)
    set_tests_properties(fastrpc_metadata PROPERTIES LABELS "metadata")
endif()
