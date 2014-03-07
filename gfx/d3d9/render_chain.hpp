/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef RENDER_CHAIN_HPP__
#define RENDER_CHAIN_HPP__

#include "d3d9.hpp"
#include "../state_tracker.h"
#include "../shader_parse.h"
#include <map>
#include <utility>

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
   struct gfx_shader_pass *pass;
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

struct lut_info
{
   LPDIRECT3DTEXTURE tex;
   std::string id;
   bool smooth;
};

typedef struct renderchain
{
   LPDIRECT3DDEVICE dev;
#ifdef HAVE_CG
   CGcontext cgCtx;
#endif
   unsigned pixel_size;
   const video_info_t *video_info;
   state_tracker_t *tracker;
   struct state_tracker_uniform uniform_info[MAX_VARIABLES];
   unsigned uniform_cnt;
   struct
   {
      LPDIRECT3DTEXTURE tex[TEXTURES];
      LPDIRECT3DVERTEXBUFFER vertex_buf[TEXTURES];
      unsigned ptr;
      unsigned last_width[TEXTURES];
      unsigned last_height[TEXTURES];
   } prev;
   std::vector<Pass> passes;
#ifdef HAVE_CG
   CGprogram vStock, fStock;
#endif
   std::vector<lut_info> luts;
   D3DVIEWPORT *final_viewport;
   unsigned frame_count;
   std::vector<unsigned> bound_tex;
   std::vector<unsigned> bound_vert;
} renderchain_t;

void renderchain_free(void *data);
bool renderchain_init(void *data, const video_info_t *video_info,
      LPDIRECT3DDEVICE dev_,
      CGcontext cgCtx_,
      const D3DVIEWPORT *final_viewport_,
      const LinkInfo *info,
      PixelFormat fmt);
void renderchain_clear(void *data);
void renderchain_set_final_viewport(void *data, const D3DVIEWPORT *final_viewport);
bool renderchain_set_pass_size(void *data, unsigned pass_index, unsigned width, unsigned height);
bool renderchain_add_pass(void *data, const LinkInfo *info);
bool renderchain_add_lut(void *data, const std::string &id,
      const std::string &path,
      bool smooth);
void renderchain_add_state_tracker(void *data, state_tracker_t *tracker);
void renderchain_start_render(void *data);
void renderchain_end_render(void *data);
bool renderchain_render(void *chain_data, const void *data,
      unsigned width, unsigned height, unsigned pitch, unsigned rotation);
D3DTEXTUREFILTERTYPE renderchain_translate_filter(enum gfx_filter_type type);
D3DTEXTUREFILTERTYPE renderchain_translate_filter(bool smooth);
bool renderchain_create_first_pass(void *data, const LinkInfo *info, PixelFormat fmt);
void renderchain_set_vertices(void *data, Pass &pass,
      unsigned width, unsigned height,
      unsigned out_width, unsigned out_height,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation);
void renderchain_set_viewport(void *data, D3DVIEWPORT *vp);
void renderchain_set_mvp(void *data, CGprogram &vPrg,
      unsigned vp_width, unsigned vp_height,
      unsigned rotation);
void renderchain_clear_texture(void *data, Pass &pass);
void renderchain_convert_geometry(void *data, const LinkInfo *info,
      unsigned &out_width, unsigned &out_height,
      unsigned width, unsigned height,
      D3DVIEWPORT *final_viewport);
void renderchain_blit_to_texture(void *data, const void *frame,
      unsigned width, unsigned height,
      unsigned pitch);
void renderchain_render_pass(void *data, Pass &pass, unsigned pass_index);
void renderchain_log_info(void *data, const LinkInfo *info);
void renderchain_unbind_all(void *data);

bool renderchain_compile_shaders(void *data, CGprogram &fPrg, CGprogram &vPrg, const std::string &shader);
void renderchain_set_shaders(void *data, CGprogram &fPrg, CGprogram &vPrg);
void renderchain_destroy_stock_shader(void *data);
void renderchain_destroy_shader(void *data, int i);
void renderchain_set_shader_mvp(void *data, CGprogram &vPrg, D3DXMATRIX &tmp);
void renderchain_set_shader_params(void *data, Pass &pass,
            unsigned video_w, unsigned video_h,
            unsigned tex_w, unsigned tex_h,
            unsigned viewport_w, unsigned viewport_h);
void renderchain_bind_tracker(void *data, Pass &pass, unsigned pass_index);
bool renderchain_init_shader_fvf(void *data, Pass &pass);
void renderchain_bind_orig(void *data, Pass &pass);
void renderchain_bind_prev(void *data, Pass &pass);
void renderchain_bind_luts(void *data, Pass &pass);
void renderchain_bind_pass(void *data, Pass &pass, unsigned pass_index);

#endif

