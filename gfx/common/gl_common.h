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

#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>
#include <gfx/scaler/scaler.h>
#include <formats/image.h>

#include "../../verbosity.h"
#include "../font_driver.h"
#include "../video_coord_array.h"
#include "../video_context_driver.h"
#include "../drivers/gl_symlinks.h"

typedef struct gl
{
   int version_major;
   int version_minor;

   bool vsync;
   GLuint texture[GFX_MAX_TEXTURES];
   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
   struct video_tex_info tex_info;
   struct video_tex_info prev_info[GFX_MAX_TEXTURES];
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
   struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];
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
   bool has_srgb_fbo_gles3;
#endif
   bool has_fp_fbo;
   bool has_srgb_fbo;
   bool hw_render_use;

   bool should_resize;
   bool quitting;
   bool fullscreen;
   bool keep_aspect;
   unsigned rotation;

   struct video_viewport vp;
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned last_width[GFX_MAX_TEXTURES];
   unsigned last_height[GFX_MAX_TEXTURES];
   unsigned tex_w, tex_h;
   math_matrix_4x4 mvp, mvp_no_rot;

   struct video_coords coords;
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

   GLuint vao;
} gl_t;

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

bool gl_query_core_context_in_use(void);
void gl_ff_vertex(const struct video_coords *coords);
void gl_ff_matrix(const math_matrix_4x4 *mat);
void gl_load_texture_image(GLenum target,
      GLint level,
      GLint internalFormat,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLenum format,
      GLenum type,
      const GLvoid * data);

#endif
