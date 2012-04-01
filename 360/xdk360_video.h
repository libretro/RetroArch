/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _XDK360_VIDEO_H
#define _XDK360_VIDEO_H

#include <stdint.h>
#include "fonts.h"
#include "xdk360_video_general.h"

typedef struct {
	float x;
	float y;
	float z;
	float rhw;
	float u;
	float v;
} primitive_t;

#define DFONT_MAX	4096
#define PRIM_FVF		(D3DFVF_XYZRHW | D3DFVF_TEX1)

typedef struct DrawVerticeFormats
{
   float x, y;
   float u, v;
} DrawVerticeFormats;

typedef struct xdk360_video
{
	bool block_swap;
	bool vsync; 
	unsigned last_width, last_height;
   IDirect3D9* xdk360_device;
   IDirect3DDevice9* xdk360_render_device;
   IDirect3DVertexShader9 *pVertexShader;
   IDirect3DPixelShader9* pPixelShader;
   IDirect3DVertexDeclaration9* pVertexDecl;
   IDirect3DVertexBuffer9* vertex_buf;
   IDirect3DTexture9* lpTexture;
   D3DPRESENT_PARAMETERS d3dpp;
   XVIDEO_MODE video_mode;
} xdk360_video_t;

enum
{
	ASPECT_RATIO_4_3,
	ASPECT_RATIO_5_4,
	ASPECT_RATIO_8_7,
	ASPECT_RATIO_16_9,
	ASPECT_RATIO_16_10,
	ASPECT_RATIO_16_15,
	ASPECT_RATIO_19_14,
	ASPECT_RATIO_2_1,
	ASPECT_RATIO_3_2,
	ASPECT_RATIO_3_4,
	ASPECT_RATIO_1_1,
	ASPECT_RATIO_AUTO,
	ASPECT_RATIO_CUSTOM
};

void xdk360_video_init(void);
void xdk360_video_deinit(void);
void xdk360_video_set_vsync(bool vsync);
void xdk360_set_aspect_ratio(uint32_t aspectratio_index);

extern void *g_d3d;

#endif
