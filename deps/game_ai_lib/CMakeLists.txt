cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(custom_ops)

find_package(Torch REQUIRED)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(test test.cpp RetroModel.cpp)
target_link_libraries(test "${TORCH_LIBRARIES}" ${OpenCV_LIBS})

add_library(game_ai SHARED GameAILocal.cpp RetroModel.cpp games/NHL94GameAI.cpp games/NHL94GameData.cpp games/DefaultGameAI.cpp utils/data.cpp utils/memory.cpp utils/utils.cpp)

target_link_libraries(game_ai "${TORCH_LIBRARIES}" ${OpenCV_LIBS})

set_property(TARGET test PROPERTY CXX_STANDARD 17)
set_property(TARGET game_ai PROPERTY CXX_STANDARD 17)

if (MSVC)
  file(GLOB TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
  add_custom_command(TARGET test
                     POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E copy_if_different
                     ${TORCH_DLLS}
                     $<TARGET_FILE_DIR:test>)
endif (MSVC)