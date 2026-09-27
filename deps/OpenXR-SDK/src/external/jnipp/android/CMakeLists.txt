CMAKE_MINIMUM_REQUIRED(VERSION 3.4.1)

GET_FILENAME_COMPONENT(root_dir "../.." ABSOLUTE)

LINK_DIRECTORIES("${root_dir}/${ANDROID_ABI}")

SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${root_dir}/${ANDROID_ABI}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DANDROID_STL=c++_static -std=gnu++11 -fexceptions")

FILE(GLOB sources ../*.cpp ../common/*.cpp ../android/*.cpp)

ADD_LIBRARY(jnipp SHARED ${sources})
