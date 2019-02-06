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
#if defined(ANDROID)

#ifndef EGLEXT_ANDROID_H
#define EGLEXT_ANDROID_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EGL_ANDROID_image_native_buffer
   #define EGL_ANDROID_image_native_buffer 1
   #if defined(EGL_EGLEXT_ANDROID_STRUCT_HEADER)
      #include <system/window.h>
   #else
      struct android_native_buffer_t;
   #endif
   #define EGL_NATIVE_BUFFER_ANDROID       0x3140  /* eglCreateImageKHR target */
#endif

/* Structure layout for android native buffers.
 *
 * Note: this will be harmonized with gralloc_brcm.h.
 */
typedef enum
{
   EGL_BRCM_ANDROID_BUFFER_TYPE_GL_RESOURCE = 0,
   EGL_BRCM_ANDROID_BUFFER_TYPE_MM_RESOURCE,
} EGL_BRCM_ANDROID_BUFFER_TYPE_T;

/* By default Android always define this internally, also due to a missing
** proper pending #define in the Android frameworks/base/opengl/libs/egl/egl.cpp
** module we cannot actually disable EGL_ANDROID_swap_rectangle support via build
** configuration (ie setting 'TARGET_GLOBAL_CPPFLAGS += -DEGL_ANDROID_swap_rectangle=0'
** in our BoardConfig.mk) which would be the preferred mechanism, instead we therefore
** have to match Android behavior and define by default what is expected to be supported,
** as well as provide an implementation for it (which implementation may be empty as
** long as it satisfies Android expectations).
*/
#ifndef EGL_ANDROID_swap_rectangle
#define EGL_ANDROID_swap_rectangle 1
#endif

#if EGL_ANDROID_swap_rectangle
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglSetSwapRectangleANDROID (EGLDisplay dpy, EGLSurface draw, EGLint left, EGLint top, EGLint width, EGLint height);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSETSWAPRECTANGLEANDROIDPROC) (EGLDisplay dpy, EGLSurface draw, EGLint left, EGLint top, EGLint width, EGLint height);
#endif /* EGL_ANDROID_swap_rectangle */

#ifndef EGL_ANDROID_render_buffer
#define EGL_ANDROID_render_buffer 1
#endif

#if EGL_ANDROID_render_buffer
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLClientBuffer EGLAPIENTRY eglGetRenderBufferANDROID (EGLDisplay dpy, EGLSurface sur);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef EGLClientBuffer (EGLAPIENTRYP PFNEGLGETRENDERBUFFERANDROIDPROC) (EGLDisplay dpy, EGLSurface sur);
#endif /* EGL_ANDROID_swap_rectangle */

#ifndef EGL_ANDROID_recordable
#define EGL_ANDROID_recordable   1
#define EGL_RECORDABLE_ANDROID   0x3142
#endif

#ifdef __cplusplus
}
#endif

#endif /* EGLEXT_ANDROID_H */

#endif /* defined(ANDROID) */
