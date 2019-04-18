# We support building both static and shared libraries
if (NOT DEFINED LIBRARY_TYPE)
set(LIBRARY_TYPE SHARED)
endif (NOT DEFINED LIBRARY_TYPE)

add_definitions(-Wall -Werror)

add_library(mmal SHARED util/mmal_util.c)

add_subdirectory(core)
add_subdirectory(util)
add_subdirectory(vc)
add_subdirectory(components)
add_subdirectory(openmaxil)
add_subdirectory(client)

target_link_libraries(mmal mmal_core mmal_util mmal_vc_client vcos mmal_components)

install(TARGETS mmal DESTINATION lib)
install(FILES
   mmal.h
   mmal_buffer.h
   mmal_clock.h
   mmal_common.h
   mmal_component.h
   mmal_encodings.h
   mmal_events.h
   mmal_format.h
   mmal_logging.h
   mmal_parameters.h
   mmal_parameters_audio.h
   mmal_parameters_camera.h
   mmal_parameters_clock.h
   mmal_parameters_common.h
   mmal_parameters_video.h
   mmal_pool.h mmal_port.h
   mmal_queue.h
   mmal_types.h
   DESTINATION include/interface/mmal
)

# Test apps
if(BUILD_MMAL_APPS)
add_subdirectory(test)
endif(BUILD_MMAL_APPS)
