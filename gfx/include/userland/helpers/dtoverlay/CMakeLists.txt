# =============================================================================
# Copyright (c) 2016 Raspberry Pi (Trading) Ltd.
# All rights reserved.
#
# FILE DESCRIPTION
# CMake build file for dtoverlay library.
# =============================================================================

cmake_minimum_required (VERSION 2.8)

include_directories (${VIDEOCORE_ROOT}
                     ${VIDEOCORE_ROOT}/helpers
                     ${VIDEOCORE_ROOT}/opensrc/helpers/libfdt
                     ${VIDEOCORE_HEADERS_BUILD_DIR})

if (CMAKE_COMPILER_IS_GNUCC)
   add_definitions (-ffunction-sections)
endif ()

add_library (dtovl ${SHARED}
             dtoverlay.c)

target_link_libraries(dtovl fdt)

install (TARGETS dtovl DESTINATION lib)
