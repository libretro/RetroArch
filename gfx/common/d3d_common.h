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

#ifndef _D3D_COMMON_H
#define _D3D_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>

#include "win32_common.h"
#include "../../defines/d3d_defines.h"

RETRO_BEGIN_DECLS

bool d3d_swap(void *data, LPDIRECT3DDEVICE dev);

LPDIRECT3DVERTEXBUFFER d3d_vertex_buffer_new(LPDIRECT3DDEVICE dev,
      unsigned length, unsigned usage, unsigned fvf,
      D3DPOOL pool, void *handle);

void *d3d_vertex_buffer_lock(void *data);
void d3d_vertex_buffer_unlock(void *data);

void d3d_vertex_buffer_free(void *vertex_data, void *vertex_declaration);

bool d3d_texture_get_level_desc(LPDIRECT3DTEXTURE tex,
      unsigned idx, void *_ppsurface_level);

bool d3d_texture_get_surface_level(LPDIRECT3DTEXTURE tex,
      unsigned idx, void **_ppsurface_level);

LPDIRECT3DTEXTURE d3d_texture_new(LPDIRECT3DDEVICE dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, void *src_info,
      PALETTEENTRY *palette);

void d3d_set_stream_source(LPDIRECT3DDEVICE dev, unsigned stream_no,
      void *stream_vertbuf, unsigned offset_bytes,
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

bool d3d_begin_scene(LPDIRECT3DDEVICE dev);

void d3d_end_scene(LPDIRECT3DDEVICE dev);

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
      void *tex_data);

HRESULT d3d_set_vertex_shader(LPDIRECT3DDEVICE dev, unsigned index,
      void *data);

void d3d_texture_blit(unsigned pixel_size,
      LPDIRECT3DTEXTURE tex,
      D3DLOCKED_RECT *lr, const void *frame,
      unsigned width, unsigned height, unsigned pitch);

bool d3d_vertex_declaration_new(LPDIRECT3DDEVICE dev,
      const void *vertex_data, void **decl_data);

void d3d_vertex_declaration_free(void *data);

void d3d_set_viewports(LPDIRECT3DDEVICE dev, D3DVIEWPORT *vp);

void d3d_enable_blend_func(void *data);

void d3d_disable_blend_func(void *data);

void d3d_set_vertex_declaration(void *data, void *vertex_data);

void d3d_enable_alpha_blend_texture_func(void *data);

void d3d_frame_postprocess(void *data);

void d3d_surface_free(void *data);

bool d3d_device_get_render_target_data(LPDIRECT3DDEVICE dev,
      void *_src, void *_dst);

bool d3d_device_get_render_target(LPDIRECT3DDEVICE dev,
      unsigned idx, void **data);

void d3d_device_set_render_target(LPDIRECT3DDEVICE dev, unsigned idx,
      void *data);

void d3d_set_render_state(void *data, D3DRENDERSTATETYPE state, DWORD value);

void d3d_device_set_render_target(LPDIRECT3DDEVICE dev, unsigned idx,
      void *data);

bool d3d_device_create_offscreen_plain_surface(
      LPDIRECT3DDEVICE dev,
      unsigned width,
      unsigned height,
      unsigned format,
      unsigned pool,
      void **surf_data,
      void *data);

bool d3d_surface_lock_rect(void *data, void *data2);

void d3d_surface_unlock_rect(void *data);

void *d3d_matrix_transpose(void *_pout, const void *_pm);

void *d3d_matrix_multiply(void *_pout,
      const void *_pm1, const void *_pm2);

void *d3d_matrix_ortho_off_center_lh(void *_pout,
      float l, float r, float b, float t, float zn, float zf);

void * d3d_matrix_identity(void *_pout);

void *d3d_matrix_rotation_z(void *_pout, float angle);

bool d3d_create_device(LPDIRECT3DDEVICE *dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      LPDIRECT3D d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d_reset(LPDIRECT3DDEVICE dev, D3DPRESENT_PARAMETERS *d3dpp);

bool d3d_device_get_backbuffer(LPDIRECT3DDEVICE dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data);

void d3d_device_free(LPDIRECT3DDEVICE dev, LPDIRECT3D pd3d);

void *d3d_create(void);

bool d3d_initialize_symbols(void);

void d3d_deinitialize_symbols(void);

bool d3dx_create_font_indirect(LPDIRECT3DDEVICE dev,
      void *desc, void **font_data);

D3DTEXTUREFILTERTYPE d3d_translate_filter(unsigned type);

RETRO_END_DECLS

#endif
