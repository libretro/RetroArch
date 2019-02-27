/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "interface/khronos/common/khrn_client_unmangle.h"
#include "interface/khronos/include/GLES/gl.h"
#include "interface/khronos/include/GLES/glext.h"
#include "interface/khronos/include/GLES2/gl2.h"
#include "interface/khronos/include/GLES2/gl2ext.h"

#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/common/khrn_options.h"

#include "interface/khronos/egl/egl_client_surface.h"
#include "interface/khronos/egl/egl_client_context.h"
#include "interface/khronos/egl/egl_client_config.h"

#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"
#include "interface/khronos/include/VG/vgext.h"

#ifdef RPC_DIRECT
#include "interface/khronos/egl/egl_int_impl.h"
#endif

#if defined(WIN32) || defined(__mips__)
#include "interface/khronos/common/khrn_int_misc_impl.h"
#endif

#ifdef KHRONOS_EGL_PLATFORM_OPENWFC
#include "interface/khronos/wf/wfc_client_stream.h"
#endif

#if defined(RPC_DIRECT_MULTI)
#include "middleware/khronos/egl/egl_server.h"
#endif

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mangle eglGetProcAddress */
#include "interface/khronos/common/khrn_client_mangle.h"

EGLAPI void EGLAPIENTRY (* eglGetProcAddress(const char *procname))(void)
{
/* Don't mangle the rest */
#include "interface/khronos/common/khrn_client_unmangle.h"
#include "interface/khronos/include/EGL/eglext.h"

   /* TODO: any other functions we need to return here?    */
   if(!procname) return (void(*)(void)) NULL;

#if EGL_KHR_image
   if (!strcmp(procname, "eglCreateImageKHR"))
      return (void(*)(void))eglCreateImageKHR;
   if (!strcmp(procname, "eglDestroyImageKHR"))
      return (void(*)(void))eglDestroyImageKHR;
#endif
#ifdef GL_EXT_discard_framebuffer
   if (!strcmp(procname, "glDiscardFramebufferEXT"))
      return (void(*)(void))glDiscardFramebufferEXT;
#endif
#ifdef GL_EXT_debug_marker
   if (!strcmp(procname, "glInsertEventMarkerEXT"))
      return (void(*)(void))glInsertEventMarkerEXT;
   if (!strcmp(procname, "glPushGroupMarkerEXT"))
      return (void(*)(void))glPushGroupMarkerEXT;
   if (!strcmp(procname, "glPopGroupMarkerEXT"))
      return (void(*)(void))glPopGroupMarkerEXT;
#endif
#if GL_OES_point_size_array
   if (!strcmp(procname, "glPointSizePointerOES"))
      return (void(*)(void))glPointSizePointerOES;
#endif
#if GL_OES_EGL_image
   if (!strcmp(procname, "glEGLImageTargetTexture2DOES"))
      return (void(*)(void))glEGLImageTargetTexture2DOES;
   if (!strcmp(procname, "glEGLImageTargetRenderbufferStorageOES"))
      return (void(*)(void))glEGLImageTargetRenderbufferStorageOES;
#endif
#if GL_OES_matrix_palette
   if (!strcmp(procname, "glCurrentPaletteMatrixOES"))
      return (void(*)(void))glCurrentPaletteMatrixOES;
   if (!strcmp(procname, "glLoadPaletteFromModelViewMatrixOES"))
      return (void(*)(void))glLoadPaletteFromModelViewMatrixOES;
   if (!strcmp(procname, "glMatrixIndexPointerOES"))
      return (void(*)(void))glMatrixIndexPointerOES;
   if (!strcmp(procname, "glWeightPointerOES"))
      return (void(*)(void))glWeightPointerOES;
#endif
#ifndef NO_OPENVG
#if VG_KHR_EGL_image
   if (!strcmp(procname, "vgCreateEGLImageTargetKHR"))
      return (void(*)(void))vgCreateEGLImageTargetKHR;
#endif
#endif /* NO_OPENVG */
#if EGL_KHR_lock_surface
   if (!strcmp(procname, "eglLockSurfaceKHR"))
      return (void(*)(void))eglLockSurfaceKHR;
   if (!strcmp(procname, "eglUnlockSurfaceKHR"))
      return (void(*)(void))eglUnlockSurfaceKHR;
#endif
#if EGL_KHR_sync
   if (!strcmp(procname, "eglCreateSyncKHR"))
      return (void(*)(void))eglCreateSyncKHR;
   if (!strcmp(procname, "eglDestroySyncKHR"))
      return (void(*)(void))eglDestroySyncKHR;
   if (!strcmp(procname, "eglClientWaitSyncKHR"))
      return (void(*)(void))eglClientWaitSyncKHR;
   if (!strcmp(procname, "eglSignalSyncKHR"))
      return (void(*)(void))eglSignalSyncKHR;
   if (!strcmp(procname, "eglGetSyncAttribKHR"))
      return (void(*)(void))eglGetSyncAttribKHR;
#endif
#if EGL_BRCM_perf_monitor
   if (!strcmp(procname, "eglInitPerfMonitorBRCM"))
      return (void(*)(void))eglInitPerfMonitorBRCM;
   if (!strcmp(procname, "eglTermPerfMonitorBRCM"))
      return (void(*)(void))eglTermPerfMonitorBRCM;
#endif
#if EGL_BRCM_driver_monitor
   if (!strcmp(procname, "eglInitDriverMonitorBRCM"))
      return (void(*)(void))eglInitDriverMonitorBRCM;
   if (!strcmp(procname, "eglGetDriverMonitorXMLBRCM"))
      return (void(*)(void))eglGetDriverMonitorXMLBRCM;
   if (!strcmp(procname, "eglTermDriverMonitorBRCM"))
      return (void(*)(void))eglTermDriverMonitorBRCM;
#endif
#if EGL_BRCM_perf_stats
   if (!strcmp(procname, "eglPerfStatsResetBRCM"))
      return (void(*)(void))eglPerfStatsResetBRCM;
   if (!strcmp(procname, "eglPerfStatsGetBRCM"))
      return (void(*)(void))eglPerfStatsGetBRCM;
#endif
#if EGL_BRCM_mem_usage
   if (!strcmp(procname, "eglProcessMemUsageGetBRCM"))
      return (void(*)(void))eglProcessMemUsageGetBRCM;
#endif
#ifdef EXPORT_DESTROY_BY_PID
   if (!strcmp(procname, "eglDestroyByPidBRCM"))
      return (void(*)(void))eglDestroyByPidBRCM;
#endif
#if GL_OES_draw_texture
   if (!strcmp(procname, "glDrawTexsOES"))
      return (void(*)(void))glDrawTexsOES;
   if (!strcmp(procname, "glDrawTexiOES"))
      return (void(*)(void))glDrawTexiOES;
   if (!strcmp(procname, "glDrawTexxOES"))
      return (void(*)(void))glDrawTexxOES;
   if (!strcmp(procname, "glDrawTexsvOES"))
      return (void(*)(void))glDrawTexsvOES;
   if (!strcmp(procname, "glDrawTexivOES"))
      return (void(*)(void))glDrawTexivOES;
   if (!strcmp(procname, "glDrawTexxvOES"))
      return (void(*)(void))glDrawTexxvOES;
   if (!strcmp(procname, "glDrawTexfOES"))
      return (void(*)(void))glDrawTexfOES;
   if (!strcmp(procname, "glDrawTexfvOES"))
      return (void(*)(void))glDrawTexfvOES;
#endif

#if GL_OES_query_matrix
   if (!strcmp(procname, "glQueryMatrixxOES"))
      return (void(*)(void))glQueryMatrixxOES;
#endif

#if GL_OES_framebuffer_object
   if (!strcmp(procname, "glIsRenderbufferOES"))
      return (void(*)(void))glIsRenderbufferOES;
   if (!strcmp(procname, "glBindRenderbufferOES"))
      return (void(*)(void))glBindRenderbufferOES;
   if (!strcmp(procname, "glDeleteRenderbuffersOES"))
      return (void(*)(void))glDeleteRenderbuffersOES;
   if (!strcmp(procname, "glGenRenderbuffersOES"))
      return (void(*)(void))glGenRenderbuffersOES;
   if (!strcmp(procname, "glRenderbufferStorageOES"))
      return (void(*)(void))glRenderbufferStorageOES;
   if (!strcmp(procname, "glGetRenderbufferParameterivOES"))
      return (void(*)(void))glGetRenderbufferParameterivOES;
   if (!strcmp(procname, "glIsFramebufferOES"))
      return (void(*)(void))glIsFramebufferOES;
   if (!strcmp(procname, "glBindFramebufferOES"))
      return (void(*)(void))glBindFramebufferOES;
   if (!strcmp(procname, "glDeleteFramebuffersOES"))
      return (void(*)(void))glDeleteFramebuffersOES;
   if (!strcmp(procname, "glGenFramebuffersOES"))
      return (void(*)(void))glGenFramebuffersOES;
   if (!strcmp(procname, "glCheckFramebufferStatusOES"))
      return (void(*)(void))glCheckFramebufferStatusOES;
   if (!strcmp(procname, "glFramebufferRenderbufferOES"))
      return (void(*)(void))glFramebufferRenderbufferOES;
   if (!strcmp(procname, "glFramebufferTexture2DOES"))
      return (void(*)(void))glFramebufferTexture2DOES;
   if (!strcmp(procname, "glGetFramebufferAttachmentParameterivOES"))
      return (void(*)(void))glGetFramebufferAttachmentParameterivOES;
   if (!strcmp(procname, "glGenerateMipmapOES"))
      return (void(*)(void))glGenerateMipmapOES;
#endif

#if GL_OES_mapbuffer
   if (!strcmp(procname, "glGetBufferPointervOES"))
      return (void(*)(void))glGetBufferPointervOES;
   if (!strcmp(procname, "glMapBufferOES"))
      return (void(*)(void))glMapBufferOES;
   if (!strcmp(procname, "glUnmapBufferOES"))
      return (void(*)(void))glUnmapBufferOES;
#endif

#if EGL_proc_state_valid
   if (!strcmp(procname, "eglProcStateValid"))
      return (void(*)(void))eglProcStateValid;
#endif

#if EGL_BRCM_flush
   if (!strcmp(procname, "eglFlushBRCM"))
      return (void(*)(void))eglFlushBRCM;
#endif

#if EGL_BRCM_global_image
   if (!strcmp(procname, "eglCreateGlobalImageBRCM"))
      return (void(*)(void))eglCreateGlobalImageBRCM;
   if (!strcmp(procname, "eglCreateCopyGlobalImageBRCM"))
      return (void(*)(void))eglCreateCopyGlobalImageBRCM;
   if (!strcmp(procname, "eglDestroyGlobalImageBRCM"))
      return (void(*)(void))eglDestroyGlobalImageBRCM;
   if (!strcmp(procname, "eglQueryGlobalImageBRCM"))
      return (void(*)(void))eglQueryGlobalImageBRCM;
#endif

   return (void(*)(void)) NULL;
}

#ifdef __cplusplus
}
#endif
