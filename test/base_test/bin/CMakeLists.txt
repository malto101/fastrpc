# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# bin/CMakeLists.txt
#
# Produces two deployable artifacts, all placed in <builddir>/bin/:
#
#   fastrpc_test
#     Linked from root_all_tests.c (owns main()) + every per-suite static lib.
#
#   libfastrpc_test_skel.so  (built when clang is in PATH)
#     Hexagon DSP shared library compiled with --target=hexagon.
#     Push to device separately — never linked into any host binary.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ============================================================================
# fastrpc_test — unified host binary
# ============================================================================
add_executable(fastrpc_test
    "${CMAKE_SOURCE_DIR}/root_all_tests.c"
    "${E2E_SUITE_DISPATCH_SOURCE}"
)

target_include_directories(fastrpc_test PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/test/feature"
    "${FASTRPC_TEST_IDL_INC_DIR}"
)

target_link_libraries(fastrpc_test PRIVATE
    common_test_dep
    ${ALL_SUITE_LIBS}
)

target_link_options(fastrpc_test PRIVATE
    ${PLATFORM_LINK_FLAGS}
)

target_link_libraries(fastrpc_test PRIVATE
    base_test_warnings
)

set_target_properties(fastrpc_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# libfastrpc_test_skel.so — Hexagon DSP skel
# ============================================================================
set(HEXAGON_CLANG "" CACHE FILEPATH
    "Clang compiler used to build the Hexagon DSP skeleton")
if(HEXAGON_CLANG)
    set(_hexagon_clang "${HEXAGON_CLANG}")
else()
    find_program(_hexagon_clang NAMES hexagon-clang clang
        NO_CMAKE_FIND_ROOT_PATH)
endif()

if(_hexagon_clang)
    set(_gen_dir  "${FASTRPC_TEST_IDL_INC_DIR}")
    set(_impl_dir "${CMAKE_SOURCE_DIR}/idl/impl")
    set(_bin_dir  "${CMAKE_BINARY_DIR}/bin")

    set(_dsp_flags
        --target=hexagon
        -mv${DSP_ARCH}
        -fPIC
        -Wall
        -Werror
        -std=c11
        -D_POSIX_C_SOURCE=200112L
        -I${_gen_dir}
        -I${FASTRPC_INC_DIR}
    )

    add_custom_command(
        OUTPUT  "${_bin_dir}/fastrpc_test_skel.dsp.o"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_bin_dir}"
        COMMAND "${_hexagon_clang}" ${_dsp_flags}
                -c "${FASTRPC_TEST_SKEL_SOURCE}"
                -o "${_bin_dir}/fastrpc_test_skel.dsp.o"
        DEPENDS
            "${FASTRPC_TEST_SKEL_SOURCE}"
            "${FASTRPC_TEST_IDL_HEADER}"
        COMMENT "Compiling DSP skel: fastrpc_test_skel.c"
        VERBATIM
    )

    add_custom_command(
        OUTPUT  "${_bin_dir}/fastrpc_test_imp.dsp.o"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_bin_dir}"
        COMMAND "${_hexagon_clang}" ${_dsp_flags}
                -c "${_impl_dir}/fastrpc_test_imp.c"
                -o "${_bin_dir}/fastrpc_test_imp.dsp.o"
        DEPENDS
            "${_impl_dir}/fastrpc_test_imp.c"
            "${FASTRPC_TEST_IDL_HEADER}"
        COMMENT "Compiling DSP imp: fastrpc_test_imp.c"
        VERBATIM
    )

    set(HEXAGON_LD "" CACHE FILEPATH
        "ld.lld linker used to link the Hexagon DSP skeleton")
    get_filename_component(_hexagon_clang_dir "${_hexagon_clang}" DIRECTORY)
    if(HEXAGON_LD)
        set(_hexagon_ld "${HEXAGON_LD}")
    else()
        find_program(_hexagon_ld NAMES ld.lld hexagon-ld.lld
            HINTS "${_hexagon_clang_dir}" NO_CMAKE_FIND_ROOT_PATH)
    endif()
    if(NOT _hexagon_ld)
        message(FATAL_ERROR
            "Hexagon linker was not found for ${_hexagon_clang}; "
            "set -DHEXAGON_LD=/path/to/ld.lld or update PATH.")
    endif()

    add_custom_command(
        OUTPUT  "${_bin_dir}/libfastrpc_test_skel.so"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_bin_dir}"
        COMMAND "${_hexagon_ld}"
                -shared
                -soname=libfastrpc_test_skel.so
                -z separate-loadable-segments
                "${_bin_dir}/fastrpc_test_skel.dsp.o"
                "${_bin_dir}/fastrpc_test_imp.dsp.o"
                -o "${_bin_dir}/libfastrpc_test_skel.so"
        DEPENDS
            "${_bin_dir}/fastrpc_test_skel.dsp.o"
            "${_bin_dir}/fastrpc_test_imp.dsp.o"
        COMMENT "Linking DSP skel: libfastrpc_test_skel.so"
        VERBATIM
    )

    add_custom_target(fastrpc_test_skel ALL
        DEPENDS "${_bin_dir}/libfastrpc_test_skel.so"
    )
    add_dependencies(fastrpc_test_skel fastrpc_test_idl)

    message(STATUS
        "DSP clang found at ${_hexagon_clang} — "
        "fastrpc_test_skel target enabled (DSP arch: mv${DSP_ARCH})")
else()
    message(STATUS
        "DSP clang not found — "
        "fastrpc_test_skel target will not be available "
        "(host binary fastrpc_test is unaffected)")
endif()

unset(_hexagon_clang)
unset(_hexagon_clang_dir)
unset(_hexagon_ld)
