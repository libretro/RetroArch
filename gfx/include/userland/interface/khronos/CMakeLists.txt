
# Khronos
#
# Note: in Android the linker is unable or unwilling to resolve
# dynamically using already loaded symbols. The libraries here
# have quite a few circular dependencies, and so the only way
# to make it work seems to be to have everything static.

set(EGL_SOURCE
   egl/egl_client_config.c
   egl/egl_client_context.c
   egl/egl_client.c
   egl/egl_client_get_proc.c
   egl/egl_client_surface.c
   ext/egl_brcm_driver_monitor_client.c
   ext/egl_brcm_perf_monitor_client.c
   ext/egl_brcm_global_image_client.c
   ext/egl_brcm_flush_client.c
   ext/egl_khr_image_client.c
   ext/egl_khr_sync_client.c
   ext/gl_oes_egl_image_client.c
   ext/egl_khr_lock_surface_client.c
   ext/ext_gl_debug_marker.c
   common/khrn_int_image.c
   common/khrn_int_util.c
   common/khrn_options.c
   common/khrn_client_global_image_map.c
   common/linux/khrn_client_rpc_linux.c
   common/linux/khrn_client_platform_linux.c
   vg/vg_int_mat3x3.c
   vg/vg_client.c
   common/khrn_client.c
   ext/egl_openmaxil_client.c
#   ../vmcs_host/vc_vchi_dispmanx.c
   ext/gl_oes_draw_texture_client.c
   ext/gl_oes_query_matrix_client.c
   ext/gl_oes_framebuffer_object.c
   ext/gl_oes_map_buffer.c
   ext/gl_oes_matrix_palette_client.c)
set(GLES_SOURCE
   glxx/glxx_client.c)
set(VG_SOURCE
   vg/vg_client.c
   vg/vg_int_mat3x3.c)
set(WFC_SOURCE
   wf/wfc_client_stream.c
   wf/wfc_client.c
   wf/wfc_client_server_api.c
   wf/wfc_client_ipc.c
   common/openwfc/khrn_client_platform_openwfc.c)
set(CLIENT_SOURCE
   common/khrn_client_pointermap.c
   common/khrn_client_vector.c
   common/khrn_int_hash.c
   common/khrn_int_hash_asm.s
   common/khrn_client_cache.c)

add_library(EGL ${SHARED} ${EGL_SOURCE})
add_library(GLESv2 ${SHARED} ${GLES_SOURCE})
add_library(OpenVG ${SHARED} ${VG_SOURCE})
add_library(WFC ${SHARED} ${WFC_SOURCE})
add_library(khrn_client ${CLIENT_SOURCE})

# TODO do we need EGL_static and GLESv2_static now that khrn_static exists?
add_library(EGL_static STATIC ${EGL_SOURCE})
add_library(GLESv2_static STATIC ${GLES_SOURCE})
add_library(khrn_static STATIC
   ${EGL_SOURCE} ${GLES_SOURCE} ${VG_SOURCE} ${WFC_SOURCE} ${CLIENT_SOURCE})

if(KHRONOS_HAVE_VCSM)
include_directories (../../host_applications/linux/libs/sm )
set(VCSM_LIBS vcsm)
add_definitions(-DKHRONOS_HAVE_VCSM)
endif()

target_link_libraries(EGL khrn_client vchiq_arm vcos bcm_host ${VCSM_LIBS} -lm)
target_link_libraries(GLESv2 EGL khrn_client vcos)
target_link_libraries(WFC EGL)
target_link_libraries(OpenVG EGL)

install(TARGETS EGL GLESv2 OpenVG WFC khrn_client DESTINATION lib)
install(TARGETS EGL_static GLESv2_static khrn_static DESTINATION lib)

# recommended names to use to avoid conflicts with mesa libs
add_library(brcmEGL ${SHARED} ${EGL_SOURCE})
add_library(brcmGLESv2 ${SHARED} ${GLES_SOURCE})
add_library(brcmOpenVG ${SHARED} ${VG_SOURCE})
add_library(brcmWFC ${SHARED} ${WFC_SOURCE})

target_link_libraries(brcmEGL khrn_client vchiq_arm vcos bcm_host ${VCSM_LIBS} -lm)
target_link_libraries(brcmGLESv2 brcmEGL khrn_client vcos)
target_link_libraries(brcmWFC brcmEGL)
target_link_libraries(brcmOpenVG brcmEGL)

install(TARGETS brcmEGL brcmGLESv2 brcmOpenVG brcmWFC DESTINATION lib)
