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

#ifndef _D3D_WRAPPER_H
#define _D3D_WRAPPER_H

#include "../context/win32_common.h"
#include "d3d_defines.h"

void d3d_swap(d3d_video_t *d3d, LPDIRECT3DDEVICE dev);

HRESULT d3d_create_vertex_buffer(LPDIRECT3DDEVICE dev,
      unsigned length, unsigned usage, unsigned fvf,
      d3DPOOL pool, LPDIRECT3DVERTEXBUFFER vert_buf, void *handle);

void d3d_set_stream_source(LPDIRECT3DDEVICE dev, unsigned stream_no,
      LPDIRECT3DVERTEXBUFFER stream_vertbuf, unsigned offset_bytes,
      unsigned stride);

void d3d_set_sampler_address_u(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type, unsigned value);

void d3d_set_sampler_address_v(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type, unsigned value);

void d3d_set_sampler_minfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type, unsigned value);

void d3d_set_sampler_magfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned type);

void d3d_draw_primitive(LPDIRECT3DDEVICE dev,
      unsigned type, unsigned start, unsigned count);

void d3d_lockrectangle_clear(LPDIRECT3DTEXTURE tex,
      unsigned tex_width, unsigned tex_height,
      unsigned level, D3DLOCKED_RECT lock_rect, RECT rect,
      unsigned flags);

void d3d_textureblit(d3d_video_t *d3d,
      LPDIRECT3DTEXTURE tex, D3DSURFACE_DESC desc,
      D3DLOCKED_RECT rect, const void *frame,
      unsigned width, unsigned height, unsigned pitch);

#endif
