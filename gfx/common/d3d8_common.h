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

#ifndef _D3D8_COMMON_H
#define _D3D8_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>

#include "../video_driver.h"

RETRO_BEGIN_DECLS

bool d3d8_swap(void *data, void *dev);

void *d3d8_vertex_buffer_new(void *dev,
      unsigned length, unsigned usage, unsigned fvf,
      INT32 pool, void *handle);

void *d3d8_vertex_buffer_lock(void *data);
void d3d8_vertex_buffer_unlock(void *data);

void d3d8_vertex_buffer_free(void *vertex_data, void *vertex_declaration);

bool d3d8_texture_get_level_desc(void *tex,
      unsigned idx, void *_ppsurface_level);

bool d3d8_texture_get_surface_level(void *tex,
      unsigned idx, void **_ppsurface_level);

void *d3d8_texture_new(void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info,
      PALETTEENTRY *palette, bool want_mipmap);

void d3d8_set_stream_source(void *dev, unsigned stream_no,
      void *stream_vertbuf, unsigned offset_bytes,
      unsigned stride);

void d3d8_texture_free(void *tex);

void d3d8_set_transform(void *dev,
      INT32 state, const void *_matrix);

void d3d8_set_sampler_address_u(void *dev,
      unsigned sampler, unsigned value);

void d3d8_set_sampler_address_v(void *dev,
      unsigned sampler, unsigned value);

void d3d8_set_sampler_minfilter(void *dev,
      unsigned sampler, unsigned value);

void d3d8_set_sampler_magfilter(void *dev,
      unsigned sampler, unsigned value);

void d3d8_set_sampler_mipfilter(void *dev,
      unsigned sampler, unsigned value);

bool d3d8_begin_scene(void *dev);

void d3d8_end_scene(void *dev);

void d3d8_draw_primitive(void *dev,
      INT32 type, unsigned start, unsigned count);

void d3d8_clear(void *dev,
      unsigned count, const void *rects, unsigned flags,
      INT32 color, float z, unsigned stencil);

bool d3d8_lock_rectangle(void *tex,
      unsigned level, void *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags);

void d3d8_lock_rectangle_clear(void *tex,
      unsigned level, void *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags);

void d3d8_unlock_rectangle(void *tex);

void d3d8_set_texture(void *dev, unsigned sampler,
      void *tex_data);

bool d3d8_create_vertex_shader(void *dev,
      const DWORD *a, void **b);

bool d3d8_create_pixel_shader(void *dev,
      const DWORD *a, void **b);

void d3d8_free_vertex_shader(void *dev, void *data);

bool d3d8_set_vertex_shader(void *dev, unsigned index,
      void *data);

void d3d8_texture_blit(unsigned pixel_size,
      void *tex,
      void *lr, const void *frame,
      unsigned width, unsigned height, unsigned pitch);

bool d3d8_vertex_declaration_new(void *dev,
      const void *vertex_data, void **decl_data);

void d3d8_vertex_declaration_free(void *data);

void d3d8_set_viewports(void *dev, void *vp);

void d3d8_enable_blend_func(void *data);

void d3d8_disable_blend_func(void *data);

void d3d8_set_vertex_declaration(void *data, void *vertex_data);

void d3d8_enable_alpha_blend_texture_func(void *data);

void d3d8_frame_postprocess(void *data);

void d3d8_surface_free(void *data);

bool d3d8_device_get_render_target_data(void *dev,
      void *_src, void *_dst);

bool d3d8_device_get_render_target(void *dev,
      unsigned idx, void **data);

void d3d8_device_set_render_target(void *dev, unsigned idx,
      void *data);

bool d3d8_get_render_state(void *data,
      INT32 state, DWORD *value);

void d3d8_set_render_state(void *data,
      INT32 state, DWORD value);

void d3d8_device_set_render_target(void *dev, unsigned idx,
      void *data);

bool d3d8_device_create_offscreen_plain_surface(
      void *dev,
      unsigned width,
      unsigned height,
      unsigned format,
      unsigned pool,
      void **surf_data,
      void *data);

bool d3d8_surface_lock_rect(void *data, void *data2);

void d3d8_surface_unlock_rect(void *data);

bool d3d8_get_adapter_display_mode(void *d3d,
      unsigned idx,
      void *display_mode);

bool d3d8_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d8_reset(void *dev, void *d3dpp);

bool d3d8_device_get_backbuffer(void *dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data);

void d3d8_device_free(void *dev, void *pd3d);

void *d3d8_create(void);

bool d3d8_initialize_symbols(enum gfx_ctx_api api);

void d3d8_deinitialize_symbols(void);

bool d3d8_check_device_type(void *d3d,
      unsigned idx,
      INT32 disp_format,
      INT32 backbuffer_format,
      bool windowed_mode);

bool d3d8x_create_font_indirect(void *dev,
      void *desc, void **font_data);

void d3d8x_font_draw_text(void *data, void *sprite_data, void *string_data,
      unsigned count, void *rect_data, unsigned format, unsigned color);

void d3d8x_font_get_text_metrics(void *data, void *metrics);

void d3d8x_font_release(void *data);

INT32 d3d8_translate_filter(unsigned type);

INT32 d3d8_get_rgb565_format(void);
INT32 d3d8_get_argb8888_format(void);
INT32 d3d8_get_xrgb8888_format(void);

RETRO_END_DECLS

#endif
