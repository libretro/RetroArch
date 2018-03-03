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

#include <d3d9.h>

#ifdef HAVE_D3DX
#ifdef _XBOX
#include <d3dx9core.h>
#include <d3dx9tex.h>
#else
#include "../include/d3d9/d3dx9tex.h"
#endif

#endif

#include "d3d9_common.h"

#ifdef _XBOX
#include <xgraphics.h>
#endif

static UINT SDKVersion = 0;

#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d9_dll;
#ifdef HAVE_D3DX
static dylib_t g_d3d9x_dll;
#endif
static bool d3d9_dylib_initialized = false;
#endif

typedef IDirect3D9 *(__stdcall *D3DCreate_t)(UINT);
#ifdef HAVE_D3DX
typedef HRESULT (__stdcall
      *D3DCompileShader_t)(
         LPCSTR              pSrcData,
         UINT                srcDataLen,
         const D3DXMACRO     *pDefines,
         LPD3DXINCLUDE       pInclude,
         LPCSTR              pFunctionName,
         LPCSTR              pProfile,
         DWORD               Flags,
         LPD3DXBUFFER        *ppShader,
         LPD3DXBUFFER        *ppErrorMsgs,
         LPD3DXCONSTANTTABLE *ppConstantTable);
typedef HRESULT (__stdcall
      *D3DCompileShaderFromFile_t)(
          LPCTSTR             pSrcFile,
    const D3DXMACRO           *pDefines,
          LPD3DXINCLUDE       pInclude,
          LPCSTR              pFunctionName,
          LPCSTR              pProfile,
          DWORD               Flags,
         LPD3DXBUFFER        *ppShader,
         LPD3DXBUFFER        *ppErrorMsgs,
         LPD3DXCONSTANTTABLE *ppConstantTable);

typedef HRESULT (__stdcall
    *D3DCreateTextureFromFile_t)(
        LPDIRECT3DDEVICE9         pDevice,
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
        LPDIRECT3DTEXTURE9*       ppTexture);

typedef HRESULT (__stdcall
    *D3DXCreateFontIndirect_t)(
        LPDIRECT3DDEVICE9       pDevice,
        D3DXFONT_DESC*   pDesc,
        LPD3DXFONT*             ppFont);
#endif


#ifdef HAVE_D3DX
static D3DXCreateFontIndirect_t   D3DCreateFontIndirect;
static D3DCreateTextureFromFile_t D3DCreateTextureFromFile;
static D3DCompileShaderFromFile_t D3DCompileShaderFromFile;
static D3DCompileShader_t         D3DCompileShader;
#endif
static D3DCreate_t D3DCreate;

void *d3d9_create(void)
{
   return D3DCreate(SDKVersion);
}

#ifdef HAVE_DYNAMIC_D3D

#ifdef HAVE_D3DX
static const char *d3dx9_dll_list[] = 
{
   "d3dx9_24.dll",
   "d3dx9_25.dll",
   "d3dx9_26.dll",
   "d3dx9_27.dll",
   "d3dx9_28.dll",
   "d3dx9_29.dll",
   "d3dx9_30.dll",
   "d3dx9_31.dll",
   "d3dx9_32.dll",
   "d3dx9_33.dll",
   "d3dx9_34.dll",
   "d3dx9_35.dll",
   "d3dx9_36.dll",
   "d3dx9_37.dll",
   "d3dx9_38.dll",
   "d3dx9_39.dll",
   "d3dx9_40.dll",
   "d3dx9_41.dll",
   "d3dx9_42.dll",
   "d3dx9_43.dll",
   NULL
};

static dylib_t dylib_load_d3d9x(void)
{
   dylib_t dll           = NULL;

   const char **dll_name = d3dx9_dll_list;

   while (!dll && *dll_name)
      dll = dylib_load(*dll_name++);

   return dll;
}
#endif

#endif

bool d3d9_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (d3d9_dylib_initialized)
      return true;

#if defined(DEBUG) || defined(_DEBUG)
   g_d3d9_dll     = dylib_load("d3d9d.dll");
   if(!g_d3d9_dll)
#endif
      g_d3d9_dll  = dylib_load("d3d9.dll");
#ifdef HAVE_D3DX
   g_d3d9x_dll    = dylib_load_d3d9x();

   if (!g_d3d9x_dll)
      return false;
#endif

   if (!g_d3d9_dll)
      return false;
#endif
   
   SDKVersion               = 31;
#ifdef HAVE_DYNAMIC_D3D
   D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d9_dll, "Direct3DCreate9");
#ifdef HAVE_D3DX
   D3DCompileShaderFromFile = (D3DCompileShaderFromFile_t)dylib_proc(g_d3d9x_dll, "D3DXCompileShaderFromFile");
   D3DCompileShader         = (D3DCompileShader_t)dylib_proc(g_d3d9x_dll, "D3DXCompileShader");
#ifdef UNICODE
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3d9x_dll, "D3DXCreateFontIndirectW");
#else
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3d9x_dll, "D3DXCreateFontIndirectA");
#endif
   D3DCreateTextureFromFile = (D3DCreateTextureFromFile_t)dylib_proc(g_d3d9x_dll, "D3DXCreateTextureFromFileExA");
#endif
#else
   D3DCreate                = Direct3DCreate9;
#ifdef HAVE_D3DX
   D3DCompileShaderFromFile = D3DXCompileShaderFromFile;
   D3DCompileShader         = D3DXCompileShader;
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
   d3d9_dylib_initialized = true;
#endif

   return true;

error:
   d3d9_deinitialize_symbols();
   return false;
}

void d3d9_deinitialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (g_d3d9_dll)
      dylib_close(g_d3d9_dll);
#ifdef HAVE_D3DX
   if (g_d3d9x_dll)
      dylib_close(g_d3d9x_dll);
   g_d3d9x_dll        = NULL;
#endif
   g_d3d9_dll         = NULL;

   d3d9_dylib_initialized = false;
#endif
}

bool d3d9_check_device_type(void *_d3d,
      unsigned idx,
      INT32 disp_format,
      INT32 backbuffer_format,
      bool windowed_mode)
{
   LPDIRECT3D9 d3d = (LPDIRECT3D9)_d3d;
   if (!d3d)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (FAILED(d3d->CheckDeviceType(
               0,
               D3DDEVTYPE_HAL,
               (D3DFORMAT)disp_format,
               (D3DFORMAT)backbuffer_format,
               windowed_mode)))
      return false;
#else
   if (FAILED(IDirect3D9_CheckDeviceType(d3d,
               0,
               D3DDEVTYPE_HAL,
               (D3DFORMAT)disp_format,
               (D3DFORMAT)backbuffer_format,
               windowed_mode)))
      return false;
#endif

   return true;
}

bool d3d9_get_adapter_display_mode(
      void *_d3d,
      unsigned idx,
      void *display_mode)
{
   LPDIRECT3D9 d3d = (LPDIRECT3D9)_d3d;
   if (!d3d)
      return false;
#ifdef _XBOX
   return true;
#elif defined(__cplusplus) && !defined(CINTERFACE) 
   if (FAILED(d3d->GetAdapterDisplayMode(idx, (D3DDISPLAYMODE*)display_mode)))
      return false;
#else
   if (FAILED(IDirect3D9_GetAdapterDisplayMode(d3d, idx, (D3DDISPLAYMODE*)display_mode)))
      return false;
#endif

   return true;
}

bool d3d9_swap(void *data, void *_dev)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus) && !defined(CINTERFACE) 
#ifdef _XBOX
   dev->Present(NULL, NULL, NULL, NULL);
#else
   if (dev->Present(NULL, NULL, NULL, NULL) != D3D_OK)
      return false;
#endif
#else
#ifdef _XBOX
   IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
#else
   if (IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL) 
         == D3DERR_DEVICELOST)
      return false;
#endif
#endif
   return true;
}

void d3d9_set_transform(void *_dev,
      INT32 state, const void *_matrix)
{
#ifndef _XBOX
   CONST D3DMATRIX *matrix = (CONST D3DMATRIX*)_matrix;
   /* XBox 360 D3D9 does not support fixed-function pipeline. */
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetTransform((D3DTRANSFORMSTATETYPE)state, matrix);
#else
   IDirect3DDevice9_SetTransform(dev, (D3DTRANSFORMSTATETYPE)state, matrix);
#endif
#endif
}

bool d3d9_texture_get_level_desc(void *_tex,
      unsigned idx, void *_ppsurface_level)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(tex->GetLevelDesc(idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
#else
#if defined(_XBOX)
   D3DTexture_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level);
   return true;
#else
   if (SUCCEEDED(IDirect3DTexture9_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
#endif
#endif

   return false;
}

bool d3d9_texture_get_surface_level(void *_tex,
      unsigned idx, void **_ppsurface_level)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (!tex)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(tex->GetSurfaceLevel(idx, (IDirect3DSurface9**)_ppsurface_level)))
      return true;
#else
   if (SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(tex, idx, (IDirect3DSurface9**)_ppsurface_level)))
      return true;
#endif

   return false;
}

#ifdef HAVE_D3DX
static void *d3d9_texture_new_from_file(
      void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette)
{
   void *buf  = NULL;
   HRESULT hr = D3DCreateTextureFromFile((LPDIRECT3DDEVICE9)dev,
         path, width, height, miplevels, usage, format,
         (D3DPOOL)pool, filter, mipfilter, color_key,
         (D3DXIMAGE_INFO*)src_info_data,
         palette, (struct IDirect3DTexture9**)&buf);

   if (FAILED(hr))
      return NULL;

   return buf;
}
#endif

void *d3d9_texture_new(void *_dev,
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
      return d3d9_texture_new_from_file(_dev,
            path, width, height, miplevels,
            usage, (D3DFORMAT)format,
            (D3DPOOL)pool, filter, mipfilter,
            color_key, src_info_data, palette);
#else
      return NULL;
#endif
   }

   {
      LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#ifndef _XBOX
      if (want_mipmap)
         usage |= D3DUSAGE_AUTOGENMIPMAP;
#endif
#if defined(__cplusplus) && !defined(CINTERFACE) 
      hr = dev->CreateTexture(
            width, height, miplevels, usage,
            (D3DFORMAT)format,
            (D3DPOOL)pool,
            (struct IDirect3DTexture9**)&buf, NULL);
#else
      hr = IDirect3DDevice9_CreateTexture(dev,
            width, height, miplevels, usage,
            (D3DFORMAT)format,
            (D3DPOOL)pool,
            (struct IDirect3DTexture9**)&buf, NULL);
#endif
   }

   if (FAILED(hr))
      return NULL;

   return buf;
}

void d3d9_texture_free(void *_tex)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (!tex)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   tex->Release();
#else
   IDirect3DTexture9_Release(tex);
#endif
}

bool d3d9_surface_lock_rect(void *data, void *data2)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   if (!surf)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (FAILED(surf->LockRect((D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
      return false;
#else
#if defined(_XBOX)
   IDirect3DSurface9_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY);
#else
   if (FAILED(IDirect3DSurface9_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
      return false;
#endif
#endif

   return true;
}

void d3d9_surface_unlock_rect(void *data)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   if (!surf)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   surf->UnlockRect();
#else
   IDirect3DSurface9_UnlockRect(surf);
#endif
}

void d3d9_surface_free(void *data)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   if (!surf)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   surf->Release();
#else
   IDirect3DSurface9_Release(surf);
#endif
}

void d3d9_vertex_declaration_free(void *data)
{
   if (!data)
      return;

#if defined(__cplusplus) && !defined(CINTERFACE) 
   {
      LPDIRECT3DVERTEXDECLARATION9 vertex_decl = 
         (LPDIRECT3DVERTEXDECLARATION9)data;
      if (vertex_decl)
         vertex_decl->Release();
   }
#else
   IDirect3DVertexDeclaration9_Release((LPDIRECT3DVERTEXDECLARATION9)data);
#endif
}

bool d3d9_vertex_declaration_new(void *_dev,
      const void *vertex_data, void **decl_data)
{
   LPDIRECT3DDEVICE9                    dev   = (LPDIRECT3DDEVICE9)_dev;
   const D3DVERTEXELEMENT9   *vertex_elements = (const D3DVERTEXELEMENT9*)vertex_data;
   LPDIRECT3DVERTEXDECLARATION9 **vertex_decl = (LPDIRECT3DVERTEXDECLARATION9**)decl_data;

#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(dev->CreateVertexDeclaration(vertex_elements,
               (IDirect3DVertexDeclaration9**)vertex_decl)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(dev,
               vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
      return true;
#endif

   return false;
}

void *d3d9_vertex_buffer_new(void *_dev,
      unsigned length, unsigned usage,
      unsigned fvf, INT32 pool, void *handle)
{
   HRESULT             hr = S_OK;
   void              *buf = NULL;
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;

   if (usage == 0)
   {
#ifndef _XBOX
#if defined(__cplusplus) && !defined(CINTERFACE) 
      if (dev->GetSoftwareVertexProcessing())
         usage = D3DUSAGE_SOFTWAREPROCESSING;
#else
      if (IDirect3DDevice9_GetSoftwareVertexProcessing(dev))
         usage = D3DUSAGE_SOFTWAREPROCESSING;
#endif
#endif         
   }

#if defined(__cplusplus) && !defined(CINTERFACE) 
   hr = dev->CreateVertexBuffer(length, usage, fvf,
         (D3DPOOL)pool,
         (LPDIRECT3DVERTEXBUFFER9*)&buf, NULL);
#else
   hr = IDirect3DDevice9_CreateVertexBuffer(dev, length, usage, fvf,
         (D3DPOOL)pool,
         (LPDIRECT3DVERTEXBUFFER9*)&buf, NULL);
#endif

   if (FAILED(hr))
      return NULL;

   return buf;
}

void d3d9_vertex_buffer_unlock(void *vertbuf_ptr)
{
   LPDIRECT3DVERTEXBUFFER9 vertbuf = (LPDIRECT3DVERTEXBUFFER9)vertbuf_ptr;

   if (!vertbuf)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   vertbuf->Unlock();
#else
   IDirect3DVertexBuffer9_Unlock(vertbuf);
#endif
}

void *d3d9_vertex_buffer_lock(void *vertbuf_ptr)
{
   void                       *buf = NULL;
   LPDIRECT3DVERTEXBUFFER9 vertbuf = (LPDIRECT3DVERTEXBUFFER9)vertbuf_ptr;
   if (!vertbuf)
      return NULL;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   vertbuf->Lock(0, 0, &buf, 0);
#else
   IDirect3DVertexBuffer9_Lock(vertbuf, 0, 0, &buf, 0);
#endif

   if (!buf)
      return NULL;

   return buf;
}

void d3d9_vertex_buffer_free(void *vertex_data, void *vertex_declaration)
{
   if (vertex_data)
   {
      LPDIRECT3DVERTEXBUFFER9 buf = (LPDIRECT3DVERTEXBUFFER9)vertex_data;
#if defined(__cplusplus) && !defined(CINTERFACE) 
      buf->Release();
#else
      IDirect3DVertexBuffer9_Release(buf);
#endif
      buf = NULL;
   }

   if (vertex_declaration)
   {
      LPDIRECT3DVERTEXDECLARATION9 vertex_decl = (LPDIRECT3DVERTEXDECLARATION9)vertex_declaration;
      d3d9_vertex_declaration_free(vertex_decl);
      vertex_decl = NULL;
   }
}

void d3d9_set_stream_source(void *_dev, unsigned stream_no,
      void *stream_vertbuf_ptr, unsigned offset_bytes,
      unsigned stride)
{
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   LPDIRECT3DVERTEXBUFFER9 stream_vertbuf = (LPDIRECT3DVERTEXBUFFER9)stream_vertbuf_ptr;
   if (!stream_vertbuf)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetStreamSource(stream_no, stream_vertbuf, offset_bytes, stride);
#else
   IDirect3DDevice9_SetStreamSource(dev, stream_no, stream_vertbuf,
         offset_bytes,
         stride);
#endif
}

bool d3d9_device_create_offscreen_plain_surface(
      void *_dev,
      unsigned width,
      unsigned height,
      unsigned format,
      unsigned pool,
      void **surf_data,
      void *data)
{
#ifndef _XBOX
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(dev->CreateOffscreenPlainSurface(width, height,
               (D3DFORMAT)format, (D3DPOOL)pool,
               (LPDIRECT3DSURFACE9*)surf_data,
               (HANDLE*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev,
               width, height,
               (D3DFORMAT)format, (D3DPOOL)pool,
               (LPDIRECT3DSURFACE9*)surf_data,
               (HANDLE*)data)))
      return true;
#endif
#endif

   return false;
}

static void d3d9_set_texture_stage_state(void *_dev,
      unsigned sampler, unsigned type, unsigned value)
{
#ifndef _XBOX
   /* XBox 360 has no fixed-function pipeline. */
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (dev->SetTextureStageState(sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
      RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#else
   if (IDirect3DDevice9_SetTextureStageState(dev, sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
      RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#endif
#endif
}

void d3d9_set_sampler_address_u(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, value);
#else
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSU, value);
#endif
}

void d3d9_set_sampler_address_v(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, value);
#else
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSV, value);
#endif
}

void d3d9_set_sampler_minfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, value);
#else
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MINFILTER, value);
#endif
}

void d3d9_set_sampler_magfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, value);
#else
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MAGFILTER, value);
#endif
}

void d3d9_set_sampler_mipfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return;
   IDirect3DDevice9_SetSamplerState(dev, sampler,
         D3DSAMP_MIPFILTER, value);
}

bool d3d9_begin_scene(void *_dev)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (FAILED(dev->BeginScene()))
      return false;
#else
#if defined(_XBOX)
   IDirect3DDevice9_BeginScene(dev);
#else
   if (FAILED(IDirect3DDevice9_BeginScene(dev)))
      return false;
#endif
#endif

   return true;
}

void d3d9_end_scene(void *_dev)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->EndScene();
#else
   IDirect3DDevice9_EndScene(dev);
#endif
}

static void d3d9_draw_primitive_internal(void *_dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->DrawPrimitive(type, start, count);
#else
   IDirect3DDevice9_DrawPrimitive(dev, type, start, count);
#endif
}

void d3d9_draw_primitive(void *dev,
      INT32 type, unsigned start, unsigned count)
{
   if (!d3d9_begin_scene(dev))
      return;

   d3d9_draw_primitive_internal(dev, (D3DPRIMITIVETYPE)type, start, count);
   d3d9_end_scene(dev);
}

void d3d9_clear(void *_dev,
      unsigned count, const void *rects, unsigned flags,
      INT32 color, float z, unsigned stencil)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->Clear(count, (const D3DRECT*)rects, flags, color, z, stencil);
#else
   IDirect3DDevice9_Clear(dev, count, (const D3DRECT*)rects, flags,
         color, z, stencil);
#endif
}

bool d3d9_device_get_render_target_data(void *_dev,
      void *_src, void *_dst)
{
#ifndef _XBOX
   LPDIRECT3DSURFACE9 src = (LPDIRECT3DSURFACE9)_src;
   LPDIRECT3DSURFACE9 dst = (LPDIRECT3DSURFACE9)_dst;
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(dev->GetRenderTargetData(src, dst)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(
               dev, src, dst)))
      return true;
#endif
#endif

   return false;
}

bool d3d9_device_get_render_target(void *_dev,
      unsigned idx, void **data)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(dev->GetRenderTarget(idx,
               (LPDIRECT3DSURFACE9*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,
               idx, (LPDIRECT3DSURFACE9*)data)))
      return true;
#endif

   return false;
}


bool d3d9_lock_rectangle(void *_tex,
      unsigned level, void *_lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   D3DLOCKED_RECT     *lr = (D3DLOCKED_RECT*)_lr;
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (!tex)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (FAILED(tex->LockRect(level, lr, rect, flags)))
      return false;
#else
#ifdef _XBOX
   IDirect3DTexture9_LockRect(tex, level, lr, (const RECT*)rect, flags);
#else
   if (IDirect3DTexture9_LockRect(tex, level, lr, (const RECT*)rect, flags) != D3D_OK)
      return false;
#endif
#endif

   return true;
}

void d3d9_unlock_rectangle(void *_tex)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
   if (!tex)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   tex->UnlockRect(0);
#else
   IDirect3DTexture9_UnlockRect(tex, 0);
#endif
}

void d3d9_lock_rectangle_clear(void *tex,
      unsigned level, void *_lr, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   D3DLOCKED_RECT *lr = (D3DLOCKED_RECT*)_lr;
#if defined(_XBOX)
   level = 0;
#endif
   memset(lr->pBits, level, rectangle_height * lr->Pitch);
   d3d9_unlock_rectangle(tex);
}

void d3d9_set_viewports(void *_dev, void *_vp)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   D3DVIEWPORT9      *vp = (D3DVIEWPORT9*)_vp;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetViewport(vp);
#else
   IDirect3DDevice9_SetViewport(dev, vp);
#endif
}

void d3d9_set_texture(void *_dev, unsigned sampler,
      void *tex_data)
{
   LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)tex_data;
   LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
   if (!dev || !tex)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetTexture(sampler, tex);
#else
   IDirect3DDevice9_SetTexture(dev, sampler,
         (IDirect3DBaseTexture9*)tex);
#endif
}

void d3d9_free_vertex_shader(void *_dev, void *data)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   IDirect3DVertexShader9 *vs = (IDirect3DVertexShader9*)data;
   if (!dev || !vs)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   vs->Release();
#else
   IDirect3DVertexShader9_Release(vs);
#endif
}

void d3d9_free_pixel_shader(void *_dev, void *data)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   IDirect3DPixelShader9 *ps  = (IDirect3DPixelShader9*)data;
   if (!dev || !ps)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   ps->Release();
#else
   IDirect3DPixelShader9_Release(ps);
#endif
}

bool d3d9_create_vertex_shader(void *_dev, const DWORD *a, void **b)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return false;

#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (dev->CreateVertexShader(a, (IDirect3DVertexShader9**)b) == D3D_OK)
      return true;
#else
   if (IDirect3DDevice9_CreateVertexShader(dev, a,
            (LPDIRECT3DVERTEXSHADER9*)b) == D3D_OK)
      return true;
#endif

   return false;
}

bool d3d9_create_pixel_shader(void *_dev, const DWORD *a, void **b)
{
   LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return false;

#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (dev->CreatePixelShader(a, (IDirect3DPixelShader9**)b) == D3D_OK)
      return true;
#else
   if (IDirect3DDevice9_CreatePixelShader(dev, a,
            (LPDIRECT3DPIXELSHADER9*)b) == D3D_OK)
      return true;
#endif

   return false;
}

bool d3d9_set_pixel_shader(void *_dev, void *data)
{
   LPDIRECT3DDEVICE9       dev  = (LPDIRECT3DDEVICE9)_dev;
   LPDIRECT3DPIXELSHADER9 d3dps = (LPDIRECT3DPIXELSHADER9)data;
   if (!dev || !d3dps)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (dev->SetPixelShader(d3dps) == D3D_OK)
      return true;
#else
#ifdef _XBOX
   /* Returns void on Xbox */
   IDirect3DDevice9_SetPixelShader(dev, d3dps);
   return true;
#else
   if (IDirect3DDevice9_SetPixelShader(dev, d3dps) == D3D_OK)
      return true;
#endif
#endif

   return false;
}

bool d3d9_set_vertex_shader(void *_dev, unsigned index,
      void *data)
{
   LPDIRECT3DDEVICE9       dev    = (LPDIRECT3DDEVICE9)_dev;
   LPDIRECT3DVERTEXSHADER9 shader = (LPDIRECT3DVERTEXSHADER9)data;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (dev->SetVertexShader(shader) != D3D_OK)
      return false;
#else
#ifdef _XBOX
   IDirect3DDevice9_SetVertexShader(dev, shader);
#else
   if (IDirect3DDevice9_SetVertexShader(dev, shader) != D3D_OK)
      return false;
#endif
#endif

   return true;
}

bool d3d9_set_vertex_shader_constantf(void *_dev,
      UINT start_register,const float* constant_data,
      unsigned vector4f_count)
{
   LPDIRECT3DDEVICE9      dev    = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus) && !defined(CINTERFACE) 
#ifdef _XBOX
   dev->SetVertexShaderConstantF(
         start_register, constant_data, vector4f_count);
#else
   if (dev->SetVertexShaderConstantF(
            start_register, constant_data, vector4f_count) == D3D_OK)
      return true;
#endif
#else
#ifdef _XBOX
   IDirect3DDevice9_SetVertexShaderConstantF(dev,
         start_register, constant_data, vector4f_count);
   return true;
#else
   if (IDirect3DDevice9_SetVertexShaderConstantF(dev,
            start_register, constant_data, vector4f_count) == D3D_OK)
      return true;
#endif
#endif

   return false;
}

void d3d9_texture_blit(unsigned pixel_size,
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

bool d3d9_get_render_state(void *data, INT32 state, DWORD *value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (!dev)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (dev->GetRenderState((D3DRENDERSTATETYPE)state, value) == D3D_OK)
      return true;
#else
#ifdef _XBOX
   IDirect3DDevice9_GetRenderState(dev, (D3DRENDERSTATETYPE)state, value);
   return true;
#else
   if (IDirect3DDevice9_GetRenderState(dev, (D3DRENDERSTATETYPE)state, value) == D3D_OK)
      return true;
#endif
#endif

   return false;
}

void d3d9_set_render_state(void *data, INT32 state, DWORD value)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetRenderState((D3DRENDERSTATETYPE)state, value);
#else
   IDirect3DDevice9_SetRenderState(dev, (D3DRENDERSTATETYPE)state, value);
#endif
}

void d3d9_enable_blend_func(void *data)
{
   if (!data)
      return;

   d3d9_set_render_state(data, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d9_set_render_state(data, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d9_set_render_state(data, D3DRS_ALPHABLENDENABLE, true);
}

void d3d9_device_set_render_target(void *_dev, unsigned idx,
      void *data)
{
   LPDIRECT3DSURFACE9 surf = (LPDIRECT3DSURFACE9)data;
   LPDIRECT3DDEVICE9   dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetRenderTarget(idx, surf);
#else
   IDirect3DDevice9_SetRenderTarget(dev, idx, surf);
#endif
}

void d3d9_enable_alpha_blend_texture_func(void *data)
{
   /* Also blend the texture with the set alpha value. */
   d3d9_set_texture_stage_state(data, 0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
   d3d9_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG1,   D3DTA_DIFFUSE);
   d3d9_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG2,   D3DTA_TEXTURE);
}

void d3d9_disable_blend_func(void *data)
{
   d3d9_set_render_state(data, D3DRS_ALPHABLENDENABLE, false);
}

void d3d9_set_vertex_declaration(void *data, void *vertex_data)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (!dev)
      return;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   dev->SetVertexDeclaration((LPDIRECT3DVERTEXDECLARATION9)vertex_data);
#else
   IDirect3DDevice9_SetVertexDeclaration(dev, (LPDIRECT3DVERTEXDECLARATION9)vertex_data);
#endif
}

static bool d3d9_reset_internal(void *data,
      D3DPRESENT_PARAMETERS *d3dpp
      )
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (!dev)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if ((dev->Reset(d3dpp) == D3D_OK))
      return true;
#else
   if (IDirect3DDevice9_Reset(dev, d3dpp) == D3D_OK)
      return true;
#endif

   return false;
}

static HRESULT d3d9_test_cooperative_level(void *data)
{
#ifdef _XBOX
   return E_FAIL;
#else
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (!dev)
      return E_FAIL;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   return dev->TestCooperativeLevel();
#else
   return IDirect3DDevice9_TestCooperativeLevel(dev);
#endif
#endif
}

static bool d3d9_create_device_internal(
      void *data,
      D3DPRESENT_PARAMETERS *d3dpp,
      void *_d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
   LPDIRECT3D9       d3d = (LPDIRECT3D9)_d3d;
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (!dev)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(d3d->CreateDevice(
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice9**)dev)))
      return true;
#else
   if (SUCCEEDED(IDirect3D9_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice9**)dev)))
      return true;
#endif

   return false;
}

bool d3d9_create_device(void *dev,
      void *d3dpp,
      void *d3d,
      HWND focus_window,
      unsigned cur_mon_id)
{
   if (!d3d9_create_device_internal(dev,
            (D3DPRESENT_PARAMETERS*)d3dpp,
            d3d,
            focus_window,
            cur_mon_id,
            D3DCREATE_HARDWARE_VERTEXPROCESSING))
      if (!d3d9_create_device_internal(
               dev,
               (D3DPRESENT_PARAMETERS*)d3dpp, d3d, focus_window,
               cur_mon_id,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING))
         return false;
   return true;
}

bool d3d9_reset(void *dev, void *d3dpp)
{
   const char *err = NULL;

   if (d3d9_reset_internal(dev, (D3DPRESENT_PARAMETERS*)d3dpp))
      return true;

   RARCH_WARN("[D3D]: Attempting to recover from dead state...\n");

#ifndef _XBOX
   /* Try to recreate the device completely. */
   switch (d3d9_test_cooperative_level(dev))
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

bool d3d9_device_get_backbuffer(void *_dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (!dev)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE) 
   if (SUCCEEDED(dev->GetBackBuffer( 
               swapchain_idx, idx, 
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE9*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 
               swapchain_idx, idx, 
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE9*)data)))
      return true;
#endif

   return false;
}


void d3d9_device_free(void *_dev, void *_pd3d)
{
   LPDIRECT3D9      pd3d = (LPDIRECT3D9)_pd3d;
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
   if (dev)
   {
#if defined(__cplusplus) && !defined(CINTERFACE) 
      dev->Release();
#else
      IDirect3DDevice9_Release(dev);
#endif
   }

   if (pd3d)
   {
#if defined(__cplusplus) && !defined(CINTERFACE) 
      pd3d->Release();
#else
      IDirect3D9_Release(pd3d);
#endif
   }
}

INT32 d3d9_translate_filter(unsigned type)
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

bool d3d9x_create_font_indirect(void *_dev,
      void *desc, void **font_data)
{
#ifdef HAVE_D3DX
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
   if (SUCCEEDED(D3DCreateFontIndirect(
               dev, (D3DXFONT_DESC*)desc,
               (struct ID3DXFont**)font_data)))
      return true;
#else
   if (SUCCEEDED(D3DCreateFontIndirect(
               dev, (D3DXFONT_DESC*)desc,
               (struct ID3DXFont**)font_data)))
      return true;
#endif
#endif

   return false;
}

void d3dxbuffer_release(void *data)
{
#ifdef HAVE_D3DX
#ifdef __cplusplus
   ID3DXBuffer *p = (ID3DXBuffer*)data;
#else
   LPD3DXBUFFER p = (LPD3DXBUFFER)data;
#endif
   if (!p)
      return;

#if defined(__cplusplus) && !defined(CINTERFACE)
   p->Release();
#else
   p->lpVtbl->Release(p);
#endif
#endif
}

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
      void *ppconstanttable)
{
#if defined(HAVE_D3DX)
   if (D3DCompileShader)
      if (D3DCompileShader(
               (LPCTSTR)src,
               (UINT)src_data_len,
               (const D3DXMACRO*)pdefines,
               (LPD3DXINCLUDE)pinclude,
               (LPCSTR)pfunctionname,
               (LPCSTR)pprofile,
               (DWORD)flags,
               (LPD3DXBUFFER*)ppshader,
               (LPD3DXBUFFER*)pperrormsgs,
               (LPD3DXCONSTANTTABLE*)ppconstanttable) >= 0)
         return true;
#endif
   return false;
}

void d3d9x_font_draw_text(void *data, void *sprite_data, void *string_data,
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

void d3d9x_font_release(void *data)
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

void d3d9x_font_get_text_metrics(void *data, void *metrics)
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

bool d3d9x_compile_shader_from_file(
      const char *src,
      const void *pdefines,
      void *pinclude,
      const char *pfunctionname,
      const char *pprofile,
      unsigned flags,
      void *ppshader,
      void *pperrormsgs,
      void *ppconstanttable)
{
#if defined(HAVE_D3DX)
   if (D3DCompileShaderFromFile)
      if (D3DCompileShaderFromFile(
               (LPCTSTR)src,
               (const D3DXMACRO*)pdefines,
               (LPD3DXINCLUDE)pinclude,
               (LPCSTR)pfunctionname,
               (LPCSTR)pprofile,
               (DWORD)flags,
               (LPD3DXBUFFER*)ppshader,
               (LPD3DXBUFFER*)pperrormsgs,
               (LPD3DXCONSTANTTABLE*)ppconstanttable) >= 0)
         return true;
#endif
   return false;
}

INT32 d3d9_get_rgb565_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_R5G6B5;
#else
   return D3DFMT_R5G6B5;
#endif
}

INT32 d3d9_get_argb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_A8R8G8B8;
#else
   return D3DFMT_A8R8G8B8;
#endif
}

INT32 d3d9_get_xrgb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_X8R8G8B8;
#else
   return D3DFMT_X8R8G8B8;
#endif
}

const void *d3d9x_get_buffer_ptr(void *data)
{
#if defined(HAVE_D3DX)
   ID3DXBuffer *listing = (ID3DXBuffer*)data;
   if (!listing)
      return NULL;
#if defined(__cplusplus) && !defined(CINTERFACE)
   return listing->GetBufferPointer();
#else
   return listing->lpVtbl->GetBufferPointer(listing);
#endif
#else
   return NULL;
#endif
}

const bool d3d9x_constant_table_set_float(void *p,
      void *a,
      const void *b, float val)
{
#if defined(HAVE_D3DX)
   LPDIRECT3DDEVICE9    dev     = (LPDIRECT3DDEVICE9)a;
   D3DXHANDLE        handle     = (D3DXHANDLE)b;
   LPD3DXCONSTANTTABLE consttbl = (LPD3DXCONSTANTTABLE)p;
   if (!consttbl || !dev || !handle)
      return false;
#if defined(__cplusplus) && !defined(CINTERFACE)
   if (consttbl->SetFloat(dev, handle, val) == D3D_OK)
      return true;
#else
   if (consttbl->lpVtbl->SetFloat(consttbl, dev, handle, val) == D3D_OK)
      return true;
#endif
#endif
   return false;
}
