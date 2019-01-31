add_library (mmal_omx ${LIBRARY_TYPE}
   mmalomx_core.c
   mmalomx_logging.c
   mmalomx_commands.c
   mmalomx_buffer.c
   mmalomx_marks.c
   mmalomx_roles.c
   mmalomx_parameters.c
   mmalomx_registry.c
)

add_library (mmal_omxutil ${LIBRARY_TYPE}
   mmalomx_util_params.c
   mmalomx_util_params_audio.c
   mmalomx_util_params_video.c
   mmalomx_util_params_camera.c
   mmalomx_util_params_misc.c
)

target_link_libraries (mmal_omx mmal_omxutil mmal_core mmal_util vcos)
