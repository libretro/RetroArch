# Copyright 2020-2021 Collabora, Ltd.
# SPDX-License-Identifier: BSL-1.0
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)
#
# Original Author:
# 2020-2021, Rylie Pavlik <rylie.pavlik@collabora.com> <rylie@ryliepavlik.com>

#[[.rst:
FindOpenGLES
---------------

Find the OpenGL ES graphics API.

Components
^^^^^^^^^^

The following components are supported:

* ``V1`` - OpenGL ES 1 (including emulation on OpenGL ES 2)
* ``V2`` - OpenGL ES 2
* ``V3`` - OpenGL ES 3
* ``V31` - OpenGL ES 3.1 - same as 3 but checking also for gl31.h
* ``V32` - OpenGL ES 3.2 - same as 3 but checking also for gl32.h

If none are specified, the default is ``V2``.

Targets
^^^^^^^

If successful, some subset of the following imported targets are created.

* ``OpenGLES::OpenGLESv1``
* ``OpenGLES::OpenGLESv2``
* ``OpenGLES::OpenGLESv3``
* ``OpenGLES::OpenGLESv31``
* ``OpenGLES::OpenGLESv32``

Cache variables
^^^^^^^^^^^^^^^

The following cache variable may also be set to assist/control the operation of this module:

``OpenGLES_ROOT_DIR``
 The root to search for OpenGLES.
#]]

set(OpenGLES_ROOT_DIR
    "${OpenGLES_ROOT_DIR}"
    CACHE PATH "Root to search for OpenGLES")

if(NOT OpenGLES_FIND_COMPONENTS)
    set(OpenGLES_FIND_COMPONENTS V2)
endif()

if(NOT ANDROID)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        set(_old_prefix_path "${CMAKE_PREFIX_PATH}")
        # So pkg-config uses OpenGLES_ROOT_DIR too.
        if(OpenGLES_ROOT_DIR)
            list(APPEND CMAKE_PREFIX_PATH ${OpenGLES_ROOT_DIR})
        endif()
        pkg_check_modules(PC_glesv1_cm QUIET glesv1_cm)
        pkg_check_modules(PC_glesv2 QUIET glesv2)
        # Restore
        set(CMAKE_PREFIX_PATH "${_old_prefix_path}")
    endif()
endif()

find_path(
    OpenGLES_V1_INCLUDE_DIR
    NAMES GLES/gl.h
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${PC_glesv2_INCLUDE_DIRS} ${PC_glesv1_cm_INCLUDE_DIRS}
    PATH_SUFFIXES include)
find_path(
    OpenGLES_V2_INCLUDE_DIR
    NAMES GLES2/gl2.h
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${PC_glesv2_INCLUDE_DIRS} ${PC_glesv1_cm_INCLUDE_DIRS}
    PATH_SUFFIXES include)
find_path(
    OpenGLES_V3_INCLUDE_DIR
    NAMES GLES3/gl3.h
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${OpenGLES_V1_INCLUDE_DIR} ${OpenGLES_V2_INCLUDE_DIR}
          ${PC_glesv2_INCLUDE_DIRS} ${PC_glesv1_cm_INCLUDE_DIRS}
    PATH_SUFFIXES include)
find_path(
    OpenGLES_V31_INCLUDE_DIR
    NAMES GLES3/gl31.h
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${OpenGLES_V1_INCLUDE_DIR} ${OpenGLES_V2_INCLUDE_DIR}
          ${OpenGLES_V3_INCLUDE_DIR} ${PC_glesv2_INCLUDE_DIRS}
          ${PC_glesv1_cm_INCLUDE_DIRS}
    PATH_SUFFIXES include)
find_path(
    OpenGLES_V32_INCLUDE_DIR
    NAMES GLES3/gl32.h
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${OpenGLES_V1_INCLUDE_DIR} ${OpenGLES_V2_INCLUDE_DIR}
          ${OpenGLES_V3_INCLUDE_DIR} ${OpenGLES_V31_INCLUDE_DIR}
          ${PC_glesv2_INCLUDE_DIRS} ${PC_glesv1_cm_INCLUDE_DIRS}
    PATH_SUFFIXES include)

find_library(
    OpenGLES_V1_LIBRARY
    NAMES GLES GLESv1_CM
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${PC_glesv1_cm_LIBRARY_DIRS}
    PATH_SUFFIXES lib)
find_library(
    OpenGLES_V2_LIBRARY
    NAMES GLESv2 OpenGLES # for Apple framework
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${PC_glesv2_LIBRARY_DIRS}
    PATH_SUFFIXES lib)
find_library(
    OpenGLES_V3_LIBRARY
    NAMES GLESv3
    PATHS ${OpenGLES_ROOT_DIR}
    HINTS ${PC_glesv2_LIBRARY_DIRS}
    PATH_SUFFIXES lib)

if(OpenGLES_V2_LIBRARY AND NOT OpenGLES_V3_LIBRARY)
    set(OpenGLES_V3_LIBRARY ${OpenGLES_V2_LIBRARY})
endif()

set(_gles_required_vars)
foreach(_comp IN LISTS OpenGLES_FIND_COMPONENTS)
    if(_comp STREQUAL "V1")
        list(APPEND _gles_required_vars OpenGLES_V1_LIBRARY
             OpenGLES_V1_INCLUDE_DIR)
        if(OpenGLES_V1_INCLUDE_DIR AND OpenGLES_V1_LIBRARY)
            set(OpenGLES_${_comp}_FOUND TRUE)
        else()
            set(OpenGLES_${_comp}_FOUND FALSE)
        endif()
    elseif(_comp STREQUAL "V2")
        list(APPEND _gles_required_vars OpenGLES_V2_LIBRARY
             OpenGLES_V2_INCLUDE_DIR)
        if(OpenGLES_V2_INCLUDE_DIR AND OpenGLES_V2_LIBRARY)
            set(OpenGLES_${_comp}_FOUND TRUE)
        else()
            set(OpenGLES_${_comp}_FOUND FALSE)
        endif()
    elseif(_comp STREQUAL "V3")
        list(APPEND _gles_required_vars OpenGLES_V3_LIBRARY
             OpenGLES_V3_INCLUDE_DIR)
        if(OpenGLES_V3_INCLUDE_DIR AND OpenGLES_V3_LIBRARY)
            set(OpenGLES_${_comp}_FOUND TRUE)
        else()
            set(OpenGLES_${_comp}_FOUND FALSE)
        endif()
    elseif(_comp STREQUAL "V31")
        list(APPEND _gles_required_vars OpenGLES_V3_LIBRARY
             OpenGLES_V31_INCLUDE_DIR)

        if(OpenGLES_V31_INCLUDE_DIR AND OpenGLES_V3_LIBRARY)
            set(OpenGLES_${_comp}_FOUND TRUE)
        else()
            set(OpenGLES_${_comp}_FOUND FALSE)
        endif()
    elseif(_comp STREQUAL "V32")
        list(APPEND _gles_required_vars OpenGLES_V3_LIBRARY
             OpenGLES_V32_INCLUDE_DIR)
        if(OpenGLES_V32_INCLUDE_DIR AND OpenGLES_V3_LIBRARY)
            set(OpenGLES_${_comp}_FOUND TRUE)
        else()
            set(OpenGLES_${_comp}_FOUND FALSE)
        endif()
    else()
        message(
            WARNING "${_comp} is not a recognized OpenGL-ES component/version")
        set(OpenGLES_${_comp}_FOUND FALSE)
    endif()
endforeach()
if(_gles_required_vars)
    list(REMOVE_DUPLICATES _gles_required_vars)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    OpenGLES
    REQUIRED_VARS ${_gles_required_vars}
    HANDLE_COMPONENTS)
if(OpenGLES_FOUND)
    if(OpenGLES_V1_FOUND AND NOT TARGET OpenGLES::OpenGLESv1)
        add_library(OpenGLES::OpenGLESv1 SHARED IMPORTED)

        set_target_properties(
            OpenGLES::OpenGLESv1
            PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                       "${OpenGLES_V1_INCLUDE_DIR}"
                       IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                       IMPORTED_LOCATION ${OpenGLES_V1_LIBRARY})
    endif()
    if(OpenGLES_V2_FOUND AND NOT TARGET OpenGLES::OpenGLESv2)
        add_library(OpenGLES::OpenGLESv2 SHARED IMPORTED)

        set_target_properties(
            OpenGLES::OpenGLESv2
            PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                       "${OpenGLES_V2_INCLUDE_DIR}"
                       IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                       IMPORTED_LOCATION ${OpenGLES_V2_LIBRARY})
    endif()
    if(OpenGLES_V3_FOUND)
        if(NOT TARGET OpenGLES::OpenGLESv3)
            add_library(OpenGLES::OpenGLESv3 SHARED IMPORTED)

            set_target_properties(
                OpenGLES::OpenGLESv3
                PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                           "${OpenGLES_V3_INCLUDE_DIR}"
                           IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                           IMPORTED_LOCATION ${OpenGLES_V3_LIBRARY})
        endif()
        if(OpenGLES_V31_FOUND AND NOT TARGET OpenGLES::OpenGLESv31)
            add_library(OpenGLES::OpenGLESv31 SHARED IMPORTED)

            set_target_properties(
                OpenGLES::OpenGLESv31
                PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                           "${OpenGLES_V31_INCLUDE_DIR}"
                           IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                           IMPORTED_LOCATION ${OpenGLES_V3_LIBRARY})
        endif()
        if(OpenGLES_V32_FOUND AND NOT TARGET OpenGLES::OpenGLESv32)
            add_library(OpenGLES::OpenGLESv32 SHARED IMPORTED)

            set_target_properties(
                OpenGLES::OpenGLESv32
                PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                           "${OpenGLES_V32_INCLUDE_DIR}"
                           IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                           IMPORTED_LOCATION ${OpenGLES_V3_LIBRARY})
        endif()
    endif()
    mark_as_advanced(
        OpenGLES_V1_LIBRARY
        OpenGLES_V1_INCLUDE_DIR
        OpenGLES_V2_LIBRARY
        OpenGLES_V2_INCLUDE_DIR
        OpenGLES_V3_LIBRARY
        OpenGLES_V3_INCLUDE_DIR
        OpenGLES_V31_INCLUDE_DIR
        OpenGLES_V32_INCLUDE_DIR)
endif()
mark_as_advanced(OpenGLES_ROOT_DIR)

include(FeatureSummary)
set_package_properties(
    OpenGLES PROPERTIES
    URL "https://www.khronos.org/opengles/"
    DESCRIPTION
        "A cross-platform graphics API, specialized for mobile and embedded, defined as a subset of desktop OpenGL."
)
