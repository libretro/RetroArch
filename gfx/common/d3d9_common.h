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

RETRO_BEGIN_DECLS

typedef struct d3d9_video d3d9_video_t;

typedef struct d3d9_renderchain_driver
{
   void (*chain_free)(void *data);
   void *(*chain_new)(void);
   bool (*init)(d3d9_video_t *d3d,
         const video_info_t *video_info,
         LPDIRECT3DDEVICE9 dev,
         const D3DVIEWPORT9 *final_viewport,
         const struct LinkInfo *info,
         bool rgb32);
   void (*set_final_viewport)(d3d9_video_t *d3d,
         void *renderchain_data, const D3DVIEWPORT9 *final_viewport);
   bool (*add_pass)(void *data, const struct LinkInfo *info);
   bool (*add_lut)(void *data,
         const char *id, const char *path,
         bool smooth);
   bool (*render)(d3d9_video_t *d3d,
         const video_frame_info_t *video_info,
         const void *frame,
         unsigned width, unsigned height, unsigned pitch, unsigned rotation);
   const char *ident;
} d3d9_renderchain_driver_t;

typedef struct d3d9_video
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
   const d3d9_renderchain_driver_t *renderchain_driver;
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

static INLINE bool d3d9_swap(void *data, LPDIRECT3DDEVICE9 dev)
{
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

static INLINE void *d3d9_vertex_buffer_lock(LPDIRECT3DVERTEXBUFFER9 vertbuf)
{
   void *buf = NULL;
   if (!vertbuf)
      return NULL;
   IDirect3DVertexBuffer9_Lock(vertbuf, 0, 0, &buf, 0);

   if (!buf)
      return NULL;

   return buf;
}

static INLINE void d3d9_vertex_buffer_unlock(LPDIRECT3DVERTEXBUFFER9 vertbuf)
{
   if (vertbuf)
      IDirect3DVertexBuffer9_Unlock(vertbuf);
}

void d3d9_vertex_buffer_free(void *vertex_data, void *vertex_declaration);

static INLINE bool d3d9_texture_get_level_desc(
      LPDIRECT3DTEXTURE9 tex,
      unsigned idx,
      D3DSURFACE_DESC *_ppsurface_level)
{
#if defined(_XBOX)
   D3DTexture_GetLevelDesc(tex, idx, _ppsurface_level);
#else
   if (FAILED(IDirect3DTexture9_GetLevelDesc(tex, idx, _ppsurface_level)))
      return false;
#endif
   return true;
}

static INLINE bool d3d9_texture_get_surface_level(
      LPDIRECT3DTEXTURE9 tex,
      unsigned idx, void **_ppsurface_level)
{
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
      LPDIRECT3DDEVICE9 dev,
      unsigned stream_no,
      LPDIRECT3DVERTEXBUFFER9 stream_vertbuf,
      unsigned offset_bytes,
      unsigned stride)
{
   if (stream_vertbuf)
      IDirect3DDevice9_SetStreamSource(dev, stream_no, stream_vertbuf,
            offset_bytes,
            stride);
}

static INLINE void d3d9_texture_free(LPDIRECT3DTEXTURE9 tex)
{
   if (tex)
      IDirect3DTexture9_Release(tex);
}

static INLINE void d3d9_set_transform(
      LPDIRECT3DDEVICE9 dev,
      D3DTRANSFORMSTATETYPE state,
      CONST D3DMATRIX *matrix)
{
   /* XBox 360 D3D9 does not support fixed-function pipeline. */
#ifndef _XBOX
   IDirect3DDevice9_SetTransform(dev, state, matrix);
#endif
}

static INLINE void d3d9_set_sampler_address_u(
      LPDIRECT3DDEVICE9 dev,
      unsigned sampler, unsigned value)
{
   IDirect3DDevice9_SetSamplerState(dev,
         sampler, D3DSAMP_ADDRESSU, value);
}

static INLINE void d3d9_set_sampler_address_v(
      LPDIRECT3DDEVICE9 dev,
      unsigned sampler, unsigned value)
{
   IDirect3DDevice9_SetSamplerState(dev,
         sampler, D3DSAMP_ADDRESSV, value);
}

static INLINE void d3d9_set_sampler_minfilter(
      LPDIRECT3DDEVICE9 dev,
      unsigned sampler, unsigned value)
{
   if (dev)
      IDirect3DDevice9_SetSamplerState(dev,
            sampler, D3DSAMP_MINFILTER, value);
}

static INLINE void d3d9_set_sampler_magfilter(
      LPDIRECT3DDEVICE9 dev,
      unsigned sampler, unsigned value)
{
   if (dev)
      IDirect3DDevice9_SetSamplerState(dev,
            sampler, D3DSAMP_MAGFILTER, value);
}

static INLINE void d3d9_set_sampler_mipfilter(
      LPDIRECT3DDEVICE9 dev,
      unsigned sampler, unsigned value)
{
   if (dev)
      IDirect3DDevice9_SetSamplerState(dev, sampler,
            D3DSAMP_MIPFILTER, value);
}

static INLINE bool d3d9_begin_scene(LPDIRECT3DDEVICE9 dev)
{
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

static INLINE void d3d9_end_scene(LPDIRECT3DDEVICE9 dev)
{
   if (dev)
      IDirect3DDevice9_EndScene(dev);
}

static INLINE void d3d9_draw_primitive(
      LPDIRECT3DDEVICE9 dev,
      D3DPRIMITIVETYPE type,
      unsigned start, unsigned count)
{
   if (!dev || !d3d9_begin_scene(dev))
      return;
   IDirect3DDevice9_DrawPrimitive(dev, type, start, count);
   d3d9_end_scene(dev);
}

static INLINE void d3d9_clear(
      LPDIRECT3DDEVICE9 dev,
      unsigned count, const D3DRECT *rects, unsigned flags,
      INT32 color, float z, unsigned stencil)
{
   if (dev)
      IDirect3DDevice9_Clear(dev, count, rects, flags,
            color, z, stencil);
}

static INLINE bool d3d9_lock_rectangle(
      LPDIRECT3DTEXTURE9 tex,
      unsigned level,
      D3DLOCKED_RECT *lr,
      const RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   if (!tex)
      return false;
#ifdef _XBOX
   IDirect3DTexture9_LockRect(tex, level, lr, rect, flags);
#else
   if (IDirect3DTexture9_LockRect(tex, level, lr, rect, flags) != D3D_OK)
      return false;
#endif

   return true;
}

static INLINE void d3d9_unlock_rectangle(LPDIRECT3DTEXTURE9 tex)
{
   if (tex)
      IDirect3DTexture9_UnlockRect(tex, 0);
}

static INLINE void d3d9_lock_rectangle_clear(void *tex,
      unsigned level, D3DLOCKED_RECT *lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
#if defined(_XBOX)
   level              = 0;
#endif
   memset(lr->pBits, level, rectangle_height * lr->Pitch);
   d3d9_unlock_rectangle((LPDIRECT3DTEXTURE9)tex);
}

static INLINE void d3d9_set_texture(
      LPDIRECT3DDEVICE9 dev,
      unsigned sampler,
      LPDIRECT3DTEXTURE9 tex)
{
   if (dev && tex)
      IDirect3DDevice9_SetTexture(dev, sampler,
            (IDirect3DBaseTexture9*)tex);
}

static INLINE bool d3d9_create_vertex_shader(
      LPDIRECT3DDEVICE9 dev, const DWORD *a, void **b)
{
   if (dev && IDirect3DDevice9_CreateVertexShader(dev, a,
            (LPDIRECT3DVERTEXSHADER9*)b) == D3D_OK)
      return true;
   return false;
}

static INLINE bool d3d9_create_pixel_shader(
      LPDIRECT3DDEVICE9 dev, const DWORD *a, void **b)
{
   if (dev &&
         IDirect3DDevice9_CreatePixelShader(dev, a,
            (LPDIRECT3DPIXELSHADER9*)b) == D3D_OK)
      return true;
   return false;
}

static INLINE void d3d9_free_vertex_shader(
      LPDIRECT3DDEVICE9 dev, IDirect3DVertexShader9 *vs)
{
   if (dev && vs)
      IDirect3DVertexShader9_Release(vs);
}

static INLINE void d3d9_free_pixel_shader(LPDIRECT3DDEVICE9 dev,
      IDirect3DPixelShader9 *ps)
{
   if (dev && ps)
      IDirect3DPixelShader9_Release(ps);
}

static INLINE bool d3d9_set_pixel_shader(
      LPDIRECT3DDEVICE9 dev,
      LPDIRECT3DPIXELSHADER9 shader)
{
#ifdef _XBOX
   /* Returns void on Xbox */
   IDirect3DDevice9_SetPixelShader(dev, shader);
#else
   if (IDirect3DDevice9_SetPixelShader(dev, shader) != D3D_OK)
      return false;
#endif
   return true;
}

static INLINE bool d3d9_set_vertex_shader(
      LPDIRECT3DDEVICE9 dev, LPDIRECT3DVERTEXSHADER9 shader)
{
#ifdef _XBOX
   IDirect3DDevice9_SetVertexShader(dev, shader);
#else
   if (IDirect3DDevice9_SetVertexShader(dev, shader) != D3D_OK)
      return false;
#endif

   return true;
}

static INLINE bool d3d9_set_vertex_shader_constantf(
      LPDIRECT3DDEVICE9 dev,
      UINT start_register,
      const float* constant_data,
      unsigned vector4f_count)
{
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

static INLINE void d3d9_texture_blit(
      unsigned pixel_size,
      void *tex,
      D3DLOCKED_RECT *lr, const void *frame,
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

static INLINE void d3d9_vertex_declaration_free(
      LPDIRECT3DVERTEXDECLARATION9 decl)
{
   if (decl)
      IDirect3DVertexDeclaration9_Release(decl);
}

static INLINE void d3d9_set_viewports(LPDIRECT3DDEVICE9 dev,
      void *vp)
{
   if (dev)
      IDirect3DDevice9_SetViewport(dev, (D3DVIEWPORT9*)vp);
}

static INLINE void d3d9_set_scissor_rect(
      LPDIRECT3DDEVICE9 dev, RECT *rect)
{
   IDirect3DDevice9_SetScissorRect(dev, rect);
}

static INLINE void d3d9_set_render_state(
      LPDIRECT3DDEVICE9 dev, D3DRENDERSTATETYPE state, DWORD value)
{
   IDirect3DDevice9_SetRenderState(dev, state, value);
}

static INLINE void d3d9_enable_blend_func(LPDIRECT3DDEVICE9 dev)
{
   if (!dev)
      return;

   d3d9_set_render_state(dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d9_set_render_state(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d9_set_render_state(dev, D3DRS_ALPHABLENDENABLE, true);
}

static INLINE void d3d9_disable_blend_func(LPDIRECT3DDEVICE9 dev)
{
   if (dev)
      d3d9_set_render_state(dev, D3DRS_ALPHABLENDENABLE, false);
}

static INLINE void
d3d9_set_vertex_declaration(LPDIRECT3DDEVICE9 dev,
      LPDIRECT3DVERTEXDECLARATION9 vertex_data)
{
   if (dev)
      IDirect3DDevice9_SetVertexDeclaration(dev, vertex_data);
}

static INLINE void d3d9_set_texture_stage_state(
      LPDIRECT3DDEVICE9 dev,
      unsigned sampler,
      D3DTEXTURESTAGESTATETYPE type,
      unsigned value)
{
#ifndef _XBOX
   /* XBox 360 has no fixed-function pipeline. */
   if (IDirect3DDevice9_SetTextureStageState(dev, sampler,
            type, value) != D3D_OK)
      RARCH_ERR("SetTextureStageState call failed, sampler"
            ": %d, value: %d, type: %d\n", sampler, value, type);
#endif
}

static INLINE void d3d9_enable_alpha_blend_texture_func(LPDIRECT3DDEVICE9 dev)
{
   if (!dev)
      return;

   /* Also blend the texture with the set alpha value. */
   d3d9_set_texture_stage_state(dev, 0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
   d3d9_set_texture_stage_state(dev, 0, D3DTSS_ALPHAARG1,   D3DTA_DIFFUSE);
   d3d9_set_texture_stage_state(dev, 0, D3DTSS_ALPHAARG2,   D3DTA_TEXTURE);
}

void d3d9_frame_postprocess(void *data);

static INLINE void d3d9_surface_free(LPDIRECT3DSURFACE9 surf)
{
   if (surf)
      IDirect3DSurface9_Release(surf);
}

static INLINE bool d3d9_device_get_render_target_data(
      LPDIRECT3DDEVICE9 dev,
      LPDIRECT3DSURFACE9 src, LPDIRECT3DSURFACE9 dst)
{
#ifndef _XBOX
   if (dev &&
         SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(
               dev, src, dst)))
      return true;
#endif

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

static INLINE void d3d9_device_set_render_target(
      LPDIRECT3DDEVICE9 dev, unsigned idx,
      LPDIRECT3DSURFACE9 surf)
{
   if (dev)
      IDirect3DDevice9_SetRenderTarget(dev, idx, surf);
}

static INLINE bool d3d9_get_render_state(
      LPDIRECT3DDEVICE9 dev, INT32 state, DWORD *value)
{
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

static INLINE bool d3d9_surface_lock_rect(LPDIRECT3DSURFACE9 surf,
      D3DLOCKED_RECT *data2)
{
   if (!surf)
      return false;
#if defined(_XBOX)
   IDirect3DSurface9_LockRect(surf,
         data2, NULL, D3DLOCK_READONLY);
#else
   if (FAILED(IDirect3DSurface9_LockRect(surf,
               data2, NULL, D3DLOCK_READONLY)))
      return false;
#endif

   return true;
}

static INLINE void d3d9_surface_unlock_rect(LPDIRECT3DSURFACE9 surf)
{
   if (surf)
      IDirect3DSurface9_UnlockRect(surf);
}

static INLINE bool d3d9_get_adapter_display_mode(
      LPDIRECT3D9 d3d,
      unsigned idx,
      D3DDISPLAYMODE *display_mode)
{
   if (!d3d)
      return false;
#ifndef _XBOX
   if (FAILED(
            IDirect3D9_GetAdapterDisplayMode(
               d3d, idx, display_mode)))
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

static INLINE bool d3d9_device_get_backbuffer(
      LPDIRECT3DDEVICE9 dev,
      unsigned idx, unsigned swapchain_idx,
      unsigned backbuffer_type, void **data)
{
   if (dev &&
         SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev,
               swapchain_idx, idx,
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE9*)data)))
      return true;
   return false;
}

static INLINE void d3d9_device_free(LPDIRECT3DDEVICE9 dev, LPDIRECT3D9 pd3d)
{
   if (dev)
      IDirect3DDevice9_Release(dev);
   if (pd3d)
      IDirect3D9_Release(pd3d);
}

void *d3d9_create(void);

bool d3d9_initialize_symbols(enum gfx_ctx_api api);

void d3d9_deinitialize_symbols(void);

static INLINE bool d3d9_check_device_type(
      LPDIRECT3D9 d3d,
      unsigned idx,
      INT32 disp_format,
      INT32 backbuffer_format,
      bool windowed_mode)
{
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

void d3d9x_buffer_release(void *data);

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

const void *d3d9x_get_buffer_ptr(void *data);

const bool d3d9x_constant_table_set_float(void *p,
      void *a, void *b, float val);

void *d3d9x_constant_table_get_constant_by_name(void *_tbl,
      void *_handle, void *_name);

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

static INLINE void d3d9_convert_geometry(
      const struct LinkInfo *info,
      unsigned *out_width,
      unsigned *out_height,
      unsigned width,
      unsigned height,
      D3DVIEWPORT9 *final_viewport)
{
   if (!info)
      return;

   switch (info->pass->fbo.type_x)
   {
      case RARCH_SCALE_VIEWPORT:
         *out_width = info->pass->fbo.scale_x * final_viewport->Width;
         break;

      case RARCH_SCALE_ABSOLUTE:
         *out_width = info->pass->fbo.abs_x;
         break;

      case RARCH_SCALE_INPUT:
         *out_width = info->pass->fbo.scale_x * width;
         break;
   }

   switch (info->pass->fbo.type_y)
   {
      case RARCH_SCALE_VIEWPORT:
         *out_height = info->pass->fbo.scale_y * final_viewport->Height;
         break;

      case RARCH_SCALE_ABSOLUTE:
         *out_height = info->pass->fbo.abs_y;
         break;

      case RARCH_SCALE_INPUT:
         *out_height = info->pass->fbo.scale_y * height;
         break;
   }
}

void d3d9_set_mvp(void *data, const void *userdata);

RETRO_END_DECLS

#endif
