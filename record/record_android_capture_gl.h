/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_GL_H
#define RECORD_ANDROID_CAPTURE_GL_H
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_android_capture_gl record_android_capture_gl_t;
typedef void(GL_APIENTRYP record_capture_gl_image_target_t)(
      GLenum target, void *image);
typedef struct record_android_capture_gl_ops
{
   PFNEGLQUERYSTRINGPROC query_string;
   PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC native_buffer;
   PFNEGLCREATEIMAGEKHRPROC create_image;
   PFNEGLDESTROYIMAGEKHRPROC destroy_image;
   PFNEGLCREATESYNCKHRPROC create_sync;
   PFNEGLDESTROYSYNCKHRPROC destroy_sync;
   PFNEGLWAITSYNCKHRPROC wait_sync;
   PFNGLGENTEXTURESPROC gen_textures;
   PFNGLDELETETEXTURESPROC delete_textures;
   PFNGLBINDTEXTUREPROC bind_texture;
   PFNGLTEXPARAMETERIPROC texture_parameter;
   record_capture_gl_image_target_t image_target;
   PFNGLGETERRORPROC error;
   PFNGLGETSTRINGPROC string;
   PFNGLFENCESYNCPROC fence;
   PFNGLCLIENTWAITSYNCPROC poll;
   PFNGLDELETESYNCPROC delete_fence;
   PFNGLFLUSHPROC flush;
} record_android_capture_gl_ops_t;
/* Cold worker setup, recording ES3 context current on display. Requires a
 * clean GL error state and dedicated context texture state. Buffers remain
 * borrowed until free. Imports fixed RGB slots; no pixel conversion. On
 * failure, a non-NULL output retains cleanup ownership for free retry. */
bool record_android_capture_gl_new(EGLDisplay display,
      const record_android_capture_gl_ops_t *ops,
      struct AHardwareBuffer *const *buffers, unsigned count,
      record_android_capture_gl_t **output);
/* Worker context current. Success consumes fd (including already-ready -1),
 * queues a server wait and lends the texture until end. On failure fd is
 * untouched unless native import was attempted, which consumes it even on
 * failure. Never issue draws after a failed begin. */
bool record_android_capture_gl_begin(record_android_capture_gl_t *capture,
      unsigned slot, int *owned_fd, GLuint *texture);
/* After the last draw reading this slot, before restoring the context.
 * Inserts a completion fence and flushes; does not wait for the GPU. */
bool record_android_capture_gl_end(
      record_android_capture_gl_t *capture, unsigned slot);
/* One zero-timeout query. 1 permits producer reuse, 0 pending, -1 error.
 * A failed end retains the slot until terminal recovery or independent
 * context retirement proves completion. */
int record_android_capture_gl_poll(
      record_android_capture_gl_t *capture, unsigned slot);
/* Terminal worker recovery, same context current, no more source draws.
 * A fresh fence covers every preceding command, including failed slot fences.
 * One creation/flush or one zero-timeout query per call. 1 proves all reads
 * retired, 0 pending, -1 retains unproven reads. No frame is drawn again. */
int record_android_capture_gl_recover_step(
      record_android_capture_gl_t *capture);
/* Worker context current, every begun slot retired. Refuses otherwise.
 * Native image destruction failures retain remaining ownership for retry. */
bool record_android_capture_gl_free(record_android_capture_gl_t *capture);
/* ONLY after context/owned GL namespace destruction AND independent proof all
 * GPU reads retired (or device loss). Shared objects must also be retired.
 * Context destruction alone is not completion proof. No GL
 * calls follow; explicit EGL images/syncs still require free. */
void record_android_capture_gl_after_context_retired(
      record_android_capture_gl_t *capture);
RETRO_END_DECLS
#endif
