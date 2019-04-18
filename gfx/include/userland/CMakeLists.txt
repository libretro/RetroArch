cmake_minimum_required(VERSION 2.8)

project(vmcs_host_apps)

SET(PROJECT_VER_MAJOR 1)
SET(PROJECT_VER_MINOR 0)
SET(PROJECT_VER_PATCH 0)
SET(PROJECT_VER "${PROJECT_VER_MAJOR}.${PROJECT_VER_MINOR}.${PROJECT_VER_PATCH}")
SET(PROJECT_APIVER "${PROJECT_VER}")

if(ARM64)
   set(BUILD_MMAL FALSE)
   set(BUILD_MMAL_APPS FALSE)
else()
   set(BUILD_MMAL TRUE)
   set(BUILD_MMAL_APPS TRUE)
endif()
set(vmcs_root ${PROJECT_SOURCE_DIR})
get_filename_component(VIDEOCORE_ROOT . ABSOLUTE)

set(VCOS_PTHREADS_BUILD_SHARED TRUE)

include(makefiles/cmake/global_settings.cmake)
include(makefiles/cmake/arm-linux.cmake)
include(makefiles/cmake/vmcs.cmake)

enable_language(ASM)

# Global include paths
include_directories(host_applications/framework)
include_directories(${PROJECT_SOURCE_DIR})
include_directories(interface/vcos/pthreads)
include_directories(interface/vmcs_host/linux)
include_directories(interface/vmcs_host)
include_directories(interface/vmcs_host/khronos)
include_directories(interface/khronos/include)
include_directories(${PROJECT_BINARY_DIR})
include_directories(interface/vchiq_arm)
#include_directories(tools/inet_transport)
include_directories(host_support/include)

# Global compiler flags
if(CMAKE_COMPILER_IS_GNUCC)
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-multichar -Wall -Wno-unused-but-set-variable -fPIC")
endif()

add_definitions(-D_REENTRANT)
add_definitions(-DUSE_VCHIQ_ARM -DVCHI_BULK_ALIGN=1 -DVCHI_BULK_GRANULARITY=1)
add_definitions(-DOMX_SKIP64BIT)
add_definitions(-DEGL_SERVER_DISPMANX)
add_definitions(-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
add_definitions(-D_GNU_SOURCE)

# do we actually need this?
add_definitions(-D__VIDEOCORE4__)
add_definitions(-DTV_SUPPORTED_MODE_NO_DEPRECATED)

# add_definitions(-DKHRONOS_CLIENT_LOGGING)

# Check for OpenWF-C value set via command line
if(KHRONOS_EGL_PLATFORM MATCHES "openwfc")
   add_definitions(-DKHRONOS_EGL_PLATFORM_OPENWFC)
endif()

# List of subsidiary CMakeLists
add_subdirectory(interface/vcos)
add_subdirectory(interface/vmcs_host)
add_subdirectory(interface/vchiq_arm)
if(NOT ARM64)
   add_subdirectory(interface/khronos)
endif()

#add_subdirectory(opensrc/tools/lua)
if(BUILD_MMAL)
   include_directories(interface/mmal)
   add_subdirectory(interface/mmal)
   add_subdirectory(containers)
endif()

# VidTex supports Android and Linux
if(BUILD_MMAL_APPS)
add_subdirectory(host_applications/android/apps/vidtex)
endif(BUILD_MMAL_APPS)

if(NOT ARM64)
   add_subdirectory(middleware/openmaxil)
endif()

# 3d demo code
#if(NOT ANDROID)
#   add_subdirectory(thirdparty/applications/demos)
#   add_subdirectory(opensrc/applications/demos)
#endif()

#if(ENABLE_3D_TESTS)
#   add_subdirectory(thirdparty/applications/test)
#endif()

# FIXME: we should use a pre-packaged version of freetype
# rather than the one included in the repo.
#add_subdirectory(opensrc/helpers/freetype)
#add_subdirectory(${PROJECT_SOURCE_DIR}/opensrc/helpers/fonts/ttf-bitstream-vera)

# VMCS Host Applications
#add_subdirectory(host_applications/framework)

# add_subdirectory(interface/vchiq/test/win32)

# Apps and libraries supporting Camera Tuning Tool
#add_subdirectory(tools/inet_transport/linux)
#add_subdirectory(host_support/vcstandalone)

# add linux apps
add_subdirectory(host_applications/linux)
add_subdirectory(opensrc/helpers/libfdt)
add_subdirectory(helpers/dtoverlay)

set(vmcs_host_apps_VERSION_MAJOR 1)
set(vmcs_host_apps_VERSION_MINOR 0)

include_directories("${PROJECT_BINARY_DIR}")
include(FindPkgConfig QUIET)
if(PKG_CONFIG_FOUND)
	# Produce a pkg-config file
	foreach(PCFILE bcm_host.pc egl.pc glesv2.pc vg.pc brcmegl.pc brcmglesv2.pc  brcmvg.pc vcsm.pc mmal.pc )
		configure_file("pkgconfig/${PCFILE}.in" "${PCFILE}" @ONLY)
		install(FILES       "${CMAKE_CURRENT_BINARY_DIR}/${PCFILE}"
			DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
	endforeach()
endif()
# Remove cache entry, if one added by command line
unset(KHRONOS_EGL_PLATFORM CACHE)
