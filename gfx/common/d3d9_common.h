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

#ifndef _D3D9_COMMON_H
#define _D3D9_COMMON_H

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <gfx/math/matrix_4x4.h>

#include <d3d9.h>

#include "d3d_common.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#define D3D9_DECL_FVF_TEXCOORD(stream, offset, index) \
   { (WORD)(stream), (WORD)(offset * sizeof(float)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, \
      D3DDECLUSAGE_TEXCOORD, (BYTE)(index) }

#ifdef _XBOX
#define D3D9_RGB565_FORMAT D3DFMT_LIN_R5G6B5
#define D3D9_ARGB8888_FORMAT D3DFMT_LIN_A8R8G8B8
#define D3D9_XRGB8888_FORMAT D3DFMT_LIN_X8R8G8B8
#else
#define D3D9_RGB565_FORMAT D3DFMT_R5G6B5
#define D3D9_ARGB8888_FORMAT D3DFMT_A8R8G8B8
#define D3D9_XRGB8888_FORMAT D3DFMT_X8R8G8B8
#endif


RETRO_BEGIN_DECLS

typedef struct d3d9_video d3d9_video_t;

typedef struct d3d9_video
{
   bool keep_aspect;
   bool should_resize;
   bool quitting;
   bool needs_restore;
   bool overlays_enabled;
   /* TODO - refactor this away properly. */
   bool resolution_hd_enable;

   /* Only used for Xbox */
   bool widescreen_mode;

   unsigned cur_mon_id;
   unsigned dev_rotation;

   overlay_t *menu;
   void *renderchain_data;

   RECT font_rect;
   RECT font_rect_shifted;
   math_matrix_4x4 mvp;
   math_matrix_4x4 mvp_rotate;
   math_matrix_4x4 mvp_transposed;

   struct video_viewport vp;
   struct video_shader shader;
   video_info_t video_info;
#ifdef HAVE_WINDOW
   WNDCLASSEX windowClass;
#endif
   LPDIRECT3DDEVICE9 dev;
   D3DVIEWPORT9 final_viewport;

   char *shader_path;

   struct
   {
      int size;
      int offset;
      void *buffer;
      void *decl;
   }menu_display;

   size_t overlays_size;
   overlay_t *overlays;
} d3d9_video_t;

void *d3d9_vertex_buffer_new(void *dev,
      unsigned length, unsigned usage, unsigned fvf,
      INT32 pool, void *handle);

void d3d9_vertex_buffer_free(void *vertex_data, void *vertex_declaration);

void *d3d9_texture_new(void *dev,
      unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info,
      PALETTEENTRY *palette, bool want_mipmap);

void *d3d9_texture_new_from_file(void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info,
      PALETTEENTRY *palette, bool want_mipmap);

static INLINE bool d3d9_vertex_declaration_new(
      LPDIRECT3DDEVICE9 dev,
      const void *vertex_data, void **decl_data)
{
   const D3DVERTEXELEMENT9   *vertex_elements = (const D3DVERTEXELEMENT9*)vertex_data;
   LPDIRECT3DVERTEXDECLARATION9 **vertex_decl = (LPDIRECT3DVERTEXDECLARATION9**)decl_data;

   if (SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(dev,
               vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
      return true;

   return false;
}

static INLINE bool d3d9_device_get_render_target(
      LPDIRECT3DDEVICE9 dev,
      unsigned idx, void **data)
{
   if (dev &&
         SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,
               idx, (LPDIRECT3DSURFACE9*)data)))
      return true;
   return false;
}

static INLINE bool d3d9_device_create_offscreen_plain_surface(
      LPDIRECT3DDEVICE9 dev,
      unsigned width,
      unsigned height,
      unsigned format,
      unsigned pool,
      void **surf_data,
      void *data)
{
#ifndef _XBOX
   if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev,
               width, height,
               (D3DFORMAT)format, (D3DPOOL)pool,
               (LPDIRECT3DSURFACE9*)surf_data,
               (HANDLE*)data)))
      return true;
#endif
   return false;
}

bool d3d9_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d9_reset(void *dev, void *d3dpp);

void *d3d9_create(void);

bool d3d9_initialize_symbols(enum gfx_ctx_api api);

void d3d9_deinitialize_symbols(void);

bool d3d9x_create_font_indirect(void *dev,
      void *desc, void **font_data);

void d3d9x_font_draw_text(void *data, void *sprite_data, void *string_data,
      unsigned count, void *rect_data, unsigned format, unsigned color);

void d3d9x_font_get_text_metrics(void *data, void *metrics);

void d3d9x_font_release(void *data);

bool d3d9x_compile_shader(
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

bool d3d9x_compile_shader_from_file(
      const char *src,
      const void *pdefines,
      void *pinclude,
      const char *pfunctionname,
      const char *pprofile,
      unsigned flags,
      void *ppshader,
      void *pperrormsgs,
      void *ppconstanttable);

void d3d9x_constant_table_set_float_array(LPDIRECT3DDEVICE9 dev,
      void *p, void *_handle, const void *_pf, unsigned count);

void d3d9x_constant_table_set_defaults(LPDIRECT3DDEVICE9 dev,
      void *p);

void d3d9x_constant_table_set_matrix(LPDIRECT3DDEVICE9 dev,
      void *p, void *data, const void *matrix);

const bool d3d9x_constant_table_set_float(void *p,
      void *a, void *b, float val);

void *d3d9x_constant_table_get_constant_by_name(void *_tbl,
      void *_handle, void *_name);

void d3d9_make_d3dpp(d3d9_video_t *d3d,
      const video_info_t *info, void *_d3dpp);

void d3d9_calculate_rect(d3d9_video_t *d3d,
      unsigned *width, unsigned *height,
      int *x, int *y,
      bool force_full,
      bool allow_rotate);

void d3d9_log_info(const struct LinkInfo *info);

#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
void d3d9_free_overlay(d3d9_video_t *d3d, overlay_t *overlay);

void d3d9_overlay_render(d3d9_video_t *d3d,
      unsigned width,
      unsigned height,
      overlay_t *overlay, bool force_linear);
#endif

#if defined(HAVE_OVERLAY)
void d3d9_free_overlays(d3d9_video_t *d3d);
void d3d9_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface);
#endif

void d3d9_set_rotation(void *data, unsigned rot);

void d3d9_viewport_info(void *data, struct video_viewport *vp);

bool d3d9_read_viewport(void *data, uint8_t *buffer, bool is_idle);

bool d3d9_has_windowed(void *data);

bool d3d9_process_shader(d3d9_video_t *d3d);

uintptr_t d3d9_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type);

void d3d9_set_osd_msg(void *data,
      const char *msg,
      const void *params, void *font);

void d3d9_unload_texture(void *data, 
      bool threaded, uintptr_t id);

void d3d9_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen);

void d3d9_set_aspect_ratio(void *data, unsigned aspect_ratio_idx);

void d3d9_set_menu_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha);

void d3d9_set_viewport(void *data,
      unsigned width, unsigned height,
      bool force_full,
      bool allow_rotate);

void d3d9_set_menu_texture_enable(void *data,
      bool state, bool full_screen);

void d3d9_blit_to_texture(
      LPDIRECT3DTEXTURE9 tex,
      const void *frame,
      unsigned tex_width,  unsigned tex_height,
      unsigned width,      unsigned height,
      unsigned last_width, unsigned last_height,
      unsigned pitch, unsigned pixel_size);

void d3d9_apply_state_changes(void *data);

extern LPDIRECT3D9 g_pD3D9;

RETRO_END_DECLS

#endif
