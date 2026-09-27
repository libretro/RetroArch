# Copyright 2021, Collabora, Ltd.
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.10.2)
project(jnipp)

find_package(JNI REQUIRED)
include(CTest)

add_library(jnipp jnipp.cpp)
target_include_directories(
  jnipp
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${JNI_INCLUDE_DIRS})
target_compile_features(jnipp PUBLIC cxx_std_11)
target_link_libraries(jnipp PUBLIC ${CMAKE_DL_LIBS})

add_subdirectory(tests)
