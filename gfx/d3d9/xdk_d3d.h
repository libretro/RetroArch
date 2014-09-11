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

#ifndef _XDK_D3D_VIDEO_H
#define _XDK_D3D_VIDEO_H

#include <stdint.h>
#ifdef _XBOX1
#include <xfont.h>
#endif
#include "d3d_defines.h"
#include "../../gfx/shader_common.h"
#include "../../gfx/shader_parse.h"
#include "../../gfx/image/image.h"
#include "../../gfx/fonts/d3d_font.h"

#include "../../gfx/gfx_context.h"
#include "../../gfx/d3d9/xdk_defines.h"

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

typedef struct Vertex
{
   float x, y;
#if defined(_XBOX1)
   float z;
   float rhw;
#endif
   float u, v;
} Vertex;

typedef struct gl_shader_backend gl_shader_backend_t;

typedef struct d3d_video
{
   const d3d_font_renderer_t *font_ctx;
   const gfx_ctx_driver_t *ctx_driver;
   const gl_shader_backend_t *shader;
   bool should_resize;
   bool quitting;
   bool vsync;
   bool needs_restore;
   bool overlays_enabled;
   unsigned screen_width;
   unsigned screen_height;
   unsigned dev_rotation;
   HWND hWnd;
   LPDIRECT3D g_pD3D;
   LPDIRECT3DDEVICE dev;
#ifndef _XBOX
   LPD3DXFONT font;
#endif
#ifdef HAVE_D3D9
   LPDIRECT3DSURFACE lpSurface;
   LPDIRECT3DTEXTURE lpTexture_ot_as16srgb;
   LPDIRECT3DTEXTURE lpTexture_ot;
#endif
#ifdef HAVE_MENU
   bool menu_texture_enable;
   bool menu_texture_full_screen;
#endif
   D3DVIEWPORT final_viewport;
   video_info_t video_info;
   HRESULT d3d_err;
   unsigned cur_mon_id;

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

#ifdef HAVE_MENU
   overlay_t *menu;
#endif
} d3d_video_t;

#include "d3d_shared.h"

extern void d3d_make_d3dpp(void *data, const video_info_t *info, D3DPRESENT_PARAMETERS *d3dpp);
extern bool texture_image_render(struct texture_image *out_img);

#endif
