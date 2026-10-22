# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Host runners and the legacy-compatible base plugin.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_executable(fastrpc_test "${FASTRPC_TEST_SOURCE_DIR}/root_all_tests.c")
target_include_directories(fastrpc_test PRIVATE
    "${FASTRPC_TEST_SOURCE_DIR}"
    "${FASTRPC_TEST_SOURCE_DIR}/feature"
    "${FASTRPC_TEST_SOURCE_DIR}/unit"
    "${FASTRPC_TEST_SOURCE_DIR}/e2e"
    "${FASTRPC_TEST_IDL_INC_DIR}")
target_link_libraries(fastrpc_test PRIVATE common_test_dep ${ALL_SUITE_LIBS})
target_link_options(fastrpc_test PRIVATE ${PLATFORM_LINK_FLAGS})
set_target_properties(fastrpc_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Keep the CMake runner distinct from the Autotools-installed fastrpc_test.
set_target_properties(fastrpc_test PROPERTIES OUTPUT_NAME "test-fastrpc")
install(TARGETS fastrpc_test RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

add_library(base_test MODULE "${FASTRPC_TEST_SOURCE_DIR}/root_all_tests.c")
target_compile_definitions(base_test PRIVATE BASE_TEST_PLUGIN)
target_include_directories(base_test PRIVATE
    "${FASTRPC_TEST_SOURCE_DIR}"
    "${FASTRPC_TEST_SOURCE_DIR}/feature"
    "${FASTRPC_TEST_SOURCE_DIR}/unit"
    "${FASTRPC_TEST_SOURCE_DIR}/e2e"
    "${FASTRPC_TEST_IDL_INC_DIR}")
target_link_libraries(base_test PRIVATE common_test_dep ${ALL_SUITE_LIBS})
set_target_properties(base_test PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    OUTPUT_NAME "base_test")
install(TARGETS base_test
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/fastrpc_test")

find_program(HEXAGON_CLANG NAMES clang)
find_program(HEXAGON_LD NAMES ld.lld)
if(HEXAGON_CLANG AND HEXAGON_LD)
    set(_gen_dir "${FASTRPC_TEST_IDL_INC_DIR}")
    set(_impl_dir "${FASTRPC_TEST_SOURCE_DIR}/idl/impl")
    set(_bin_dir "${CMAKE_BINARY_DIR}/bin")
    set(_dsp_flags
        --target=hexagon -mv${DSP_ARCH} -fPIC -Wall -Werror -std=c11
        -D_POSIX_C_SOURCE=200112L
        -I${_gen_dir} -I${FASTRPC_INC_DIR})

    add_custom_command(
        OUTPUT "${_bin_dir}/fastrpc_test_skel.dsp.o"
        COMMAND "${HEXAGON_CLANG}" ${_dsp_flags}
                -c "${FASTRPC_TEST_SKEL_SOURCE}"
                -o "${_bin_dir}/fastrpc_test_skel.dsp.o"
        DEPENDS "${FASTRPC_TEST_SKEL_SOURCE}"
        COMMENT "Compiling base DSP skeleton"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_bin_dir}/fastrpc_test_imp.dsp.o"
        COMMAND "${HEXAGON_CLANG}" ${_dsp_flags}
                -c "${_impl_dir}/fastrpc_test_imp.c"
                -o "${_bin_dir}/fastrpc_test_imp.dsp.o"
        DEPENDS "${_impl_dir}/fastrpc_test_imp.c" "${FASTRPC_TEST_IDL_HEADER}"
        COMMENT "Compiling base DSP implementation"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_bin_dir}/libfastrpc_test_skel.so"
        COMMAND "${HEXAGON_LD}" -shared
                -soname=libfastrpc_test_skel.so
                -z separate-loadable-segments
                "${_bin_dir}/fastrpc_test_skel.dsp.o"
                "${_bin_dir}/fastrpc_test_imp.dsp.o"
                -o "${_bin_dir}/libfastrpc_test_skel.so"
        DEPENDS "${_bin_dir}/fastrpc_test_skel.dsp.o"
                "${_bin_dir}/fastrpc_test_imp.dsp.o"
        COMMENT "Linking base DSP skeleton"
        VERBATIM)
    add_custom_target(fastrpc_test_skel ALL
        DEPENDS "${_bin_dir}/libfastrpc_test_skel.so")
    add_dependencies(fastrpc_test_skel fastrpc_test_idl)
    message(STATUS "Base DSP skeleton enabled for mv${DSP_ARCH}")
else()
    message(STATUS "Hexagon clang/ld.lld not found; base DSP skeleton disabled")
endif()
