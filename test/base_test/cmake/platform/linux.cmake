# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Platform: aarch64-linux-gnu  (GCC or Clang cross-compilation)
#
# Loaded automatically by the root CMakeLists.txt when
# TARGET_PLATFORM=linux (the default).
#
# Do NOT include this file directly.  It is selected by TARGET_PLATFORM=linux.

# ----------------------------------------------------------------------------
# System identity.
# ----------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ----------------------------------------------------------------------------
# Toolchain binaries.
#
# Prefer the standard FastRPC cross-tool environment (CC, CXX, AS, LD, AR,
# RANLIB, and STRIP).  Explicit CMake -D values take precedence over it.
# When no compiler is supplied, prefer the GCC AArch64 cross toolchain and use
# Clang with an explicit target only as a fallback.
# ----------------------------------------------------------------------------
if(NOT DEFINED CMAKE_C_COMPILER AND DEFINED ENV{CC} AND NOT "$ENV{CC}" STREQUAL "")
    set(CMAKE_C_COMPILER "$ENV{CC}")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER AND DEFINED ENV{CXX} AND NOT "$ENV{CXX}" STREQUAL "")
    set(CMAKE_CXX_COMPILER "$ENV{CXX}")
endif()
if(NOT DEFINED CMAKE_ASM_COMPILER AND DEFINED ENV{AS} AND NOT "$ENV{AS}" STREQUAL "")
    set(CMAKE_ASM_COMPILER "$ENV{AS}")
endif()
if(NOT DEFINED CMAKE_AR AND DEFINED ENV{AR} AND NOT "$ENV{AR}" STREQUAL "")
    set(CMAKE_AR "$ENV{AR}" CACHE FILEPATH "Archiver")
endif()
if(NOT DEFINED CMAKE_RANLIB AND DEFINED ENV{RANLIB} AND NOT "$ENV{RANLIB}" STREQUAL "")
    set(CMAKE_RANLIB "$ENV{RANLIB}" CACHE FILEPATH "Ranlib")
endif()
if(NOT DEFINED CMAKE_LINKER AND DEFINED ENV{LD} AND NOT "$ENV{LD}" STREQUAL "")
    set(CMAKE_LINKER "$ENV{LD}" CACHE FILEPATH "Linker")
endif()
if(NOT DEFINED CMAKE_STRIP AND DEFINED ENV{STRIP} AND NOT "$ENV{STRIP}" STREQUAL "")
    set(CMAKE_STRIP "$ENV{STRIP}" CACHE FILEPATH "Strip")
endif()

set(_base_test_standalone_clang FALSE)
if(NOT DEFINED CMAKE_C_COMPILER)
    find_program(_base_test_gcc NAMES aarch64-linux-gnu-gcc)
    if(_base_test_gcc)
        set(CMAKE_C_COMPILER "${_base_test_gcc}")
    else()
        set(CMAKE_C_COMPILER clang)
        set(_base_test_standalone_clang TRUE)
    endif()
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    if(_base_test_standalone_clang)
        set(CMAKE_CXX_COMPILER clang++)
    else()
        find_program(_base_test_gxx NAMES aarch64-linux-gnu-g++)
        if(_base_test_gxx)
            set(CMAKE_CXX_COMPILER "${_base_test_gxx}")
        endif()
    endif()
endif()

if(NOT DEFINED CMAKE_AR)
    if(_base_test_standalone_clang)
        set(CMAKE_AR llvm-ar CACHE FILEPATH "Archiver")
    else()
        find_program(_base_test_ar NAMES aarch64-linux-gnu-ar)
        if(_base_test_ar)
            set(CMAKE_AR "${_base_test_ar}" CACHE FILEPATH "Archiver")
        endif()
    endif()
endif()
if(NOT DEFINED CMAKE_RANLIB)
    if(_base_test_standalone_clang)
        set(CMAKE_RANLIB llvm-ranlib CACHE FILEPATH "Ranlib")
    else()
        find_program(_base_test_ranlib NAMES aarch64-linux-gnu-ranlib)
        if(_base_test_ranlib)
            set(CMAKE_RANLIB "${_base_test_ranlib}" CACHE FILEPATH "Ranlib")
        endif()
    endif()
endif()
if(NOT DEFINED CMAKE_LINKER)
    if(_base_test_standalone_clang)
        set(CMAKE_LINKER ld.lld CACHE FILEPATH "Linker")
    else()
        find_program(_base_test_ld NAMES aarch64-linux-gnu-ld)
        if(_base_test_ld)
            set(CMAKE_LINKER "${_base_test_ld}" CACHE FILEPATH "Linker")
        endif()
    endif()
endif()
if(NOT DEFINED CMAKE_STRIP)
    if(_base_test_standalone_clang)
        set(CMAKE_STRIP llvm-strip CACHE FILEPATH "Strip")
    else()
        find_program(_base_test_strip NAMES aarch64-linux-gnu-strip)
        if(_base_test_strip)
            set(CMAKE_STRIP "${_base_test_strip}" CACHE FILEPATH "Strip")
        endif()
    endif()
endif()

# Standalone clang builds need an explicit target triple.  Never add this
# Clang-only flag to a compiler supplied by Autotools (for example GCC).
if(_base_test_standalone_clang)
    set(CMAKE_C_FLAGS_INIT "--target=aarch64-linux-gnu")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "--target=aarch64-linux-gnu")
endif()
unset(_base_test_standalone_clang)
unset(_base_test_gcc)
unset(_base_test_gxx)
unset(_base_test_ar)
unset(_base_test_ranlib)
unset(_base_test_ld)
unset(_base_test_strip)

# Caller-provided CFLAGS/LDFLAGS arrive through the environment and take
# precedence over these standalone defaults.

# ----------------------------------------------------------------------------
# Sysroot search behaviour
# Prevent CMake from accidentally resolving headers/libs from the build host.
# ----------------------------------------------------------------------------
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
