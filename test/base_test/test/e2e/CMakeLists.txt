# Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
#
# Calculator, HAP, and multithreading sample E2E suites.
# Host bindings are generated independently for each sample. DSP skeletons are
# optional and are enabled only when the Hexagon SDK/toolchain is available.

set(_e2e_names calculator hap_example multithreading)
set(_e2e_idl_dir "${CMAKE_CURRENT_SOURCE_DIR}/idl")
set(_e2e_gen_root "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_e2e_suite_libs)
set(_e2e_dsp_targets)

add_custom_target(fastrpc_e2e_idl)
add_library(e2e_common STATIC common/e2e_test_utils.c)
set_target_properties(e2e_common PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(e2e_common PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/common"
    "${CMAKE_SOURCE_DIR}/utils/fastrpc_utils"
    "${FASTRPC_INC_DIR}")
target_link_libraries(e2e_common PRIVATE common_test_dep)

foreach(_name IN LISTS _e2e_names)
    set(_src "${_e2e_idl_dir}/${_name}.idl")
    set(_out "${_e2e_gen_root}/${_name}")
    set(_header "${_out}/${_name}.h")
    set(_stub "${_out}/${_name}_stub.c")
    set(_skel "${_out}/${_name}_skel.c")

    add_custom_command(
        OUTPUT "${_header}" "${_stub}" "${_skel}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_out}"
        COMMAND "${QAIC_EXECUTABLE}" "-I=${FASTRPC_ROOT}/idl"
                "${_src}" -st -o "${_out}"
        DEPENDS "${_src}" "${FASTRPC_ROOT}/idl/AEEStdDef.idl"
                "${FASTRPC_ROOT}/idl/remote.idl" "${QAIC_EXECUTABLE}"
        COMMENT "Generating ${_name} FastRPC bindings"
        VERBATIM)

    add_custom_target("${_name}_qaic"
        DEPENDS "${_header}" "${_stub}" "${_skel}")
    add_dependencies(fastrpc_e2e_idl "${_name}_qaic")
    set(_e2e_${_name}_header "${_header}")
    set(_e2e_${_name}_skel "${_skel}")
    set(_e2e_${_name}_out "${_out}")
    set(_e2e_${_name}_src "${_src}")

    add_library("e2e_${_name}_stub" STATIC "${_stub}")
    add_dependencies("e2e_${_name}_stub" "${_name}_qaic")
    set_target_properties("e2e_${_name}_stub" PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories("e2e_${_name}_stub" PRIVATE
        "${_out}" "${FASTRPC_INC_DIR}")
    target_compile_definitions("e2e_${_name}_stub" PRIVATE ${PLATFORM_C_DEFS})

    set(_suite_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/${_name}/all_tests.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/${_name}/test_${_name}.c")
    add_library("e2e_${_name}_all" STATIC ${_suite_sources})
    set_target_properties("e2e_${_name}_all" PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories("e2e_${_name}_all" PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/${_name}"
        "${CMAKE_CURRENT_SOURCE_DIR}/common"
        "${_out}")
    target_link_libraries("e2e_${_name}_all" PRIVATE
        common_test_dep e2e_common "e2e_${_name}_stub")
    list(APPEND _e2e_suite_libs "e2e_${_name}_all")
endforeach()

set(ALL_SUITE_LIBS ${ALL_SUITE_LIBS} ${_e2e_suite_libs} PARENT_SCOPE)

# Optional DSP skeletons. The SDK is deliberately not a host dependency.
set(DSP_ARCHES "68;75" CACHE STRING "DSP architectures for E2E skeletons")
set(HEXAGON_SDK_ROOT "" CACHE PATH "Hexagon SDK root for E2E DSP builds")
set(E2E_HEXAGON_CLANG "" CACHE FILEPATH
    "Hexagon clang executable for E2E DSP skeletons")
set(E2E_HEXAGON_LD "" CACHE FILEPATH
    "Hexagon linker executable for E2E DSP skeletons")
if(E2E_HEXAGON_CLANG AND E2E_HEXAGON_LD AND HEXAGON_SDK_ROOT AND
   EXISTS "${HEXAGON_SDK_ROOT}/incs")
    set(_e2e_dsp_dir "${CMAKE_BINARY_DIR}/e2e")
    foreach(_arch IN LISTS DSP_ARCHES)
        foreach(_name IN LISTS _e2e_names)
            set(_out "${_e2e_gen_root}/${_name}")
            set(_arch_dir "${_e2e_dsp_dir}/v${_arch}/${_name}")
            set(_skel_obj "${_arch_dir}/${_name}_skel.o")
            set(_imp_obj "${_arch_dir}/${_name}_imp.o")
            set(_so "${_arch_dir}/lib${_name}_skel.so")
            set(_dsp_inc "-I${HEXAGON_SDK_ROOT}/incs")
            if(_name STREQUAL "multithreading")
                set(_extra_inc "-I${HEXAGON_SDK_ROOT}/incs/qurt")
            else()
                set(_extra_inc)
            endif()
            set(_dsp_flags --target=hexagon -mv${_arch} -fPIC -Wall -Werror
                -std=c11 -I${_out} -I${FASTRPC_INC_DIR} ${_dsp_inc} ${_extra_inc})

            add_custom_command(OUTPUT "${_skel_obj}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arch_dir}"
                COMMAND "${E2E_HEXAGON_CLANG}" ${_dsp_flags}
                        -c "${_out}/${_name}_skel.c" -o "${_skel_obj}"
                DEPENDS fastrpc_e2e_idl "${_out}/${_name}_skel.c"
                COMMENT "Compiling ${_name} DSP skeleton for v${_arch}"
                VERBATIM)
            add_custom_command(OUTPUT "${_imp_obj}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${_arch_dir}"
                COMMAND "${E2E_HEXAGON_CLANG}" ${_dsp_flags}
                        -c "${CMAKE_CURRENT_SOURCE_DIR}/${_name}/${_name}_imp.c"
                        -o "${_imp_obj}"
                DEPENDS fastrpc_e2e_idl
                        "${CMAKE_CURRENT_SOURCE_DIR}/${_name}/${_name}_imp.c"
                COMMENT "Compiling ${_name} DSP implementation for v${_arch}"
                VERBATIM)
            add_custom_command(OUTPUT "${_so}"
                COMMAND "${E2E_HEXAGON_LD}" -shared
                        -soname=lib${_name}_skel.so -z separate-loadable-segments
                        "${_skel_obj}" "${_imp_obj}" -o "${_so}"
                DEPENDS "${_skel_obj}" "${_imp_obj}"
                COMMENT "Linking lib${_name}_skel.so for v${_arch}"
                VERBATIM)
            set(_target "fastrpc_e2e_${_name}_skel_v${_arch}")
            add_custom_target("${_target}" ALL DEPENDS "${_so}")
            install(FILES "${_so}"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/fastrpc_test/v${_arch}")
            list(APPEND _e2e_dsp_targets "${_target}")
        endforeach()
    endforeach()
    add_custom_target(fastrpc_e2e_skels DEPENDS ${_e2e_dsp_targets})
    message(STATUS "E2E DSP skeletons enabled for ${DSP_ARCHES}")
else()
    message(STATUS "E2E DSP skeletons disabled; set HEXAGON_SDK_ROOT and Hexagon clang/ld.lld")
endif()

# The sample interfaces have no legacy plugin entry point in this unified
# runner; the existing Autotools modules remain the compatibility path.
