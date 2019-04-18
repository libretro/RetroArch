set(BUILD_FONT FALSE)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/host_applications/linux/libs/bcm_host/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/ilclient)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libs/vgfont)

set(ILCLIENT_SRCS libs/ilclient/ilclient.c libs/ilclient/ilcore.c)
add_library(ilclient ${ILCLIENT_SRCS})

set(HELLO_PI_LIBS ilclient openmaxil bcm_host vcos vchiq_arm)

add_subdirectory(hello_world)
add_subdirectory(hello_video)
add_subdirectory(hello_audio)
add_subdirectory(hello_triangle)
add_subdirectory(hello_triangle2)
add_subdirectory(hello_dispmanx)
add_subdirectory(hello_tiger)
add_subdirectory(hello_encode)
add_subdirectory(hello_jpeg)
add_subdirectory(hello_videocube)
add_subdirectory(hello_teapot)

if(BUILD_FONT)
set(VGFONT_SRCS libs/vgfont/font.c libs/vgfont/vgft.c libs/vgfont/graphics.c)
set_source_files_properties(${VGFONT_SRCS} PROPERTIES COMPILE_DEFINITIONS)
add_library(vgfont ${VGFONT_SRCS})

add_subdirectory(hello_font)
endif(BUILD_FONT)
