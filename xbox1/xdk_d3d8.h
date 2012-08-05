/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _XDK_VIDEO_H
#define _XDK_VIDEO_H

#include <stdint.h>
#include <xfont.h>

#include "../xdk/xdk_defines.h"

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW | D3DFVF_TEX1)

#define MIN_SCALING_FACTOR (1.0f)
#define MAX_SCALING_FACTOR (2.0f)

typedef struct
{
   float x;
   float y;
   float z;
   float rhw;
   float u;
   float v;
} primitive_t;

typedef struct DrawVerticeFormats
{
   float x, y, z;
   float rhw;
   float u, v;
} DrawVerticeFormats;

typedef struct xdk_d3d_video
{
   bool block_swap;
   bool fbo_enabled;
   bool should_resize;
   bool quitting;
   bool vsync;
   unsigned frame_count;
   unsigned last_width;
   unsigned last_height;
   unsigned win_width;
   unsigned win_height;
   LPDIRECT3D d3d_device;
   LPDIRECT3DDEVICE d3d_render_device;
   LPDIRECT3DVERTEXBUFFER vertex_buf;
   LPDIRECT3DTEXTURE lpTexture;
   DWORD video_mode;
   D3DPRESENT_PARAMETERS d3dpp;
   XFONT *debug_font;
   D3DSurface *pBackBuffer, *pFrontBuffer;
} xdk_d3d_video_t;

#endif
