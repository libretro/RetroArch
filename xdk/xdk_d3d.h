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
#include "../gfx/shader_common.h"
#include "../gfx/shader_parse.h"
#include "../gfx/image/image.h"
#include "../gfx/fonts/d3d_font.h"

#include "../gfx/gfx_context.h"
#include "../xdk/xdk_defines.h"

#define DFONT_MAX	4096
#define D3DFVF_CUSTOMVERTEX	(D3DFVF_XYZRHW | D3DFVF_TEX1)

typedef struct DrawVerticeFormats
{
   float x, y;
#if defined(_XBOX1)
   float z;
   float rhw;
#endif
   float u, v;
} DrawVerticeFormats;

typedef struct gl_shader_backend gl_shader_backend_t;

typedef struct xdk_d3d_video
{
   const gfx_ctx_driver_t *ctx_driver;
   const gl_shader_backend_t *shader;
   bool should_resize;
   bool quitting;
   bool vsync;
   unsigned last_width;
   unsigned last_height;
   unsigned screen_width;
   unsigned screen_height;
   unsigned dev_rotation;
   unsigned tex_w, tex_h;
   LPDIRECT3D g_pD3D;
   LPDIRECT3DDEVICE dev;
   LPDIRECT3DVERTEXBUFFER vertex_buf;
   LPDIRECT3DTEXTURE lpTexture;
#ifdef HAVE_D3D9
   LPDIRECT3DTEXTURE lpTexture_ot_as16srgb;
   LPDIRECT3DTEXTURE lpTexture_ot;
   LPDIRECT3DVERTEXDECLARATION v_decl;
#endif
#ifdef HAVE_MENU
   bool rgui_texture_enable;
   bool rgui_texture_full_screen;
#endif
   const d3d_font_renderer_t *font_ctx;
   D3DFORMAT internal_fmt;
   D3DFORMAT texture_fmt;
   D3DVIEWPORT final_viewport;
   unsigned base_size;
   LPDIRECT3DSURFACE lpSurface;
   video_info_t video_info;
} xdk_d3d_video_t;

extern void xdk_d3d_generate_pp(D3DPRESENT_PARAMETERS *d3dpp, const video_info_t *video);
extern bool texture_image_render(struct texture_image *out_img);

#endif
