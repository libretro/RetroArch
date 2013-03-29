/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
#include <map>
#include <utility>
#include <memory>

struct Vertex
{
   float x, y, z;
   float u, v;
   float lut_u, lut_v;
   float r, g, b, a;
};

struct LinkInfo
{
   enum ScaleType { Relative, Absolute, Viewport };

   unsigned tex_w, tex_h;

   float scale_x, scale_y;
   unsigned abs_x, abs_y;
   bool filter_linear;
   ScaleType scale_type_x, scale_type_y;

   unsigned frame_count_mod;
   bool float_framebuffer;

   std::string shader_path;
};

class RenderChain
{
   public:
      enum PixelFormat { RGB565, ARGB };

      RenderChain(const video_info_t &video_info,
            IDirect3DDevice9 *dev,
#ifdef HAVE_CG
            CGcontext cgCtx,
#endif
            const LinkInfo &info,
            PixelFormat fmt,
            const D3DVIEWPORT9 &final_viewport);

      void add_pass(const LinkInfo &info);
      void add_lut(const std::string &id, const std::string &path, bool smooth);
      void add_state_tracker(std::shared_ptr<state_tracker_t> tracker);

      bool render(const void *data,
            unsigned width, unsigned height, unsigned pitch, unsigned rotation);

      static void convert_geometry(const LinkInfo &info,
            unsigned &out_width, unsigned &out_height,
            unsigned width, unsigned height,
            const D3DVIEWPORT9 &final_viewport);

      void clear();
      ~RenderChain();

   private:

      IDirect3DDevice9 *dev;
#ifdef HAVE_CG
      CGcontext cgCtx;
#endif
      unsigned pixel_size;

      const video_info_t &video_info;

#define MAX_VARIABLES 64
      std::shared_ptr<state_tracker_t> tracker;
      struct state_tracker_uniform uniform_info[MAX_VARIABLES];
      unsigned uniform_cnt;

      enum { Textures = 8, TexturesMask = Textures - 1 };
      struct
      {
         IDirect3DTexture9 *tex[Textures];
         IDirect3DVertexBuffer9 *vertex_buf[Textures];
         unsigned ptr;
         unsigned last_width[Textures];
         unsigned last_height[Textures];
      } prev;

      struct Pass
      {
         LinkInfo info;
         IDirect3DTexture9 *tex;
         IDirect3DVertexBuffer9 *vertex_buf;
#ifdef HAVE_CG
         CGprogram vPrg, fPrg;
#endif
         unsigned last_width, last_height;

         IDirect3DVertexDeclaration9 *vertex_decl;
         std::vector<unsigned> attrib_map;
      };
      std::vector<Pass> passes;

      struct lut_info
      {
         IDirect3DTexture9 *tex;
         std::string id;
         bool smooth;
      };
      std::vector<lut_info> luts;

      D3DVIEWPORT9 final_viewport;
      unsigned frame_count;

      void create_first_pass(const LinkInfo &info, PixelFormat fmt);
      void compile_shaders(Pass &pass, const std::string &shader);

      void set_vertices(Pass &pass,
            unsigned width, unsigned height,
            unsigned out_width, unsigned out_height,
            unsigned vp_width, unsigned vp_height,
            unsigned rotation);
      void set_viewport(const D3DVIEWPORT9 &vp);

      void set_shaders(Pass &pass);
      void set_cg_mvp(Pass &pass, const D3DXMATRIX &matrix);
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

      void init_fvf(Pass &pass);
};

#endif

