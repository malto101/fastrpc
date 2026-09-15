# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# test/fuzz/CMakeLists.txt
#
# Only processed when -DENABLE_FUZZ_TESTS=ON (see root CMakeLists.txt) — these
# harnesses need clang's libFuzzer/sanitizer runtime for the target, which not
# every toolchain has, so they're opt-in.
#
# Unlike test/unit and test/feature suites, each fuzz suite's *.c is compiled
# with -fsanitize=fuzzer-no-link (coverage instrumentation, no auto-generated
# main — see https://llvm.org/docs/LibFuzzer.html#using-libfuzzer-as-a-library)
# so it can be linked into the single test-fastrpc binary: root_all_tests.c
# owns main() and hands off to libFuzzer's driver via LLVMFuzzerRunDriver()
# when invoked with --fuzz, instead of each suite getting its own executable
# and its own libFuzzer-supplied main().
#
# For each suite in _fuzz_suites:
#   1. Discovers all *.c files (fuzz_*.c) via file(GLOB CONFIGURE_DEPENDS).
#   2. Embeds every test/fuzz/<suite>/corpus/<entry_point>/ dir as generated
#      C byte arrays (see cmake/embed_corpus.cmake) so root_all_tests.c can
#      self-seed a suite's default corpus directory on disk when it's absent
#      — no separate `adb push` of the checked-in seed corpus required.
#   3. Builds a per-suite static library  fuzz_<name>_all.
#
# Propagates ALL_FUZZ_SUITE_LIBS to the parent scope so bin/CMakeLists.txt can
# link the fuzz suite libraries into test-fastrpc.

include("${CMAKE_CURRENT_LIST_DIR}/cmake/embed_corpus.cmake")

# Suite names — add a new entry here when creating a new test/fuzz/<name>/ dir.
# Named after the module under test (matches _unit_suites/_feat_suites), not
# the specific API — e.g. dspqueue, not dspqueue_create. A suite dir can hold
# more than one fuzz_<api>.c entry point as more APIs become fuzzable.
set(_fuzz_suites dspqueue)

set(_new_fuzz_suite_libs "")

foreach(_suite IN LISTS _fuzz_suites)
    set(_suite_dir "${CMAKE_SOURCE_DIR}/test/fuzz/${_suite}")

    # Discover all *.c sources for this suite
    file(GLOB _suite_srcs
        CONFIGURE_DEPENDS
        "${_suite_dir}/*.c"
    )

    if(NOT _suite_srcs)
        message(WARNING "test/fuzz/${_suite}: no .c files found — skipping suite")
        continue()
    endif()

    # Embed each corpus/<entry_point>/ dir's seed files, one generated source
    # per entry point (skip any with zero seed files — nothing to embed).
    file(GLOB _corpus_dirs LIST_DIRECTORIES true "${_suite_dir}/corpus/*")
    foreach(_corpus_dir IN LISTS _corpus_dirs)
        if(IS_DIRECTORY "${_corpus_dir}")
            file(GLOB _corpus_seed_files LIST_DIRECTORIES false "${_corpus_dir}/*")
            if(_corpus_seed_files)
                get_filename_component(_entry_point "${_corpus_dir}" NAME)
                fastrpc_embed_corpus("${_entry_point}" "${_corpus_dir}" _embedded_corpus_src)
                list(APPEND _suite_srcs "${_embedded_corpus_src}")
            endif()
        endif()
    endforeach()

    add_library("fuzz_${_suite}_all" STATIC
        ${_suite_srcs}
    )

    set_target_properties("fuzz_${_suite}_all" PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    target_include_directories("fuzz_${_suite}_all" PRIVATE
        "${_suite_dir}"
        "${CMAKE_SOURCE_DIR}/test/fuzz"
    )

    target_link_libraries("fuzz_${_suite}_all" PRIVATE
        fastrpc_dep
    )

    target_compile_options("fuzz_${_suite}_all" PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -g
    )

    list(APPEND _new_fuzz_suite_libs "fuzz_${_suite}_all")
endforeach()

set(ALL_FUZZ_SUITE_LIBS ${_new_fuzz_suite_libs} PARENT_SCOPE)
