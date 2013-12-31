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

#ifndef D3DVIDEO_HPP__
#define D3DVIDEO_HPP__

#include "../../general.h"
#include "../../driver.h"
#include "../shader_parse.h"

#include "../gfx_common.h"

#include <d3d9.h>
#include <d3dx9.h>
#include <d3dx9core.h>

#ifdef HAVE_CG
#include <Cg/cg.h>
#include <Cg/cgD3D9.h>
#endif
#include <string>
#include <vector>
#include <memory>

class RenderChain;

typedef struct
{
   struct Coords
   {
      float x, y, w, h;
   };
   Coords tex_coords;
   Coords vert_coords;
   unsigned tex_w, tex_h;
   bool fullscreen;
   bool enabled;
   float alpha_mod;
   IDirect3DTexture9 *tex;
   IDirect3DVertexBuffer9 *vert_buf;
} overlay_t;

class D3DVideo
{
   public:
      D3DVideo(const video_info_t* info);
      bool frame(const void* frame,
            unsigned width, unsigned height, unsigned pitch,
            const char *msg);
      ~D3DVideo();

      bool alive();
      bool focus() const;
      void set_nonblock_state(bool state);
      void set_rotation(unsigned rot);
      void viewport_info(rarch_viewport &vp);
      bool read_viewport(uint8_t *buffer);
      void resize(unsigned new_width, unsigned new_height);
      bool set_shader(const std::string &path);
      void process_shader();

      void set_filtering(unsigned index, bool smooth);
      void set_font_rect(font_params_t *params);

      void overlay_render(overlay_t &overlay);

#ifdef HAVE_OVERLAY
      bool overlay_load(const texture_image *images, unsigned num_images);
      void overlay_tex_geom(unsigned index, float x, float y, float w, float h);
      void overlay_vertex_geom(unsigned index, float x, float y, float w, float h);
      void overlay_enable(bool state);
      void overlay_full_screen(bool enable);
      void overlay_set_alpha(unsigned index, float mod);
#endif

#ifdef HAVE_MENU
      void set_rgui_texture_frame(const void *frame,
            bool rgb32, unsigned width, unsigned height,
            float alpha);
      void set_rgui_texture_enable(bool state, bool fullscreen);
#endif

      bool restore();
      void render_msg(const char *msg, font_params_t *params = nullptr);

      bool should_resize;
      inline video_info_t& info() { return video_info; }

   private:

      WNDCLASSEX windowClass;
      HWND hWnd;
      IDirect3D9 *g_pD3D;
      IDirect3DDevice9 *dev;
      LPD3DXFONT font;

      void recompute_pass_sizes();
      void calculate_rect(unsigned width, unsigned height, bool keep, float aspect);
      void set_viewport(int x, int y, unsigned width, unsigned height);
      unsigned screen_width;
      unsigned screen_height;
      unsigned rotation;
      D3DVIEWPORT9 final_viewport;

      std::string cg_shader;

      struct gfx_shader shader;

      void process();

      void init(const video_info_t &info);
      void init_base(const video_info_t &info);
      void make_d3dpp(const video_info_t &info, D3DPRESENT_PARAMETERS &d3dpp);
      void deinit();
      RECT monitor_rect();

      video_info_t video_info;

      bool needs_restore;

#ifdef HAVE_CG
      CGcontext cgCtx;
      bool init_cg();
      void deinit_cg();
#endif

      void init_imports();
      void init_luts();
      void init_singlepass();
      void init_multipass();
      bool init_chain(const video_info_t &video_info);
      std::unique_ptr<RenderChain> chain;
      void deinit_chain();

      bool init_font();
      void deinit_font();
      RECT font_rect;
      RECT font_rect_shifted;
      uint32_t font_color;

      void update_title();

#ifdef HAVE_OVERLAY
      bool overlays_enabled;
      std::vector<overlay_t> overlays;
      void free_overlays();
#endif

      void free_overlay(overlay_t &overlay);

#ifdef HAVE_MENU
      overlay_t rgui;
#endif
};

#endif

