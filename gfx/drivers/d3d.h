/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __D3DVIDEO_INTF_H__
#define __D3DVIDEO_INTF_H__

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef _XBOX
#define HAVE_WINDOW
#endif

#include "../../defines/d3d_defines.h"

#ifdef _XBOX1
#include <xfont.h>
#endif

#include "../../driver.h"

#include "../font_driver.h"
#include "../video_driver.h"
#include "../common/d3d_common.h"
#ifdef _XBOX
#include "../../defines/xdk_defines.h"
#endif

typedef struct
{
   bool fullscreen;
   bool enabled;
   unsigned tex_w, tex_h;
   float tex_coords[4];
   float vert_coords[4];
   float alpha_mod;
   LPDIRECT3DTEXTURE tex;
   void *vert_buf;
} overlay_t;

typedef struct Vertex
{
   float x, y;
#if defined(HAVE_D3D8)
   float z;
   float rhw;
#endif
   float u, v;
} Vertex;

typedef struct d3d_video
{
   bool keep_aspect;
   bool should_resize;
   bool quitting;
   bool needs_restore;
#ifdef HAVE_OVERLAY
   bool overlays_enabled;
#endif
   /* TODO - refactor this away properly. */
   bool resolution_hd_enable;

   unsigned cur_mon_id;
   unsigned dev_rotation;

   overlay_t *menu;
   const d3d_renderchain_driver_t *renderchain_driver;
   void *renderchain_data;

   RECT font_rect;
   RECT font_rect_shifted;

   struct video_viewport vp;
   struct video_shader shader;
   video_info_t video_info;
   WNDCLASSEX windowClass;
   LPDIRECT3DDEVICE dev;
   D3DVIEWPORT final_viewport;

   char *shader_path;

#ifdef HAVE_OVERLAY
   size_t overlays_size;
   overlay_t *overlays;
#endif
} d3d_video_t;

void d3d_make_d3dpp(void *data,
      const video_info_t *info, D3DPRESENT_PARAMETERS *d3dpp);

#endif

