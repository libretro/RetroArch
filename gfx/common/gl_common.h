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

#ifndef __GL_COMMON_H
#define __GL_COMMON_H

#include <boolean.h>
#include <string.h>
#include <libretro.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>
#include <gfx/scaler/scaler.h>
#include <glsym/glsym.h>
#include <formats/image.h>

#include "../video_coord_array.h"
#include "../../retroarch.h"

RETRO_BEGIN_DECLS

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#else
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0
#endif

#if defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3) || defined(HAVE_OPENGLES_3_1) || defined(HAVE_OPENGLES_3_2)
#define RARCH_GL_RENDERBUFFER GL_RENDERBUFFER
#if defined(HAVE_OPENGLES2)
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#else
#define RARCH_GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8
#endif
#define RARCH_GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT
#define RARCH_GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT
#elif (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
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

#if (defined(__MACH__) && (defined(__ppc__) || defined(__ppc64__)))
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_EXT
#elif defined(HAVE_PSGL)
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_OES
#else
#define RARCH_GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE
#endif

#if defined(HAVE_PSGL)
#define glGenerateMipmap glGenerateMipmapOES
#endif

#if defined(__APPLE__) || defined(HAVE_PSGL)
#ifndef GL_RGBA32F
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
#ifndef GL_BGRA8_EXT
#define GL_BGRA8_EXT 0x93A1
#endif
#ifdef IOS
/* Stupid Apple */
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

typedef struct gl gl_t;

struct gl
{
   GLenum internal_fmt;
   GLenum texture_type; /* RGB565 or ARGB */
   GLenum texture_fmt;
   GLenum wrap_mode;

   bool vsync;
   bool tex_mipmap;
   bool fbo_inited;
   bool fbo_feedback_enable;
   bool hw_render_fbo_init;
   bool has_fbo;
   bool hw_render_use;
   bool core_context_in_use;

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   bool support_unpack_row_length;
   bool have_es2_compat;
   bool have_full_npot_support;
   bool have_mipmap;

   bool overlay_enable;
   bool overlay_full_screen;
   bool menu_texture_enable;
   bool menu_texture_full_screen;
   bool have_sync;
   bool pbo_readback_valid[4];
   bool pbo_readback_enable;

   int version_major;
   int version_minor;

   GLuint tex_mag_filter;
   GLuint tex_min_filter;
   GLuint fbo_feedback;
   GLuint fbo_feedback_texture;
   GLuint pbo;
   GLuint *overlay_tex;
   GLuint menu_texture;
   GLuint pbo_readback[4];
   GLuint texture[GFX_MAX_TEXTURES];
   GLuint hw_render_fbo[GFX_MAX_TEXTURES];

#ifdef HAVE_VIDEO_LAYOUT
   bool   video_layout_resize;
   GLuint video_layout_fbo;
   GLuint video_layout_fbo_texture;
   GLuint video_layout_white_texture;
#endif

   unsigned video_width;
   unsigned video_height;

   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   unsigned fbo_feedback_pass;
   unsigned rotation;
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned tex_w;
   unsigned tex_h;
   unsigned base_size; /* 2 or 4 */
   unsigned overlays;
   unsigned pbo_readback_index;
   unsigned last_width[GFX_MAX_TEXTURES];
   unsigned last_height[GFX_MAX_TEXTURES];

   float menu_texture_alpha;

   void *empty_buf;
   void *conv_buffer;
   void *readback_buffer_screenshot;
   const float *vertex_ptr;
   const float *white_color_ptr;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;

   struct video_tex_info tex_info;
   struct scaler_ctx pbo_readback_scaler;
   struct video_viewport vp;
   math_matrix_4x4 mvp, mvp_no_rot;
   struct video_coords coords;
   struct scaler_ctx scaler;
   video_info_t video_info;
   struct video_tex_info prev_info[GFX_MAX_TEXTURES];
   struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];

   const shader_backend_t *shader;
   void *shader_data;
   void *renderchain_data;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
};

static INLINE void gl_bind_texture(GLuint id, GLint wrap_mode, GLint mag_filter,
      GLint min_filter)
{
   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
}

bool gl_query_core_context_in_use(void);

bool gl_load_luts(
      const void *shader_data,
      GLuint *textures_lut);

RETRO_END_DECLS

#endif
