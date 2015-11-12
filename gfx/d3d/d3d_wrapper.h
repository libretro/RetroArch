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

#ifndef _D3D_WRAPPER_H
#define _D3D_WRAPPER_H

#include <boolean.h>

#include "../common/win32_common.h"
#include "d3d_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

bool d3d_swap(void *data, LPDIRECT3DDEVICE dev);

LPDIRECT3DVERTEXBUFFER d3d_vertex_buffer_new(LPDIRECT3DDEVICE dev,
      unsigned length, unsigned usage, unsigned fvf,
      D3DPOOL pool, void *handle);

void *d3d_vertex_buffer_lock(LPDIRECT3DVERTEXBUFFER vertbuf);
void d3d_vertex_buffer_unlock(LPDIRECT3DVERTEXBUFFER vertbuf);

void d3d_vertex_buffer_free(void *vertex_data, void *vertex_declaration);

LPDIRECT3DTEXTURE d3d_texture_new(LPDIRECT3DDEVICE dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, D3DXIMAGE_INFO *src_info, 
      PALETTEENTRY *palette);

void d3d_set_stream_source(LPDIRECT3DDEVICE dev, unsigned stream_no,
      LPDIRECT3DVERTEXBUFFER stream_vertbuf, unsigned offset_bytes,
      unsigned stride);

void d3d_texture_free(LPDIRECT3DTEXTURE tex);

void d3d_set_transform(LPDIRECT3DDEVICE dev,
      D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX *matrix);

void d3d_set_sampler_address_u(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value);

void d3d_set_sampler_address_v(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value);

void d3d_set_sampler_minfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value);

void d3d_set_sampler_magfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value);

void d3d_draw_primitive(LPDIRECT3DDEVICE dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count);

void d3d_clear(LPDIRECT3DDEVICE dev,
      unsigned count, const D3DRECT *rects, unsigned flags,
      D3DCOLOR color, float z, unsigned stencil);

bool d3d_lock_rectangle(LPDIRECT3DTEXTURE tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags);

void d3d_lock_rectangle_clear(LPDIRECT3DTEXTURE tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags);

void d3d_unlock_rectangle(LPDIRECT3DTEXTURE tex);

void d3d_set_texture(LPDIRECT3DDEVICE dev, unsigned sampler,
      LPDIRECT3DTEXTURE tex);

HRESULT d3d_set_vertex_shader(LPDIRECT3DDEVICE dev, unsigned index,
      void *data);

void d3d_texture_blit(unsigned pixel_size,
      LPDIRECT3DTEXTURE tex,
      D3DLOCKED_RECT *lr, const void *frame,
      unsigned width, unsigned height, unsigned pitch);

bool d3d_vertex_declaration_new(LPDIRECT3DDEVICE dev,
      const void *vertex_data, void **decl_data);

void d3d_set_viewport(LPDIRECT3DDEVICE dev, D3DVIEWPORT *vp);

void d3d_enable_blend_func(void *data);

void d3d_disable_blend_func(void *data);

void d3d_set_vertex_declaration(void *data, void *vertex_data);

void d3d_enable_alpha_blend_texture_func(void *data);

void d3d_frame_postprocess(void *data);

void d3d_set_render_state(void *data, D3DRENDERSTATETYPE state, DWORD value);

#ifdef __cplusplus
}
#endif

#endif
