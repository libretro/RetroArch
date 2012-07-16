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

#ifndef _XDK360_VIDEO_H
#define _XDK360_VIDEO_H

#include <stdint.h>
#include "../xdk/xdk_defines.h"

#define DFONT_MAX	4096
#define PRIM_FVF	(D3DFVF_XYZRHW | D3DFVF_TEX1)

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
   float x, y;
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
   LPDIRECT3D d3d_device;
   LPDIRECT3DDEVICE d3d_render_device;
   LPDIRECT3DVERTEXBUFFER vertex_buf;
   LPDIRECT3DTEXTURE lpTexture;
   D3DTexture lpTexture_ot_as16srgb;
   LPDIRECT3DTEXTURE lpTexture_ot;
   IDirect3DVertexDeclaration9* v_decl;
   XVIDEO_MODE video_mode;
   D3DPRESENT_PARAMETERS d3dpp;
   LPDIRECT3DSURFACE lpSurface;
} xdk_d3d_video_t;

#endif
