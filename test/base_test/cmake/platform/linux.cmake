# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Platform: aarch64-linux-gnu  (clang/LLVM cross-compilation)
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
# ----------------------------------------------------------------------------
# Preserve compiler/tool selections forwarded by the Autotools build.  These
# defaults apply only to standalone CMake invocations.
set(_base_test_external_toolchain FALSE)
if(DEFINED CMAKE_C_COMPILER OR DEFINED ENV{CC})
    set(_base_test_external_toolchain TRUE)
endif()
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    set(CMAKE_C_COMPILER clang)
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER clang++)
endif()
if(NOT DEFINED CMAKE_AR AND NOT DEFINED ENV{AR})
    set(CMAKE_AR llvm-ar CACHE FILEPATH "Archiver")
endif()
if(NOT DEFINED CMAKE_RANLIB AND NOT DEFINED ENV{RANLIB})
    set(CMAKE_RANLIB llvm-ranlib CACHE FILEPATH "Ranlib")
endif()
if(NOT DEFINED CMAKE_LINKER AND NOT DEFINED ENV{LD})
    set(CMAKE_LINKER ld.lld CACHE FILEPATH "Linker")
endif()
if(NOT DEFINED CMAKE_STRIP AND NOT DEFINED ENV{STRIP})
    set(CMAKE_STRIP llvm-strip CACHE FILEPATH "Strip")
endif()

# Standalone clang builds need an explicit target triple.  Never add this
# Clang-only flag to a compiler supplied by Autotools (for example GCC).
if(NOT _base_test_external_toolchain)
    set(CMAKE_C_FLAGS_INIT "--target=aarch64-linux-gnu")
    set(CMAKE_EXE_LINKER_FLAGS_INIT "--target=aarch64-linux-gnu")
endif()
unset(_base_test_external_toolchain)

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
