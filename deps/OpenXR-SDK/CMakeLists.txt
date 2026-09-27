# Copyright (c) 2017-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Note: This is the top-level CMake file for the OpenXR project.
# It should contain only definitions that are applicable to the
# entire project and includes for the sub-directories.

cmake_minimum_required(VERSION 3.16)
project(OPENXR)

find_package(Python3 COMPONENTS Interpreter)
include(CTest)

# Enable IDE GUI folders.  "Helper targets" that don't have interesting source code should set their FOLDER property to this
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(
    GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake predefined targets"
)
set(LOADER_FOLDER "Loader")
set(HELPER_FOLDER "Helpers")
set(CODEGEN_FOLDER "Generated")
set(TESTS_FOLDER "Tests")
set(CONFORMANCE_TESTS_FOLDER "Conformance Test Suite")
set(LOADER_TESTS_FOLDER "Loader Tests")
set(API_LAYERS_FOLDER "Layers")
set(SAMPLES_FOLDER "Samples")

option(
    BUILD_FORCE_GENERATION
    "Force re-generation of files even in the presence of pre-generated copies, replacing those copies."
    OFF
)

if(BUILD_FORCE_GENERATION AND NOT Python3_EXECUTABLE)
    message(FATAL_ERROR "BUILD_FORCE_GENERATION requires Python")
endif()

if(MSVC)
    option(
        INSTALL_SYMBOLS
        "Install the .pdb debug symbols, if applicable, when installing binaries"
        ON
    )
endif()

string(TOUPPER "${CMAKE_GENERATOR_PLATFORM}" CMAKE_GENERATOR_PLATFORM_UPPER)

# Artifact organization
if(WIN32 OR ANDROID)
    option(
        INSTALL_TO_ARCHITECTURE_PREFIXES
        "Install platform-specific files to architecture-specific directories, for packaging"
        OFF
    )
endif()

if(WIN32 AND INSTALL_TO_ARCHITECTURE_PREFIXES)
    unset(_UWP_SUFFIX)
    if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
        set(_UWP_SUFFIX _uwp)
    endif()
    if(CMAKE_GENERATOR_PLATFORM_UPPER MATCHES "ARM.*")
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_PLATFORM ARM64)
        else()
            set(_PLATFORM ARM)
        endif()
    else()
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_PLATFORM x64)
        else()
            set(_PLATFORM Win32)
        endif()
    endif()

    include(GNUInstallDirs)
    set(CMAKE_INSTALL_BINDIR ${_PLATFORM}${_UWP_SUFFIX}/${CMAKE_INSTALL_BINDIR})
    set(CMAKE_INSTALL_LIBDIR ${_PLATFORM}${_UWP_SUFFIX}/${CMAKE_INSTALL_LIBDIR})

elseif(ANDROID AND INSTALL_TO_ARCHITECTURE_PREFIXES)
    # This organizes things like a prefab module
    set(PREFAB_INSTALL_DIR prefab)
    set(PREFAB_HEADER_MODULE_INSTALL_DIR
        "${PREFAB_INSTALL_DIR}/modules/headers"
    )
    set(PREFAB_LOADER_MODULE_INSTALL_DIR
        "${PREFAB_INSTALL_DIR}/modules/openxr_loader"
    )
    set(CMAKE_INSTALL_LIBDIR
        "${PREFAB_LOADER_MODULE_INSTALL_DIR}/libs/android.${ANDROID_ABI}"
        CACHE STRING "Where to install libraries"
    )
    set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_LIBDIR})
    set(CMAKE_INSTALL_INCLUDEDIR ${PREFAB_HEADER_MODULE_INSTALL_DIR}/include)

    unset(NDK_MAJOR_VERSION)
    if(CMAKE_ANDROID_NDK)
        file(STRINGS "${CMAKE_ANDROID_NDK}/source.properties" NDK_PROPERTIES)
        foreach(_line ${NDK_PROPERTIES})
            if("${_line}" MATCHES
               "Pkg.Revision = ([0-9]+)[.]([0-9]+)[.]([0-9]+)"
            )
                set(NDK_MAJOR_VERSION ${CMAKE_MATCH_1})
            endif()
        endforeach()
    else()
        message(FATAL_ERROR "Please set CMAKE_ANDROID_NDK to your NDK root!")
    endif()
    if(NDK_MAJOR_VERSION)
        message(STATUS "Building using NDK major version ${NDK_MAJOR_VERSION}")
    else()
        message(
            FATAL_ERROR
                "Could not parse the major version from ${CMAKE_ANDROID_NDK}/source.properties"
        )
    endif()

else()
    include(GNUInstallDirs)
endif()

# Path separators ( : or ; ) are not handled well in CMake.
# This seems like a reasonable approach.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL Windows)
    set(CODEGEN_PYTHON_PATH
        "${PROJECT_SOURCE_DIR}/specification/scripts;${PROJECT_SOURCE_DIR}/src/scripts;$ENV{PYTHONPATH}"
    )
else()
    set(CODEGEN_PYTHON_PATH
        "${PROJECT_SOURCE_DIR}/specification/scripts:${PROJECT_SOURCE_DIR}/src/scripts:$ENV{PYTHONPATH}"
    )
endif()

add_subdirectory(include)
add_subdirectory(src)

# uninstall target
if(NOT TARGET uninstall)
    configure_file(
        src/cmake/cmake_uninstall.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE
        @ONLY
    )
    add_custom_target(
        uninstall
        COMMAND
            ${CMAKE_COMMAND} -P
            ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
    )
    set_target_properties(uninstall PROPERTIES FOLDER ${HELPER_FOLDER})
endif()

find_program(BASH_COMMAND NAMES bash)
if(BASH_COMMAND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/specification/Makefile")
    option(
        BUILD_SPECIFICATION
        "Run './makeAllExts all' in the specification directory as part of the build - intended for one-step checking of spec changes"
        OFF
    )
    if(BUILD_SPECIFICATION)
        add_custom_target(
            spec-all ALL
            "${BASH_COMMAND}" ./makeAllExts all
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/specification"
            VERBATIM
            COMMENT "Running './makeAllExts all' in the specification directory"
            USES_TERMINAL
        )
    endif()
endif()

if(ANDROID AND INSTALL_TO_ARCHITECTURE_PREFIXES)
    set(LICENSE_DEST META-INF)
else()
    set(LICENSE_DEST share/doc/openxr)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    install(
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION ${LICENSE_DEST}
        COMPONENT License
    )
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../../../../LICENSES/Apache-2.0.txt")
    # Building from a sources jar, where we omit that single file
    # to avoid confusion regarding the combination of licenses in the sources.
    # But for consistency, we will put it in the binaries.
    install(
        FILES "${CMAKE_SOURCE_DIR}/../../../../LICENSES/Apache-2.0.txt"
        RENAME LICENSE
        DESTINATION ${LICENSE_DEST}
        COMPONENT License
    )
endif()
