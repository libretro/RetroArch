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

class RenderChain
{
   public:
      enum PixelFormat { RGB565, ARGB };

      RenderChain(const video_info_t *video_info,
            LPDIRECT3DDEVICE dev,
#ifdef HAVE_CG
            CGcontext cgCtx,
#endif
            const D3DVIEWPORT &final_viewport);
      ~RenderChain();

      bool init(const LinkInfo &info, PixelFormat fmt);

      bool set_pass_size(unsigned pass, unsigned width, unsigned height);
      void set_final_viewport(const D3DVIEWPORT &final_viewport);
      bool add_pass(const LinkInfo &info);
      bool add_lut(const std::string &id, const std::string &path, bool smooth);
      void add_state_tracker(state_tracker_t *tracker);

      bool render(const void *data,
            unsigned width, unsigned height, unsigned pitch, unsigned rotation);

      static void convert_geometry(const LinkInfo &info,
            unsigned &out_width, unsigned &out_height,
            unsigned width, unsigned height,
            const D3DVIEWPORT &final_viewport);

      void clear();

   private:

      LPDIRECT3DDEVICE dev;
#ifdef HAVE_CG
      CGcontext cgCtx;
#endif
      unsigned pixel_size;

      const video_info_t &video_info;

#define MAX_VARIABLES 64
      state_tracker_t *tracker;
      struct state_tracker_uniform uniform_info[MAX_VARIABLES];
      unsigned uniform_cnt;

      enum { Textures = 8, TexturesMask = Textures - 1 };
      struct
      {
         LPDIRECT3DTEXTURE tex[Textures];
         LPDIRECT3DVERTEXBUFFER vertex_buf[Textures];
         unsigned ptr;
         unsigned last_width[Textures];
         unsigned last_height[Textures];
      } prev;

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
      std::vector<Pass> passes;

#ifdef HAVE_CG
      CGprogram vStock, fStock;
#endif

      struct lut_info
      {
         LPDIRECT3DTEXTURE tex;
         std::string id;
         bool smooth;
      };
      std::vector<lut_info> luts;

      D3DVIEWPORT final_viewport;
      unsigned frame_count;

      bool create_first_pass(const LinkInfo &info, PixelFormat fmt);
#if defined(HAVE_CG)
      bool compile_shaders(CGprogram &fPrg, CGprogram &vPrg, const std::string &shader);
      void set_shaders(CGprogram &fPrg, CGprogram &vPrg);
      void set_cg_mvp(CGprogram &vPrg,
            unsigned vp_width, unsigned vp_height,
            unsigned rotation);
#endif

      void set_vertices(Pass &pass,
            unsigned width, unsigned height,
            unsigned out_width, unsigned out_height,
            unsigned vp_width, unsigned vp_height,
            unsigned rotation);
      void set_viewport(const D3DVIEWPORT &vp);

      void set_cg_params(Pass &pass,
            unsigned input_w, unsigned input_h,
            unsigned tex_w, unsigned tex_h,
            unsigned vp_w, unsigned vp_h);

      void clear_texture(Pass &pass);

      void blit_to_texture(const void *data,
            unsigned width, unsigned height,
            unsigned pitch);

      void render_pass(Pass &pass, unsigned pass_index);
      void log_info(const LinkInfo &info);

      D3DTEXTUREFILTERTYPE translate_filter(enum gfx_filter_type type);
      D3DTEXTUREFILTERTYPE translate_filter(bool smooth);

      void start_render();
      void end_render();

      std::vector<unsigned> bound_tex;
      std::vector<unsigned> bound_vert;
      void bind_luts(Pass &pass);
      void bind_orig(Pass &pass);
      void bind_prev(Pass &pass);
      void bind_pass(Pass &pass, unsigned pass_index);
      void bind_tracker(Pass &pass, unsigned pass_index);
      void unbind_all();

      bool init_fvf(Pass &pass);
};

#endif

