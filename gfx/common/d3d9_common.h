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

#include <d3d9.h>

#include "../video_driver.h"
#include "../../verbosity.h"

RETRO_BEGIN_DECLS

static INLINE bool d3d9_swap(void *data, void *_dev)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#ifdef _XBOX
   IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
#else
   if (IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL) 
         == D3DERR_DEVICELOST)
      return false;
#endif
   return true;
}

void *d3d9_vertex_buffer_new(void *dev,
      unsigned length, unsigned usage, unsigned fvf,
      INT32 pool, void *handle);

static INLINE void *d3d9_vertex_buffer_lock(void *vertbuf_ptr)
{
   void                       *buf = NULL;
   LPDIRECT3DVERTEXBUFFER9 vertbuf = (LPDIRECT3DVERTEXBUFFER9)vertbuf_ptr;
   if (!vertbuf)
      return NULL;
   IDirect3DVertexBuffer9_Lock(vertbuf, 0, 0, &buf, 0);

   if (!buf)
      return NULL;

   return buf;
}

static INLINE void d3d9_vertex_buffer_unlock(void *vertbuf_ptr)
{
   LPDIRECT3DVERTEXBUFFER9 vertbuf = (LPDIRECT3DVERTEXBUFFER9)vertbuf_ptr;

   if (!vertbuf)
      return;
   IDirect3DVertexBuffer9_Unlock(vertbuf);
}

void d3d9_vertex_buffer_free(void *vertex_data, void *vertex_declaration);

static INLINE bool d3d9_texture_get_level_desc(void *_tex,
      unsigned idx, void *_ppsurface_level)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
#if defined(_XBOX)
   D3DTexture_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level);
#else
   if (FAILED(IDirect3DTexture9_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return false;
#endif
   return true;
}

static INLINE bool d3d9_texture_get_surface_level(void *_tex,
      unsigned idx, void **_ppsurface_level)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (tex && 
         SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(
               tex, idx, (IDirect3DSurface9**)_ppsurface_level)))
      return true;

   return false;
}

void *d3d9_texture_new(void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info,
      PALETTEENTRY *palette, bool want_mipmap);

static INLINE void d3d9_set_stream_source(
      void *_dev, unsigned stream_no,
      void *stream_vertbuf_ptr, unsigned offset_bytes,
      unsigned stride)
{
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   LPDIRECT3DVERTEXBUFFER9 
      stream_vertbuf      = (LPDIRECT3DVERTEXBUFFER9)stream_vertbuf_ptr;
   if (stream_vertbuf)
      IDirect3DDevice9_SetStreamSource(dev, stream_no, stream_vertbuf,
            offset_bytes,
            stride);
}

static INLINE void d3d9_texture_free(void *_tex)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (!tex)
      return;
   IDirect3DTexture9_Release(tex);
}

static INLINE void d3d9_set_transform(void *_dev,
      INT32 state, const void *_matrix)
{
#ifndef _XBOX
   CONST D3DMATRIX *matrix = (CONST D3DMATRIX*)_matrix;
   /* XBox 360 D3D9 does not support fixed-function pipeline. */
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   IDirect3DDevice9_SetTransform(dev, (D3DTRANSFORMSTATETYPE)state, matrix);
#endif
}

static INLINE void d3d9_set_sampler_address_u(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSU, value);
}

static INLINE void d3d9_set_sampler_address_v(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSV, value);
}

static INLINE void d3d9_set_sampler_minfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev)
      IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MINFILTER, value);
}

static INLINE void d3d9_set_sampler_magfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev)
      IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MAGFILTER, value);
}

static INLINE void d3d9_set_sampler_mipfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev)
      IDirect3DDevice9_SetSamplerState(dev, sampler,
            D3DSAMP_MIPFILTER, value);
}

static INLINE bool d3d9_begin_scene(void *_dev)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return false;
#if defined(_XBOX)
   IDirect3DDevice9_BeginScene(dev);
#else
   if (FAILED(IDirect3DDevice9_BeginScene(dev)))
      return false;
#endif

   return true;
}

static INLINE void d3d9_end_scene(void *_dev)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev)
      IDirect3DDevice9_EndScene(dev);
}

static INLINE void d3d9_draw_primitive(void *_dev,
      INT32 _type, unsigned start, unsigned count)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   D3DPRIMITIVETYPE type = (D3DPRIMITIVETYPE)_type;
   if (!dev || !d3d9_begin_scene(dev))
      return;
   IDirect3DDevice9_DrawPrimitive(dev, type, start, count);
   d3d9_end_scene(dev);
}

static INLINE void d3d9_clear(void *_dev,
      unsigned count, const void *rects, unsigned flags,
      INT32 color, float z, unsigned stencil)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev)
      IDirect3DDevice9_Clear(dev, count, (const D3DRECT*)rects, flags,
            color, z, stencil);
}

static INLINE bool d3d9_lock_rectangle(void *_tex,
      unsigned level, void *_lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   D3DLOCKED_RECT     *lr = (D3DLOCKED_RECT*)_lr;
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (!tex)
      return false;
#ifdef _XBOX
   IDirect3DTexture9_LockRect(tex, level, lr, (const RECT*)rect, flags);
#else
   if (IDirect3DTexture9_LockRect(tex, level, lr, (const RECT*)rect, flags) != D3D_OK)
      return false;
#endif

   return true;
}

static INLINE void d3d9_unlock_rectangle(void *_tex)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (tex)
      IDirect3DTexture9_UnlockRect(tex, 0);
}

static INLINE void d3d9_lock_rectangle_clear(void *tex,
      unsigned level, void *_lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   D3DLOCKED_RECT *lr = (D3DLOCKED_RECT*)_lr;
#if defined(_XBOX)
   level              = 0;
#endif
   memset(lr->pBits, level, rectangle_height * lr->Pitch);
   d3d9_unlock_rectangle(tex);
}

static INLINE void d3d9_set_texture(void *_dev, unsigned sampler,
      void *tex_data)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)tex_data;
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   if (!dev || !tex)
      return;
   IDirect3DDevice9_SetTexture(dev, sampler,
         (IDirect3DBaseTexture9*)tex);
}

static INLINE bool d3d9_create_vertex_shader(
      void *_dev, const DWORD *a, void **b)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev && IDirect3DDevice9_CreateVertexShader(dev, a,
            (LPDIRECT3DVERTEXSHADER9*)b) == D3D_OK)
      return true;
   return false;
}

static INLINE bool d3d9_create_pixel_shader(
      void *_dev, const DWORD *a, void **b)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev &&
         IDirect3DDevice9_CreatePixelShader(dev, a,
            (LPDIRECT3DPIXELSHADER9*)b) == D3D_OK)
      return true;
   return false;
}

static INLINE void d3d9_free_vertex_shader(void *_dev, void *data)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   IDirect3DVertexShader9 *vs = (IDirect3DVertexShader9*)data;
   if (!dev || !vs)
      return;
   IDirect3DVertexShader9_Release(vs);
}

static INLINE void d3d9_free_pixel_shader(void *_dev, void *data)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   IDirect3DPixelShader9 *ps  = (IDirect3DPixelShader9*)data;
   if (!dev || !ps)
      return;
   IDirect3DPixelShader9_Release(ps);
}

static INLINE bool d3d9_set_pixel_shader(
      void *_dev, void *data)
{
   LPDIRECT3DDEVICE9       dev  = (LPDIRECT3DDEVICE9)_dev;
   LPDIRECT3DPIXELSHADER9 d3dps = (LPDIRECT3DPIXELSHADER9)data;
   if (!dev || !d3dps)
      return false;

#ifdef _XBOX
   /* Returns void on Xbox */
   IDirect3DDevice9_SetPixelShader(dev, d3dps);
#else
   if (IDirect3DDevice9_SetPixelShader(dev, d3dps) != D3D_OK)
      return false;
#endif
   return true;
}

static INLINE bool d3d9_set_vertex_shader(
      void *_dev, unsigned index,
      void *data)
{
   LPDIRECT3DDEVICE9       dev    = (LPDIRECT3DDEVICE9)_dev;
   LPDIRECT3DVERTEXSHADER9 shader = (LPDIRECT3DVERTEXSHADER9)data;

#ifdef _XBOX
   IDirect3DDevice9_SetVertexShader(dev, shader);
#else
   if (IDirect3DDevice9_SetVertexShader(dev, shader) != D3D_OK)
      return false;
#endif

   return true;
}

static INLINE bool d3d9_set_vertex_shader_constantf(void *_dev,
      UINT start_register,const float* constant_data,
      unsigned vector4f_count)
{
   LPDIRECT3DDEVICE9      dev    = (LPDIRECT3DDEVICE9)_dev;

#ifdef _XBOX
   IDirect3DDevice9_SetVertexShaderConstantF(dev,
         start_register, constant_data, vector4f_count);
#else
   if (IDirect3DDevice9_SetVertexShaderConstantF(dev,
            start_register, constant_data, vector4f_count) != D3D_OK)
      return false;
#endif

   return true;
}

static INLINE void d3d9_texture_blit(unsigned pixel_size,
      void *tex,
      void *_lr, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   unsigned y;
   D3DLOCKED_RECT *lr = (D3DLOCKED_RECT*)_lr;

   for (y = 0; y < height; y++)
   {
      const uint8_t *in = (const uint8_t*)frame + y * pitch;
      uint8_t *out = (uint8_t*)lr->pBits + y * lr->Pitch;
      memcpy(out, in, width * pixel_size);
   }
}

static INLINE bool d3d9_vertex_declaration_new(void *_dev,
      const void *vertex_data, void **decl_data)
{
   LPDIRECT3DDEVICE9                    dev   = (LPDIRECT3DDEVICE9)_dev;
   const D3DVERTEXELEMENT9   *vertex_elements = (const D3DVERTEXELEMENT9*)vertex_data;
   LPDIRECT3DVERTEXDECLARATION9 **vertex_decl = (LPDIRECT3DVERTEXDECLARATION9**)decl_data;

   if (SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(dev,
               vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
      return true;

   return false;
}

static INLINE void d3d9_vertex_declaration_free(void *data)
{
   if (!data)
      return;

   IDirect3DVertexDeclaration9_Release((LPDIRECT3DVERTEXDECLARATION9)data);
}

static INLINE void d3d9_set_viewports(void *_dev, void *_vp)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   D3DVIEWPORT9      *vp = (D3DVIEWPORT9*)_vp;
   if (dev)
      IDirect3DDevice9_SetViewport(dev, vp);
}

static INLINE void d3d9_set_render_state(
      LPDIRECT3DDEVICE9 dev, D3DRENDERSTATETYPE state, DWORD value)
{
   IDirect3DDevice9_SetRenderState(dev, state, value);
}

static INLINE void d3d9_enable_blend_func(void *data)
{
   if (!data)
      return;

   d3d9_set_render_state(data, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d9_set_render_state(data, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d9_set_render_state(data, D3DRS_ALPHABLENDENABLE, true);
}

static INLINE void d3d9_disable_blend_func(void *data)
{
   d3d9_set_render_state(data, D3DRS_ALPHABLENDENABLE, false);
}

static INLINE void 
d3d9_set_vertex_declaration(void *data, void *vertex_data)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (dev)
      IDirect3DDevice9_SetVertexDeclaration(dev,
            (LPDIRECT3DVERTEXDECLARATION9)vertex_data);
}

static INLINE void d3d9_set_texture_stage_state(void *_dev,
      unsigned sampler, unsigned type, unsigned value)
{
#ifndef _XBOX
   /* XBox 360 has no fixed-function pipeline. */
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   if (IDirect3DDevice9_SetTextureStageState(dev, sampler,
            (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
      RARCH_ERR("SetTextureStageState call failed, sampler"
            ": %d, value: %d, type: %d\n", sampler, value, type);
#endif
}

static INLINE void d3d9_enable_alpha_blend_texture_func(void *data)
{
   /* Also blend the texture with the set alpha value. */
   d3d9_set_texture_stage_state(data, 0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
   d3d9_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG1,   D3DTA_DIFFUSE);
   d3d9_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG2,   D3DTA_TEXTURE);
}

void d3d9_frame_postprocess(void *data);

static INLINE void d3d9_surface_free(void *data)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   if (!surf)
      return;
   IDirect3DSurface9_Release(surf);
}

bool d3d9_device_get_render_target_data(void *dev,
      void *_src, void *_dst);

static INLINE bool d3d9_device_get_render_target(void *_dev,
      unsigned idx, void **data)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev &&
         SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,
               idx, (LPDIRECT3DSURFACE9*)data)))
      return true;

   return false;
}

static INLINE void d3d9_device_set_render_target(
      void *_dev, unsigned idx,
      void *data)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   LPDIRECT3DDEVICE9   dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev)
      IDirect3DDevice9_SetRenderTarget(dev, idx, surf);
}

static INLINE bool d3d9_get_render_state(
      void *data, INT32 state, DWORD *value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (!dev)
      return false;

#ifdef _XBOX
   IDirect3DDevice9_GetRenderState(dev,
         (D3DRENDERSTATETYPE)state, value);
#else
   if (IDirect3DDevice9_GetRenderState(dev,
            (D3DRENDERSTATETYPE)state, value) != D3D_OK)
      return false;
#endif
   return true;
}

void d3d9_device_set_render_target(void *dev, unsigned idx,
      void *data);

bool d3d9_device_create_offscreen_plain_surface(
      void *dev,
      unsigned width,
      unsigned height,
      unsigned format,
      unsigned pool,
      void **surf_data,
      void *data);

static INLINE bool d3d9_surface_lock_rect(void *data, void *data2)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   if (!surf)
      return false;
#if defined(_XBOX)
   IDirect3DSurface9_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY);
#else
   if (FAILED(IDirect3DSurface9_LockRect(surf,
               (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
      return false;
#endif

   return true;
}

static INLINE void d3d9_surface_unlock_rect(void *data)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   if (!surf)
      return;
   IDirect3DSurface9_UnlockRect(surf);
}
void d3d9_surface_unlock_rect(void *data);

static INLINE bool d3d9_get_adapter_display_mode(
      void *_d3d,
      unsigned idx,
      void *display_mode)
{
   LPDIRECT3D9 d3d = (LPDIRECT3D9)_d3d;
   if (!d3d)
      return false;
#ifndef _XBOX
   if (FAILED(IDirect3D9_GetAdapterDisplayMode(d3d, idx, (D3DDISPLAYMODE*)display_mode)))
      return false;
#endif

   return true;
}

bool d3d9_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id);

bool d3d9_reset(void *dev, void *d3dpp);

static INLINE bool d3d9_device_get_backbuffer(void *_dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev &&
         SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 
               swapchain_idx, idx, 
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE9*)data)))
      return true;
   return false;
}

void d3d9_device_free(void *dev, void *pd3d);

void *d3d9_create(void);

bool d3d9_initialize_symbols(enum gfx_ctx_api api);

void d3d9_deinitialize_symbols(void);

static INLINE bool d3d9_check_device_type(void *_d3d,
      unsigned idx,
      INT32 disp_format,
      INT32 backbuffer_format,
      bool windowed_mode)
{
   LPDIRECT3D9 d3d = (LPDIRECT3D9)_d3d;
   if (d3d &&
         SUCCEEDED(IDirect3D9_CheckDeviceType(d3d,
               0,
               D3DDEVTYPE_HAL,
               (D3DFORMAT)disp_format,
               (D3DFORMAT)backbuffer_format,
               windowed_mode)))
      return true;

   return false;
}

bool d3d9x_create_font_indirect(void *dev,
      void *desc, void **font_data);

void d3d9x_font_draw_text(void *data, void *sprite_data, void *string_data,
      unsigned count, void *rect_data, unsigned format, unsigned color);

void d3d9x_font_get_text_metrics(void *data, void *metrics);

void d3dxbuffer_release(void *data);

void d3d9x_font_release(void *data);

INT32 d3d9_translate_filter(unsigned type);

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

const void *d3d9x_get_buffer_ptr(void *data);

const bool d3d9x_constant_table_set_float(void *p,
      void *a, const void *b, float val);

static INLINE INT32 d3d9_get_rgb565_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_R5G6B5;
#else
   return D3DFMT_R5G6B5;
#endif
}

static INLINE INT32 d3d9_get_argb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_A8R8G8B8;
#else
   return D3DFMT_A8R8G8B8;
#endif
}

static INLINE INT32 d3d9_get_xrgb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_X8R8G8B8;
#else
   return D3DFMT_X8R8G8B8;
#endif
}

RETRO_END_DECLS

#endif
