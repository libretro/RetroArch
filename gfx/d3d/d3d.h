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

#ifndef __D3DVIDEO_INTF_H__
#define __D3DVIDEO_INTF_H__

#include <string>
#include <vector>

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

#include "../../defines/d3d_defines.h"

#ifdef _XBOX1
#include <xfont.h>
#endif

#include "../../general.h"
#include "../../driver.h"

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_HLSL)
#include "../video_shader_driver.h"
#endif

#include "../font_driver.h"
#include "../video_context_driver.h"
#include "../common/d3d_common.h"
#include "render_chain_driver.h"
#ifdef _XBOX
#include "../../defines/xdk_defines.h"
#endif

typedef struct
{
   float tex_coords[4];
   float vert_coords[4];
   unsigned tex_w, tex_h;
   bool fullscreen;
   bool enabled;
   float alpha_mod;
   LPDIRECT3DTEXTURE tex;
#ifdef HAVE_D3D9
   LPDIRECT3DVERTEXBUFFER vert_buf;
#endif
} overlay_t;

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

typedef struct d3d_video
{
   uint64_t frame_count;
   bool keep_aspect;
   bool should_resize;
   bool quitting;

   struct video_viewport vp;
   WNDCLASSEX windowClass;
   LPDIRECT3D g_pD3D;
   LPDIRECT3DDEVICE dev;
   HRESULT d3d_err;
   unsigned cur_mon_id;
   unsigned dev_rotation;
   D3DVIEWPORT final_viewport;

   std::string shader_path;

#if defined(HAVE_CG) || defined(HAVE_HLSL) || defined(HAVE_GLSL)
#ifdef _XBOX
   const shader_backend_t *shader;
#else
   struct video_shader shader;
#endif
#endif
   video_info_t video_info;

   bool needs_restore;

   RECT font_rect;
   RECT font_rect_shifted;

#ifdef HAVE_OVERLAY
   bool overlays_enabled;
   std::vector<overlay_t> overlays;
#endif

#if defined(HAVE_MENU)
   overlay_t *menu;
#endif
   const renderchain_driver_t *renderchain_driver;
   void *renderchain_data;

   /* TODO - refactor this away properly. */
   bool resolution_hd_enable;
} d3d_video_t;

void d3d_make_d3dpp(void *data,
      const video_info_t *info, D3DPRESENT_PARAMETERS *d3dpp);

#endif

