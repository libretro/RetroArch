add_library(brcmjpeg SHARED brcmjpeg.c)
target_link_libraries(brcmjpeg mmal_core mmal_util mmal_vc_client)

include_directories(../../../../host_applications/linux/libs/sm)
add_executable(brcmjpeg_test brcmjpeg_test.c)
target_link_libraries(brcmjpeg_test brcmjpeg vcsm vcos)
