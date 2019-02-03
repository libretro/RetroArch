cmake_minimum_required(VERSION 2.8)

if (WIN32)
   set(VCOS_PLATFORM win32)
else ()
   set(VCOS_PLATFORM pthreads)
   add_definitions(-Wall -Werror)
endif ()

include_directories( ../../../..
                     ../../../../interface/vcos
                     ../../../../interface/vcos/${VCOS_PLATFORM} )

#add_subdirectory( ../../../../interface/vcos/${VCOS_PLATFORM} vcos)
#add_subdirectory( ../../bin/gencmd)

add_executable(vcgencmd gencmd.c)
target_link_libraries(vcgencmd vcos vchiq_arm vchostif)
install(TARGETS vcgencmd RUNTIME DESTINATION bin)
