# Copyright (c) 2019-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

find_program(FXC_EXECUTABLE fxc)
function(fxc_shader)
    set(options)
    set(oneValueArgs INPUT OUTPUT TYPE VARIABLE PROFILE)
    set(multiValueArgs DEFINES EXTRA_DEPENDS)
        cmake_parse_arguments(_fxc "${options}" "${oneValueArgs}"
                            "${multiValueArgs}" ${ARGN})
    if(FXC_EXECUTABLE)
        set(_fxc "${FXC_EXECUTABLE}")
    else()
        # Hope/assume that it will be in the path at build time
        set(_fxc "fxc.exe")
    endif()
    set(defineArgList)
    foreach(define IN LISTS _fxc_DEFINES)
        list(APPEND defineArgList "/D" "${define}=1")
    endforeach()
    add_custom_command(
        OUTPUT "${_fxc_OUTPUT}"
        BYPRODUCTS "${_fxc_OUTPUT}.pdb"
        COMMAND
            "${_fxc}" /nologo "/Fh${_fxc_OUTPUT}"
            "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Fd${_fxc_OUTPUT}.pdb>"
            "/T${_fxc_PROFILE}"
            "/Vn" "${_fxc_VARIABLE}"
            ${defineArgList}
            $<$<CONFIG:Debug>:/Od> $<$<CONFIG:Debug>:/Zss>
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/Zi> "${_fxc_INPUT}"
        MAIN_DEPENDENCY "${_fxc_INPUT}"
        DEPENDS "${_fxc_INPUT}" ${_fxc_EXTRA_DEPENDS}
        USES_TERMINAL)

endfunction()
