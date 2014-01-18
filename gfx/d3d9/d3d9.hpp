/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef _XBOX
#define HAVE_WINDOW
#endif

#if defined(_XBOX1)
#ifndef HAVE_D3D8
#define HAVE_D3D8
#endif
#else
#ifndef HAVE_D3D9
#define HAVE_D3D9
#endif
#endif

#include "../../general.h"
#include "../../driver.h"
#include "../shader_parse.h"

#include "../gfx_context.h"
#include "../gfx_common.h"

#ifdef HAVE_CG
#include <Cg/cg.h>
#include <Cg/cgD3D9.h>
#endif
#include "d3d_defines.h"
#include <string>
#include <vector>

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
   LPDIRECT3DTEXTURE tex;
   LPDIRECT3DVERTEXBUFFER vert_buf;
} overlay_t;

void d3d_set_font_rect(void *data, font_params_t *params);
bool d3d_process_shader(void *data);
void d3d_update_title(void *data);
void d3d_recompute_pass_sizes(void *data);
RECT d3d_monitor_rect(void *data);
bool d3d_init_shader(void *data);
void d3d_deinit_shader(void *data);
bool d3d_init_imports(void *data);
bool d3d_init_luts(void *data);
bool d3d_init_singlepass(void *data);
bool d3d_init_multipass(void *data);
bool d3d_init_chain(void *data, const video_info_t *video_info);
void d3d_deinit_chain(void *data);
bool d3d_init_font(void *data);
void d3d_deinit_font(void *data);
void d3d_font_msg(void *data, const char *msg, void *userdata);
void d3d_show_cursor(void *data, bool state);
void d3d_make_d3dpp(void *data, const video_info_t *info, D3DPRESENT_PARAMETERS *d3dpp);
bool d3d_alive_func(void *data);

struct D3DVideo
{
      const gfx_ctx_driver_t *ctx_driver;
      bool should_resize;

#ifdef HAVE_WINDOW
      WNDCLASSEX windowClass;
#endif
      HWND hWnd;
      LPDIRECT3D g_pD3D;
      LPDIRECT3DDEVICE dev;
#ifndef _XBOX
      LPD3DXFONT font;
#endif
      HRESULT d3d_err;
      unsigned cur_mon_id;

      unsigned screen_width;
      unsigned screen_height;
      unsigned dev_rotation;
      D3DVIEWPORT final_viewport;

      std::string cg_shader;

      struct gfx_shader shader;

      video_info_t video_info;

      bool needs_restore;

#ifdef HAVE_CG
      CGcontext cgCtx;
#endif
      RECT font_rect;
      RECT font_rect_shifted;
      uint32_t font_color;

#ifdef HAVE_OVERLAY
      bool overlays_enabled;
      std::vector<overlay_t> overlays;
#endif

#ifdef HAVE_MENU
      overlay_t rgui;
#endif

      RenderChain *chain;
};

#ifndef _XBOX
extern "C" bool dinput_handle_message(void *dinput, UINT message, WPARAM wParam, LPARAM lParam);
#endif

#endif

