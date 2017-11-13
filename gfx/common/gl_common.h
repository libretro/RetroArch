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
#include "../video_driver.h"
#include "../drivers/gl_symlinks.h"

RETRO_BEGIN_DECLS

#define MAX_FENCES 4

#ifndef ARB_sync
typedef struct __GLsync *GLsync;
#endif

typedef struct gl
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
   bool hw_render_depth_init;
   bool has_fbo;
   bool has_srgb_fbo_gles3;
   bool has_fp_fbo;
   bool has_srgb_fbo;
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

   bool egl_images;
   bool overlay_enable;
   bool overlay_full_screen;
   bool menu_texture_enable;
   bool menu_texture_full_screen;
   bool have_sync;
   bool pbo_readback_valid[4];
   bool pbo_readback_enable;

   int version_major;
   int version_minor;
   int fbo_pass;

   GLuint tex_mag_filter;
   GLuint tex_min_filter;
   GLuint fbo_feedback;
   GLuint fbo_feedback_texture;
   GLuint pbo;
   GLuint *overlay_tex;
   GLuint menu_texture;
   GLuint vao;
   GLuint pbo_readback[4];
   GLuint texture[GFX_MAX_TEXTURES];
   GLuint fbo[GFX_MAX_SHADERS];
   GLuint fbo_texture[GFX_MAX_SHADERS];
   GLuint hw_render_fbo[GFX_MAX_TEXTURES];
   GLuint hw_render_depth[GFX_MAX_TEXTURES];

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
   unsigned fence_count;
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
   struct gfx_fbo_scale fbo_scale[GFX_MAX_SHADERS];

   GLsync fences[MAX_FENCES];
   const gl_renderchain_driver_t *renderchain_driver;
   void *renderchain_data;
} gl_t;

#ifdef NO_GL_FF_VERTEX
#define gl_ff_vertex(coords) ((void)0)
#else
static INLINE void gl_ff_vertex(const struct video_coords *coords)
{
   /* Fall back to fixed function-style if needed and possible. */
   glClientActiveTexture(GL_TEXTURE1);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->lut_tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glClientActiveTexture(GL_TEXTURE0);
   glVertexPointer(2, GL_FLOAT, 0, coords->vertex);
   glEnableClientState(GL_VERTEX_ARRAY);
   glColorPointer(4, GL_FLOAT, 0, coords->color);
   glEnableClientState(GL_COLOR_ARRAY);
   glTexCoordPointer(2, GL_FLOAT, 0, coords->tex_coord);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}
#endif

#ifdef NO_GL_FF_MATRIX
#define gl_ff_matrix(mat) ((void)0)
#else
static INLINE void gl_ff_matrix(const math_matrix_4x4 *mat)
{
   math_matrix_4x4 ident;

   /* Fall back to fixed function-style if needed and possible. */
   glMatrixMode(GL_PROJECTION);
   glLoadMatrixf(mat->data);
   glMatrixMode(GL_MODELVIEW);
   matrix_4x4_identity(ident);
   glLoadMatrixf(ident.data);
}
#endif

static INLINE void gl_bind_texture(GLuint id, GLint wrap_mode, GLint mag_filter,
      GLint min_filter)
{
   glBindTexture(GL_TEXTURE_2D, id);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
}

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

void gl_load_texture_image(GLenum target,
      GLint level,
      GLint internalFormat,
      GLsizei width,
      GLsizei height,
      GLint border,
      GLenum format,
      GLenum type,
      const GLvoid * data);

void gl_load_texture_data(
      uint32_t id_data,
      enum gfx_wrap_type wrap_type,
      enum texture_filter_type filter_type,
      unsigned alignment,
      unsigned width, unsigned height,
      const void *frame, unsigned base_size);

RETRO_END_DECLS

#endif
