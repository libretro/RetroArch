add_library(mmal_components ${LIBRARY_TYPE}
	    container_reader.c
	    null_sink.c
	    passthrough.c
	    scheduler.c
	    splitter.c
	    copy.c
	    artificial_camera.c
	    aggregator.c
	    clock.c
	    spdif.c
	   )

set(extra_components_SRCS avcodec_video_decoder.c avcodec_audio_decoder.c
    sdl_video_render.c sdl_audio_render.c aaf_audio_render.cpp android_media_codec.cpp)

#target_link_libraries(mmal_components avcodec avutil)
#target_link_libraries(mmal_components SDL)
#if (WIN32)
#target_link_libraries(mmal_components avcore avutil z) # For avcodec
#target_link_libraries(mmal_components gdi32 winmm) # For SDL
#endif (WIN32)

add_custom_target(mmal_components_extra ALL
    COMMAND touch ${extra_components_SRCS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/interface/mmal/components)

set(container_libs ${container_libs} containers)

target_link_libraries(mmal_components ${container_libs} mmal_util)
target_link_libraries(mmal_components mmal_core)

install(TARGETS mmal_components DESTINATION lib)
