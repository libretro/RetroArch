/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __D3D_RENDER_CHAIN_H
#define __D3D_RENDER_CHAIN_H

#include "d3d.h"
#include "../video_state_tracker.h"
#include "../video_shader_parse.h"
#include "../inc/Cg/cg.h"

struct Vertex
{
   float x, y, z;
   float u, v;
   float lut_u, lut_v;
   float r, g, b, a;
};

struct LinkInfo
{
   unsigned tex_w, tex_h;
   struct video_shader_pass *pass;
};

enum PixelFormat
{
   RGB565 = 0,
   ARGB
};

#define MAX_VARIABLES 64

enum
{
   TEXTURES = 8,
   TEXTURESMASK = TEXTURES - 1
};

struct Pass
{
   LinkInfo info;
   LPDIRECT3DTEXTURE tex;
   LPDIRECT3DVERTEXBUFFER vertex_buf;
#ifdef HAVE_CG
   CGprogram vPrg, fPrg;
#endif
   unsigned last_width, last_height;
#ifdef HAVE_D3D9
   LPDIRECT3DVERTEXDECLARATION vertex_decl;
#endif
   std::vector<unsigned> attrib_map;
};

void renderchain_free(void *data);

void *renderchain_new(void);

void renderchain_deinit(void *data);

bool renderchain_init(void *data, const video_info_t *video_info,
      void *dev_,
      void *shader_data,
      const void *final_viewport_,
      const void *info_data,
      PixelFormat fmt);

void renderchain_clear(void *data);

void renderchain_set_final_viewport(void *data, const void *viewport_data);

bool renderchain_set_pass_size(void *data, unsigned pass_index,
      unsigned width, unsigned height);

bool renderchain_add_pass(void *data, const void *info_data);

bool renderchain_add_lut(void *data,
      const char *id, const char *path,
      bool smooth);

void renderchain_add_state_tracker(void *data, void *tracker_data);

void renderchain_start_render(void *data);

void renderchain_end_render(void *data);

bool renderchain_render(void *chain_data, const void *data,
      unsigned width, unsigned height, unsigned pitch, unsigned rotation);

bool renderchain_create_first_pass(void *data,
      const void *info_data, PixelFormat fmt);

void renderchain_set_vertices(
	  void *data, void *pass_data,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation);

void renderchain_set_viewport(void *data, void *viewport_data);

void renderchain_set_mvp(void *data, void *vertex_program,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation);

void renderchain_convert_geometry(void *data, const void *info_data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height,
      D3DVIEWPORT *final_viewport);

void renderchain_blit_to_texture(void *data, const void *frame,
      unsigned width, unsigned height,
      unsigned pitch);

void renderchain_render_pass(void *data, void *pass_data, unsigned pass_index);

void renderchain_log_info(void *data, const void *info_data);

void renderchain_unbind_all(void *data);

bool renderchain_compile_shaders(void *data, void *fragment_data,
      void *vertex_data, const std::string &shader);

void renderchain_set_shaders(void *data, void *fragment_data, void *vertex_data);

void renderchain_destroy_stock_shader(void *data);

void renderchain_destroy_shader(void *data, int i);

void renderchain_set_shader_mvp(void *data, void *shader_data, void *matrix_data);

void renderchain_set_shader_params(void *data, void *pass_data,
            unsigned video_w, unsigned video_h,
            unsigned tex_w, unsigned tex_h,
            unsigned viewport_w, unsigned viewport_h);

void renderchain_bind_tracker(void *data, void *pass_data, unsigned pass_index);

bool renderchain_init_shader_fvf(void *data, void *pass_data);

void renderchain_bind_orig(void *data, void *pass_data);

void renderchain_bind_prev(void *data, void *pass_data);

void renderchain_bind_luts(void *data, void *pass_data);

void renderchain_bind_pass(void *data, void *pass_data, unsigned pass_index);

#endif

