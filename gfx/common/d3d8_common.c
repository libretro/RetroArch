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

/* For Xbox we will just link statically 
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
#endif

#include "../../configuration.h"
#include "../../verbosity.h"

#include <d3d8.h>

#ifdef HAVE_D3DX
#ifdef _XBOX
#include <d3dx8core.h>
#include <d3dx8tex.h>
#else
#include "../include/d3d8/d3dx8tex.h"
#endif
#endif

#include "d3d8_common.h"

#ifdef _XBOX
#include <xgraphics.h>
#endif

static UINT SDKVersion = 0;

#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d8_dll;
#ifdef HAVE_D3DX
static dylib_t g_d3d8x_dll;
#endif
static bool dylib_initialized = false;
#endif

typedef IDirect3D8 *(__stdcall *D3DCreate_t)(UINT);
#ifdef HAVE_D3DX
typedef HRESULT (__stdcall
    *D3DCreateTextureFromFile_t)(
        LPDIRECT3DDEVICE8         pDevice,
        LPCSTR                    pSrcFile,
        UINT                      Width,
        UINT                      Height,
        UINT                      MipLevels,
        DWORD                     Usage,
        D3DFORMAT                 Format,
        D3DPOOL                   Pool,
        DWORD                     Filter,
        DWORD                     MipFilter,
        D3DCOLOR                  ColorKey,
        D3DXIMAGE_INFO*           pSrcInfo,
        PALETTEENTRY*             pPalette,
        LPDIRECT3DTEXTURE8*       ppTexture);

typedef HRESULT (__stdcall
    *D3DXCreateFontIndirect_t)(
        LPDIRECT3DDEVICE8       pDevice,
        CONST LOGFONT*   pDesc,
        LPD3DXFONT*             ppFont);
#endif


#ifdef HAVE_D3DX
static D3DXCreateFontIndirect_t   D3DCreateFontIndirect;
static D3DCreateTextureFromFile_t D3DCreateTextureFromFile;
#endif
static D3DCreate_t D3DCreate;

void *d3d8_create(void)
{
   return D3DCreate(SDKVersion);
}

#ifdef HAVE_DYNAMIC_D3D
#ifdef HAVE_D3DX
static dylib_t dylib_load_d3dx(void)
{
   dylib_t dll           = NULL;

   return dll;
}
#endif

#endif

bool d3d8_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (dylib_initialized)
      return true;

#if defined(DEBUG) || defined(_DEBUG)
   g_d3d8_dll     = dylib_load("d3d8d.dll");
   if(!g_d3d8_dll)
#endif
      g_d3d8_dll  = dylib_load("d3d8.dll");

   if (!g_d3d8_dll)
      return false;
#endif
   
   SDKVersion               = 220;
#ifdef HAVE_DYNAMIC_D3D
   D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d8_dll, "Direct3DCreate8");
#ifdef HAVE_D3DX
#ifdef UNICODE
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3d8x_dll, "D3DXCreateFontIndirectW");
#else
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3d8x_dll, "D3DXCreateFontIndirectA");
#endif
   D3DCreateTextureFromFile = (D3DCreateTextureFromFile_t)dylib_proc(g_d3d8x_dll, "D3DXCreateTextureFromFileExA");
#endif
#else
   D3DCreate                = Direct3DCreate8;
#ifdef HAVE_D3DX
   D3DCreateFontIndirect    = D3DXCreateFontIndirect;
   D3DCreateTextureFromFile = D3DXCreateTextureFromFileExA;
#endif
#endif

   if (!D3DCreate)
      goto error;

#ifdef _XBOX
   SDKVersion        = 0;
#endif
#ifdef HAVE_DYNAMIC_D3D
   dylib_initialized = true;
#endif

   return true;

error:
   d3d8_deinitialize_symbols();
   return false;
}

void d3d8_deinitialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (g_d3d8_dll)
      dylib_close(g_d3d8_dll);
#ifdef HAVE_D3DX
   if (g_d3d8x_dll)
      dylib_close(g_d3d8x_dll);
   g_d3d8x_dll        = NULL;
#endif
   g_d3d8_dll         = NULL;

   dylib_initialized = false;
#endif
}

bool d3d8_check_device_type(void *_d3d,
      unsigned idx,
      INT32 disp_format,
      INT32 backbuffer_format,
      bool windowed_mode)
{
   LPDIRECT3D8 d3d = (LPDIRECT3D8)_d3d;
   if (!d3d)
      return false;
#ifdef __cplusplus
   if (FAILED(d3d->CheckDeviceType(
               0,
               D3DDEVTYPE_HAL,
               (D3DFORMAT)disp_format,
               (D3DFORMAT)backbuffer_format,
               windowed_mode)))
      return false;
#else
   if (FAILED(IDirect3D8_CheckDeviceType(d3d,
               0,
               D3DDEVTYPE_HAL,
               disp_format,
               backbuffer_format,
               windowed_mode)))
      return false;
#endif

   return true;
}

bool d3d8_get_adapter_display_mode(
      void *_d3d,
      unsigned idx,
      void *display_mode)
{
   LPDIRECT3D8 d3d = (LPDIRECT3D8)_d3d;
   if (!d3d)
      return false;
#ifdef __cplusplus
   if (FAILED(d3d->GetAdapterDisplayMode(idx, (D3DDISPLAYMODE*)display_mode)))
      return false;
#else
   if (FAILED(IDirect3D8_GetAdapterDisplayMode(d3d, idx, (D3DDISPLAYMODE*)display_mode)))
      return false;
#endif

   return true;
}

bool d3d8_swap(void *data, void *_dev)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
   if (dev->Present(NULL, NULL, NULL, NULL) != D3D_OK)
      return false;
#else
   if (IDirect3DDevice8_Present(dev, NULL, NULL, NULL, NULL) 
         == D3DERR_DEVICELOST)
      return false;
#endif
   return true;
}

void d3d8_set_transform(void *_dev,
      INT32 state, const void *_matrix)
{
   CONST D3DMATRIX *matrix = (CONST D3DMATRIX*)_matrix;
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
   dev->SetTransform((D3DTRANSFORMSTATETYPE)state, matrix);
#else
   IDirect3DDevice8_SetTransform(dev, (D3DTRANSFORMSTATETYPE)state, matrix);
#endif
}

bool d3d8_texture_get_level_desc(void *_tex,
      unsigned idx, void *_ppsurface_level)
{
   LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
#ifdef __cplusplus
   if (SUCCEEDED(tex->GetLevelDesc(idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
#else
   if (SUCCEEDED(IDirect3DTexture8_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
#endif

   return false;
}

bool d3d8_texture_get_surface_level(void *_tex,
      unsigned idx, void **_ppsurface_level)
{
   LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
   if (!tex)
      return false;
#ifdef __cplusplus
   if (SUCCEEDED(tex->GetSurfaceLevel(idx, (IDirect3DSurface8**)_ppsurface_level)))
      return true;
#else
   if (SUCCEEDED(IDirect3DTexture8_GetSurfaceLevel(tex, idx, (IDirect3DSurface8**)_ppsurface_level)))
      return true;
#endif

   return false;
}

#ifdef HAVE_D3DX
static void *d3d8_texture_new_from_file(
      void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette)
{
   void *buf  = NULL;
   HRESULT hr = D3DCreateTextureFromFile((LPDIRECT3DDEVICE8)dev,
         path, width, height, miplevels, usage, format,
         (D3DPOOL)pool, filter, mipfilter, color_key, src_info_data,
         palette, (struct IDirect3DTeture8**)&buf);

   if (FAILED(hr))
      return NULL;

   return buf;
}
#endif

void *d3d8_texture_new(void *_dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   HRESULT hr            = S_OK;
   void *buf             = NULL;

   if (path)
   {
#ifdef HAVE_D3DX
      return d3d8_texture_new_from_file(_dev,
            path, width, height, miplevels,
            usage, (D3DFORMAT)format,
            (D3DPOOL)pool, filter, mipfilter,
            color_key, src_info_data, palette);
#else
      return NULL;
#endif
   }

   {
      LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
      hr = dev->CreateTexture(
            width, height, miplevels, usage,
            (D3DFORMAT)format, (D3DPOOL)pool, (IDirect3DTexture8**)&buf);
#else
      hr = IDirect3DDevice8_CreateTexture(dev,
            width, height, miplevels, usage,
            (D3DFORMAT)format, (D3DPOOL)pool, (struct IDirect3DTexture8**)&buf);
#endif
   }

   if (FAILED(hr))
      return NULL;

   return buf;
}

void d3d8_texture_free(void *_tex)
{
   LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
   if (!tex)
      return;
#ifdef __cplusplus
   tex->Release();
#else
   IDirect3DTexture8_Release(tex);
#endif
}

bool d3d8_surface_lock_rect(void *data, void *data2)
{
   LPDIRECT3DSURFACE8 surf = (LPDIRECT3DSURFACE8)data;
   if (!surf)
      return false;
#ifdef __cplusplus
   if (FAILED(surf->LockRect((D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
      return false;
#else
   if (FAILED(IDirect3DSurface8_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
      return false;
#endif

   return true;
}

void d3d8_surface_unlock_rect(void *data)
{
   LPDIRECT3DSURFACE8 surf = (LPDIRECT3DSURFACE8)data;
   if (!surf)
      return;
#ifdef __cplusplus
   surf->UnlockRect();
#else
   IDirect3DSurface8_UnlockRect(surf);
#endif
}

void d3d8_surface_free(void *data)
{
   LPDIRECT3DSURFACE8 surf = (LPDIRECT3DSURFACE8)data;
   if (!surf)
      return;
#ifdef __cplusplus
   surf->Release();
#else
   IDirect3DSurface8_Release(surf);
#endif
}

void *d3d8_vertex_buffer_new(void *_dev,
      unsigned length, unsigned usage,
      unsigned fvf, INT32 pool, void *handle)
{
   void              *buf = NULL;
   LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
   HRESULT             hr = dev->CreateVertexBuffer(
         length, usage, fvf, (D3DPOOL)pool, (IDirect3DVertexBuffer8**)&buf);
#else
   HRESULT             hr = IDirect3DDevice8_CreateVertexBuffer(
         dev, length, usage, fvf,
         (D3DPOOL)pool,
         (struct IDirect3DVertexBuffer8**)&buf);
#endif

   if (FAILED(hr))
      return NULL;

   return buf;
}

void d3d8_vertex_buffer_unlock(void *vertbuf_ptr)
{
   LPDIRECT3DVERTEXBUFFER8 vertbuf = (LPDIRECT3DVERTEXBUFFER8)vertbuf_ptr;

   if (!vertbuf)
      return;
#ifdef __cplusplus
   vertbuf->Unlock();
#else
   IDirect3DVertexBuffer8_Unlock(vertbuf);
#endif
}

void *d3d8_vertex_buffer_lock(void *vertbuf_ptr)
{
   void                       *buf = NULL;
   LPDIRECT3DVERTEXBUFFER8 vertbuf = (LPDIRECT3DVERTEXBUFFER8)vertbuf_ptr;

   if (!vertbuf)
      return NULL;

#ifdef __cplusplus
   vertbuf->Lock(0, 0, (BYTE**)&buf, 0);
#else
   IDirect3DVertexBuffer8_Lock(vertbuf, 0, 0, (BYTE**)&buf, 0);
#endif

   if (!buf)
      return NULL;

   return buf;
}

void d3d8_vertex_buffer_free(void *vertex_data, void *vertex_declaration)
{
   if (vertex_data)
   {
      LPDIRECT3DVERTEXBUFFER8 buf = (LPDIRECT3DVERTEXBUFFER8)vertex_data;
#ifdef __cplusplus
      buf->Release();
#else
      IDirect3DVertexBuffer8_Release(buf);
#endif
      buf = NULL;
   }
}

void d3d8_set_stream_source(void *_dev, unsigned stream_no,
      void *stream_vertbuf_ptr, unsigned offset_bytes,
      unsigned stride)
{
   LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
   LPDIRECT3DVERTEXBUFFER8 stream_vertbuf = (LPDIRECT3DVERTEXBUFFER8)stream_vertbuf_ptr;
   if (!stream_vertbuf)
      return;
#ifdef __cplusplus
   dev->SetStreamSource(stream_no, stream_vertbuf, offset_bytes, stride);
#else
   IDirect3DDevice8_SetStreamSource(dev, stream_no, stream_vertbuf, stride);
#endif
}

static void d3d8_set_texture_stage_state(void *_dev,
      unsigned sampler, unsigned type, unsigned value)
{
   LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
   if (dev->SetTextureStageState(sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
      RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#else
   if (IDirect3DDevice8_SetTextureStageState(dev, sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
      RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#endif
}

void d3d8_set_sampler_address_u(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
   d3d8_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSU, value);
}

void d3d8_set_sampler_address_v(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
   d3d8_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSV, value);
}

void d3d8_set_sampler_minfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   d3d8_set_texture_stage_state(_dev, sampler, D3DTSS_MINFILTER, value);
}

void d3d8_set_sampler_magfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   d3d8_set_texture_stage_state(_dev, sampler, D3DTSS_MAGFILTER, value);
}

bool d3d8_begin_scene(void *_dev)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (!dev)
      return false;
#ifdef __cplusplus
#ifdef _XBOX
   dev->BeginScene();
#else
   if (FAILED(dev->BeginScene()))
      return false;
#endif
#else
#ifdef _XBOX
   IDirect3DDevice8_BeginScene(dev);
#else
   if (FAILED(IDirect3DDevice8_BeginScene(dev)))
      return false;
#endif
#endif

   return true;
}

void d3d8_end_scene(void *_dev)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (!dev)
      return;
#ifdef __cplusplus
   dev->EndScene();
#else
   IDirect3DDevice8_EndScene(dev);
#endif
}

static void d3d8_draw_primitive_internal(void *_dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (!dev)
      return;
#ifdef __cplusplus
   dev->DrawPrimitive(type, start, count);
#else
   IDirect3DDevice8_DrawPrimitive(dev, type, start, count);
#endif
}

void d3d8_draw_primitive(void *dev,
      INT32 type, unsigned start, unsigned count)
{
   if (!d3d8_begin_scene(dev))
      return;

   d3d8_draw_primitive_internal(dev, (D3DPRIMITIVETYPE)type, start, count);
   d3d8_end_scene(dev);
}

void d3d8_clear(void *_dev,
      unsigned count, const void *rects, unsigned flags,
      INT32 color, float z, unsigned stencil)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (!dev)
      return;
#ifdef __cplusplus
   dev->Clear(count, (const D3DRECT*)rects, flags, color, z, stencil);
#else
   IDirect3DDevice8_Clear(dev, count, (const D3DRECT*)rects, flags,
         color, z, stencil);
#endif
}

bool d3d8_device_get_render_target(void *_dev,
      unsigned idx, void **data)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (!dev)
      return false;
#ifdef __cplusplus
   if (SUCCEEDED(dev->GetRenderTarget(
               (LPDIRECT3DSURFACE8*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice8_GetRenderTarget(dev,
               (LPDIRECT3DSURFACE8*)data)))
      return true;
#endif

   return false;
}


bool d3d8_lock_rectangle(void *_tex,
      unsigned level, void *_lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   D3DLOCKED_RECT     *lr = (D3DLOCKED_RECT*)_lr;
   LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
   if (!tex)
      return false;
#ifdef __cplusplus
   if (FAILED(tex->LockRect(level, lr, rect, flags)))
      return false;
#else
   if (IDirect3DTexture8_LockRect(tex, level, lr, rect, flags) != D3D_OK)
      return false;
#endif

   return true;
}

void d3d8_unlock_rectangle(void *_tex)
{
   LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
   if (!tex)
      return;
#ifdef __cplusplus
   tex->UnlockRect(0);
#else
   IDirect3DTexture8_UnlockRect(tex, 0);
#endif
}

void d3d8_lock_rectangle_clear(void *tex,
      unsigned level, void *_lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   D3DLOCKED_RECT *lr = (D3DLOCKED_RECT*)_lr;
#if defined(_XBOX)
   level = 0;
#endif
   memset(lr->pBits, level, rectangle_height * lr->Pitch);
   d3d8_unlock_rectangle(tex);
}

void d3d8_set_viewports(void *_dev, void *_vp)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   D3DVIEWPORT8      *vp = (D3DVIEWPORT8*)_vp;
   if (!dev)
      return;
#ifdef __cplusplus
   dev->SetViewport(vp);
#else
   IDirect3DDevice8_SetViewport(dev, vp);
#endif
}

void d3d8_set_texture(void *_dev, unsigned sampler,
      void *tex_data)
{
   LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)tex_data;
   LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
   if (!dev || !tex)
      return;
#ifdef __cplusplus
   dev->SetTexture(sampler, tex);
#else
   IDirect3DDevice8_SetTexture(dev, sampler,
         (IDirect3DBaseTexture8*)tex);
#endif
}

bool d3d8_set_vertex_shader(void *_dev, unsigned index,
      void *data)
{
   LPDIRECT3DDEVICE8      dev     = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
   LPDIRECT3DVERTEXSHADER8 shader = (LPDIRECT3DVERTEXSHADER8)data;

   if (dev->SetVertexShader(shader) != D3D_OK)
      return false;
#else
   if (IDirect3DDevice8_SetVertexShader(dev, index) != D3D_OK)
      return false;
#endif

   return true;
}

void d3d8_texture_blit(unsigned pixel_size,
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

bool d3d8_get_render_state(void *data, INT32 state, DWORD *value)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
#ifdef __cplusplus
   if (dev && dev->GetRenderState((D3DRENDERSTATETYPE)state, value) == D3D_OK)
      return true;
#else
   if (dev && IDirect3DDevice8_GetRenderState(dev, (D3DRENDERSTATETYPE)state, value) == D3D_OK)
      return true;
#endif

   return false;
}

void d3d8_set_render_state(void *data, INT32 state, DWORD value)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
   if (!dev)
      return;
#ifdef __cplusplus
   dev->SetRenderState((D3DRENDERSTATETYPE)state, value);
#else
   IDirect3DDevice8_SetRenderState(dev, (D3DRENDERSTATETYPE)state, value);
#endif
}

void d3d8_enable_blend_func(void *data)
{
   if (!data)
      return;

   d3d8_set_render_state(data, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d8_set_render_state(data, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d8_set_render_state(data, D3DRS_ALPHABLENDENABLE, true);
}

void d3d8_device_set_render_target(void *_dev, unsigned idx,
      void *data)
{
   LPDIRECT3DSURFACE8 surf = (LPDIRECT3DSURFACE8)data;
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (!dev)
      return;
#ifdef __cplusplus
   dev->SetRenderTarget(idx, surf);
#else
   IDirect3DDevice8_SetRenderTarget(dev, surf, NULL);
#endif
}

void d3d8_enable_alpha_blend_texture_func(void *data)
{
   /* Also blend the texture with the set alpha value. */
   d3d8_set_texture_stage_state(data, 0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
   d3d8_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG1,   D3DTA_DIFFUSE);
   d3d8_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG2,   D3DTA_TEXTURE);
}

void d3d8_frame_postprocess(void *data)
{
#if defined(_XBOX)
   global_t        *global = global_get_ptr();
#ifdef __cplusplus
   LPDIRECT3DDEVICE8   dev = (LPDIRECT3DDEVICE8)data;
   if (!dev)
      return;

   dev->SetFlickerFilter(global->console.screen.flicker_filter_index);
   dev->SetSoftDisplayFilter(global->console.softfilter_enable);
#else
   D3DDevice_SetFlickerFilter(global->console.screen.flicker_filter_index);
   D3DDevice_SetSoftDisplayFilter(global->console.softfilter_enable);
#endif
#endif
}

void d3d8_disable_blend_func(void *data)
{
   d3d8_set_render_state(data, D3DRS_ALPHABLENDENABLE, false);
}

static bool d3d8_reset_internal(void *data,
      D3DPRESENT_PARAMETERS *d3dpp
      )
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
   if (!dev)
      return false;
#ifdef __cplusplus
   if ((dev->Reset(d3dpp) == D3D_OK))
      return true;
#else
   if (IDirect3DDevice8_Reset(dev, d3dpp) == D3D_OK)
      return true;
#endif

   return false;
}

static HRESULT d3d8_test_cooperative_level(void *data)
{
#ifdef _XBOX
   return E_FAIL;
#else
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
   if (!dev)
      return E_FAIL;

#ifdef __cplusplus
   return dev->TestCooperativeLevel();
#else
   return IDirect3DDevice8_TestCooperativeLevel(dev);
#endif
#endif
}

static bool d3d8_create_device_internal(
      void *data,
      D3DPRESENT_PARAMETERS *d3dpp,
      void *_d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
   LPDIRECT3D8       d3d = (LPDIRECT3D8)_d3d;
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
   if (!dev)
      return false;
#ifdef __cplusplus
   if (SUCCEEDED(d3d->CreateDevice(
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice8**)dev)))
      return true;
#else
   if (SUCCEEDED(IDirect3D8_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice8**)dev)))
      return true;
#endif

   return false;
}

bool d3d8_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id)
{
   if (!d3d8_create_device_internal(dev,
            (D3DPRESENT_PARAMETERS*)d3dpp,
            d3d,
            focus_window,
            cur_mon_id,
            D3DCREATE_HARDWARE_VERTEXPROCESSING))
      if (!d3d8_create_device_internal(
               dev,
               (D3DPRESENT_PARAMETERS*)d3dpp, d3d, focus_window,
               cur_mon_id,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING))
         return false;
   return true;
}

bool d3d8_reset(void *dev, void *d3dpp)
{
   const char *err = NULL;

   if (d3d8_reset_internal(dev, (D3DPRESENT_PARAMETERS*)d3dpp))
      return true;

   RARCH_WARN("[D3D]: Attempting to recover from dead state...\n");

#ifndef _XBOX
   /* Try to recreate the device completely. */
   switch (d3d8_test_cooperative_level(dev))
   {
      case D3DERR_DEVICELOST:
         err = "DEVICELOST";
         break;

      case D3DERR_DEVICENOTRESET:
         err = "DEVICENOTRESET";
         break;

      case D3DERR_DRIVERINTERNALERROR:
         err = "DRIVERINTERNALERROR";
         break;

      default:
         err = "Unknown";
   }
   RARCH_WARN("[D3D]: recovering from dead state: (%s).\n", err);
#endif

   return false;
}

bool d3d8_device_get_backbuffer(void *_dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data)
{
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (!dev)
      return false;
#ifdef __cplusplus
   if (SUCCEEDED(dev->GetBackBuffer(idx,
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE8*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice8_GetBackBuffer(dev, idx,
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE8*)data)))
      return true;
#endif

   return false;
}


void d3d8_device_free(void *_dev, void *_pd3d)
{
   LPDIRECT3D8      pd3d = (LPDIRECT3D8)_pd3d;
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (dev)
   {
#ifdef __cplusplus
      dev->Release();
#else
      IDirect3DDevice8_Release(dev);
#endif
   }

   if (pd3d)
   {
#if defined(__cplusplus)
      pd3d->Release();
#else
      IDirect3D8_Release(pd3d);
#endif
   }
}

INT32 d3d8_translate_filter(unsigned type)
{
   switch (type)
   {
      case RARCH_FILTER_UNSPEC:
         {
            settings_t *settings = config_get_ptr();
            if (!settings->bools.video_smooth)
               break;
         }
         /* fall-through */
      case RARCH_FILTER_LINEAR:
         return D3DTEXF_LINEAR;
      case RARCH_FILTER_NEAREST:
         break;
   }

   return D3DTEXF_POINT;
}

bool d3d8x_create_font_indirect(void *_dev,
      void *desc, void **font_data)
{
#ifdef HAVE_D3DX
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
   if (SUCCEEDED(D3DCreateFontIndirect(
               dev, (CONST LOGFONT*)desc,
               (struct ID3DXFont**)font_data)))
      return true;
#endif

   return false;
}

void d3d8x_font_draw_text(void *data, void *sprite_data, void *string_data,
      unsigned count, void *rect_data, unsigned format, unsigned color)
{
#ifdef HAVE_D3DX
#if !defined(__cplusplus) || defined(CINTERFACE)
   ID3DXFont *font = (ID3DXFont*)data;
   if (!font)
      return;
   font->lpVtbl->DrawText(font, (LPD3DXSPRITE)sprite_data,
         (LPCTSTR)string_data, count, (LPRECT)rect_data,
         (DWORD)format, (D3DCOLOR)color);
#else
   LPD3DXFONT font = (LPD3DXFONT)data;
   if (!font)
      return;
   font->DrawText((LPD3DXSPRITE)sprite_data,
         (LPCTSTR)string_data, count, (LPRECT)rect_data,
         (DWORD)format, (D3DCOLOR)color);
#endif
#endif
}

void d3d8x_font_release(void *data)
{
#ifdef HAVE_D3DX
#if !defined(__cplusplus) || defined(CINTERFACE)
   ID3DXFont *font = (ID3DXFont*)data;
   if (!font)
      return;
   font->lpVtbl->Release(font);
#else
   LPD3DXFONT font = (LPD3DXFONT)data;
   if (!font)
      return;
   font->Release();
#endif
#endif
}

void d3d8x_font_get_text_metrics(void *data, void *metrics)
{
#ifdef HAVE_D3DX
#if !defined(__cplusplus) || defined(CINTERFACE)
   ID3DXFont *font = (ID3DXFont*)data;
   if (!font)
      return;
   font->lpVtbl->GetTextMetrics(font, (TEXTMETRICA*)metrics);
#else
   LPD3DXFONT font = (LPD3DXFONT)data;
   if (!font)
      return;
   font->GetTextMetricsA((TEXTMETRICA*)metrics);
#endif
#endif
}

INT32 d3d8_get_rgb565_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_R5G6B5;
#else
   return D3DFMT_R5G6B5;
#endif
}

INT32 d3d8_get_argb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_A8R8G8B8;
#else
   return D3DFMT_A8R8G8B8;
#endif
}

INT32 d3d8_get_xrgb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_X8R8G8B8;
#else
   return D3DFMT_X8R8G8B8;
#endif
}
