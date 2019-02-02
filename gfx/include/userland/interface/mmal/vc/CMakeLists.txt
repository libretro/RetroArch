add_definitions(-DENABLE_MMAL_VCSM)

add_library(mmal_vc_client ${LIBRARY_TYPE} mmal_vc_client.c mmal_vc_shm.c mmal_vc_api.c mmal_vc_opaque_alloc.c mmal_vc_msgnames.c mmal_vc_api_drm.c)
#target_link_libraries(mmal_vc_client vchiq_arm vcos)

target_link_libraries(mmal_vc_client vchiq_arm vcos vcsm)

if(BUILD_MMAL_APPS)
add_executable(mmal_vc_diag mmal_vc_diag.c)
target_link_libraries(mmal_vc_diag mmal mmal_vc_client debug_sym vcos)
install(TARGETS mmal_vc_diag RUNTIME DESTINATION bin)
endif(BUILD_MMAL_APPS)

include_directories ( ../../../host_applications/linux/libs/sm )

install(TARGETS mmal_vc_client DESTINATION lib)
install(FILES
   mmal_vc_api.h
   mmal_vc_api_drm.h
   mmal_vc_client_priv.h
   mmal_vc_msgnames.h
   mmal_vc_msgs.h
   mmal_vc_opaque_alloc.h
   mmal_vc_shm.h
   DESTINATION include/interface/mmal/vc
)
