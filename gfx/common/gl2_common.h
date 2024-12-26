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

#include "../video_driver.h"

RETRO_BEGIN_DECLS

#define GL2_BIND_TEXTURE(id, wrap_mode, mag_filter, min_filter) \
   glBindTexture(GL_TEXTURE_2D, id); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter); \
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter)

#if defined(HAVE_PSGL)
#define RARCH_GL_FRAMEBUFFER GL_FRAMEBUFFER_OES
#define RARCH_GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_OES
#define RARCH_GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#elif (defined(__MACH__)  && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200))
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
#elif (defined(__MACH__) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200))
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

#if (defined(__MACH__) && defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED < 101200))
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

typedef struct gl2 gl2_t;

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
   GL2_FLAG_FRAME_DUPE_LOCK        = (1 << 22)
};

struct gl2
{
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
   GLuint pbo_readback[4];
   GLuint texture[GFX_MAX_TEXTURES];
   GLuint hw_render_fbo[GFX_MAX_TEXTURES];

   uint32_t flags;

   unsigned video_width;
   unsigned video_height;

   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   unsigned fbo_feedback_pass;
   unsigned rotation;
   unsigned out_vp_width;
   unsigned out_vp_height;
   unsigned tex_w;
   unsigned tex_h;
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
   bool pbo_readback_valid[4];
};

bool gl2_load_luts(
      const void *shader_data,
      GLuint *textures_lut);

RETRO_END_DECLS

#endif
