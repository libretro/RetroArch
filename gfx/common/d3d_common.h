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

#include "../../defines/d3d_defines.h"
#include "../video_driver.h"

RETRO_BEGIN_DECLS

typedef struct d3d_texture
{
   void *data;
   D3DPOOL pool;
} d3d_texture_t;

bool d3d_swap(void *data, void *dev);

LPDIRECT3DVERTEXBUFFER d3d_vertex_buffer_new(void *dev,
      unsigned length, unsigned usage, unsigned fvf,
      D3DPOOL pool, void *handle);

void *d3d_vertex_buffer_lock(void *data);
void d3d_vertex_buffer_unlock(void *data);

void d3d_vertex_buffer_free(void *vertex_data, void *vertex_declaration);

bool d3d_texture_get_level_desc(void *tex,
      unsigned idx, void *_ppsurface_level);

bool d3d_texture_get_surface_level(void *tex,
      unsigned idx, void **_ppsurface_level);

void *d3d_texture_new(void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, void *src_info,
      PALETTEENTRY *palette, bool want_mipmap);

void d3d_set_stream_source(void *dev, unsigned stream_no,
      void *stream_vertbuf, unsigned offset_bytes,
      unsigned stride);

void d3d_texture_free(void *tex);

void d3d_set_transform(void *dev,
      D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX *matrix);

void d3d_set_sampler_address_u(void *dev,
      unsigned sampler, unsigned value);

void d3d_set_sampler_address_v(void *dev,
      unsigned sampler, unsigned value);

void d3d_set_sampler_minfilter(void *dev,
      unsigned sampler, unsigned value);

void d3d_set_sampler_magfilter(void *dev,
      unsigned sampler, unsigned value);

void d3d_set_sampler_mipfilter(void *dev,
      unsigned sampler, unsigned value);

bool d3d_begin_scene(void *dev);

void d3d_end_scene(void *dev);

void d3d_draw_primitive(void *dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count);

void d3d_clear(void *dev,
      unsigned count, const D3DRECT *rects, unsigned flags,
      D3DCOLOR color, float z, unsigned stencil);

bool d3d_lock_rectangle(void *tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags);

void d3d_lock_rectangle_clear(void *tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags);

void d3d_unlock_rectangle(void *tex);

void d3d_set_texture(void *dev, unsigned sampler,
      void *tex_data);

bool d3d_create_vertex_shader(void *dev,
      const DWORD *a, void **b);

bool d3d_create_pixel_shader(void *dev,
      const DWORD *a, void **b);

void d3d_free_pixel_shader(void *dev, void *data);

void d3d_free_vertex_shader(void *dev, void *data);

bool d3d_set_pixel_shader(void *dev, void *data);

bool d3d_set_vertex_shader(void *dev, unsigned index,
      void *data);

bool d3d_set_vertex_shader_constantf(void *dev,
      UINT start_register,const float* constant_data, unsigned vector4f_count);

void d3d_texture_blit(unsigned pixel_size,
      void *tex,
      D3DLOCKED_RECT *lr, const void *frame,
      unsigned width, unsigned height, unsigned pitch);

bool d3d_vertex_declaration_new(void *dev,
      const void *vertex_data, void **decl_data);

void d3d_vertex_declaration_free(void *data);

void d3d_set_viewports(void *dev, void *vp);

void d3d_enable_blend_func(void *data);

void d3d_disable_blend_func(void *data);

void d3d_set_vertex_declaration(void *data, void *vertex_data);

void d3d_enable_alpha_blend_texture_func(void *data);

void d3d_frame_postprocess(void *data);

void d3d_surface_free(void *data);

bool d3d_device_get_render_target_data(void *dev,
      void *_src, void *_dst);

bool d3d_device_get_render_target(void *dev,
      unsigned idx, void **data);

void d3d_device_set_render_target(void *dev, unsigned idx,
      void *data);

bool d3d_get_render_state(void *data,
      D3DRENDERSTATETYPE state, DWORD *value);

void d3d_set_render_state(void *data,
      D3DRENDERSTATETYPE state, DWORD value);

void d3d_device_set_render_target(void *dev, unsigned idx,
      void *data);

bool d3d_device_create_offscreen_plain_surface(
      void *dev,
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

bool d3d_get_adapter_display_mode(void *d3d,
      unsigned idx,
      D3DDISPLAYMODE *display_mode);

bool d3d_create_device(void *dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d_reset(void *dev, D3DPRESENT_PARAMETERS *d3dpp);

bool d3d_device_get_backbuffer(void *dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data);

void d3d_device_free(void *dev, void *pd3d);

void *d3d_create(void);

bool d3d_initialize_symbols(enum gfx_ctx_api api);

void d3d_deinitialize_symbols(void);

bool d3d_check_device_type(void *d3d,
      unsigned idx,
      D3DFORMAT disp_format,
      D3DFORMAT backbuffer_format,
      bool windowed_mode);

bool d3dx_create_font_indirect(void *dev,
      void *desc, void **font_data);

void d3dxbuffer_release(void *data);

D3DTEXTUREFILTERTYPE d3d_translate_filter(unsigned type);

bool d3dx_compile_shader(
      const char *src,
      unsigned src_data_len,
      const void *pdefines,
      void *pinclude,
      const char *pfunctionname,
      const char *pprofile,
      unsigned flags,
      void *ppshader,
      void *pperrormsgs,
      void *ppconstanttable);

bool d3dx_compile_shader_from_file(
      const char *src,
      const void *pdefines,
      void *pinclude,
      const char *pfunctionname,
      const char *pprofile,
      unsigned flags,
      void *ppshader,
      void *pperrormsgs,
      void *ppconstanttable);

D3DFORMAT d3d_get_rgb565_format(void);
D3DFORMAT d3d_get_argb8888_format(void);
D3DFORMAT d3d_get_xrgb8888_format(void);

RETRO_END_DECLS

#endif
