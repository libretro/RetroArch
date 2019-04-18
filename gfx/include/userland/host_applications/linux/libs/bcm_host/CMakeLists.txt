
if (WIN32)
   set(VCOS_PLATFORM win32)
else ()
   set(VCOS_PLATFORM pthreads)
   add_definitions(-Wall -Werror)
endif ()

# set this as we want all the source of vchostif to be available in libbcm_host
set (CMAKE_SHARED_LINKER_FLAGS "-u vc_gpuserv_init ${CMAKE_SHARED_LINKER_FLAGS}")

include_directories( ../../../..
		     ../../../../interface/vcos/${VCOS_PLATFORM}
		     ../../../../host_support/linux/added/include
		     ../../../../host_applications/vmcs/test_apps/iltest
                     ../../../../host_applications/framework/common )

add_library(bcm_host ${SHARED} bcm_host.c
       ../../../../interface/vmcs_host/linux/vcfilesys.c
       ../../../../interface/vmcs_host/linux/vcfiled/vcfiled_check.c)

target_link_libraries(bcm_host vcos vchostif)

install(TARGETS bcm_host DESTINATION lib)
