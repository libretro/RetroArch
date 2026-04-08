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
