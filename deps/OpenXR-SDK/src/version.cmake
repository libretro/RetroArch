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

# Set up the OpenXR version variables, used by several targets in this project.
set(MAJOR "0")
set(MINOR "0")
set(PATCH "0")
set(OPENXR_SDK_HOTFIX_VERSION)

if(EXISTS "${PROJECT_SOURCE_DIR}/specification/registry/xr.xml")
    file(
        STRINGS ${PROJECT_SOURCE_DIR}/specification/registry/xr.xml lines
        REGEX "#define <name>XR_CURRENT_API_VERSION"
    )
else()
    file(
        STRINGS ${PROJECT_SOURCE_DIR}/include/openxr/openxr.h lines
        REGEX "#define XR_CURRENT_API_VERSION"
    )
endif()

list(LENGTH lines len)
if(${len} EQUAL 1)
    list(
        GET
        lines
        0
        cur_line
    )

    # Grab just the stuff in the parentheses of XR_MAKE_VERSION( ),
    # by replacing the whole line with the stuff in the parentheses
    string(
        REGEX
        REPLACE
            "^.+\\(([^\)]+)\\).+$"
            "\\1"
            VERSION_WITH_WHITESPACE
            ${cur_line}
    )

    # Remove whitespace
    string(
        REPLACE
            " "
            ""
            VERSION_NO_WHITESPACE
            ${VERSION_WITH_WHITESPACE}
    )

    # Grab components
    string(
        REGEX
        REPLACE
            "^([0-9]+)\\,[0-9]+\\,[0-9]+"
            "\\1"
            MAJOR
            "${VERSION_NO_WHITESPACE}"
    )
    string(
        REGEX
        REPLACE
            "^[0-9]+\\,([0-9]+)\\,[0-9]+"
            "\\1"
            MINOR
            "${VERSION_NO_WHITESPACE}"
    )
    string(
        REGEX
        REPLACE
            "^[0-9]+\\,[0-9]+\\,([0-9]+)"
            "\\1"
            PATCH
            "${VERSION_NO_WHITESPACE}"
    )
else()
    message(
        FATAL_ERROR
            "Unable to fetch major/minor/patch version from registry or header"
    )
endif()

# Check for an SDK hotfix version indicator file.
if(EXISTS "${PROJECT_SOURCE_DIR}/HOTFIX")
    file(STRINGS "${PROJECT_SOURCE_DIR}/HOTFIX" OPENXR_SDK_HOTFIX_VERSION)
endif()
if(OPENXR_SDK_HOTFIX_VERSION)
    set(OPENXR_SDK_HOTFIX_VERSION_SUFFIX ".${OPENXR_SDK_HOTFIX_VERSION}")
endif()

set(OPENXR_VERSION "${MAJOR}.${MINOR}.${PATCH}")
set(OPENXR_FULL_VERSION "${OPENXR_VERSION}${OPENXR_SDK_HOTFIX_VERSION_SUFFIX}")

if(PROJECT_NAME STREQUAL "OPENXR")
    # Only show the message if we aren't a subproject
    message(STATUS "OpenXR ${OPENXR_FULL_VERSION}")
endif()
