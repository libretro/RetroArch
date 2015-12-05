/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2015 - Daniel De Matteis
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

#ifndef __GL_COMMON_H
#define __GL_COMMON_H

#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <glsym/glsym.h>

#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>
#include <gfx/scaler/scaler.h>
#include <formats/image.h>

#include "../../general.h"
#include "../../verbosity.h"
#include "../font_driver.h"
#include "../video_common.h"
#include "../video_context_driver.h"
#include "../video_shader_driver.h"
#include "../video_shader_parse.h"

#if (!defined(HAVE_OPENGLES) || defined(HAVE_OPENGLES3))
#ifdef GL_PIXEL_PACK_BUFFER
#define HAVE_GL_ASYNC_READBACK
#endif
#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif defined(OSX_PPC)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#else
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
#endif

#if defined(HAVE_OPENGLES2)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#elif defined(OSX_PPC)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_EXT
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER_OES
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_SCE
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_OES
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_OES
#else
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#endif

#ifdef OSX_PPC
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_OES
#else
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE
#endif

#if defined(HAVE_PSGL)
#define glGenerateMipmap glGenerateMipmapOES
#endif

#ifdef HAVE_FBO

#if defined(__APPLE__) || defined(HAVE_PSGL)
#define GL_RGBA32F GL_RGBA32F_ARB
#endif

#endif

#if defined(HAVE_PSGL)
#define RARCH_GL_INTERNAL_FORMAT32 GL_ARGB_SCE
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB5 /* TODO: Verify if this is really 565 or just 555. */
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_RGB5
#elif defined(HAVE_OPENGLES)
/* Imgtec/SGX headers have this missing. */
#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifdef IOS
/* Stupid Apple. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA
#else
#define RARCH_GL_INTERNAL_FORMAT32 GL_BGRA_EXT
#endif
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGB
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA_EXT
#define RARCH_GL_TEXTURE_TYPE16 GL_RGB
#define RARCH_GL_FORMAT32 GL_UNSIGNED_BYTE
#define RARCH_GL_FORMAT16 GL_UNSIGNED_SHORT_5_6_5
#else
/* On desktop, we always use 32-bit. */
#define RARCH_GL_INTERNAL_FORMAT32 GL_RGBA8
#define RARCH_GL_INTERNAL_FORMAT16 GL_RGBA8
#define RARCH_GL_TEXTURE_TYPE32 GL_BGRA
#define RARCH_GL_TEXTURE_TYPE16 GL_BGRA
#define RARCH_GL_FORMAT32 GL_UNSIGNED_INT_8_8_8_8_REV
#define RARCH_GL_FORMAT16 GL_UNSIGNED_INT_8_8_8_8_REV

/* GL_RGB565 internal format isn't in desktop GL 
 * until 4.1 core (ARB_ES2_compatibility).
 * Check for this. */
#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif
#define RARCH_GL_INTERNAL_FORMAT16_565 GL_RGB565
#define RARCH_GL_TEXTURE_TYPE16_565 GL_RGB
#define RARCH_GL_FORMAT16_565 GL_UNSIGNED_SHORT_5_6_5
#endif

/* Platform specific workarounds/hacks. */
#if defined(__CELLOS_LV2__)
#define NO_GL_READ_PIXELS
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#ifndef NO_GL_FF_VERTEX
#define NO_GL_FF_VERTEX
#endif
#endif

#if defined(HAVE_OPENGL_MODERN) || defined(HAVE_OPENGLES2) || defined(HAVE_PSGL)
#ifndef NO_GL_FF_MATRIX
#define NO_GL_FF_MATRIX
#endif
#endif

#if defined(HAVE_OPENGLES2) /* TODO: Figure out exactly what. */
#define NO_GL_CLAMP_TO_BORDER
#endif

#if defined(HAVE_OPENGLES)
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH  0x0CF2
#endif

#ifndef GL_SRGB_ALPHA_EXT
#define GL_SRGB_ALPHA_EXT 0x8C42
#endif
#endif

struct gl_font_renderer;

typedef struct gl
{
   uint64_t frame_count;
   const shader_backend_t *shader;

   bool vsync;
   GLuint texture[GFX_MAX_TEXTURES];
   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   struct gfx_tex_info tex_info;
   struct gfx_tex_info prev_info[GFX_MAX_TEXTURES];
   GLuint tex_mag_filter;
   GLuint tex_min_filter;
   bool tex_mipmap;

   void *empty_buf;

   void *conv_buffer;
   struct scaler_ctx scaler;

#ifdef HAVE_FBO
   /* Render-to-texture, multipass shaders. */
   GLuint fbo[GFX_MAX_SHADERS];
   GLuint fbo_texture[GFX_MAX_SHADERS];
   struct gfx_fbo_rect fbo_rect[GFX_MAX_SHADERS];
   struct gfx_fbo_scale fbo_scale[GFX_MAX_SHADERS];
   int fbo_pass;
   bool fbo_inited;

   bool fbo_feedback_enable;
   unsigned fbo_feedback_pass;
   GLuint fbo_feedback;
   GLuint fbo_feedback_texture;

   GLuint hw_render_fbo[GFX_MAX_TEXTURES];
   GLuint hw_render_depth[GFX_MAX_TEXTURES];
   bool hw_render_fbo_init;
   bool hw_render_depth_init;
   bool has_fp_fbo;
   bool has_srgb_fbo;
   bool has_srgb_fbo_gles3;
#endif
   bool hw_render_use;
   bool shared_context_use;

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   unsigned rotation;

   unsigned full_x, full_y;

   struct video_viewport vp;
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned last_width[GFX_MAX_TEXTURES];
   unsigned last_height[GFX_MAX_TEXTURES];
   unsigned tex_w, tex_h;
   math_matrix_4x4 mvp, mvp_no_rot;

   struct gfx_coords coords;
   const float *vertex_ptr;
   const float *white_color_ptr;

   GLuint pbo;

   GLenum internal_fmt;
   GLenum texture_type; /* RGB565 or ARGB */
   GLenum texture_fmt;
   GLenum wrap_mode;
   unsigned base_size; /* 2 or 4 */
#ifdef HAVE_OPENGLES
   bool support_unpack_row_length;
#else
   bool have_es2_compat;
#endif
   bool have_full_npot_support;

   bool egl_images;
   video_info_t video_info;

#ifdef HAVE_OVERLAY
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
   GLuint *overlay_tex;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;
#endif

#ifdef HAVE_GL_ASYNC_READBACK
   /* PBOs used for asynchronous viewport readbacks. */
   GLuint pbo_readback[4];
   bool pbo_readback_valid[4];
   bool pbo_readback_enable;
   unsigned pbo_readback_index;
   struct scaler_ctx pbo_readback_scaler;
#endif
   void *readback_buffer_screenshot;

#if defined(HAVE_MENU)
   GLuint menu_texture;
   bool menu_texture_enable;
   bool menu_texture_full_screen;
   float menu_texture_alpha;
#endif

#ifdef HAVE_GL_SYNC
#define MAX_FENCES 4
   bool have_sync;
   GLsync fences[MAX_FENCES];
   unsigned fence_count;
#endif

   bool core_context;
   GLuint vao;
} gl_t;

static INLINE void context_bind_hw_render(gl_t *gl, bool enable)
{
   if (!gl)
      return;

   if (gl->shared_context_use)
      gfx_ctx_bind_hw_render(gl, enable);
}

static INLINE bool gl_check_error(void)
{
   int error = glGetError();
   switch (error)
   {
      case GL_INVALID_ENUM:
         RARCH_ERR("GL: Invalid enum.\n");
         break;
      case GL_INVALID_VALUE:
         RARCH_ERR("GL: Invalid value.\n");
         break;
      case GL_INVALID_OPERATION:
         RARCH_ERR("GL: Invalid operation.\n");
         break;
      case GL_OUT_OF_MEMORY:
         RARCH_ERR("GL: Out of memory.\n");
         break;
      case GL_NO_ERROR:
         return true;
   }

   RARCH_ERR("Non specified GL error.\n");
   return false;
}

bool gl_load_luts(const struct video_shader *generic_shader,
      GLuint *lut_textures);

static INLINE unsigned gl_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
#ifndef HAVE_OPENGLES
      case RARCH_WRAP_BORDER:
         return GL_CLAMP_TO_BORDER;
#else
      case RARCH_WRAP_BORDER:
#endif
      case RARCH_WRAP_EDGE:
         return GL_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GL_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GL_MIRRORED_REPEAT;
   }

   return 0;
}

void gl_ff_vertex(const void *data);
void gl_ff_matrix(const void *data);

#endif
