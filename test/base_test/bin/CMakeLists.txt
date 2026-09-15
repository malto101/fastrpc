# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# bin/CMakeLists.txt
#
# Produces three deployable artifacts, all placed in <builddir>/bin/:
#
#   test-fastrpc
#     Linked from root_all_tests.c (owns main()) + every per-suite static lib.
#
#   libbase_test.so
#     Legacy fastrpc_test plugin exporting run_test(domain_id, unsigned_pd).
#
#   libfastrpc_test_skel.so  (built when clang is in PATH)
#     Hexagon DSP shared library compiled with --target=hexagon.
#     Push to device separately — never linked into any host binary.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ============================================================================
# test-fastrpc — unified host binary
# ============================================================================
add_executable(test-fastrpc
    "${CMAKE_SOURCE_DIR}/root_all_tests.c"
)

target_include_directories(test-fastrpc PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/test/feature"
    "${FASTRPC_TEST_IDL_INC_DIR}"
)

target_link_libraries(test-fastrpc PRIVATE
    common_test_dep
    ${ALL_SUITE_LIBS}
)

target_link_options(test-fastrpc PRIVATE
    ${PLATFORM_LINK_FLAGS}
)

set_target_properties(test-fastrpc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# When ENABLE_FUZZ_TESTS is on, link the fuzz suite libs (built by
# test/fuzz/CMakeLists.txt, sanitized on their own) into test-fastrpc and give
# it a --fuzz flag (see root_all_tests.c). Only the fuzz suite objects are
# compiled with the sanitizers; the link step still needs the same
# -fsanitize= flags so the runtime they depend on is present in the binary.
if(ENABLE_FUZZ_TESTS)
    target_link_libraries(test-fastrpc PRIVATE
        ${ALL_FUZZ_SUITE_LIBS}
    )

    target_compile_definitions(test-fastrpc PRIVATE
        ENABLE_FUZZ_TESTS
    )

    target_link_options(test-fastrpc PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
    )

    # root_all_tests.c owns main() and calls LLVMFuzzerRunDriver() directly
    # instead of using libFuzzer's own driver entry point (see
    # test/fuzz/CMakeLists.txt) — -fsanitize=fuzzer-no-link deliberately
    # skips auto-linking any libFuzzer runtime, so the "no_main" variant
    # (LLVMFuzzerRunDriver + friends, no main()) needs linking explicitly.
    # Resolved through the compiler itself, not hardcoded, so it honours
    # whatever --target/-resource-dir this build is already using.
    separate_arguments(_c_flags_list NATIVE_COMMAND "${CMAKE_C_FLAGS}")
    execute_process(
        COMMAND "${CMAKE_C_COMPILER}" ${_c_flags_list}
                -print-file-name=libclang_rt.fuzzer_no_main-aarch64.a
        OUTPUT_VARIABLE _fuzzer_no_main_lib
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NOT EXISTS "${_fuzzer_no_main_lib}")
        message(FATAL_ERROR
            "Could not locate libclang_rt.fuzzer_no_main-aarch64.a via the "
            "compiler (got: '${_fuzzer_no_main_lib}'). --fuzz needs this "
            "compiler-rt runtime linked explicitly since root_all_tests.c "
            "owns main() itself — ensure your clang installation ships "
            "aarch64 compiler-rt libraries for the target triple.")
    endif()

    # libclang_rt.fuzzer_no_main is C++ (std::string, containers, operator
    # new/delete) but test-fastrpc links as a C program (plain clang, not
    # clang++) — pull in libstdc++ explicitly for those symbols.
    target_link_libraries(test-fastrpc PRIVATE "${_fuzzer_no_main_lib}" stdc++)
endif()

# ============================================================================
# libbase_test.so — plugin for the legacy fastrpc_test dlopen() runner
# ============================================================================
add_library(base_test MODULE
    "${CMAKE_SOURCE_DIR}/root_all_tests.c"
)

target_compile_definitions(base_test PRIVATE
    BASE_TEST_PLUGIN
)

target_include_directories(base_test PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/test/feature"
    "${FASTRPC_TEST_IDL_INC_DIR}"
)

target_link_libraries(base_test PRIVATE
    common_test_dep
    ${ALL_SUITE_LIBS}
)

set_target_properties(base_test PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "base_test"
)

install(TARGETS base_test
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/fastrpc_test"
)

# ============================================================================
# libfastrpc_test_skel.so — Hexagon DSP skel
# ============================================================================
find_program(HEXAGON_CLANG clang)

if(HEXAGON_CLANG)
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
        COMMAND "${HEXAGON_CLANG}" ${_dsp_flags}
                -c "${FASTRPC_TEST_SKEL_SOURCE}"
                -o "${_bin_dir}/fastrpc_test_skel.dsp.o"
        DEPENDS "${FASTRPC_TEST_SKEL_SOURCE}"
        COMMENT "Compiling DSP skel: fastrpc_test_skel.c"
        VERBATIM
    )

    add_custom_command(
        OUTPUT  "${_bin_dir}/fastrpc_test_imp.dsp.o"
        COMMAND "${HEXAGON_CLANG}" ${_dsp_flags}
                -c "${_impl_dir}/fastrpc_test_imp.c"
                -o "${_bin_dir}/fastrpc_test_imp.dsp.o"
        DEPENDS
            "${_impl_dir}/fastrpc_test_imp.c"
            "${FASTRPC_TEST_IDL_HEADER}"
        COMMENT "Compiling DSP imp: fastrpc_test_imp.c"
        VERBATIM
    )

    add_custom_command(
        OUTPUT  "${_bin_dir}/libfastrpc_test_skel.so"
        COMMAND ld.lld
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
        "clang found at ${HEXAGON_CLANG} — "
        "fastrpc_test_skel target enabled (DSP arch: mv${DSP_ARCH})")
else()
    message(STATUS
        "clang not found — "
        "fastrpc_test_skel target will not be available "
        "(host binary test-fastrpc is unaffected)")
endif()
