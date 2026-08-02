# CMake for rpi-rgb-led-matrix core library

#cmake_minimum_required(VERSION 3.22)
#project(
#    rgbmatrix
#    VERSION "0.0.1"
#    LANGUAGES CXX)
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#include(CMakePrintHelpers) # so we can print cmake variable values

# cmake_print_variables(CMAKE_CURRENT_SOURCE_DIR)

set(RGBMATRIX_RP1_PIO_VENDOR_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/rp1/rp1_pio_vendor)

# Define the RGBMATRIX headers.
set(RGBMATRIX_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/canvas.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/content-streamer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/graphics.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/led-matrix.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/led-matrix-c.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/pixel-mapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/threaded-canvas-manipulator.h
)
cmake_print_variables(RGBMATRIX_HEADERS)

# Define the RGBMATRIX source files
set(RGBMATRIX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bdf-font.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/content-streamer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/framebuffer.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/gpio.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/graphics.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/hardware-mapping.c
    ${CMAKE_CURRENT_SOURCE_DIR}/led-matrix.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/led-matrix-c.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/multiplex-mappers.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/options-initialize.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/pixel-mapper.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rp1/rp1_pio_backend.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rp1/rp1_pio_support.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rp1/rp1_rio_backend.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/thread.cc
)
cmake_print_variables(RGBMATRIX_SOURCES)

# Declare the library
if(PROJECT_IS_TOP_LEVEL)
    add_library(rgbmatrix SHARED)
endif()

# Add the sources
target_sources(rgbmatrix PRIVATE ${RGBMATRIX_SOURCES})

# Add include directories
target_include_directories(rgbmatrix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include> )
target_include_directories(rgbmatrix PRIVATE
    ${RGBMATRIX_RP1_PIO_VENDOR_DIR}/piolib/include
    ${RGBMATRIX_RP1_PIO_VENDOR_DIR}/include
)

# Install the library.
#install(
#    TARGETS rgbmatrix
#    EXPORT rgbmatrixTargets
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#)
install(
    TARGETS rgbmatrix
    EXPORT rgbmatrixTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
#install(
#    TARGETS rgbmatrix
#    EXPORT rgbmatrixTargets
#    FILE_SET HEADERS
#    LIBRARY DESTINATION lib
#    ARCHIVE DESTINATION lib
#    RUNTIME DESTINATION bin
#    INCLUDES DESTINATION include
#)
cmake_print_variables(${rgbmatrixTargets})
####
#
# cmake install files, so other packages can find this library.
#
####
set(RGBMATRIX_CMAKE_PACKAGE_INSTALL_SUBDIR "share/rgbmatrix/cmake")

#set(SYSCONFIG_INSTALL_DIR ${CMAKE_INSTALL_SYSCONFDIR}
#    CACHE PATH "Location of configuration files" )
install(
  EXPORT rgbmatrixTargets  # this name before Targets should match the name of the variable set above in a previous install call
  NAMESPACE rgbmatrix::
  DESTINATION ${RGBMATRIX_CMAKE_PACKAGE_INSTALL_SUBDIR})

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  rgbmatrixConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})  #set so we can use in configure_package_config_file()
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}) # ditto

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/rgbmatrixConfig.cmake.in" rgbmatrixConfig.cmake
  PATH_VARS INCLUDE_INSTALL_DIR SYSCONFIG_INSTALL_DIR LIB_INSTALL_DIR
  INSTALL_DESTINATION ${RGBMATRIX_CMAKE_PACKAGE_INSTALL_SUBDIR}
  )

install(FILES "${PROJECT_BINARY_DIR}/rgbmatrixConfig.cmake"
              "${PROJECT_BINARY_DIR}/rgbmatrixConfigVersion.cmake"
        DESTINATION ${RGBMATRIX_CMAKE_PACKAGE_INSTALL_SUBDIR})
