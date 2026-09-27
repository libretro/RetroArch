# Copyright (c) 2017-2026 The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

set(PRESENTATION_BACKENDS xlib xcb wayland)
set(PRESENTATION_BACKEND
    xlib
    CACHE STRING "Presentation backend chosen at configure time"
)
set_property(CACHE PRESENTATION_BACKEND PROPERTY STRINGS ${PRESENTATION_BACKENDS})

list(FIND PRESENTATION_BACKENDS ${PRESENTATION_BACKEND} index)
if(index EQUAL -1)
    message(FATAL_ERROR "Presentation backend must be one of ${PRESENTATION_BACKENDS}")
endif()

message(STATUS "Presentation backend selected for hello_xr, loader_test, conformance: ${PRESENTATION_BACKEND}")

find_package(X11)

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
    pkg_search_module(XCB xcb)
    pkg_search_module(XCB_GLX xcb-glx)

    pkg_search_module(WAYLAND_CLIENT wayland-client)
endif()

include(CMakeDependentOption)
cmake_dependent_option(
    BUILD_WITH_XLIB_HEADERS "Build with support for X11/Xlib-related features." ON "X11_FOUND" OFF
)
cmake_dependent_option(
    BUILD_WITH_XCB_HEADERS "Build with support for XCB-related features." ON "X11_FOUND AND XCB_FOUND AND XCB_GLX_FOUND" OFF
)
cmake_dependent_option(
    BUILD_WITH_WAYLAND_HEADERS "Build with support for Wayland-related features." ON "WAYLAND_CLIENT_FOUND"
    OFF
)

message(STATUS "BUILD_WITH_XLIB_HEADERS: ${BUILD_WITH_XLIB_HEADERS}")
message(STATUS "BUILD_WITH_XCB_HEADERS: ${BUILD_WITH_XCB_HEADERS}")
message(STATUS "BUILD_WITH_WAYLAND_HEADERS: ${BUILD_WITH_WAYLAND_HEADERS}")

if(PRESENTATION_BACKEND MATCHES "xlib")
    if(NOT BUILD_WITH_XLIB_HEADERS)
        message(
            FATAL_ERROR
                "xlib backend selected, but BUILD_WITH_XLIB_HEADERS either disabled or unavailable due to missing dependencies."
        )
    endif()
    if(BUILD_TESTS AND (NOT X11_Xxf86vm_LIB OR NOT X11_Xrandr_LIB))
        message(FATAL_ERROR "OpenXR tests using xlib backend requires Xxf86vm and Xrandr")
    endif()

    if(TARGET openxr-gfxwrapper)
        target_compile_definitions(openxr-gfxwrapper PUBLIC OS_LINUX_XLIB)
        target_link_libraries(openxr-gfxwrapper PRIVATE ${X11_X11_LIB} ${X11_Xxf86vm_LIB} ${X11_Xrandr_LIB})

        # OpenGL::OpenGL already linked, we just need to add GLX.
        if(TARGET OpenGL::GLX)
            target_link_libraries(openxr-gfxwrapper PUBLIC OpenGL::GLX)
        else()
            if(${OPENGL_glx_LIBRARY})
                target_link_libraries(openxr-gfxwrapper PUBLIC ${OPENGL_glx_LIBRARY})
            endif()
            target_link_libraries(openxr-gfxwrapper PUBLIC ${OPENGL_LIBRARIES})
        endif()
    endif()
elseif(PRESENTATION_BACKEND MATCHES "xcb")
    if(NOT BUILD_WITH_XCB_HEADERS)
        message(
            FATAL_ERROR
                "xcb backend selected, but BUILD_WITH_XCB_HEADERS either disabled or unavailable due to missing dependencies."
        )
    endif()
    pkg_search_module(XCB_RANDR REQUIRED xcb-randr)
    pkg_search_module(XCB_KEYSYMS REQUIRED xcb-keysyms)
    pkg_search_module(XCB_GLX REQUIRED xcb-glx)
    pkg_search_module(XCB_DRI2 REQUIRED xcb-dri2)
    pkg_search_module(XCB_ICCCM REQUIRED xcb-icccm)

    if(TARGET openxr-gfxwrapper)
        # XCB + XCB GLX is limited to OpenGL 2.1
        # target_compile_definitions(openxr-gfxwrapper PUBLIC OS_LINUX_XCB )
        # XCB + Xlib GLX 1.3
        target_compile_definitions(openxr-gfxwrapper PUBLIC OS_LINUX_XCB_GLX)

        target_link_libraries(openxr-gfxwrapper PRIVATE ${X11_X11_LIB} ${XCB_KEYSYMS_LIBRARIES} ${XCB_RANDR_LIBRARIES})

    endif()
elseif(PRESENTATION_BACKEND MATCHES "wayland")
    if(NOT BUILD_WITH_WAYLAND_HEADERS)
        message(
            FATAL_ERROR
                "wayland backend selected, but BUILD_WITH_WAYLAND_HEADERS either disabled or unavailable due to missing dependencies."
        )
    endif()

    pkg_search_module(WAYLAND_EGL REQUIRED wayland-egl)
    pkg_search_module(WAYLAND_SCANNER REQUIRED wayland-scanner)
    pkg_search_module(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.7)
    pkg_search_module(EGL REQUIRED egl)

    if(TARGET openxr-gfxwrapper)
        # generate wayland protocols
        set(WAYLAND_PROTOCOLS_DIR ${PROJECT_BINARY_DIR}/wayland-protocols/)
        file(MAKE_DIRECTORY ${WAYLAND_PROTOCOLS_DIR})

        pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)
        pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)

        set(PROTOCOL xdg-shell-unstable-v6)
        set(PROTOCOL_XML ${WAYLAND_PROTOCOLS_DATADIR}/unstable/xdg-shell/${PROTOCOL}.xml)

        if(NOT EXISTS ${PROTOCOL_XML})
            message(FATAL_ERROR "xdg-shell-unstable-v6.xml not found in " ${WAYLAND_PROTOCOLS_DATADIR}
                                "\nYour wayland-protocols package does not " "contain xdg-shell-unstable-v6."
            )
        endif()
        add_custom_command(
            OUTPUT ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.c
            COMMAND ${WAYLAND_SCANNER} code ${PROTOCOL_XML} ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.c
            VERBATIM
        )
        add_custom_command(
            OUTPUT ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.h
            COMMAND ${WAYLAND_SCANNER} client-header ${PROTOCOL_XML} ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.h
            VERBATIM
        )

        target_sources(openxr-gfxwrapper
            PRIVATE
            ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.c
            ${WAYLAND_PROTOCOLS_DIR}/${PROTOCOL}.h
        )

        target_include_directories(openxr-gfxwrapper PUBLIC ${WAYLAND_PROTOCOLS_DIR} ${WAYLAND_CLIENT_INCLUDE_DIRS})
        target_link_libraries(
            openxr-gfxwrapper PRIVATE ${EGL_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${WAYLAND_EGL_LIBRARIES}
        )
        target_compile_definitions(openxr-gfxwrapper PUBLIC OS_LINUX_WAYLAND)
        target_link_libraries(openxr-gfxwrapper PRIVATE ${EGL_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES})
    endif()
endif()


if(TARGET openxr-gfxwrapper AND NOT (PRESENTATION_BACKEND MATCHES "wayland"))
    if(TARGET OpenGL::GLX)
        # OpenGL::OpenGL already linked, we just need to add GLX.
        target_link_libraries(openxr-gfxwrapper PUBLIC OpenGL::GLX)
    else()
        if(${OPENGL_glx_LIBRARY})
            target_link_libraries(openxr-gfxwrapper PUBLIC ${OPENGL_glx_LIBRARY})
        endif()
            target_link_libraries(openxr-gfxwrapper PUBLIC ${OPENGL_LIBRARIES})
    endif()
endif()
