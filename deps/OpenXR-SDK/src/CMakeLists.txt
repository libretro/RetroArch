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
#

if(POLICY CMP0075)
    cmake_policy(SET CMP0075 NEW)
endif()

# Entire project uses C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(
    APPEND CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/external/sanitizers-cmake/cmake"
)

include(StdFilesystemFlags)

find_package(Sanitizers QUIET)

### Dependencies

set(XR_USE_GRAPHICS_API_OPENGL FALSE)
set(XR_USE_GRAPHICS_API_OPENGL_ES FALSE)
set(XR_USE_GRAPHICS_API_VULKAN FALSE)
set(XR_USE_GRAPHICS_API_D3D11 FALSE)
set(XR_USE_GRAPHICS_API_D3D12 FALSE)
set(XR_USE_GRAPHICS_API_METAL FALSE)

set(OPENGLES_INCOMPATIBLE TRUE)
set(EGL_INCOMPATIBLE FALSE)
set(OPENGL_INCOMPATIBLE FALSE)
set(VULKAN_INCOMPATIBLE FALSE)
set(METAL_INCOMPATIBLE TRUE)

# CMake will detect OpenGL/Vulkan which are not compatible with UWP and ARM/ARM64 on Windows so skip it in these cases.
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    set(OPENGL_INCOMPATIBLE TRUE)
    set(VULKAN_INCOMPATIBLE TRUE)
    message(STATUS "OpenGL/Vulkan disabled due to incompatibility with UWP.")
elseif(WIN32)
    if(CMAKE_SYSTEM_NAME STREQUAL "ARM" OR CMAKE_C_COMPILER_ARCHITECTURE_ID
                                           STREQUAL "ARMV7"
    )
        set(OPENGL_INCOMPATIBLE TRUE)
        set(VULKAN_INCOMPATIBLE TRUE)
        message(
            STATUS
                "OpenGL/Vulkan disabled due to incompatibility with Windows on ARM 32-bit"
        )
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(OPENGL_INCOMPATIBLE TRUE)
    set(EGL_INCOMPATIBLE TRUE)
    message(STATUS "OpenGL and EGL disabled as no bindings exist for OpenGL on Darwin")
    set(METAL_INCOMPATIBLE FALSE)
elseif(ANDROID)
    set(OPENGL_INCOMPATIBLE TRUE)
    find_path(
        ANDROID_NATIVE_APP_GLUE android_native_app_glue.h
        PATHS "${ANDROID_NDK}/sources/android/native_app_glue"
    )
    if(ANDROID_NATIVE_APP_GLUE)
        # Needed by gfxwrapper
        set(OPENGLES_INCOMPATIBLE FALSE)
    endif()
    if(ANDROID_PLATFORM_LEVEL LESS 24)
        set(VULKAN_INCOMPATIBLE TRUE)
        message(
            STATUS
                "Vulkan disabled due to incompatibility: need to target at least API 24"
        )
    endif()
endif()

if(WIN32)
    set(EGL_INCOMPATIBLE TRUE)
    message(STATUS "EGL disabled")
endif()

if(NOT OPENGL_INCOMPATIBLE)
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL)

    if(OPENGL_FOUND)
        set(XR_USE_GRAPHICS_API_OPENGL TRUE)
        add_definitions(-DXR_USE_GRAPHICS_API_OPENGL)
        message(STATUS "Enabling OpenGL support")
    elseif(BUILD_ALL_EXTENSIONS)
        message(FATAL_ERROR "OpenGL not found")
    endif()
endif()

if(NOT OPENGLES_INCOMPATIBLE)
    find_package(OpenGLES COMPONENTS V3 V2)
    find_package(EGL)
    if(OPENGLES_FOUND AND EGL_FOUND)
        set(XR_USE_GRAPHICS_API_OPENGL_ES TRUE)
        add_definitions(-DXR_USE_GRAPHICS_API_OPENGL_ES)
        message(STATUS "Enabling OpenGL|ES support")
    elseif(BUILD_ALL_EXTENSIONS)
        message(FATAL_ERROR "OpenGL|ES not found")
    endif()
endif()


if(NOT EGL_INCOMPATIBLE)
    find_package(EGL)
    if(EGL_FOUND)
        set(XR_USE_PLATFORM_EGL TRUE)
        add_definitions(-DXR_USE_PLATFORM_EGL)
        message(STATUS "Enabling EGL support")
    elseif(BUILD_ALL_EXTENSIONS)
        message(FATAL_ERROR "EGL not found")
    endif()
endif()

if(NOT VULKAN_INCOMPATIBLE)
    # Find the Vulkan headers
    find_package(Vulkan)
    if(Vulkan_FOUND)
        set(XR_USE_GRAPHICS_API_VULKAN TRUE)
        add_definitions(-DXR_USE_GRAPHICS_API_VULKAN)
        message(STATUS "Enabling Vulkan support")
    elseif(BUILD_ALL_EXTENSIONS)
        message(FATAL_ERROR "Vulkan headers not found")
    endif()
endif()

if(NOT METAL_INCOMPATIBLE)
    find_package(MetalTools)
    if(MetalTools_FOUND)
        set(XR_USE_GRAPHICS_API_METAL TRUE)
        add_definitions(-DXR_USE_GRAPHICS_API_METAL)
        message(STATUS "Enabling Metal support")
    elseif(BUILD_ALL_EXTENSIONS)
        message(FATAL_ERROR "Metal not found - full Xcode install required")
    else()
        message(STATUS "Metal not found - full Xcode install required")
    endif()
endif()

find_package(Threads REQUIRED)
find_package(JsonCpp)

### All options defined here
option(BUILD_LOADER "Build loader" ON)
option(BUILD_ALL_EXTENSIONS "Build loader and layers with all extensions" OFF)
option(
    BUILD_LOADER_WITH_EXCEPTION_HANDLING
    "Enable exception handling in the loader. Leave this on unless your standard library is built to not throw."
    ON
)

if(WIN32)
    set(OPENXR_DEBUG_POSTFIX
        d
        CACHE STRING "OpenXR loader debug postfix."
    )
else()
    set(OPENXR_DEBUG_POSTFIX
        ""
        CACHE STRING "OpenXR loader debug postfix."
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    option(DYNAMIC_LOADER "Build the loader as a .dll library" OFF)
else()
    option(DYNAMIC_LOADER "Build the loader as a .dll library" ON)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/api_layers/CMakeLists.txt")
    option(BUILD_API_LAYERS "Build API layers" ON)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    option(BUILD_TESTS "Build tests" ON)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/conformance/CMakeLists.txt")
    option(BUILD_CONFORMANCE_TESTS "Build conformance tests" ON)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/hello_xr/CMakeLists.txt")
    option(BUILD_SDK_TESTS "Build SDK samples" ON)
endif()
include(CMakeDependentOption)

cmake_dependent_option(
    BUILD_WITH_SYSTEM_JSONCPP
    "Use system jsoncpp instead of vendored source"
    ON
    "JSONCPP_FOUND"
    OFF
)
cmake_dependent_option(
    BUILD_WITH_STD_FILESYSTEM
    "Use std::[experimental::]filesystem."
    ON
    "HAVE_FILESYSTEM_WITHOUT_LIB OR HAVE_FILESYSTEM_NEEDING_LIBSTDCXXFS OR HAVE_FILESYSTEM_NEEDING_LIBCXXFS"
    OFF
)

# Several files use these compile time OS switches
if(WIN32)
    add_definitions(-DXR_OS_WINDOWS)
    add_definitions(-DNOMINMAX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_definitions(-DXR_OS_LINUX)
elseif(ANDROID)
    add_definitions(-DXR_OS_ANDROID)
elseif(APPLE)
    add_definitions(-DXR_OS_APPLE)
endif()

# /EHsc (support for C++ exceptions) is default in most configurations but seems missing when building arm/arm64.
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

# OpenGL function loader: generated by GLAD2.
# Not present in -SDK because not needed for the loader
if((OPENGL_FOUND OR OpenGLES_FOUND)
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glad2"
)
    add_subdirectory(external/glad2)
endif()

# This is a little helper library for setting up OpenGL
# Not present in -SDK because not needed for the loader
if((OPENGL_FOUND OR OpenGLES_FOUND)
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/common/gfxwrapper_opengl.c"
)
    add_library(
        openxr-gfxwrapper STATIC common/gfxwrapper_opengl.c
                                 common/gfxwrapper_opengl.h
    )
    add_sanitizers(openxr-gfxwrapper)
    target_link_libraries(openxr-gfxwrapper PUBLIC openxr-glad-loader)

    if(ANDROID)
        target_include_directories(
            openxr-gfxwrapper PUBLIC "${ANDROID_NATIVE_APP_GLUE}"
        )

        # Note: For some reason, just adding this to the gfxwrapper library results in failure at load time.
        # So, each consuming target must add $<TARGET_OBJECTS:android_native_app_glue> to their sources
        add_library(
            android_native_app_glue OBJECT
            "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c"
        )
        target_include_directories(
            android_native_app_glue PUBLIC "${ANDROID_NATIVE_APP_GLUE}"
        )
        target_compile_options(
            android_native_app_glue PRIVATE -Wno-unused-parameter
        )
    elseif(APPLE)
        target_compile_options(openxr-gfxwrapper PRIVATE -x objective-c++)
        target_link_libraries(
            openxr-gfxwrapper
            PUBLIC "-framework AppKit" "-framework Foundation"
                   "-framework CoreGraphics"
        )
    endif()
    set_target_properties(openxr-gfxwrapper PROPERTIES FOLDER ${HELPER_FOLDER})
    message(
        STATUS
            "Enabling OpenGL support in hello_xr, loader_test, and conformance, if configured"
    )
endif()

# Determine the presentation backend for Linux systems.
# Use an include because the code is pretty big.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(presentation)
endif()

# Several files use these compile time platform switches
if(WIN32)
    add_definitions(-DXR_USE_PLATFORM_WIN32)
elseif(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(DARWIN_TARGET_OS_NAME STREQUAL "ios")
            add_definitions(-DXR_USE_PLATFORM_IOS)
        else()
            add_definitions(-DXR_USE_PLATFORM_MACOS)
        endif()
    endif()
elseif(ANDROID)
    add_definitions(-DXR_USE_PLATFORM_ANDROID)
    set(OPENXR_ANDROID_VERSION_SUFFIX
        ""
        CACHE STRING "Suffix for generated Android artifacts."
    )
elseif(PRESENTATION_BACKEND MATCHES "xlib")
    add_definitions(-DXR_USE_PLATFORM_XLIB)
elseif(PRESENTATION_BACKEND MATCHES "xcb")
    add_definitions(-DXR_USE_PLATFORM_XCB)

    # TODO remove once conformance supports XCB
    set(BUILD_CONFORMANCE_TESTS OFF)
elseif(PRESENTATION_BACKEND MATCHES "wayland")
    add_definitions(-DXR_USE_PLATFORM_WAYLAND)

    # TODO remove once conformance supports Wayland
    set(BUILD_CONFORMANCE_TESTS OFF)
endif()

if(BUILD_CONFORMANCE_TESTS AND NOT ANDROID)
    set(BUILD_CONFORMANCE_CLI ON)
else()
    set(BUILD_CONFORMANCE_CLI OFF)
endif()

set(OPENXR_ALL_SUPPORTED_DEFINES)
if(BUILD_WITH_XLIB_HEADERS)
    list(APPEND OPENXR_ALL_SUPPORTED_DEFINES XR_USE_PLATFORM_XLIB)
endif()

if(BUILD_WITH_XCB_HEADERS)
    list(APPEND OPENXR_ALL_SUPPORTED_DEFINES XR_USE_PLATFORM_XCB)
endif()

if(BUILD_WITH_WAYLAND_HEADERS)
    list(APPEND OPENXR_ALL_SUPPORTED_DEFINES XR_USE_PLATFORM_WAYLAND)
endif()

# Find glslc shader compiler.
# On Android, the NDK includes the binary, so no external dependency.
if(ANDROID)
    file(
        GLOB
        glslc_folders
        CONFIGURE_DEPENDS
        ${ANDROID_NDK}/shader-tools/*
    )
    find_program(
        GLSL_COMPILER glslc
        PATHS ${glslc_folders}
        NO_DEFAULT_PATH
    )
else()
    if($ENV{VULKAN_SDK})
        file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" VULKAN_SDK)
        file(
            GLOB
            glslc_folders
            CONFIGURE_DEPENDS
            ${VULKAN_SDK}/*
        )
    endif()
    find_program(
        GLSL_COMPILER glslc
        PATHS ${glslc_folders}
        HINTS "${Vulkan_GLSLC_EXECUTABLE}"
    )
endif()
find_program(
    GLSLANG_VALIDATOR glslangValidator
    HINTS "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}"
)
if(GLSL_COMPILER)
    message(STATUS "Found glslc: ${GLSL_COMPILER}")
elseif(GLSLANG_VALIDATOR)
    message(STATUS "Found glslangValidator: ${GLSLANG_VALIDATOR}")
else()
    message(STATUS "Could NOT find glslc, using precompiled .spv files")
endif()

function(compile_glsl run_target_name)
    set(glsl_output_files "")
    foreach(in_file IN LISTS ARGN)
        get_filename_component(glsl_stage "${in_file}" NAME_WE)
        set(out_file "${CMAKE_CURRENT_BINARY_DIR}/${glsl_stage}.spv")
        if(GLSL_COMPILER)
            # Run glslc if we can find it
            add_custom_command(
                OUTPUT "${out_file}"
                COMMAND
                    "${GLSL_COMPILER}" -mfmt=c -fshader-stage=${glsl_stage}
                    "${in_file}" -o "${out_file}"
                DEPENDS "${in_file}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                VERBATIM
            )
        elseif(GLSLANG_VALIDATOR)
            # Run glslangValidator if we can find it
            add_custom_command(
                OUTPUT "${out_file}"
                COMMAND
                    "${GLSLANG_VALIDATOR}" -V -S ${glsl_stage} "${in_file}" -x
                    -o "${out_file}"
                DEPENDS "${in_file}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                VERBATIM
            )
        else()
            # Use the precompiled .spv files
            get_filename_component(glsl_src_dir "${in_file}" DIRECTORY)
            set(precompiled_file "${glsl_src_dir}/${glsl_stage}.spv")
            configure_file("${precompiled_file}" "${out_file}" COPYONLY)
        endif()
        list(APPEND glsl_output_files "${out_file}")
    endforeach()
    add_custom_target(${run_target_name} ALL DEPENDS ${glsl_output_files})
    set_target_properties(${run_target_name} PROPERTIES FOLDER ${HELPER_FOLDER})

endfunction()

# Not available in MinGW
if(MSVC)
    set(XR_USE_GRAPHICS_API_D3D11 TRUE)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D11)
    set(XR_USE_GRAPHICS_API_D3D12 TRUE)
    add_definitions(-DXR_USE_GRAPHICS_API_D3D12)
endif()

# Check for the existence of the secure_getenv or __secure_getenv commands
include(CheckFunctionExists)

check_function_exists(secure_getenv HAVE_SECURE_GETENV)
check_function_exists(__secure_getenv HAVE___SECURE_GETENV)
configure_file(common_config.h.in "${CMAKE_CURRENT_BINARY_DIR}/common_config.h")
add_definitions(-DOPENXR_HAVE_COMMON_CONFIG)

include(CheckSymbolExists)
if(WIN32)
    check_symbol_exists(timespec_get time.h HAVE_TIMESPEC)
else()
    check_symbol_exists(clock_gettime time.h HAVE_TIMESPEC)
endif()
if(HAVE_TIMESPEC)
    add_definitions(-DXR_USE_TIMESPEC)
endif()

# Set up the OpenXR version variables, used by several targets in this project.
include(${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)

# General code generation macro used by several targets.
macro(run_xr_xml_generate dependency output)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${output}"
       AND NOT BUILD_FORCE_GENERATION
    )
        # pre-generated found
        message(
            STATUS "Found and will use pre-generated ${output} in source tree"
        )
        list(APPEND GENERATED_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${output}")
    else()
        if(NOT Python3_EXECUTABLE)
            message(
                FATAL_ERROR
                    "Python 3 not found, but pre-generated ${CMAKE_CURRENT_SOURCE_DIR}/${output} not found"
            )
        endif()
        add_custom_command(
            OUTPUT "${output}"
            COMMAND
                "${CMAKE_COMMAND}" -E env "PYTHONPATH=${CODEGEN_PYTHON_PATH}"
                "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/src/scripts/src_genxr.py" -registry
                "${PROJECT_SOURCE_DIR}/specification/registry/xr.xml"
                "${output}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS
                "${PROJECT_SOURCE_DIR}/specification/registry/xr.xml"
                "${PROJECT_SOURCE_DIR}/specification/scripts/generator.py"
                "${PROJECT_SOURCE_DIR}/specification/scripts/reg.py"
                "${PROJECT_SOURCE_DIR}/src/scripts/${dependency}"
                "${PROJECT_SOURCE_DIR}/src/scripts/src_genxr.py"
                ${ARGN}
            VERBATIM
            COMMENT
                "Generating ${output} using ${Python3_EXECUTABLE} on ${dependency}"
        )
        # cmake-format: off
        set_source_files_properties(${output}
            PROPERTIES
               GENERATED TRUE
               SKIP_LINTING ON
        )
        # cmake-format: on
        list(APPEND GENERATED_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${output}")
        list(APPEND GENERATED_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${output}")
    endif()
endmacro()

# Layer JSON generation macro used by several targets.
macro(
    gen_xr_layer_json
    filename
    layername
    libfile
    version
    desc
    genbad
)
    add_custom_command(
        OUTPUT "${filename}"
        COMMAND
            "${CMAKE_COMMAND}" -E env "PYTHONPATH=${CODEGEN_PYTHON_PATH}"
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/src/scripts/generate_api_layer_manifest.py"
            -f "${filename}" -n ${layername} -l ${libfile} -a ${MAJOR}.${MINOR}
            -v ${version} ${genbad} -d ${desc}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS
            "${PROJECT_SOURCE_DIR}/src/scripts/generate_api_layer_manifest.py"
        COMMENT
            "Generating API Layer JSON ${filename} using -f ${filename} -n ${layername} -l ${libfile} -a ${MAJOR}.${MINOR} -v ${version} ${genbad} -d ${desc}"
        VERBATIM
    )
endmacro()

# Custom target for generated dispatch table sources, used by several targets.
unset(GENERATED_OUTPUT)
unset(GENERATED_DEPENDS)
run_xr_xml_generate(utility_source_generator.py xr_generated_dispatch_table.h)
run_xr_xml_generate(utility_source_generator.py xr_generated_dispatch_table.c)
set(COMMON_GENERATED_OUTPUT ${GENERATED_OUTPUT})
set(COMMON_GENERATED_DEPENDS ${GENERATED_DEPENDS})

if(COMMON_GENERATED_DEPENDS)
    add_custom_target(
        xr_common_generated_files DEPENDS ${COMMON_GENERATED_DEPENDS}
    )
else()
    add_custom_target(xr_common_generated_files)
endif()

set_target_properties(
    xr_common_generated_files PROPERTIES FOLDER ${CODEGEN_FOLDER}
)

unset(GENERATED_OUTPUT)
unset(GENERATED_DEPENDS)
run_xr_xml_generate(
    utility_source_generator.py xr_generated_dispatch_table_core.h
)
run_xr_xml_generate(
    utility_source_generator.py xr_generated_dispatch_table_core.c
)
set(LOADER_GENERATED_OUTPUT ${GENERATED_OUTPUT})
set(LOADER_GENERATED_DEPENDS ${GENERATED_DEPENDS})

unset(GENERATED_OUTPUT)
unset(GENERATED_DEPENDS)

if(LOADER_GENERATED_DEPENDS)
    add_custom_target(
        xr_global_generated_files DEPENDS ${LOADER_GENERATED_DEPENDS}
    )
else()
    add_custom_target(xr_global_generated_files)
endif()

set_target_properties(
    xr_global_generated_files PROPERTIES FOLDER ${CODEGEN_FOLDER}
)

if(NOT MSVC)
    include(CheckCXXCompilerFlag)
    include(CheckCCompilerFlag)
    foreach(
        FLAG
        -Wall
        -Werror=unused-parameter
        -Werror=unused-argument
        -Wpointer-arith
    )
        string(
            REGEX
            REPLACE
                "[^A-Za-z0-9]"
                ""
                _flagvar
                "${FLAG}"
        )
        check_cxx_compiler_flag(${FLAG} SUPPORTS_${_flagvar})
        if(SUPPORTS_${_flagvar})
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAG}")
        endif()
        check_c_compiler_flag(${FLAG} SUPPORTS_C_${_flagvar})
        if(SUPPORTS_C_${_flagvar})
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAG}")
        endif()
    endforeach()
    if(APPLE)
        set(CMAKE_SHARED_LINKER_FLAGS
            "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-undefined,error"
        )
    elseif(NOT WIN32)
        if(NOT SANITIZE_ADDRESS)
            set(CMAKE_SHARED_LINKER_FLAGS
                "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined"
            )
        endif()
    endif()
endif()

if(ANDROID)
    find_library(ANDROID_LIBRARY NAMES android)
    find_library(ANDROID_LOG_LIBRARY NAMES log)
endif()

if(BUILD_LOADER)
    add_subdirectory(loader)
endif()

if(BUILD_API_LAYERS)
    add_subdirectory(api_layers)
endif()

if(BUILD_TESTS OR BUILD_CONFORMANCE_TESTS)
    add_subdirectory(external/catch2)
    add_sanitizers(Catch2)
    add_sanitizers(Catch2WithMain)

    file(GLOB_RECURSE EXTERNAL_SOURCES external/catch2/*.cpp)
    # cmake-format: off
    set_source_files_properties(${EXTERNAL_SOURCES}
        TARGET_DIRECTORY Catch2::Catch2
        PROPERTIES SKIP_LINTING ON
    )
    # cmake-format: on
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BUILD_CONFORMANCE_TESTS)
    add_subdirectory(conformance)
endif()
