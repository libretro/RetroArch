cmake_minimum_required(VERSION 2.8)

SET(COMPILE_DEFINITIONS -Werror -Wall)

# Set --no-as-needed to stop the linker discarding mmal_vc_client
# as it can't see that the constructor registers a load of functionality
# with the MMAL core.
SET( CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed" )

include_directories(${PROJECT_SOURCE_DIR}/host_applications/linux/libs/bcm_host/include)

set (VIDTEX_SOURCES
   main.cpp
   launcher_rpi.c
   svp.c
   vidtex.c)
add_executable(vidtex ${VIDTEX_SOURCES})
target_link_libraries(vidtex GLESv2 EGL m bcm_host mmal_core mmal_components mmal_util mmal_vc_client vcos)
