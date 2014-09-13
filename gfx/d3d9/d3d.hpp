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

#ifdef _XBOX1
#include <xfont.h>
#endif

#include "../../general.h"
#include "../../driver.h"

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
#include "../shader_parse.h"
#include "../shader_common.h"
#endif

#include "../fonts/d3d_font.h"
#include "../gfx_context.h"
#include "../gfx_common.h"

#ifdef HAVE_CG
#include <Cg/cg.h>
#include <Cg/cgD3D9.h>
#endif
#include "d3d_wrapper.h"
#include <string>
#include <vector>

#ifdef HAVE_OVERLAY
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
#endif

#ifdef _XBOX
typedef struct Vertex
{
   float x, y;
#if defined(_XBOX1)
   float z;
   float rhw;
#endif
   float u, v;
} Vertex;
#endif

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#ifdef _XBOX
typedef struct gl_shader_backend gl_shader_backend_t;
#endif
#endif

void d3d_make_d3dpp(void *data, const video_info_t *info, D3DPRESENT_PARAMETERS *d3dpp);

typedef struct d3d_video
{
      const d3d_font_renderer_t *font_ctx;
      const gfx_ctx_driver_t *ctx_driver;
#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#ifdef _XBOX
      const gl_shader_backend_t *shader;
#endif
#endif
      bool should_resize;
      bool quitting;

#ifdef HAVE_WINDOW
      WNDCLASSEX windowClass;
#endif
      HWND hWnd;
      LPDIRECT3D g_pD3D;
      LPDIRECT3DDEVICE dev;
#ifndef _XBOX
      LPD3DXFONT font;
#endif
#if defined(HAVE_D3D9) && defined(_XBOX)
   LPDIRECT3DSURFACE lpSurface;
   LPDIRECT3DTEXTURE lpTexture_ot_as16srgb;
   LPDIRECT3DTEXTURE lpTexture_ot;
#endif
      HRESULT d3d_err;
      unsigned cur_mon_id;

      unsigned screen_width;
      unsigned screen_height;
      unsigned dev_rotation;
      D3DVIEWPORT final_viewport;

      std::string cg_shader;

#ifndef _XBOX
      struct gfx_shader shader;
#endif

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

      bool menu_texture_enable;
      bool menu_texture_full_screen;
#if defined(HAVE_MENU) && defined(HAVE_OVERLAY)
      overlay_t *menu;
#endif
      void *chain;

#ifdef _XBOX
      /* TODO _ should all be refactored */
      // RENDERCHAIN PASS
      unsigned pixel_size;
      LPDIRECT3DTEXTURE tex;
      LPDIRECT3DVERTEXBUFFER vertex_buf;
      unsigned last_width;
      unsigned last_height;
#ifdef HAVE_D3D9
      LPDIRECT3DVERTEXDECLARATION vertex_decl;
#endif
      // RENDERCHAIN PASS -> INFO
      unsigned tex_w;
      unsigned tex_h;
#endif
#ifdef _XBOX
	  bool vsync;
#endif
} d3d_video_t;

#ifndef _XBOX
extern "C" bool dinput_handle_message(void *dinput, UINT message, WPARAM wParam, LPARAM lParam);
#endif

#endif

