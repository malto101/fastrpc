# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Platform: aarch64-linux-android  API 35  (NDK cross-compilation)
#
# Loaded automatically by the root CMakeLists.txt when
# TARGET_PLATFORM=android.
#
# Do NOT include this file directly.  It is selected by TARGET_PLATFORM=android.
# ANDROID_NDK_HOME can also be set in the environment before invoking cmake:
#   export ANDROID_NDK_HOME=/path/to/android-ndk

# ----------------------------------------------------------------------------
# Resolve ANDROID_NDK_HOME
# Priority: CMake cache variable  >  environment variable  >  fatal error
# ----------------------------------------------------------------------------
if(NOT DEFINED ANDROID_NDK_HOME OR ANDROID_NDK_HOME STREQUAL "")
    if(DEFINED ENV{ANDROID_NDK_HOME})
        set(ANDROID_NDK_HOME "$ENV{ANDROID_NDK_HOME}" CACHE PATH
            "Path to the Android NDK root")
        message(STATUS "ANDROID_NDK_HOME resolved from environment: ${ANDROID_NDK_HOME}")
    else()
        message(FATAL_ERROR
            "ANDROID_NDK_HOME is not set.\n"
            "Provide it in one of two ways:\n"
            "  1. cmake ... -DTARGET_PLATFORM=android "
            "-DANDROID_NDK_HOME=/path/to/android-ndk\n"
            "  2. export ANDROID_NDK_HOME=/path/to/android-ndk  "
            "before running cmake")
    endif()
endif()

set(_ndk_bin
    "${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin")

message(STATUS "Android NDK bin dir: ${_ndk_bin}")

# ----------------------------------------------------------------------------
# Toolchain binaries
# API level 35 is encoded in the compiler wrapper name:
#   aarch64-linux-android35-clang
# ----------------------------------------------------------------------------
# Preserve tools forwarded by the Autotools build.  These NDK wrappers are
# defaults for standalone CMake invocations only.
if(NOT DEFINED CMAKE_C_COMPILER AND NOT DEFINED ENV{CC})
    set(CMAKE_C_COMPILER "${_ndk_bin}/aarch64-linux-android35-clang")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER AND NOT DEFINED ENV{CXX})
    set(CMAKE_CXX_COMPILER "${_ndk_bin}/aarch64-linux-android35-clang++")
endif()
if(NOT DEFINED CMAKE_AR AND NOT DEFINED ENV{AR})
    set(CMAKE_AR "${_ndk_bin}/llvm-ar" CACHE FILEPATH "Archiver")
endif()
if(NOT DEFINED CMAKE_RANLIB AND NOT DEFINED ENV{RANLIB})
    set(CMAKE_RANLIB "${_ndk_bin}/llvm-ranlib" CACHE FILEPATH "Ranlib")
endif()
if(NOT DEFINED CMAKE_LINKER AND NOT DEFINED ENV{LD})
    set(CMAKE_LINKER "${_ndk_bin}/ld.lld" CACHE FILEPATH "Linker")
endif()
if(NOT DEFINED CMAKE_STRIP AND NOT DEFINED ENV{STRIP})
    set(CMAKE_STRIP "${_ndk_bin}/llvm-strip" CACHE FILEPATH "Strip")
endif()

if(NOT DEFINED ENV{CFLAGS})
    set(CMAKE_C_FLAGS_INIT "-DANDROID")
endif()
if(NOT DEFINED ENV{LDFLAGS})
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-pie")
endif()

# ----------------------------------------------------------------------------
# System identity
# ----------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME      Android)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ----------------------------------------------------------------------------
# Sysroot search behaviour
# ----------------------------------------------------------------------------
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
