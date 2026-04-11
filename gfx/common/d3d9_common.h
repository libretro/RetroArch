/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _D3D9_COMMON_H
#define _D3D9_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d9.h>

#include "d3d_common.h"
#include "../../verbosity.h"

#ifdef _XBOX
#define D3D9_RGB565_FORMAT D3DFMT_LIN_R5G6B5
#define D3D9_ARGB8888_FORMAT D3DFMT_LIN_A8R8G8B8
#define D3D9_XRGB8888_FORMAT D3DFMT_LIN_X8R8G8B8
#else
#define D3D9_RGB565_FORMAT D3DFMT_R5G6B5
#define D3D9_ARGB8888_FORMAT D3DFMT_A8R8G8B8
#define D3D9_XRGB8888_FORMAT D3DFMT_X8R8G8B8
#endif

RETRO_BEGIN_DECLS

typedef struct d3d9_video
{
   overlay_t *menu;
   void *renderchain_data;

   char *shader_path;
   overlay_t *overlays;

   RECT font_rect;
   RECT font_rect_shifted;
   math_matrix_4x4 mvp;
   math_matrix_4x4 mvp_rotate;
   math_matrix_4x4 mvp_transposed;

   struct video_viewport vp;
   struct video_shader shader;
   video_info_t video_info;
#ifdef HAVE_WINDOW
   WNDCLASSEX windowClass;
#endif
   LPDIRECT3DDEVICE9 dev;
   D3DVIEWPORT9 out_vp;
   float translate_x;
   float translate_y;

   struct
   {
      int size;
      int offset;
      void *buffer;
      void *decl;
   }menu_display;

   size_t overlays_size;

   unsigned cur_mon_id;
   unsigned dev_rotation;

   bool keep_aspect;
   bool should_resize;
   bool quitting;
   bool needs_restore;
   bool overlays_enabled;
   /* TODO - refactor this away properly. */
   bool resolution_hd_enable;

   /* Only used for Xbox */
   bool widescreen_mode;
} d3d9_video_t;

bool d3d9_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d9_reset(void *dev, void *d3dpp);

void *d3d9_create(void);

bool d3d9_initialize_symbols(enum gfx_ctx_api api);

void d3d9_deinitialize_symbols(void);

void d3d9_make_d3dpp(d3d9_video_t *d3d,
      const video_info_t *info, void *_d3dpp);

extern LPDIRECT3D9 g_pD3D9;

RETRO_END_DECLS

#endif
