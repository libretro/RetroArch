/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GL2_COMMON_H
#define __GL2_COMMON_H

#include <boolean.h>
#include <string.h>
#include <libretro.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <gfx/math/matrix_4x4.h>
#include <gfx/scaler/scaler.h>
#include <glsym/glsym.h>
#include <formats/image.h>
#include <retro_atomic.h>

#include "../video_driver.h"
#include "gl_common.h"

/* A software core's frames sampled where it writes them, on KMS */
#if defined(HAVE_KMS) && defined(HAVE_EGL) && !defined(HAVE_PSGL)
#define GL2_KMS_FRAMES
#include "egl_gbm_frames.h"
#endif

/* The hardware ring's per-slot sync, handed from the core's thread to
 * the video thread by exchange so that one thread waits and deletes
 * each. A backend without pointer atomics only serves single-core
 * targets, where a plain swap is the same. */
#ifndef GL_HW_RING_SYNC_T
#if defined(RETRO_ATOMIC_HAS_PTR)
#define GL_HW_RING_SYNC_T retro_atomic_ptr_t
#define GL_HW_RING_SYNC_SWAP(p, v, out) \
   ((out) = retro_atomic_exchange_ptr((p), (v)))
#else
#define GL_HW_RING_SYNC_T void *
#define GL_HW_RING_SYNC_SWAP(p, v, out) ((out) = *(p), *(p) = (v))
#endif
#endif

RETRO_BEGIN_DECLS

typedef struct gl2 gl2_t;

/* How many of each view's copies a preset can read: the newest and
 * up to seven before it */
#define GL2_VIEWS_HISTORY 8

/* A render target views draw into: a texture and its framebuffer */
struct gl2_views_fbo
{
   GLuint fbo;
   GLuint tex;
};

/* A program views draw with, and where its uniforms are */
struct gl2_views_prog
{
   GLuint prog;
   GLint  loc_mvp;
   GLint  loc_tex;
   GLint  loc_clamp;
   GLint  loc_height;
};

/* A window of screens' own: the context's record, which stays put
 * until its destroy, and what the views draw into it with */
struct gl2_window
{
   video_window_native_t native;
   /* As last drawn and published */
   video_views_layout_t  layout;
   /* Anaglyph and interlaced draw both eyes into it first */
   struct gl2_views_fbo  canvas;
   video_views_target_t  canvas_target;
};

enum gl2_flags
{
   GL2_FLAG_TEXTURE_MIPMAP         = (1 <<  0),
   GL2_FLAG_SHOULD_RESIZE          = (1 <<  1),
   GL2_FLAG_HAVE_MIPMAP            = (1 <<  2),
   GL2_FLAG_QUITTING               = (1 <<  3),
   GL2_FLAG_FULLSCREEN             = (1 <<  4),
   GL2_FLAG_KEEP_ASPECT            = (1 <<  5),
   GL2_FLAG_HAVE_FBO               = (1 <<  6),
   GL2_FLAG_HW_RENDER_USE          = (1 <<  7),
   GL2_FLAG_FBO_INITED             = (1 <<  8),
   GL2_FLAG_FBO_FEEDBACK_ENABLE    = (1 <<  9),
   GL2_FLAG_HW_RENDER_FBO_INIT     = (1 << 10),
   GL2_FLAG_SHARED_CONTEXT_USE     = (1 << 11),
   GL2_FLAG_CORE_CONTEXT_IN_USE    = (1 << 12),
   GL2_FLAG_HAVE_SYNC              = (1 << 13),
   GL2_FLAG_HAVE_UNPACK_ROW_LENGTH = (1 << 14),
   GL2_FLAG_HAVE_ES2_COMPAT        = (1 << 15),
   GL2_FLAG_PBO_READBACK_ENABLE    = (1 << 16),
   GL2_FLAG_OVERLAY_ENABLE         = (1 << 17),
   GL2_FLAG_OVERLAY_FULLSCREEN     = (1 << 18),
   GL2_FLAG_MENU_TEXTURE_ENABLE    = (1 << 19),
   GL2_FLAG_MENU_TEXTURE_FULLSCREEN= (1 << 20),
   GL2_FLAG_NONE                   = (1 << 21),
   GL2_FLAG_FRAME_DUPE_LOCK        = (1 << 22),
   /* Latched at init: the wrapper was active and the driver
    * configuration allowed a hw ring when this context came up.
    * core_context_is_mains() reads this instead of consulting the
    * live settings from the frame path (the video thread, with the
    * main thread running free). */
   GL2_FLAG_HW_RING_EXPECTED = (1 << 27),
   /* overlay_tex names belong to the overlay pack (load_textures):
    * not deleted with the page. */
   GL2_FLAG_OVERLAY_BORROWED       = (1 << 28),
   /* GPU recording is on: taken from the frame the frontend hands over,
    * so this thread never reads the recording state the main thread
    * writes (video_frame_info_t::gpu_recording). */
   GL2_FLAG_GPU_RECORDING          = (1 << 24),
   /* What the last frame said context scaling should be:
    * set_aspect_ratio() runs on the video thread under the threaded
    * wrapper, and reading the setting there races the menu writing it. */
   GL2_FLAG_CTX_SCALING            = (1 << 25),
   /* What the last frame said the menu filter should be:
    * set_texture_frame() is applied by the video thread in
    * thread_update_driver_state(), and reading the setting there races
    * the menu writing it. */
   GL2_FLAG_MENU_LINEAR_FILTER     = (1 << 26),
   /* How the passes' textures were last made, by init or set_shader:
    * a frame past the textures makes them again on the video thread,
    * and reading the settings there races the menu writing them. */
   GL2_FLAG_FBO_SMOOTH             = (1 << 29),
   GL2_FLAG_FBO_SRGB_DISABLE       = (1 << 30)
};

struct gl2
{
   /* gfx_display meshes kept on the GPU, by mesh id: vertices, and
    * indices where the mesh has them. GL retires a deleted buffer once
    * nothing in flight reads it, so the one drawn longest ago simply
    * gives way when all are taken. */
   struct
   {
      GLuint vbo;
      GLuint ibo;
      uint64_t last_draw;
      uint32_t id;
   } meshes[8];
   uint64_t mesh_draws;
   const shader_backend_t *shader;
   void *shader_data;
   void *renderchain_data;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
   void *empty_buf;
   void *conv_buffer;
   void *readback_buffer_screenshot;
   const float *vertex_ptr;
   const float *white_color_ptr;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;

   int version_major;
   int version_minor;

   GLuint tex_mag_filter;
   GLuint tex_min_filter;
   GLuint fbo_feedback;
   GLuint fbo_feedback_texture;
   GLuint pbo;
   GLuint *overlay_tex;
   GLuint menu_texture;
   /* Sampled by a quad drawn with no texture: solid colour */
   GLuint white_texture;
   /* Copy of the last presented backbuffer, taken with
    * glCopyTexSubImage2D before the swap of a frame() that asked for it
    * (retain_output), plus the group that frame put on screen for
    * present_last() to replay. GL 1.1 / GLES2, so every context. */
   GLuint retained_texture;
   /* In VIDEO_SCALE_PACK's layout. */
   unsigned retained_dims;
   unsigned retained_light;
   unsigned retained_dark;
   GLuint pbo_readback[4];
   GLuint texture[GFX_MAX_TEXTURES];
   GLuint hw_render_fbo[GFX_MAX_TEXTURES];
   /* The threaded wrapper's hardware ring: the fence the core's thread
    * placed after rendering into each slot's texture, for the frame on
    * this thread to wait before reading it. Sync objects are shared
    * between the two contexts (GL_HW_RING_SYNC_SWAP). */
   GL_HW_RING_SYNC_T hw_ring_sync[3];
   void *hw_ring_done_sync; /* taken ahead of the swap */
   /* Placed after each PBO readback: a slot is read only once its
    * fence has signalled, so mapping it never waits on the GPU. */
   void *pbo_readback_sync[4];

   uint32_t flags;
   /* The threaded wrapper's hardware ring is driving this driver: set
    * and cleared on the core's thread, read on the video thread. Kept
    * apart from flags, whose bits the video thread moves: a bit the
    * core's thread moved in the same word could be lost either way. */
   retro_atomic_int_t hw_ring;

   /* In VIDEO_SCALE_PACK's layout. */
   unsigned video_dims;

   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   unsigned fbo_feedback_pass;
   unsigned rotation;
   /* The viewport the last frame went out at, and the streaming
    * texture's size, both in VIDEO_SCALE_PACK's layout. */
   unsigned out_vp_dims;
   unsigned tex_dims;
   unsigned base_size; /* 2 or 4 */
   unsigned overlays;
   unsigned pbo_readback_index;
   unsigned last_width[GFX_MAX_TEXTURES];
   unsigned last_height[GFX_MAX_TEXTURES];

   float menu_texture_alpha;

   GLenum internal_fmt;
   GLenum texture_type; /* RGB565 or ARGB */
   GLenum texture_fmt;
   GLenum wrap_mode;

   struct scaler_ctx pbo_readback_scaler;
   struct video_viewport vp;                          /* int alignment */
   math_matrix_4x4 mvp, mvp_no_rot;
   struct video_coords coords;                        /* ptr alignment */
   struct scaler_ctx scaler;
   video_info_t video_info;
   struct video_tex_info tex_info;                    /* unsigned int alignment */
   struct video_tex_info prev_info[GFX_MAX_TEXTURES]; /* unsigned alignment */
   struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];   /* unsigned alignment */

   char device_str[128];
#ifdef HAVE_GL_TEXTURE_LEND
   gl_texture_lend_t *lend;
#endif
#ifdef HAVE_GL_FRAME_MEM
   gl_frame_mem_t *frame_mem;
   gl_sw_fb_t sw_fb;
#endif
#ifdef GL2_KMS_FRAMES
   egl_gbm_frames_t *kms_frames;
   /* Could not be made: frames are uploaded */
   bool kms_frames_off;
#endif
#ifdef HAVE_GL_PLANAR
   gl_planar_t planar;
#endif
   bool pbo_readback_valid[4];
   /* At init: textures take half floats (TEXTURE_GPU_FORMAT_RGBA16F) */
   bool fp16_textures;
   /* At init: ARB_sync is there (GL2_FLAG_HAVE_SYNC). Read by the
    * ring's capture on the core's thread, where flags is not read. */
   bool hw_ring_have_sync;
   /* scRGB (FP16) default framebuffer support (Windows/WGL HDR):
    * everything renders into this SDR offscreen and one GLSL 1.20
    * pass encodes it into the FP16 backbuffer at end of frame.
    * All zero / inactive on SDR contexts. */
   struct
   {
      GLuint fbo;
      GLuint tex;
      GLuint program;
      GLint  loc_tex;
      GLint  loc_nits;
      GLint  loc_expand;
      unsigned dims;
      /* Separate layer for the SDR UI when the content is PQ: one
       * encode cannot treat some pixels as Rec.2020 PQ and others as
       * gamma. Mirrors the glcore driver's ui_fbo/ui_tex. */
      GLuint   ui_fbo;
      GLuint   ui_tex;
      GLint    loc_ui_tex;
      GLint    loc_mode;
      GLint    loc_ui_nits;
      GLint    loc_out_pq;
      /* Draws a half-float texture, linear scRGB, into the SDR layer
       * as the encode's inverse (gl2_linear_program) */
      GLuint   lin_program;
      GLint    lin_loc_mvp;
      GLint    lin_loc_tex;
      GLint    lin_loc_params;
      bool     lin_tried;
      bool   active;
      /* At init: the SDR layer can be RGBA16F, so a linear texture
       * keeps what is above menu white (TEXTURE_GPU_FORMAT_SCRGB) */
      bool     fp16_ok;
      /* The layer the UI draws into is RGBA16F */
      bool     tex_fp16;
      /* The layers were last made for an RGBA16F UI layer */
      bool     fp16_made;
      /* The backbuffer is 10-bit Rec.2020 PQ, not FP16 scRGB */
      bool   pq_out;
      /* The HDR settings this frame carried (video_frame_info_t), so the
       * thread that draws never reads what the menu writes */
      float    menu_nits;
      float    paper_white_nits;
      unsigned expand_gamut;
   } scrgb;

   /* Views, drawn straight from the frame as the stock shader draws
    * it, or with a preset through its passes, one view after another.
    * A view's chain is the driver itself. The canvas holds both eyes
    * for anaglyph and interlaced, the UI layer the UI drawn once for
    * both eyes. */
   struct
   {
      video_views_chains_t  chains;
      struct gl2_views_fbo  canvas;
      struct gl2_views_fbo  ui;
      video_views_target_t  canvas_target;
      video_views_target_t  ui_target;
      video_views_ui_dims_t saved;
      struct gl2_views_prog rect;
      struct gl2_views_prog layer;
      struct gl2_views_prog anaglyph;
      struct gl2_views_prog interlaced;
      /* With a preset each view is drawn from a copy of its own, the
       * last few kept for a preset that reads earlier frames: head is
       * the newest, prev what the preset reads as earlier ones. */
      GLuint                tex[RETRO_VIDEO_VIEWS_MAX][GL2_VIEWS_HISTORY];
      unsigned              tex_dims[RETRO_VIDEO_VIEWS_MAX][GL2_VIEWS_HISTORY];
      /* Made for a software frame's view written straight in, as the
       * frame's textures are, not for a copy drawn into it */
      bool                  tex_written[RETRO_VIDEO_VIEWS_MAX][GL2_VIEWS_HISTORY];
      unsigned              head[RETRO_VIDEO_VIEWS_MAX];
      struct video_tex_info prev[RETRO_VIDEO_VIEWS_MAX][GL2_VIEWS_HISTORY];
      /* What a feedback pass drew for each view the frame before */
      struct gl2_views_fbo  feedback[RETRO_VIDEO_VIEWS_MAX];
      unsigned              feedback_dims[RETRO_VIDEO_VIEWS_MAX];
      /* For the view being drawn: its earlier frames and feedback */
      const struct video_tex_info *prev_cur;
      struct video_tex_info feedback_cur;
      GLuint                copy_fbo;
      GLuint                vbo;
      /* For get_flags on the main thread: whether views may be drawn,
       * and whether the last frame with a map was drawn whole. */
      retro_atomic_int_t    allowed;
      retro_atomic_int_t    fallback;
      /* A shader preset is loaded */
      bool                  preset;
      bool                  preset_seen;
      bool                  tried;
      bool                  failed;
      bool                  active;
      /* Drawing into the UI layer */
      bool                  ui_pass;
      bool                  direct_seen;
      bool                  written_seen;
      /* Some copy or feedback target is held */
      bool                  copies;
      /* Each view's prev is filled */
      bool                  prev_live[RETRO_VIDEO_VIEWS_MAX];
   } views;

#ifdef HAVE_VIDEO_WINDOWS
   /* The screens' own windows, by window id; [0] is the main window
    * and stays unused */
   struct
   {
      struct gl2_window     win[RETRO_VIDEO_VIEWS_MAX];
      video_windows_held_t  held;
      /* gl2_frame()'s, for the preset's passes in them */
      unsigned              frame_count;
   } windows;
#endif

#if defined(HAVE_OPENXR) && defined(ANDROID) && defined(HAVE_OPENGLES3)
   /* The framebuffer the headset's tracked eyes are drawn through. */
   GLuint xr_fbo;
#endif
#if defined(HAVE_OPENXR_GL) && !defined(ANDROID)
   /* Desktop headset output, while its host runs; gl2.c's own. */
   struct gl2_desktop_xr *desktop_xr;
#endif
};

bool gl2_load_luts(
      const void *shader_data,
      GLuint *textures_lut);

RETRO_END_DECLS

#endif
