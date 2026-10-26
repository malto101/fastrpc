# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# base_test — CMake build root

cmake_minimum_required(VERSION 3.21)

# TARGET_PLATFORM is read before project() so the selected toolchain configures
# the compiler before CMake performs its compiler checks.
set(TARGET_PLATFORM "linux" CACHE STRING
    "Target platform to cross-compile for.  Choices: linux (default), android")

set(_valid_platforms linux android)
if(NOT TARGET_PLATFORM IN_LIST _valid_platforms)
    message(FATAL_ERROR
        "TARGET_PLATFORM='${TARGET_PLATFORM}' is not recognised.\n"
        "Valid choices: linux, android")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/platform/${TARGET_PLATFORM}.cmake")

project(base_test
    VERSION     1.0.0
    LANGUAGES   C
    DESCRIPTION "FastRPC base test suite"
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
        "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

set(FASTRPC_ROOT "" CACHE PATH
    "Path to the fastrpc repository root, when using an in-tree build")
set(FASTRPC_PREFIX "" CACHE PATH
    "FastRPC installation prefix, when using an installed library")
set(FASTRPC_INCLUDE_DIR "" CACHE PATH
    "FastRPC include directory override")
set(FASTRPC_LIBRARY "" CACHE FILEPATH
    "FastRPC libcdsprpc shared library override")
set(_fastrpc_library_explicit FALSE)
if(FASTRPC_LIBRARY)
    set(_fastrpc_library_explicit TRUE)
endif()
set(QAIC_EXECUTABLE "" CACHE FILEPATH
    "Path to the QAIC compiler used to generate the test IDL sources")
set(HEXAGON_CLANG "" CACHE FILEPATH
    "Clang compiler used to build the Hexagon DSP skeleton")
set(HEXAGON_LD "" CACHE FILEPATH
    "Linker used to link the Hexagon DSP skeleton")
set(ANDROID_NDK_HOME "" CACHE PATH
    "Path to the Android NDK root (required when TARGET_PLATFORM=android)")
set(DSP_ARCH "73" CACHE STRING
    "Hexagon DSP architecture version number (e.g. 68, 73, 75)")
if(NOT DSP_ARCH MATCHES "^[0-9]+$")
    message(FATAL_ERROR
        "DSP_ARCH='${DSP_ARCH}' is invalid; expected a decimal architecture number")
endif()

# Resolve the source-tree root when base_test is used in the fastrpc checkout.
if(FASTRPC_ROOT STREQUAL "")
    get_filename_component(_default_root "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
    set(FASTRPC_ROOT "${_default_root}" CACHE PATH
        "Path to the fastrpc repository root, when using an in-tree build" FORCE)
    message(STATUS "FASTRPC_ROOT not set — auto-detected: ${FASTRPC_ROOT}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(IS_ANDROID TRUE)
else()
    set(IS_ANDROID FALSE)
endif()

set(PLATFORM_C_DEFS _POSIX_C_SOURCE=200112L)
if(IS_ANDROID)
    list(APPEND PLATFORM_C_DEFS ANDROID)
endif()

# The source build uses the selected compiler driver/toolchain for linking.
# Do not override its dynamic linker; Android's toolchain requires PIE.
set(PLATFORM_LINK_FLAGS)
if(IS_ANDROID)
    list(APPEND PLATFORM_LINK_FLAGS -pie)
endif()

# Resolve FastRPC headers from an explicit override, install prefix, or source
# checkout.  This also supports using a system-installed FastRPC library.
set(_fastrpc_include_candidates)
if(FASTRPC_INCLUDE_DIR)
    list(APPEND _fastrpc_include_candidates "${FASTRPC_INCLUDE_DIR}")
endif()
if(FASTRPC_PREFIX)
    list(APPEND _fastrpc_include_candidates
        "${FASTRPC_PREFIX}/include/fastrpc"
        "${FASTRPC_PREFIX}/include")
endif()
if(FASTRPC_ROOT)
    list(APPEND _fastrpc_include_candidates "${FASTRPC_ROOT}/inc")
endif()

if(FASTRPC_INCLUDE_DIR STREQUAL "")
    foreach(_include_dir IN LISTS _fastrpc_include_candidates)
        if(EXISTS "${_include_dir}/remote.h")
            set(FASTRPC_INCLUDE_DIR "${_include_dir}" CACHE PATH
                "FastRPC include directory override" FORCE)
            break()
        endif()
    endforeach()
endif()

if(FASTRPC_INCLUDE_DIR STREQUAL "")
    find_path(FASTRPC_INCLUDE_DIR
        NAMES remote.h
        PATH_SUFFIXES fastrpc)
endif()

if(FASTRPC_INCLUDE_DIR STREQUAL "" OR NOT EXISTS "${FASTRPC_INCLUDE_DIR}/remote.h")
    message(FATAL_ERROR
        "FastRPC headers were not found. Set -DFASTRPC_INCLUDE_DIR=<path> or "
        "provide -DFASTRPC_ROOT=<fastrpc source root> or "
        "-DFASTRPC_PREFIX=<install prefix>.")
endif()

set(_fastrpc_library_paths)
if(FASTRPC_PREFIX)
    list(APPEND _fastrpc_library_paths
        "${FASTRPC_PREFIX}/lib"
        "${FASTRPC_PREFIX}/lib64")
    if(CMAKE_LIBRARY_ARCHITECTURE)
        list(APPEND _fastrpc_library_paths
            "${FASTRPC_PREFIX}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
    endif()
endif()
if(FASTRPC_ROOT)
    list(APPEND _fastrpc_library_paths "${FASTRPC_ROOT}/src/.libs")
endif()

if(FASTRPC_LIBRARY STREQUAL "")
    find_library(FASTRPC_LIBRARY
        NAMES cdsprpc libcdsprpc.so.1 libcdsprpc
        PATHS ${_fastrpc_library_paths}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH)
endif()

# Autotools may provide only a fully versioned file without the development
# symlink. Accept any normal shared-library output before system search paths.
if(FASTRPC_LIBRARY STREQUAL "" OR NOT EXISTS "${FASTRPC_LIBRARY}")
    foreach(_library_dir IN LISTS _fastrpc_library_paths)
        file(GLOB _versioned_libraries CONFIGURE_DEPENDS LIST_DIRECTORIES false
            "${_library_dir}/libcdsprpc.so*")
        if(_versioned_libraries)
            list(SORT _versioned_libraries COMPARE NATURAL ORDER DESCENDING)
            list(GET _versioned_libraries 0 FASTRPC_LIBRARY)
            break()
        endif()
    endforeach()
endif()

if(_fastrpc_library_explicit AND
   (FASTRPC_LIBRARY STREQUAL "" OR NOT EXISTS "${FASTRPC_LIBRARY}"))
    message(FATAL_ERROR
        "Explicit FASTRPC_LIBRARY does not exist: ${FASTRPC_LIBRARY}")
endif()

if(FASTRPC_LIBRARY STREQUAL "")
    find_library(FASTRPC_LIBRARY
        NAMES cdsprpc libcdsprpc.so.1 libcdsprpc)
endif()

if(FASTRPC_LIBRARY STREQUAL "" OR NOT EXISTS "${FASTRPC_LIBRARY}")
    message(FATAL_ERROR
        "libcdsprpc was not found. Set -DFASTRPC_LIBRARY=/path/to/libcdsprpc.so, "
        "or provide -DFASTRPC_ROOT/-DFASTRPC_PREFIX for a normal FastRPC build.")
endif()

unset(_versioned_libraries)
unset(_library_dir)
unset(_fastrpc_library_explicit)

set(FASTRPC_INC_DIR "${FASTRPC_INCLUDE_DIR}")
message(STATUS "FastRPC headers: ${FASTRPC_INC_DIR}")
message(STATUS "FastRPC library: ${FASTRPC_LIBRARY}")

add_library(cdsprpc SHARED IMPORTED GLOBAL)
set_target_properties(cdsprpc PROPERTIES
    IMPORTED_LOCATION "${FASTRPC_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${FASTRPC_INC_DIR}"
)

# Keep the selected library directory available to linkers that need it for
# transitive runtime dependencies, without replacing compiler-driver defaults.
get_filename_component(_fastrpc_library_dir "${FASTRPC_LIBRARY}" DIRECTORY)
set_property(TARGET cdsprpc APPEND PROPERTY
    INTERFACE_LINK_DIRECTORIES "${_fastrpc_library_dir}")

unset(_fastrpc_include_candidates)
unset(_fastrpc_library_paths)
unset(_fastrpc_library_dir)

if(QAIC_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR
        "QAIC_EXECUTABLE is not set.\n"
        "Configure with -DQAIC_EXECUTABLE=/path/to/qaic")
endif()

if(NOT EXISTS "${QAIC_EXECUTABLE}" OR IS_DIRECTORY "${QAIC_EXECUTABLE}")
    message(FATAL_ERROR
        "QAIC compiler is not executable at: ${QAIC_EXECUTABLE}\n"
        "Set -DQAIC_EXECUTABLE= to a valid QAIC compiler path.")
endif()

execute_process(
    COMMAND "${QAIC_EXECUTABLE}" --version
    RESULT_VARIABLE _qaic_status
    OUTPUT_QUIET
    ERROR_VARIABLE _qaic_error
)
if(NOT _qaic_status EQUAL 0)
    message(FATAL_ERROR
        "QAIC compiler could not be executed: ${QAIC_EXECUTABLE}\n"
        "${_qaic_error}")
endif()
unset(_qaic_status)
unset(_qaic_error)

add_library(base_test_warnings INTERFACE)
target_compile_options(base_test_warnings INTERFACE -Wall -Wextra -Werror)
target_compile_features(base_test_warnings INTERFACE c_std_11)

set(UNITY_EXTENSION_FIXTURE ON CACHE BOOL
    "Build Unity with the fixture extension" FORCE)
set(UNITY_EXTENSION_MEMORY ON CACHE BOOL
    "Build Unity with the memory extension" FORCE)
add_subdirectory(vendor/unity EXCLUDE_FROM_ALL)

set(UNITY_COMPILE_DEFS
    UNITY_INCLUDE_PRINT_FORMATTED
    UNITY_OUTPUT_CHAR=fastrpc_unity_output_char
    UNITY_OUTPUT_CHAR_HEADER_DECLARATION=fastrpc_unity_output_char\(int\))
target_compile_definitions(unity PUBLIC ${UNITY_COMPILE_DEFS})
target_compile_definitions(unity PRIVATE ${UNITY_COMPILE_DEFS})
# The custom reporter provides fastrpc_unity_output_char(). Keep Unity's
# output callback shared by the framework and the test-side wrapper so Unity
# lifecycle output can be suppressed without redirecting test stdout.

add_library(unity_dep INTERFACE)
target_link_libraries(unity_dep INTERFACE unity)

add_subdirectory(idl)
add_subdirectory(utils)

add_library(fastrpc_dep INTERFACE)
target_include_directories(fastrpc_dep INTERFACE "${FASTRPC_INC_DIR}")
target_link_libraries(fastrpc_dep INTERFACE cdsprpc)

add_library(common_test_iface INTERFACE)
add_dependencies(common_test_iface fastrpc_test_idl)
target_include_directories(common_test_iface INTERFACE
    "${CMAKE_SOURCE_DIR}"
    "${FASTRPC_TEST_IDL_INC_DIR}")
target_compile_definitions(common_test_iface INTERFACE
    ${PLATFORM_C_DEFS}
    ${UNITY_COMPILE_DEFS})
target_compile_options(common_test_iface INTERFACE
    "-include${CMAKE_SOURCE_DIR}/utils/reporting/unity/unity_fixture_file_output.h")

add_library(common_test_dep INTERFACE)
target_link_libraries(common_test_dep INTERFACE
    common_test_iface
    utils_dep
    unity_dep
    fastrpc_dep
    fastrpc_test_stub
    base_test_warnings)

set(ALL_SUITE_LIBS "")
set(REGULAR_CATEGORY_DISPATCH_SOURCES "")
add_subdirectory(test/e2e)
add_subdirectory(bin)
