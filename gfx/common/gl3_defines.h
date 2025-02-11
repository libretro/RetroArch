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

#ifndef __GL3_DEFINES_H
#define __GL3_DEFINES_H

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

#include "../video_driver.h"
#include "../drivers_shader/shader_gl3.h"

RETRO_BEGIN_DECLS

#define GL_CORE_NUM_TEXTURES 4
#define GL_CORE_NUM_PBOS 4
#define GL_CORE_NUM_VBOS 256
#define GL_CORE_NUM_FENCES 8

enum gl3_flags
{
   GL3_FLAG_PBO_READBACK_ENABLE    = (1 <<  0),
   GL3_FLAG_HW_RENDER_BOTTOM_LEFT  = (1 <<  1),
   GL3_FLAG_HW_RENDER_ENABLE       = (1 <<  2),
   GL3_FLAG_USE_SHARED_CONTEXT     = (1 <<  3),
   GL3_FLAG_OVERLAY_ENABLE         = (1 <<  4),
   GL3_FLAG_OVERLAY_FULLSCREEN     = (1 <<  5),
   GL3_FLAG_MENU_TEXTURE_ENABLE    = (1 <<  6),
   GL3_FLAG_MENU_TEXTURE_FULLSCREEN= (1 <<  7),
   GL3_FLAG_VSYNC                  = (1 <<  8),
   GL3_FLAG_FULLSCREEN             = (1 <<  9),
   GL3_FLAG_QUITTING               = (1 << 10),
   GL3_FLAG_SHOULD_RESIZE          = (1 << 11),
   GL3_FLAG_KEEP_ASPECT            = (1 << 12),
   GL3_FLAG_FRAME_DUPE_LOCK        = (1 << 13)
};

struct gl3_streamed_texture
{
   GLuint tex;
   unsigned width;
   unsigned height;
};


typedef struct gl3
{
   const gfx_ctx_driver_t *ctx_driver;
   void *ctx_data;
   gl3_filter_chain_t *filter_chain;
   GLuint *overlay_tex;
   float *overlay_vertex_coord;
   float *overlay_tex_coord;
   float *overlay_color_coord;
   GLsync fences[GL_CORE_NUM_FENCES];
   void *readback_buffer_screenshot;
   struct scaler_ctx pbo_readback_scaler;

   video_info_t video_info;
   video_viewport_t vp;
   struct gl3_viewport filter_chain_vp;
   struct gl3_streamed_texture textures[GL_CORE_NUM_TEXTURES];

   GLuint vao;
   GLuint menu_texture;
   GLuint pbo_readback[GL_CORE_NUM_PBOS];

   struct
   {
      GLuint alpha_blend;
      GLuint font;
      GLuint ribbon;
      GLuint ribbon_simple;
      GLuint snow_simple;
      GLuint snow;
      GLuint bokeh;
      struct gl3_buffer_locations alpha_blend_loc;
      struct gl3_buffer_locations font_loc;
      struct gl3_buffer_locations ribbon_loc;
      struct gl3_buffer_locations ribbon_simple_loc;
      struct gl3_buffer_locations snow_simple_loc;
      struct gl3_buffer_locations snow_loc;
      struct gl3_buffer_locations bokeh_loc;
   } pipelines;


   unsigned video_width;
   unsigned video_height;
   unsigned overlays;
   unsigned version_major;
   unsigned version_minor;
   unsigned out_vp_width;
   unsigned out_vp_height;
   unsigned rotation;
   unsigned textures_index;
   unsigned scratch_vbo_index;
   unsigned fence_count;
   unsigned pbo_readback_index;
   unsigned hw_render_max_width;
   unsigned hw_render_max_height;
   GLuint scratch_vbos[GL_CORE_NUM_VBOS];
   GLuint hw_render_texture;
   GLuint hw_render_fbo;
   GLuint hw_render_rb_ds;

   float menu_texture_alpha;
   math_matrix_4x4 mvp;                /* float alignment */
   math_matrix_4x4 mvp_yflip;
   math_matrix_4x4 mvp_no_rot;
   math_matrix_4x4 mvp_no_rot_yflip;

   uint16_t flags;

   bool pbo_readback_valid[GL_CORE_NUM_PBOS];
} gl3_t;

RETRO_END_DECLS

#endif
