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

# Generate OpenXR header files.
# Get the list of generated headers from the common single-point-of-truth
file(STRINGS "../generated_header_list.txt" HEADERS LENGTH_MINIMUM 8)

set(HAVE_PREGENERATED TRUE)
set(SOURCE_HEADERS)
foreach(output ${HEADERS})
    list(APPEND SOURCE_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/${output}")
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${output}")
        set(HAVE_PREGENERATED FALSE)
    endif()
endforeach()

add_library(headers INTERFACE)
# Create alias so that it can be used the same whether vendored as source or found with CMake.
# Other targets that need the OpenXR headers should specify OpenXR::headers in their target_link_libraries.
add_library(OpenXR::headers ALIAS headers)
target_include_directories(
    headers INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(HAVE_PREGENERATED AND NOT BUILD_FORCE_GENERATION)
    add_custom_target(
        generate_openxr_header
        COMMENT "Using found pre-generated OpenXR headers."
    )

    set(INSTALL_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/openxr_platform_defines.h"
                        ${SOURCE_HEADERS}
    )

    # All headers are in the source dir
    target_include_directories(
        headers INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    )
else()

    set(GENERATED_HEADERS)
    set(OUTPUT_STAMPS)
    # Copy the openxr_platform_defines.h file and place it in the binary (build) directory.
    configure_file(
        openxr_platform_defines.h
        "${CMAKE_CURRENT_BINARY_DIR}/openxr_platform_defines.h" COPYONLY
    )

    # Generate the header files and place it in the binary (build) directory.
    file(
        GLOB _templates
        LIST_DIRECTORIES false
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/specification/scripts/template_*"
    )
    foreach(output ${HEADERS})
        if("${output}" MATCHES "reflection")
            set(_extra_deps
                "${PROJECT_SOURCE_DIR}/specification/scripts/jinja_helpers.py"
                ${_templates}
            )
        else()
            unset(_extra_deps)
        endif()
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${output}"
            COMMAND
                "${CMAKE_COMMAND}" -E env "PYTHONPATH=${CODEGEN_PYTHON_PATH}"
                "${Python3_EXECUTABLE}"
                "${PROJECT_SOURCE_DIR}/specification/scripts/genxr.py" -registry
                "${PROJECT_SOURCE_DIR}/specification/registry/xr.xml" -o
                "${CMAKE_CURRENT_BINARY_DIR}" ${output}
            DEPENDS
                "${PROJECT_SOURCE_DIR}/specification/scripts/genxr.py"
                "${PROJECT_SOURCE_DIR}/specification/scripts/cgenerator.py"
                "${PROJECT_SOURCE_DIR}/specification/scripts/creflectiongenerator.py"
                "${PROJECT_SOURCE_DIR}/specification/scripts/generator.py"
                "${PROJECT_SOURCE_DIR}/specification/scripts/reg.py"
                "${PROJECT_SOURCE_DIR}/specification/registry/xr.xml"
                ${_extra_deps}
            COMMENT "Generating ${CMAKE_CURRENT_BINARY_DIR}/${output}"
            VERBATIM
        )
        list(APPEND GENERATED_HEADERS "${CMAKE_CURRENT_BINARY_DIR}/${output}")
    endforeach()

    # cmake-format: off
    set_source_files_properties(${GENERATED_HEADERS}
        PROPERTIES
            GENERATED TRUE
            SKIP_LINTING ON
    )
    # cmake-format: on

    set(INSTALL_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/openxr_platform_defines.h
                        ${GENERATED_HEADERS}
    )

    # Define generate_openxr_header target to generate the OpenXR header files.
    # Not used directly - depended on by OpenXR::headers
    add_custom_target(
        generate_openxr_header
        SOURCES "${PROJECT_SOURCE_DIR}/specification/registry/xr.xml"
        DEPENDS ${GENERATED_HEADERS} ${OUTPUT_STAMPS}
    )
    # Headers are all in the binary dir
    target_include_directories(
        headers INTERFACE $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    )
endif()
set_target_properties(
    generate_openxr_header PROPERTIES FOLDER ${CODEGEN_FOLDER}
)

add_dependencies(headers generate_openxr_header)

install(
    FILES ${INSTALL_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/openxr
    COMPONENT Headers
)
