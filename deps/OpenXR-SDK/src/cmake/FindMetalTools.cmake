# Copyright 2024, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
# Original Author:
# 2024 Rylie Pavlik <rylie.pavlik@collabora.com> <rylie@ryliepavlik.com>

#.rst:
# FindMetalTools
# ---------------
#
# Find the ``metal`` and ``metallib`` tools via xcode.
#
# Cache variables
# ^^^^^^^^^^^^^^^
#
# ``MetalTools_METAL_EXECUTABLE``
#
# ``MetalTools_METALLIB_EXECUTABLE``
#
# The following cache variable may also be set to assist/control the operation of this module:
#
# ``MetalTools_ROOT_DIR``
#  The root to search for MetalTools.

if(NOT APPLE)
    return()
endif()

# Use xcrun to locate the tools
if(NOT MetalTools_METAL_EXECUTABLE)
    execute_process(
        COMMAND xcrun --find --sdk macosx metal
        OUTPUT_VARIABLE MetalTools_METAL_EXECUTABLE_HINT
    )
    get_filename_component(
        MetalTools_METAL_HINT_DIR "${MetalTools_METAL_EXECUTABLE_HINT}" PATH
    )
endif()
if(NOT MetalTools_METALLIB_EXECUTABLE)
    execute_process(
        COMMAND xcrun --find --sdk macosx metallib
        OUTPUT_VARIABLE MetalTools_METALLIB_EXECUTABLE_HINT
    )
    get_filename_component(
        MetalTools_METALLIB_HINT_DIR "${MetalTools_METALLIB_EXECUTABLE_HINT}"
        PATH
    )
endif()

find_program(
    MetalTools_METAL_EXECUTABLE metal
    HINTS "${MetalTools_METAL_HINT_DIR}"
    PATHS "${MetalTools_ROOT_DIR}"
)

find_program(
    MetalTools_METALLIB_EXECUTABLE metallib
    HINTS "${MetalTools_METALLIB_HINT_DIR}"
    PATHS "${MetalTools_ROOT_DIR}"
)

find_package_handle_standard_args(
    MetalTools REQUIRED_VARS MetalTools_METAL_EXECUTABLE
                             MetalTools_METALLIB_EXECUTABLE
)
