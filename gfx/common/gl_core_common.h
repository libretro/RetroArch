/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2019 - Hans-Kristian Arntzen
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

#ifndef __GL_CORE_COMMON_H
#define __GL_CORE_COMMON_H

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
#include "../drivers_shader/shader_gl_core.h"

RETRO_BEGIN_DECLS

#define GL_CORE_NUM_TEXTURES 4
#define GL_CORE_NUM_PBOS 4
#define GL_CORE_NUM_VBOS 256
#define GL_CORE_NUM_FENCES 8
struct gl_core_streamed_texture
{
   GLuint tex;
   unsigned width;
   unsigned height;
};

typedef struct gl_core
{
   const gfx_ctx_driver_t *ctx_driver;
   void *ctx_data;
   gl_core_filter_chain_t *filter_chain;

   video_info_t video_info;

   bool vsync;
   bool fullscreen;
   bool quitting;
   bool should_resize;
   bool keep_aspect;
   unsigned version_major;
   unsigned version_minor;

   video_viewport_t vp;
   struct gl_core_viewport filter_chain_vp;
   unsigned vp_out_width;
   unsigned vp_out_height;

   math_matrix_4x4 mvp;
   math_matrix_4x4 mvp_yflip;
   math_matrix_4x4 mvp_no_rot;
   math_matrix_4x4 mvp_no_rot_yflip;
   unsigned rotation;

   GLuint vao;
   struct gl_core_streamed_texture textures[GL_CORE_NUM_TEXTURES];
   unsigned textures_index;

   GLuint menu_texture;
   float menu_texture_alpha;
   bool menu_texture_enable;
   bool menu_texture_full_screen;

   struct
   {
      GLuint alpha_blend;
      GLuint font;
      GLuint ribbon;
      GLuint ribbon_simple;
      GLuint snow_simple;
      GLuint snow;
      GLuint bokeh;
      struct gl_core_buffer_locations alpha_blend_loc;
      struct gl_core_buffer_locations font_loc;
      struct gl_core_buffer_locations ribbon_loc;
      struct gl_core_buffer_locations ribbon_simple_loc;
      struct gl_core_buffer_locations snow_simple_loc;
      struct gl_core_buffer_locations snow_loc;
      struct gl_core_buffer_locations bokeh_loc;
   } pipelines;

   unsigned video_width;
   unsigned video_height;

   GLuint *overlay_tex;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;

   GLuint scratch_vbos[GL_CORE_NUM_VBOS];
   unsigned scratch_vbo_index;

   bool use_shared_context;
   GLuint hw_render_texture;
   GLuint hw_render_fbo;
   GLuint hw_render_rb_ds;
   bool hw_render_enable;
   unsigned hw_render_max_width;
   unsigned hw_render_max_height;
   bool hw_render_bottom_left;

   GLsync fences[GL_CORE_NUM_FENCES];
   unsigned fence_count;

   void *readback_buffer_screenshot;
   struct scaler_ctx pbo_readback_scaler;
   bool pbo_readback_enable;
   unsigned pbo_readback_index;
   bool pbo_readback_valid[GL_CORE_NUM_PBOS];
   GLuint pbo_readback[GL_CORE_NUM_PBOS];
} gl_core_t;

void gl_core_bind_scratch_vbo(gl_core_t *gl, const void *data, size_t size);

RETRO_END_DECLS

#endif
