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
#include <retro_inline.h>

#include <d3d8.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

RETRO_BEGIN_DECLS

typedef struct d3d8_video
{
   bool keep_aspect;
   bool should_resize;
   bool quitting;
   bool needs_restore;
   bool overlays_enabled;
   /* TODO - refactor this away properly. */
   bool resolution_hd_enable;

   unsigned cur_mon_id;
   unsigned dev_rotation;

   overlay_t *menu;
   void *renderchain_data;

   math_matrix_4x4 mvp;
   math_matrix_4x4 mvp_rotate;
   math_matrix_4x4 mvp_transposed;

   struct video_viewport vp;
   struct video_shader shader;
   video_info_t video_info;
#ifdef HAVE_WINDOW
   WNDCLASSEX windowClass;
#endif
   LPDIRECT3DDEVICE8 dev;
   D3DVIEWPORT8 final_viewport;

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
} d3d8_video_t;

static INLINE bool d3d8_swap(void *data, LPDIRECT3DDEVICE8 dev)
{
   if (IDirect3DDevice8_Present(dev, NULL, NULL, NULL, NULL)
         == D3DERR_DEVICELOST)
      return false;
   return true;
}

static INLINE void *d3d8_vertex_buffer_new(
      LPDIRECT3DDEVICE8 dev,
      unsigned length, unsigned usage,
      unsigned fvf, D3DPOOL pool, void *handle)
{
   void              *buf = NULL;
   if (FAILED(IDirect3DDevice8_CreateVertexBuffer(
               dev, length, usage, fvf,
               pool,
               (struct IDirect3DVertexBuffer8**)&buf)))
      return NULL;
   return buf;
}

static INLINE void *
d3d8_vertex_buffer_lock(LPDIRECT3DVERTEXBUFFER8 vertbuf)
{
   void                       *buf = NULL;

   if (!vertbuf)
      return NULL;

   IDirect3DVertexBuffer8_Lock(vertbuf, 0, 0, (BYTE**)&buf, 0);

   if (!buf)
      return NULL;

   return buf;
}

static INLINE void d3d8_vertex_buffer_unlock(
      LPDIRECT3DVERTEXBUFFER8 vertbuf)
{
   if (vertbuf)
      IDirect3DVertexBuffer8_Unlock(vertbuf);
}

static INLINE void d3d8_vertex_buffer_free(
      LPDIRECT3DVERTEXBUFFER8 buf,
      void *vertex_declaration)
{
   if (buf)
   {
      IDirect3DVertexBuffer8_Release(buf);
      buf = NULL;
   }
}

static INLINE bool d3d8_texture_get_level_desc(
      LPDIRECT3DTEXTURE8 tex,
      unsigned idx, void *_ppsurface_level)
{
   if (SUCCEEDED(IDirect3DTexture8_GetLevelDesc(
               tex, idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
   return false;
}

static INLINE bool d3d8_texture_get_surface_level(
      LPDIRECT3DTEXTURE8 tex,
      unsigned idx, void **_ppsurface_level)
{
   if (tex &&
         SUCCEEDED(
            IDirect3DTexture8_GetSurfaceLevel(
               tex, idx, (IDirect3DSurface8**)_ppsurface_level)))
      return true;
   return false;
}

void *d3d8_texture_new(LPDIRECT3DDEVICE8 dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap);

static INLINE void d3d8_set_stream_source(LPDIRECT3DDEVICE8 dev,
      unsigned stream_no,
      LPDIRECT3DVERTEXBUFFER8 stream_vertbuf,
      unsigned offset_bytes,
      unsigned stride)
{
   if (stream_vertbuf)
      IDirect3DDevice8_SetStreamSource(dev,
            stream_no, stream_vertbuf, stride);
}

static INLINE void d3d8_texture_free(LPDIRECT3DTEXTURE8 tex)
{
   if (tex)
      IDirect3DTexture8_Release(tex);
}

static INLINE void d3d8_set_transform(LPDIRECT3DDEVICE8 dev,
      D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix)
{
   IDirect3DDevice8_SetTransform(dev, state, matrix);
}

static INLINE void d3d8_set_texture_stage_state(LPDIRECT3DDEVICE8 dev,
      unsigned sampler, D3DTEXTURESTAGESTATETYPE type, unsigned value)
{
   if (IDirect3DDevice8_SetTextureStageState(dev, sampler,
            (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
      RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
}

static INLINE void d3d8_set_sampler_address_u(LPDIRECT3DDEVICE8 dev,
      unsigned sampler, unsigned value)
{
   d3d8_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSU, value);
}

static INLINE void d3d8_set_sampler_address_v(LPDIRECT3DDEVICE8 dev,
      unsigned sampler, unsigned value)
{
   d3d8_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSV, value);
}

static INLINE void d3d8_set_sampler_minfilter(void *_dev,
      unsigned sampler, enum D3DTEXTUREFILTERTYPE value)
{
   d3d8_set_texture_stage_state(_dev, sampler, D3DTSS_MINFILTER, value);
}

static INLINE void d3d8_set_sampler_magfilter(void *_dev,
      unsigned sampler, enum D3DTEXTUREFILTERTYPE value)
{
   d3d8_set_texture_stage_state(_dev, sampler, D3DTSS_MAGFILTER, value);
}

void d3d8_set_sampler_mipfilter(void *dev,
      unsigned sampler, unsigned value);

static INLINE bool d3d8_begin_scene(LPDIRECT3DDEVICE8 dev)
{
   if (!dev)
      return false;
#ifdef _XBOX
   IDirect3DDevice8_BeginScene(dev);
#else
   if (FAILED(IDirect3DDevice8_BeginScene(dev)))
      return false;
#endif

   return true;
}

static INLINE void d3d8_end_scene(LPDIRECT3DDEVICE8 dev)
{
   if (dev)
      IDirect3DDevice8_EndScene(dev);
}

static INLINE void d3d8_draw_primitive(LPDIRECT3DDEVICE8 dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
   if (!d3d8_begin_scene(dev))
      return;

   IDirect3DDevice8_DrawPrimitive(dev, type, start, count);
   d3d8_end_scene(dev);
}

static INLINE void d3d8_clear(LPDIRECT3DDEVICE8 dev,
      unsigned count, const void *rects, unsigned flags,
      INT32 color, float z, unsigned stencil)
{
   if (dev)
      IDirect3DDevice8_Clear(dev, count, (const D3DRECT*)rects, flags,
            color, z, stencil);
}

static INLINE bool d3d8_lock_rectangle(
      LPDIRECT3DTEXTURE8 tex,
      unsigned level, D3DLOCKED_RECT *lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   if (tex &&
         IDirect3DTexture8_LockRect(tex,
            level, lr, rect, flags) == D3D_OK)
      return true;
   return false;
}

static INLINE void d3d8_unlock_rectangle(LPDIRECT3DTEXTURE8 tex)
{
   if (tex)
      IDirect3DTexture8_UnlockRect(tex, 0);
}

static INLINE void d3d8_lock_rectangle_clear(
      void *tex,
      unsigned level, D3DLOCKED_RECT *lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
#if defined(_XBOX)
   level              = 0;
#endif
   memset(lr->pBits, level, rectangle_height * lr->Pitch);
   d3d8_unlock_rectangle(tex);
}

static INLINE void d3d8_set_texture(
      LPDIRECT3DDEVICE8 dev, unsigned sampler,
      LPDIRECT3DTEXTURE8 tex)
{
   if (dev && tex)
      IDirect3DDevice8_SetTexture(dev, sampler,
            (IDirect3DBaseTexture8*)tex);
}

static INLINE bool d3d8_set_vertex_shader(
      LPDIRECT3DDEVICE8 dev,
      unsigned index,
      void *data)
{
   if (IDirect3DDevice8_SetVertexShader(dev, index) != D3D_OK)
      return false;
   return true;
}

static INLINE void d3d8_texture_blit(
      unsigned pixel_size,
      void *tex,
      D3DLOCKED_RECT *lr,
      const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned y;

   for (y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)lr->pBits + y * lr->Pitch;
      memcpy(out, in, width * pixel_size);
   }
}

static INLINE void d3d8_set_viewports(
      LPDIRECT3DDEVICE8 dev,
      void *vp)
{
   if (dev)
      IDirect3DDevice8_SetViewport(dev, (D3DVIEWPORT8*)vp);
}

static INLINE void d3d8_set_render_state(
      LPDIRECT3DDEVICE8 dev,
      D3DRENDERSTATETYPE state,
      DWORD value)
{
   if (dev)
      IDirect3DDevice8_SetRenderState(dev, state, value);
}

static INLINE void d3d8_enable_blend_func(void *data)
{
   if (!data)
      return;

   d3d8_set_render_state(data, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d8_set_render_state(data, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d8_set_render_state(data, D3DRS_ALPHABLENDENABLE, true);
}

static INLINE void d3d8_disable_blend_func(void *data)
{
   d3d8_set_render_state(data, D3DRS_ALPHABLENDENABLE, false);
}

static INLINE void d3d8_enable_alpha_blend_texture_func(void *data)
{
   /* Also blend the texture with the set alpha value. */
   d3d8_set_texture_stage_state(data, 0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
   d3d8_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG1,   D3DTA_DIFFUSE);
   d3d8_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG2,   D3DTA_TEXTURE);
}

void d3d8_frame_postprocess(void *data);

static INLINE void d3d8_surface_free(LPDIRECT3DSURFACE8 surf)
{
   if (surf)
      IDirect3DSurface8_Release(surf);
}

static INLINE bool d3d8_device_get_render_target(
      LPDIRECT3DDEVICE8 dev,
      unsigned idx, void **data)
{
   if (dev &&
         SUCCEEDED(IDirect3DDevice8_GetRenderTarget(dev,
               (LPDIRECT3DSURFACE8*)data)))
      return true;
   return false;
}

static INLINE void d3d8_device_set_render_target(
      LPDIRECT3DDEVICE8 dev, unsigned idx,
      LPDIRECT3DSURFACE8 surf)
{
   if (dev)
      IDirect3DDevice8_SetRenderTarget(dev, surf, NULL);
}

static INLINE bool d3d8_get_render_state(LPDIRECT3DDEVICE8 dev,
      D3DRENDERSTATETYPE state, DWORD *value)
{
   if (dev &&
         IDirect3DDevice8_GetRenderState(dev, state, value) == D3D_OK)
      return true;
   return false;
}

static INLINE bool d3d8_surface_lock_rect(
      LPDIRECT3DSURFACE8 surf, void *data2)
{
   if (surf &&
         SUCCEEDED(
            IDirect3DSurface8_LockRect(
               surf, (D3DLOCKED_RECT*)data2,
               NULL, D3DLOCK_READONLY)))
      return true;
   return false;
}

static INLINE void d3d8_surface_unlock_rect(LPDIRECT3DSURFACE8 surf)
{
   if (surf)
      IDirect3DSurface8_UnlockRect(surf);
}

static INLINE bool d3d8_get_adapter_display_mode(
      LPDIRECT3D8 d3d,
      unsigned idx,
      void *display_mode)
{
   if (d3d &&
         SUCCEEDED(IDirect3D8_GetAdapterDisplayMode(
               d3d, idx, (D3DDISPLAYMODE*)display_mode)))
      return true;
   return false;
}

bool d3d8_create_device(void *dev,
      void *d3dpp,
      LPDIRECT3D8 d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d8_reset(void *dev, void *d3dpp);

static INLINE bool d3d8_device_get_backbuffer(
      LPDIRECT3DDEVICE8 dev,
      unsigned idx, unsigned swapchain_idx,
      unsigned backbuffer_type, void **data)
{
   if (dev &&
         SUCCEEDED(IDirect3DDevice8_GetBackBuffer(dev, idx,
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE8*)data)))
      return true;
   return false;
}

static INLINE void d3d8_device_free(
      LPDIRECT3DDEVICE8 dev, LPDIRECT3D8 pd3d)
{
   if (dev)
      IDirect3DDevice8_Release(dev);
   if (pd3d)
      IDirect3D8_Release(pd3d);
}

void *d3d8_create(void);

bool d3d8_initialize_symbols(enum gfx_ctx_api api);

void d3d8_deinitialize_symbols(void);

static INLINE bool d3d8_check_device_type(
      LPDIRECT3D8 d3d,
      unsigned idx,
      INT32 disp_format,
      INT32 backbuffer_format,
      bool windowed_mode)
{
   if (d3d &&
         SUCCEEDED(IDirect3D8_CheckDeviceType(d3d,
               0,
               D3DDEVTYPE_HAL,
               disp_format,
               backbuffer_format,
               windowed_mode)))
      return true;
   return false;
}

bool d3d8x_create_font_indirect(LPDIRECT3DDEVICE8 dev,
      void *desc, void **font_data);

void d3d8x_font_draw_text(void *data, void *sprite_data, void *string_data,
      unsigned count, void *rect_data, unsigned format, unsigned color);

void d3d8x_font_get_text_metrics(void *data, void *metrics);

void d3d8x_font_release(void *data);

static INLINE INT32 d3d8_get_rgb565_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_R5G6B5;
#else
   return D3DFMT_R5G6B5;
#endif
}

static INLINE INT32 d3d8_get_argb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_A8R8G8B8;
#else
   return D3DFMT_A8R8G8B8;
#endif
}

static INLINE INT32 d3d8_get_xrgb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_X8R8G8B8;
#else
   return D3DFMT_X8R8G8B8;
#endif
}

void d3d8_set_mvp(void *data, const void *userdata);

RETRO_END_DECLS

#endif
