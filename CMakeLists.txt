# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 2.8)
project(SPIRV-Cross)
enable_testing()

option(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS "Instead of throwing exceptions assert" OFF)

if(${CMAKE_GENERATOR} MATCHES "Makefile")
  if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
    message(FATAL_ERROR "Build out of tree to avoid overwriting Makefile")
  endif()
endif()

add_library(spirv-cross-core STATIC
		${CMAKE_CURRENT_SOURCE_DIR}/GLSL.std.450.h
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_common.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cfg.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cfg.cpp)

add_library(spirv-cross-glsl STATIC
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_glsl.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_glsl.hpp)

add_library(spirv-cross-cpp STATIC
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cpp.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cpp.cpp)

add_library(spirv-cross-msl STATIC
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_msl.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_msl.cpp)

add_executable(spirv-cross main.cpp)
target_link_libraries(spirv-cross spirv-cross-glsl spirv-cross-cpp spirv-cross-msl spirv-cross-core)
target_link_libraries(spirv-cross-glsl spirv-cross-core)
target_link_libraries(spirv-cross-msl spirv-cross-glsl)
target_link_libraries(spirv-cross-cpp spirv-cross-glsl)
target_include_directories(spirv-cross-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(spirv-compiler-options "")
set(spirv-compiler-defines "")

if(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
  set(spirv-compiler-defines ${spirv-compiler-defines} SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
endif()

# To specify special debug or optimization options, use
# -DCMAKE_CXX_COMPILE_FLAGS
# However, we require the C++11 dialect.
if (NOT "${MSVC}")
  set(spirv-compiler-options ${spirv-compiler-options} -std=c++11 -Wall -Wextra -Werror -Wshadow)
  set(spirv-compiler-defines ${spirv-compiler-defines} __STDC_LIMIT_MACROS)

  if(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
    set(spirv-compiler-options ${spirv-compiler-options} -fno-exceptions)
  endif()
endif()

target_compile_options(spirv-cross-core PRIVATE ${spirv-compiler-options})
target_compile_options(spirv-cross-glsl PRIVATE ${spirv-compiler-options})
target_compile_options(spirv-cross-msl PRIVATE ${spirv-compiler-options})
target_compile_options(spirv-cross-cpp PRIVATE ${spirv-compiler-options})
target_compile_options(spirv-cross PRIVATE ${spirv-compiler-options})
target_compile_definitions(spirv-cross-core PRIVATE ${spirv-compiler-defines})
target_compile_definitions(spirv-cross-glsl PRIVATE ${spirv-compiler-defines})
target_compile_definitions(spirv-cross-msl PRIVATE ${spirv-compiler-defines})
target_compile_definitions(spirv-cross-cpp PRIVATE ${spirv-compiler-defines})
target_compile_definitions(spirv-cross PRIVATE ${spirv-compiler-defines})

# Set up tests, using only the simplest modes of the test_shaders
# script.  You have to invoke the script manually to:
#  - Update the reference files
#  - Get cycle counts from malisc
#  - Keep failing outputs
find_package(PythonInterp)
if(${PYTHONINTERP_FOUND} AND ${PYTHON_VERSION_MAJOR} GREATER 2)
  add_test(NAME spirv-cross-test
	   COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_shaders.py
		   ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
else()
  message(WARNING "Testing disabled. Could not find python3. If you have python3 installed try running "
		  "cmake with -DPYTHON_EXECUTABLE:FILEPATH=/path/to/python3 to help it find the executable")
endif()
