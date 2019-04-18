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
#if defined(KHRN_IMPL_STRUCT)
#define FN(type, name, args) type (*name) args;
#elif defined(KHRN_IMPL_STRUCT_INIT)
#define FN(type, name, args) name,
#else
#define FN(type, name, args) extern type name args;
#endif

#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"
#ifndef KHRN_NO_WFC
#include "interface/khronos/include/WF/wfc.h"
#endif
#include "interface/khronos/include/VG/openvg.h"
#include "interface/khronos/common/khrn_int_image.h"
#include "interface/khronos/egl/egl_int.h"

FN(int, eglIntCreateSurface_impl, (
   uint32_t win,
   uint32_t buffers,
   uint32_t width,
   uint32_t height,
   KHRN_IMAGE_FORMAT_T colorformat,
   KHRN_IMAGE_FORMAT_T depthstencilformat,
   KHRN_IMAGE_FORMAT_T maskformat,
   KHRN_IMAGE_FORMAT_T multisampleformat,
   uint32_t largest,
   uint32_t mipmap,
   uint32_t config_depth_bits,
   uint32_t config_stencil_bits,
   uint32_t sem,
   uint32_t type,
   uint32_t *results))

FN(int, eglIntCreatePbufferFromVGImage_impl, (
   VGImage vg_handle,
   KHRN_IMAGE_FORMAT_T colorformat,
   KHRN_IMAGE_FORMAT_T depthstencilformat,
   KHRN_IMAGE_FORMAT_T maskformat,
   KHRN_IMAGE_FORMAT_T multisampleformat,
   uint32_t mipmap,
   uint32_t config_depth_bits,
   uint32_t config_stencil_bits,
   uint32_t *results))

FN(EGL_SURFACE_ID_T, eglIntCreateWrappedSurface_impl, (
   uint32_t handle_0, uint32_t handle_1,
   KHRN_IMAGE_FORMAT_T depthstencilformat,
   KHRN_IMAGE_FORMAT_T maskformat,
   KHRN_IMAGE_FORMAT_T multisample,
   uint32_t config_depth_bits,
   uint32_t config_stencil_bits))

// Create server states. To actually use these, call, eglIntMakeCurrent.
FN(EGL_GL_CONTEXT_ID_T, eglIntCreateGLES11_impl, (EGL_GL_CONTEXT_ID_T share_id, EGL_CONTEXT_TYPE_T share_type))
FN(EGL_GL_CONTEXT_ID_T, eglIntCreateGLES20_impl, (EGL_GL_CONTEXT_ID_T share, EGL_CONTEXT_TYPE_T share_type))
FN(EGL_VG_CONTEXT_ID_T, eglIntCreateVG_impl, (EGL_VG_CONTEXT_ID_T share, EGL_CONTEXT_TYPE_T share_type))

// Disassociates surface or context objects from their handles. The objects
// themselves still exist as long as there is a reference to them. In
// particular, if you delete part of a triple buffer group, the remaining color
// buffers plus the ancillary buffers all survive.
// If, eglIntDestroySurface is called on a locked surface then that ID is
// guaranteed not to be reused until the surface is unlocked (otherwise a call
// to makevcimage or unlock might target the wrong surface)
FN(int, eglIntDestroySurface_impl, (EGL_SURFACE_ID_T))
FN(void, eglIntDestroyGL_impl, (EGL_GL_CONTEXT_ID_T))
FN(void, eglIntDestroyVG_impl, (EGL_VG_CONTEXT_ID_T))

// Selects the given process id for all operations. Most resource creation is
//  associated with the currently selected process id
// Selects the given context, draw and read surfaces for GL operations.
// Selects the given context and surface for VG operations.
// Any of the surfaces may be identical to each other.
// If the GL context or surfaces have changed then GL will be flushed. Similarly for VG.
// If any of the surfaces have been resized then the color and ancillary buffers
//  are freed and recreated in the new size.
FN(void, eglIntMakeCurrent_impl, (uint32_t pid_0, uint32_t pid_1, uint32_t glversion, EGL_GL_CONTEXT_ID_T, EGL_SURFACE_ID_T, EGL_SURFACE_ID_T, EGL_VG_CONTEXT_ID_T, EGL_SURFACE_ID_T))

// Flushes one or both context, and waits for the flushes to complete before returning.
// Equivalent to:
// if (flushgl) glFinish())
// if (flushvg) vgFinish())
FN(int, eglIntFlushAndWait_impl, (uint32_t flushgl, uint32_t flushvg))
FN(void, eglIntFlush_impl, (uint32_t flushgl, uint32_t flushvg))

FN(void, eglIntSwapBuffers_impl, (EGL_SURFACE_ID_T s, uint32_t width, uint32_t height, uint32_t handle, uint32_t preserve, uint32_t position))
FN(void, eglIntSelectMipmap_impl, (EGL_SURFACE_ID_T s, int level))

FN(void, eglIntGetColorData_impl, (EGL_SURFACE_ID_T s, KHRN_IMAGE_FORMAT_T format, uint32_t width, uint32_t height, int32_t stride, uint32_t y_offset, void *data))
FN(void, eglIntSetColorData_impl, (EGL_SURFACE_ID_T s, KHRN_IMAGE_FORMAT_T format, uint32_t width, uint32_t height, int32_t stride, uint32_t y_offset, const void *data))

FN(bool, eglIntBindTexImage_impl, (EGL_SURFACE_ID_T s))
FN(void, eglIntReleaseTexImage_impl, (EGL_SURFACE_ID_T s))

FN(void, eglIntSwapInterval_impl, (EGL_SURFACE_ID_T s, uint32_t swap_interval))

FN(void, eglIntGetProcessMemUsage_impl, (uint32_t id_0, uint32_t id_1, uint32_t buffer_len, char *buffer))
FN(void, eglIntGetGlobalMemUsage_impl, (uint32_t *result))

FN(EGL_SYNC_ID_T, eglIntCreateSync_impl, (uint32_t type, uint32_t condition, int32_t threshold, uint32_t sem))
FN(void, eglIntCreateSyncFence_impl, (uint32_t condition, int32_t threshold, uint32_t sem))

FN(void, eglIntDestroySync_impl, (EGL_SYNC_ID_T))

FN(void, eglIntDestroyByPid_impl, (uint32_t pid_0, uint32_t pid_1))

FN(void, eglIntCheckCurrent_impl, (uint32_t pid_0, uint32_t pid_1))

#if EGL_KHR_image
FN(int, eglCreateImageKHR_impl, (uint32_t glversion, EGL_CONTEXT_ID_T ctx, EGLenum target, uint32_t buffer, KHRN_IMAGE_FORMAT_T buffer_format, uint32_t buffer_width, uint32_t buffer_height, uint32_t buffer_stride, EGLint texture_level, EGLint *results))
FN(EGLBoolean, eglDestroyImageKHR_impl, (EGLImageKHR image))
#endif

#if EGL_BRCM_global_image
FN(void, eglCreateGlobalImageBRCM_impl, (EGLint width, EGLint height, EGLint pixel_format, EGLint *id))
FN(void, eglFillGlobalImageBRCM_impl, (EGLint id_0, EGLint id_1, EGLint y, EGLint height, const void *data, EGLint data_stride, EGLint data_pixel_format))
FN(void, eglCreateCopyGlobalImageBRCM_impl, (EGLint src_id_0, EGLint src_id_1, EGLint *id))
FN(bool, eglDestroyGlobalImageBRCM_impl, (EGLint id_0, EGLint id_1))
FN(bool, eglQueryGlobalImageBRCM_impl, (EGLint id_0, EGLint id_1, EGLint *width_height_pixel_format))
#endif

#if EGL_BRCM_perf_monitor
FN(bool, eglInitPerfMonitorBRCM_impl, (void))
FN(void, eglTermPerfMonitorBRCM_impl, (void))
#endif

#if EGL_BRCM_driver_monitor
FN(bool, eglInitDriverMonitorBRCM_impl, (EGLint hw_bank, EGLint l3c_bank))
FN(void, eglTermDriverMonitorBRCM_impl, (void))
FN(void, eglGetDriverMonitorXMLBRCM_impl, (EGLint bufSize, char *xmlStats))
#endif

#if EGL_BRCM_perf_stats
FN(void, eglPerfStatsResetBRCM_impl, ())
FN(void, eglPerfStatsGetBRCM_impl, (EGLint buffer_len, EGLBoolean reset, char *buffer))
#endif

FN(int, eglIntOpenMAXILDoneMarker_impl, (void* component_handle, EGLImageKHR egl_image))

FN(void, eglIntImageSetColorData_impl, (EGLImageKHR image, KHRN_IMAGE_FORMAT_T format,
        uint32_t x_offset, uint32_t y_offset,
        uint32_t width, uint32_t height, int32_t stride, const void *data))

#ifdef ANDROID
#ifndef KHRN_NO_WFC
FN(void, eglPushRenderingImage_impl, (uint64_t pid, uint32_t window, EGLImageKHR image))
#endif
#endif

#ifdef DIRECT_RENDERING
FN(void, eglDirectRenderingPointer_impl, (EGL_SURFACE_ID_T surface, uint32_t buffer, KHRN_IMAGE_FORMAT_T buffer_format, uint32_t buffer_width, uint32_t buffer_height, uint32_t buffer_stride))
#endif

#undef FN
