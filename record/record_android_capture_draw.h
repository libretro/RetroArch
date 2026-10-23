/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RECORD_ANDROID_CAPTURE_DRAW_H
#define RECORD_ANDROID_CAPTURE_DRAW_H
#include <GLES3/gl3.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_android_capture_draw record_android_capture_draw_t;
typedef struct record_android_capture_draw_ops
{
   PFNGLCREATESHADERPROC create_shader;
   PFNGLSHADERSOURCEPROC shader_source;
   PFNGLCOMPILESHADERPROC compile_shader;
   PFNGLGETSHADERIVPROC shader_status;
   PFNGLDELETESHADERPROC delete_shader;
   PFNGLCREATEPROGRAMPROC create_program;
   PFNGLATTACHSHADERPROC attach_shader;
   PFNGLLINKPROGRAMPROC link_program;
   PFNGLGETPROGRAMIVPROC program_status;
   PFNGLDELETEPROGRAMPROC delete_program;
   PFNGLGETUNIFORMLOCATIONPROC uniform_location;
   PFNGLGENVERTEXARRAYSPROC gen_arrays;
   PFNGLDELETEVERTEXARRAYSPROC delete_arrays;
   PFNGLBINDVERTEXARRAYPROC bind_array;
   PFNGLUSEPROGRAMPROC use_program;
   PFNGLUNIFORM1IPROC uniform;
   PFNGLACTIVETEXTUREPROC active_texture;
   PFNGLBINDTEXTUREPROC bind_texture;
   PFNGLBINDFRAMEBUFFERPROC bind_framebuffer;
   PFNGLVIEWPORTPROC viewport;
   PFNGLDISABLEPROC disable;
   PFNGLCOLORMASKPROC color_mask;
   PFNGLDRAWARRAYSPROC draw;
   PFNGLGETERRORPROC error;
} record_android_capture_draw_ops_t;
/* Cold setup in the dedicated recording ES3 context. Input modes match Metal:
 * 0 encoded SDR RGB8, 1 encoded PQ/BT.2020 RGB10, 2 linear scRGB RGBA16F at
 * 80 nits/unit. Modes 1/2 require a PQ RGB10 encoder Surface and matching
 * encoder metadata. Texture imports must preserve the native RGB values. */
record_android_capture_draw_t *record_android_capture_draw_new(
      const record_android_capture_draw_ops_t *ops, unsigned width,
      unsigned height, unsigned mode, bool flip_y);
/* Worker, encoder Surface current, readiness dependency already queued.
 * Texture dimensions equal the configured dimensions. One full-size draw,
 * no scaling, CPU readback, tone mapping or PQ re-encoding of PQ input.
 * Even a failed draw may have issued GPU reads: fence it before retirement. */
bool record_android_capture_draw_frame(
      record_android_capture_draw_t *draw, GLuint texture);
/* Context current and all reads retired. After context destruction and GPU
 * retirement instead use abandon=true, which performs no GL calls. */
void record_android_capture_draw_free(
      record_android_capture_draw_t *draw, bool abandon);
RETRO_END_DECLS
#endif
