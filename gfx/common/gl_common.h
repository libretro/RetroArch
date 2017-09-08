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

typedef struct gl
{
   GLenum internal_fmt;
   GLenum texture_type; /* RGB565 or ARGB */
   GLenum texture_fmt;
   GLenum wrap_mode;

   bool vsync;
   bool tex_mipmap;
#ifdef HAVE_FBO
   bool fbo_inited;
   bool fbo_feedback_enable;
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
#ifdef HAVE_OPENGLES
   bool support_unpack_row_length;
#else
   bool have_es2_compat;
#endif
   bool have_full_npot_support;

   bool egl_images;
#ifdef HAVE_OVERLAY
   bool overlay_enable;
   bool overlay_full_screen;
#endif
#ifdef HAVE_MENU
   bool menu_texture_enable;
   bool menu_texture_full_screen;
#endif
#ifdef HAVE_GL_SYNC
   bool have_sync;
#endif
#ifdef HAVE_GL_ASYNC_READBACK
   bool pbo_readback_valid[4];
   bool pbo_readback_enable;
#endif

   int version_major;
   int version_minor;
   int fbo_pass;

   GLuint tex_mag_filter;
   GLuint tex_min_filter;
#ifdef HAVE_FBO
   GLuint fbo_feedback;
   GLuint fbo_feedback_texture;
#endif
   GLuint pbo;
#ifdef HAVE_OVERLAY
   GLuint *overlay_tex;
#endif
#if defined(HAVE_MENU)
   GLuint menu_texture;
#endif
   GLuint vao;
#ifdef HAVE_GL_ASYNC_READBACK
   GLuint pbo_readback[4];
#endif
   GLuint texture[GFX_MAX_TEXTURES];
#ifdef HAVE_FBO
   GLuint fbo[GFX_MAX_SHADERS];
   GLuint fbo_texture[GFX_MAX_SHADERS];
   GLuint hw_render_fbo[GFX_MAX_TEXTURES];
   GLuint hw_render_depth[GFX_MAX_TEXTURES];
#endif

   unsigned tex_index; /* For use with PREV. */
   unsigned textures;
#ifdef HAVE_FBO
   unsigned fbo_feedback_pass;
#endif
   unsigned rotation;
   unsigned vp_out_width;
   unsigned vp_out_height;
   unsigned tex_w;
   unsigned tex_h;
   unsigned base_size; /* 2 or 4 */
#ifdef HAVE_OVERLAY
   unsigned overlays;
#endif
#ifdef HAVE_GL_ASYNC_READBACK
   unsigned pbo_readback_index;
#endif
#ifdef HAVE_GL_SYNC
   unsigned fence_count;
#endif
   unsigned last_width[GFX_MAX_TEXTURES];
   unsigned last_height[GFX_MAX_TEXTURES];

#if defined(HAVE_MENU)
   float menu_texture_alpha;
#endif

   void *empty_buf;
   void *conv_buffer;
   void *readback_buffer_screenshot;
   const float *vertex_ptr;
   const float *white_color_ptr;
#ifdef HAVE_OVERLAY
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;
#endif

   struct video_tex_info tex_info;
#ifdef HAVE_GL_ASYNC_READBACK
   struct scaler_ctx pbo_readback_scaler;
#endif
   struct video_viewport vp;
   math_matrix_4x4 mvp, mvp_no_rot;
   struct video_coords coords;
   struct scaler_ctx scaler;
   video_info_t video_info;
   struct video_tex_info prev_info[GFX_MAX_TEXTURES];
#ifdef HAVE_FBO
   struct video_fbo_rect fbo_rect[GFX_MAX_SHADERS];
   struct gfx_fbo_scale fbo_scale[GFX_MAX_SHADERS];
#endif

#ifdef HAVE_GL_SYNC
   GLsync fences[MAX_FENCES];
#endif
} gl_t;

bool gl_load_luts(const struct video_shader *generic_shader,
      GLuint *lut_textures);

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

RETRO_END_DECLS

#endif
