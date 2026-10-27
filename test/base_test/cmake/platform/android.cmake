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
            "Path to the Android NDK root" FORCE)
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
# Toolchain binaries.
#
# Reuse the standard FastRPC Android environment (CC, CXX, AS, LD, AR,
# RANLIB, and STRIP).  Explicit CMake -D values take precedence over it.
# The NDK wrapper defaults below are used only when the corresponding
# environment variable is not set.
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

if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "${_ndk_bin}/aarch64-linux-android35-clang")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${_ndk_bin}/aarch64-linux-android35-clang++")
endif()
if(NOT DEFINED CMAKE_AR)
    set(CMAKE_AR "${_ndk_bin}/llvm-ar" CACHE FILEPATH "Archiver")
endif()
if(NOT DEFINED CMAKE_RANLIB)
    set(CMAKE_RANLIB "${_ndk_bin}/llvm-ranlib" CACHE FILEPATH "Ranlib")
endif()
if(NOT DEFINED CMAKE_LINKER)
    set(CMAKE_LINKER "${_ndk_bin}/ld.lld" CACHE FILEPATH "Linker")
endif()
if(NOT DEFINED CMAKE_STRIP)
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
