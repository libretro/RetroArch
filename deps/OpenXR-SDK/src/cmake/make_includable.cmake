# Copyright (c) 2019-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

# inspired by: https://stackoverflow.com/a/47801116
function(make_includable input_file output_file)
    cmake_parse_arguments(
        PARSE_ARGV 2 MAKE_INCLUDABLE "" "" "REPLACE")

    file(READ "${input_file}" content)

    # regex replace
    list(LENGTH MAKE_INCLUDABLE_REPLACE length)
    while(length GREATER_EQUAL 2)
        list(GET MAKE_INCLUDABLE_REPLACE 0 find)
        list(GET MAKE_INCLUDABLE_REPLACE 1 replace)
        list(REMOVE_AT MAKE_INCLUDABLE_REPLACE 0 1)
        string(REGEX REPLACE "${find}" "${replace}" content_after_replace "${content}")
        if("${content_after_replace}" STREQUAL "${content}")
            # Error on no change for now. If no-op replaces are required, another replace argument can be added.
            message(
                FATAL_ERROR
                    "replacement rule \"${find}\" -> \"${replace}\" had no effect on ${input_file}."
            )
        endif()
        set(content "${content_after_replace}")
        list(LENGTH MAKE_INCLUDABLE_REPLACE length)
    endwhile()

    file(WRITE "${output_file}" "R\"raw_text(${content})raw_text\"")
    # using https://stackoverflow.com/a/65945763
    # see https://stackoverflow.com/a/56828572 for a different approach
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input_file}")
endfunction()
