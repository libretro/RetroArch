# Copyright (c) 2019-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

function(glsl_spv_shader)
    set(options)
    set(oneValueArgs INPUT OUTPUT STAGE VARIABLE TARGET_ENV)
    set(multiValueArgs DEFINES EXTRA_DEPENDS)
    cmake_parse_arguments(_glsl_spv "${options}" "${oneValueArgs}"
                          "${multiValueArgs}" ${ARGN})

    set(defineArgList)
    foreach(define IN LISTS _glsl_spv_DEFINES)
        list(APPEND defineArgList "-D${define}")
    endforeach()

    if(GLSL_COMPILER)
        add_custom_command(
            OUTPUT "${_glsl_spv_OUTPUT}"
            COMMAND
                "${GLSL_COMPILER}" #
                "-mfmt=c" #
                "-fshader-stage=${_glsl_spv_STAGE}" #
                ${defineArgList} #
                "${_glsl_spv_INPUT}" #
                -o "${_glsl_spv_OUTPUT}" #
                "--target-env=${_glsl_spv_TARGET_ENV}" #
                $<IF:$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>,-g,> #
                $<IF:$<CONFIG:Debug>,-O0,-O> #
            MAIN_DEPENDENCY "${_glsl_spv_INPUT}"
            DEPENDS "${_glsl_spv_INPUT}" ${_glsl_spv_EXTRA_DEPENDS}
            USES_TERMINAL)

    elseif(GLSLANG_VALIDATOR)
        # Run glslangValidator if we can find it
        add_custom_command(
            OUTPUT "${_glsl_spv_OUTPUT}"
            COMMAND
                "${GLSLANG_VALIDATOR}" #
                -S "${_glsl_spv_STAGE}" #
                ${defineArgList} #
                #--nan-clamp #
                -x # output as hex
                -o "${_glsl_spv_OUTPUT}" #
                $<$<CONFIG:Debug,RelWithDebInfo>:-gVS> #
                $<$<CONFIG:Debug>:-Od> #
                $<$<CONFIG:Release>:-g0> #
                "--target-env" "${_glsl_spv_TARGET_ENV}" #
                "${_glsl_spv_INPUT}" #
            MAIN_DEPENDENCY "${_glsl_spv_INPUT}"
            DEPENDS "${_glsl_spv_INPUT}" ${_glsl_spv_EXTRA_DEPENDS}
            USES_TERMINAL)

    else()
        # Use the precompiled .spv files
        get_filename_component(glsl_src_dir "${_glsl_spv_INPUT}" DIRECTORY)

        get_filename_component(glsl_name_we "${_glsl_spv_INPUT}" NAME_WE)
        set(precompiled_file ${glsl_src_dir}/${glsl_name_we}.spv)
        configure_file("${precompiled_file}" "${_glsl_spv_OUTPUT}" COPYONLY)
    endif()
endfunction()
