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

#ifndef EGLEXT_BRCM_H
#define EGLEXT_BRCM_H

#ifdef __cplusplus
extern "C" {
#endif

#define EGL_NATIVE_PIXMAP_CLIENT_SIDE_BRCM 0x99930B0	/* eglCreateImageKHR server-side target */
#define EGL_IMAGE_FROM_SURFACE_BRCM 0x99930B1 /* eglCreateImageKHR server-side target */
/* Multimedia target - created by gralloc */
#define EGL_IMAGE_BRCM_MULTIMEDIA         0x99930B2
#define EGL_IMAGE_BRCM_DUPLICATE         0x99930B3 /* a new EGL Image pointing at the same underlying object */
#define EGL_IMAGE_BRCM_RAW_PIXELS        0x99930B4 /* Raw YUV multimedia pixels */

/* Fastpath for creating greyscale textures from a single plane of a
 * MMAL opaque buffers. */
#define EGL_IMAGE_BRCM_MULTIMEDIA_Y      0x99930C0
#define EGL_IMAGE_BRCM_MULTIMEDIA_U      0x99930C1
#define EGL_IMAGE_BRCM_MULTIMEDIA_V      0x99930C2

/* EGL image buffer allocated in shared memory */
#define EGL_IMAGE_BRCM_VCSM              0x99930C3
struct egl_image_brcm_vcsm_info {
   unsigned width;
   unsigned height;
   unsigned int vcsm_handle;
};

#ifndef EGL_BRCM_sane_choose_config
#define EGL_BRCM_sane_choose_config 1
#endif
#if EGL_BRCM_sane_choose_config
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglSaneChooseConfigBRCM(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSANECHOOSECONFIGBRCM)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
#endif

#ifndef EGL_BRCM_global_image
#define EGL_BRCM_global_image 1
#endif
#if EGL_BRCM_global_image
#define EGL_PIXEL_FORMAT_ARGB_8888_PRE_BRCM 0
#define EGL_PIXEL_FORMAT_ARGB_8888_BRCM     1
#define EGL_PIXEL_FORMAT_XRGB_8888_BRCM     2
#define EGL_PIXEL_FORMAT_RGB_565_BRCM       3
#define EGL_PIXEL_FORMAT_A_8_BRCM           4
#define EGL_PIXEL_FORMAT_RENDER_GL_BRCM     (1 << 3)
#define EGL_PIXEL_FORMAT_RENDER_GLES_BRCM   (1 << 4)
#define EGL_PIXEL_FORMAT_RENDER_GLES2_BRCM  (1 << 5)
#define EGL_PIXEL_FORMAT_RENDER_VG_BRCM     (1 << 6)
#define EGL_PIXEL_FORMAT_RENDER_MASK_BRCM   0x78
#define EGL_PIXEL_FORMAT_VG_IMAGE_BRCM      (1 << 7)
#define EGL_PIXEL_FORMAT_GLES_TEXTURE_BRCM  (1 << 8)
#define EGL_PIXEL_FORMAT_GLES2_TEXTURE_BRCM (1 << 9)
#define EGL_PIXEL_FORMAT_TEXTURE_MASK_BRCM  0x380
#define EGL_PIXEL_FORMAT_USAGE_MASK_BRCM    0x3f8
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void EGLAPIENTRY eglCreateGlobalImageBRCM(EGLint width, EGLint height, EGLint pixel_format, const void *data, EGLint data_stride, EGLint *id);
EGLAPI void EGLAPIENTRY eglCreateCopyGlobalImageBRCM(const EGLint *src_id, EGLint *id);
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyGlobalImageBRCM(const EGLint *id);
EGLAPI EGLBoolean EGLAPIENTRY eglQueryGlobalImageBRCM(const EGLint *id, EGLint *width_height_pixel_format);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef void (EGLAPIENTRYP PFNEGLCREATEGLOBALIMAGEBRCMPROC)(EGLint width, EGLint height, EGLint pixel_format, const void *data, EGLint data_stride, EGLint *id);
typedef void (EGLAPIENTRYP PFNEGLCREATECOPYGLOBALIMAGEBRCMPROC)(const EGLint *src_id, EGLint *id);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYGLOBALIMAGEBRCMPROC)(const EGLint *id);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLQUERYGLOBALIMAGEBRCMPROC)(const EGLint *id, EGLint *width_height_pixel_format);
#endif

#ifndef EGL_BRCM_perf_monitor
#define EGL_BRCM_perf_monitor 0
#endif
#if EGL_BRCM_perf_monitor
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglInitPerfMonitorBRCM(EGLDisplay display);
EGLAPI EGLBoolean EGLAPIENTRY eglTermPerfMonitorBRCM(EGLDisplay display);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef void (EGLAPIENTRYP PFNEGLINITPERFMONITORBRCMPROC)(EGLDisplay display);
typedef void (EGLAPIENTRYP PFNEGLTERMPERFMONITORBRCMPROC)(EGLDisplay display);
#endif

#ifndef EGL_BRCM_driver_monitor
#define EGL_BRCM_driver_monitor 1
#endif
#if EGL_BRCM_driver_monitor
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI EGLBoolean EGLAPIENTRY eglInitDriverMonitorBRCM(EGLDisplay display, EGLint hw_bank, EGLint l3c_bank);
EGLAPI void EGLAPIENTRY eglGetDriverMonitorXMLBRCM(EGLDisplay display, EGLint bufSize, EGLint *length, char *xmlStats);
EGLAPI EGLBoolean EGLAPIENTRY eglTermDriverMonitorBRCM(EGLDisplay display);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef void (EGLAPIENTRYP PFNEGLINITDRIVERMONITORBRCMPROC)(EGLDisplay display, EGLint hw_bank, EGLint l3c_bank);
typedef void (EGLAPIENTRYP PFNEGLGETDRIVERMONITORXMLBRCMPROC)(EGLDisplay display, EGLint bufSize, EGLint *length, char *xmlStats);
typedef void (EGLAPIENTRYP PFNEGLTERMDRIVERMONITORBRCMPROC)(EGLDisplay display);
#endif

#ifndef EGL_BRCM_perf_stats
#define EGL_BRCM_perf_stats 0
#endif
#if EGL_BRCM_perf_stats
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void eglPerfStatsResetBRCM(void);
EGLAPI void eglPerfStatsGetBRCM(char *buffer, EGLint buffer_len, EGLBoolean reset);
#endif
typedef void (EGLAPIENTRYP PFNEGLPERFSTATSRESETBRCM) (void);
typedef void (EGLAPIENTRYP PFNEGLPERFSTATSGETBRCM) (char *buffer, EGLint buffer_len, EGLBoolean reset);
#endif

#ifndef EGL_proc_state_valid
#define EGL_proc_state_valid 1
#endif
#if EGL_proc_state_valid
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void EGLAPIENTRY eglProcStateValid( EGLDisplay dpy, EGLBoolean *result );
#endif
typedef void (EGLAPIENTRYP PFNEGLPROCSTATEVALID) ( EGLDisplay dpy, EGLBoolean *valid );
#endif

#ifndef EGL_BRCM_flush
#define EGL_BRCM_flush 1
#endif
#if EGL_BRCM_flush
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void EGLAPIENTRY eglFlushBRCM(void);
#endif /* EGL_EGLEXT_PROTOTYPES */
typedef void (EGLAPIENTRYP PFNEGLFLUSHBRCMPROC)(void);
#endif

#ifndef EGL_BRCM_image_wrap
#define EGL_BRCM_image_wrap 1
#define EGL_IMAGE_WRAP_BRCM 0x9993140
#endif

#ifndef EGL_BRCM_image_wrap_bcg
#define EGL_BRCM_image_wrap_bcg 0
#define EGL_IMAGE_WRAP_BRCM_BCG 0x9993141

#if EGL_BRCM_image_wrap_bcg
typedef struct {
   BEGL_BufferFormat format;

   uint16_t width;
   uint16_t height;

   int32_t stride; /* in bytes */

   void *storage;
} EGL_IMAGE_WRAP_BRCM_BCG_IMAGE_T;
#endif

#endif

#ifndef EGL_BRCM_mem_usage
#define EGL_BRCM_mem_usage 0
#endif
#if EGL_BRCM_mem_usage
#ifdef EGL_EGLEXT_PROTOTYPES
EGLAPI void eglProcessMemUsageGetBRCM(uint32_t id_0, uint32_t id_1, char *buffer, uint32_t buffer_len);
#endif
typedef void (EGLAPIENTRYP PFNEGLPROCESSMEMUSAGEGETBRCM) (uint32_t id_0, uint32_t id_1, char *buffer, uint32_t buffer_len);
#endif

/*
Only enable this #define if the application (or wrapper layer) is going to call eglDirectRenderingPointer
when appropriate (i.e. the first time eglMakeCurrent is called, and then at eglSwapBuffers)

Only used for testing purposes on 2763
*/
//#define DIRECT_RENDERING

#ifdef DIRECT_RENDERING
EGLAPI EGLBoolean EGLAPIENTRY eglDirectRenderingPointer(EGLDisplay dpy, EGLSurface surf, void *image /* KHRN_IMAGE_WRAP_T */);
#endif

#ifdef __cplusplus
}
#endif

#endif
