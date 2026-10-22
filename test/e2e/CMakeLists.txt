# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# End-to-end examples migrated from test.bak.  The host tests use the same
# Unity/common_test_dep targets as the unit and feature suites.  The DSP
# artifacts are optional because a Hexagon compiler and SDK are not present in
# every host build.

set(_e2e_names calculator hap_example multithreading)
set(_e2e_idl_dir "${CMAKE_CURRENT_SOURCE_DIR}/idl")
set(_e2e_gen_root "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_e2e_generated_headers)
set(_e2e_generated_stubs)
set(_e2e_generated_skels)

foreach(_name IN LISTS _e2e_names)
    set(_src "${_e2e_idl_dir}/${_name}.idl")
    set(_out "${_e2e_gen_root}/${_name}")
    set(_header "${_out}/${_name}.h")
    set(_stub "${_out}/${_name}_stub.c")
    set(_skel "${_out}/${_name}_skel.c")

    add_custom_command(
        OUTPUT "${_header}" "${_stub}" "${_skel}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_out}"
        COMMAND "${QAIC_EXECUTABLE}"
                "-I=${FASTRPC_IDL_DIR}"
                "${_src}" -st -o "${_out}"
        DEPENDS "${_src}" "${FASTRPC_IDL_DIR}/AEEStdDef.idl"
                "${FASTRPC_IDL_DIR}/remote.idl" "${QAIC_EXECUTABLE}"
        COMMENT "Generating ${_name} FastRPC bindings"
        VERBATIM)

    list(APPEND _e2e_generated_headers "${_header}")
    list(APPEND _e2e_generated_stubs "${_stub}")
    list(APPEND _e2e_generated_skels "${_skel}")
endforeach()

add_custom_target(fastrpc_e2e_idl DEPENDS
    ${_e2e_generated_headers} ${_e2e_generated_stubs} ${_e2e_generated_skels})

add_library(fastrpc_e2e_stub STATIC ${_e2e_generated_stubs})
add_dependencies(fastrpc_e2e_stub fastrpc_e2e_idl)
set_target_properties(fastrpc_e2e_stub PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(fastrpc_e2e_stub PRIVATE
    ${_e2e_gen_root}/calculator
    ${_e2e_gen_root}/hap_example
    ${_e2e_gen_root}/multithreading
    "${FASTRPC_INC_DIR}")
target_compile_definitions(fastrpc_e2e_stub PRIVATE ${PLATFORM_C_DEFS})

add_library(e2e_common STATIC
    common/e2e_test_utils.c)
set_target_properties(e2e_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(e2e_common PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${FASTRPC_TEST_SOURCE_DIR}/utils/fastrpc_utils"
    "${FASTRPC_INC_DIR}")
target_link_libraries(e2e_common PRIVATE common_test_dep)

set(_e2e_suite_libs)
foreach(_name IN LISTS _e2e_names)
    file(GLOB _sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${_name}/*.c")
    list(FILTER _sources EXCLUDE REGEX "(_imp|_legacy)\\.c$")
    add_library("e2e_${_name}_all" STATIC ${_sources})
    set_target_properties("e2e_${_name}_all" PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories("e2e_${_name}_all" PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/${_name}"
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
        "${_e2e_gen_root}/${_name}")
    target_link_libraries("e2e_${_name}_all" PRIVATE
        common_test_dep e2e_common fastrpc_e2e_stub)
    list(APPEND _e2e_suite_libs "e2e_${_name}_all")

    # Preserve the legacy dynamically loaded module ABI for each example.
    add_library("${_name}" MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/${_name}/${_name}_legacy.c"
        "${_e2e_gen_root}/${_name}/${_name}_stub.c")
    target_include_directories("${_name}" PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/${_name}"
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
        "${_e2e_gen_root}/${_name}")
    target_link_libraries("${_name}" PRIVATE common_test_dep e2e_common)
    set_target_properties("${_name}" PROPERTIES
        OUTPUT_NAME "${_name}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/e2e/${TARGET_PLATFORM}")
    install(TARGETS "${_name}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/fastrpc_test")
endforeach()

add_dependencies(calculator fastrpc_e2e_idl)
add_dependencies(hap_example fastrpc_e2e_idl)
add_dependencies(multithreading fastrpc_e2e_idl)

# The host legacy modules use the same generated stubs as the suite tests.
target_include_directories(calculator PRIVATE "${_e2e_gen_root}/calculator")
target_include_directories(hap_example PRIVATE "${_e2e_gen_root}/hap_example")
target_include_directories(multithreading PRIVATE "${_e2e_gen_root}/multithreading")

set(ALL_SUITE_LIBS ${ALL_SUITE_LIBS} ${_e2e_suite_libs} PARENT_SCOPE)

# -----------------------------------------------------------------------------
# Optional DSP skels
# -----------------------------------------------------------------------------
find_program(E2E_HEXAGON_CLANG NAMES clang)
find_program(E2E_HEXAGON_LD NAMES ld.lld)
if(E2E_HEXAGON_CLANG AND E2E_HEXAGON_LD AND NOT HEXAGON_SDK_ROOT STREQUAL "")
    set(_e2e_dsp_dir "${CMAKE_BINARY_DIR}/e2e")
    set(_e2e_sdk_inc "${HEXAGON_SDK_ROOT}/incs")
    set(_e2e_dsp_targets)
    foreach(_arch IN LISTS DSP_ARCHES)
        foreach(_name IN LISTS _e2e_names)
            set(_gen_dir "${_e2e_gen_root}/${_name}")
            set(_arch_dir "${_e2e_dsp_dir}/v${_arch}")
            set(_skel_obj "${_arch_dir}/${_name}_skel.o")
            set(_imp_obj "${_arch_dir}/${_name}_imp.o")
            set(_so "${_arch_dir}/lib${_name}_skel.so")
            set(_imp "${CMAKE_CURRENT_SOURCE_DIR}/${_name}/${_name}_imp.c")
            set(_dsp_flags --target=hexagon -mv${_arch} -fPIC -Wall -Werror
                -std=c11 -D_POSIX_C_SOURCE=200112L
                -I${_gen_dir} -I${FASTRPC_INC_DIR} -I${_e2e_sdk_inc})
            if(_name STREQUAL "multithreading")
                list(APPEND _dsp_flags "-I${_e2e_sdk_inc}/qurt")
            endif()

            add_custom_command(
                OUTPUT "${_skel_obj}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arch_dir}"
                COMMAND "${E2E_HEXAGON_CLANG}" ${_dsp_flags}
                        -c "${_gen_dir}/${_name}_skel.c" -o "${_skel_obj}"
                DEPENDS fastrpc_e2e_idl "${_gen_dir}/${_name}_skel.c"
                COMMENT "Compiling ${_name} DSP skeleton for v${_arch}"
                VERBATIM)
            add_custom_command(
                OUTPUT "${_imp_obj}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arch_dir}"
                COMMAND "${E2E_HEXAGON_CLANG}" ${_dsp_flags}
                        -c "${_imp}" -o "${_imp_obj}"
                DEPENDS "${_imp}" "${_gen_dir}/${_name}.h"
                COMMENT "Compiling ${_name} DSP implementation for v${_arch}"
                VERBATIM)
            add_custom_command(
                OUTPUT "${_so}"
                COMMAND "${E2E_HEXAGON_LD}" -shared
                        -soname=lib${_name}_skel.so -z separate-loadable-segments
                        "${_skel_obj}" "${_imp_obj}" -o "${_so}"
                DEPENDS "${_skel_obj}" "${_imp_obj}"
                COMMENT "Linking lib${_name}_skel.so for v${_arch}"
                VERBATIM)
            set(_skel_target "fastrpc_e2e_${_name}_skel_v${_arch}")
            add_custom_target("${_skel_target}" ALL DEPENDS "${_so}")
            add_dependencies("${_skel_target}" fastrpc_e2e_idl)
            install(FILES "${_so}"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/fastrpc_test/v${_arch}")
            list(APPEND _e2e_dsp_targets "${_skel_target}")
        endforeach()
    endforeach()
    add_custom_target(fastrpc_e2e_skels DEPENDS ${_e2e_dsp_targets})
    message(STATUS "e2e DSP skeletons enabled for ${DSP_ARCHES} (SDK: ${HEXAGON_SDK_ROOT})")
else()
    message(STATUS "e2e DSP skeletons disabled; set HEXAGON_SDK_ROOT and provide clang/ld.lld")
endif()
