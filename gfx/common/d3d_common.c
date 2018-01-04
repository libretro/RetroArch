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

#include "d3d_common.h"

#if defined(HAVE_D3D9)
#include "../include/d3d9/d3dx9tex.h"
#elif defined(HAVE_D3D8)
#include "../include/d3d8/d3dx8tex.h"
#endif

#ifdef _XBOX
#include <xgraphics.h>
#endif

static UINT SDKVersion = 0;

#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d_dll;
#ifdef HAVE_D3DX
static dylib_t g_d3dx_dll;
#endif
static bool dylib_initialized = false;
#endif

#if defined(HAVE_D3D9)
typedef IDirect3D9 *(__stdcall *D3DCreate_t)(UINT);
#ifdef HAVE_D3DX
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
#elif defined(HAVE_D3D8)
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
#endif


#ifdef HAVE_D3DX
static D3DXCreateFontIndirect_t   D3DCreateFontIndirect;
static D3DCreateTextureFromFile_t D3DCreateTextureFromFile;
#endif
static D3DCreate_t D3DCreate;

void *d3d_create(void)
{
   return D3DCreate(SDKVersion);
}

#ifdef HAVE_DYNAMIC_D3D
dylib_t dylib_load_d3dx(void)
{
   dylib_t dll = NULL;

#if defined(HAVE_D3D9)
   dll = dylib_load("d3dx9_24.dll");

   if (!dll)
      dll = dylib_load("d3dx9_25.dll");
   if (!dll)
      dll = dylib_load("d3dx9_26.dll");
   if (!dll)
      dll = dylib_load("d3dx9_27.dll");
   if (!dll)
      dll = dylib_load("d3dx9_28.dll");
   if (!dll)
      dll = dylib_load("d3dx9_29.dll");
   if (!dll)
      dll = dylib_load("d3dx9_30.dll");
   if (!dll)
      dll = dylib_load("d3dx9_31.dll");
   if (!dll)
      dll = dylib_load("d3dx9_32.dll");
   if (!dll)
      dll = dylib_load("d3dx9_33.dll");
   if (!dll)
      dll = dylib_load("d3dx9_34.dll");
   if (!dll)
      dll = dylib_load("d3dx9_35.dll");
   if (!dll)
      dll = dylib_load("d3dx9_36.dll");
   if (!dll)
      dll = dylib_load("d3dx9_37.dll");
   if (!dll)
      dll = dylib_load("d3dx9_38.dll");
   if (!dll)
      dll = dylib_load("d3dx9_39.dll");
   if (!dll)
      dll = dylib_load("d3dx9_40.dll");
   if (!dll)
      dll = dylib_load("d3dx9_41.dll");
   if (!dll)
      dll = dylib_load("d3dx9_42.dll");
   if (!dll)
      dll = dylib_load("d3dx9_43.dll");
#endif

   return dll;
}
#endif

bool d3d_initialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (dylib_initialized)
      return true;

#if defined(HAVE_D3D9)
   g_d3d_dll  = dylib_load("d3d9.dll");
#ifdef HAVE_D3DX
   g_d3dx_dll = dylib_load_d3dx();
#endif

   if (!g_d3d_dll)
      return false;
#ifdef HAVE_D3DX
   if (!g_d3dx_dll)
      return false;
#endif

#elif defined(HAVE_D3D8)
   g_d3d_dll  = dylib_load("d3d8.dll");

   if (!g_d3d_dll)
      return false;
#endif
#endif

#if defined(HAVE_D3D9)
   SDKVersion               = 31;
#ifdef HAVE_DYNAMIC_D3D
   D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d_dll, "Direct3DCreate9");
#ifdef HAVE_D3DX
#ifdef UNICODE
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3dx_dll, "D3DXCreateFontIndirectW");
#else
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3dx_dll, "D3DXCreateFontIndirectA");
#endif
   D3DCreateTextureFromFile = (D3DCreateTextureFromFile_t)dylib_proc(g_d3dx_dll, "D3DXCreateTextureFromFileExA");
#endif
#else
   D3DCreate                = Direct3DCreate9;
#ifdef HAVE_D3DX
   D3DCreateFontIndirect    = D3DXCreateFontIndirect;
   D3DCreateTextureFromFile = D3DXCreateTextureFromFileExA;
#endif
#endif
#elif defined(HAVE_D3D8)
   SDKVersion = 220;
#ifdef HAVE_DYNAMIC_D3D
   D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d_dll, "Direct3DCreate8");
#ifdef HAVE_D3DX
   D3DCreateFontIndirect    = D3DXCreateFontIndirect;
   D3DCreateTextureFromFile = D3DXCreateTextureFromFileExA;
#endif
#else
   D3DCreate                = Direct3DCreate8;
#ifdef HAVE_D3DX
   D3DCreateFontIndirect    = D3DXCreateFontIndirect;
   D3DCreateTextureFromFile = D3DXCreateTextureFromFileExA;
#endif
#endif
#endif

   if (!D3DCreate)
      goto error;

#ifdef _XBOX
   SDKVersion = 0;
#endif
#ifdef HAVE_DYNAMIC_D3D
   dylib_initialized = true;
#endif

   return true;

error:
   d3d_deinitialize_symbols();
   return false;
}

void d3d_deinitialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (g_d3d_dll)
      dylib_close(g_d3d_dll);
#ifdef HAVE_D3DX
   if (g_d3dx_dll)
      dylib_close(g_d3dx_dll);
   g_d3dx_dll = NULL;
#endif
   g_d3d_dll  = NULL;

   dylib_initialized = false;
#endif
}

bool d3d_swap(void *data, LPDIRECT3DDEVICE dev)
{
#if defined(_XBOX1)
   D3DDevice_Swap(0);
#elif defined(_XBOX360)
   D3DDevice_Present(dev);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   if (IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST)
      return false;
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   if (IDirect3DDevice8_Present(dev, NULL, NULL, NULL, NULL) == D3DERR_DEVICELOST)
      return false;
#else
   if (dev->Present(NULL, NULL, NULL, NULL) != D3D_OK)
      return false;
#endif
   return true;
}

void d3d_set_transform(LPDIRECT3DDEVICE dev,
      D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX *matrix)
{
#if defined(_XBOX1)
   D3DDevice_SetTransform(state, matrix);
#elif !defined(_XBOX360)
   /* XBox 360 D3D9 does not support fixed-function pipeline. */

#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetTransform(dev, state, matrix);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_SetTransform(dev, state, matrix);
#else
   dev->SetTransform(state, matrix);
#endif

#endif
}

bool d3d_texture_get_level_desc(LPDIRECT3DTEXTURE tex,
      unsigned idx, void *_ppsurface_level)
{
   if (!tex)
      return false;
#if defined(HAVE_D3D9) && !defined(__cplusplus)
#if defined(_XBOX)
   D3DTexture_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level);
   return true;
#else
   if (SUCCEEDED(IDirect3DTexture9_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
#endif
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   if (SUCCEEDED(IDirect3DTexture8_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
#else
   if (SUCCEEDED(tex->GetLevelDesc(idx, (D3DSURFACE_DESC*)_ppsurface_level)))
      return true;
#endif
   return false;
}

bool d3d_texture_get_surface_level(LPDIRECT3DTEXTURE tex,
      unsigned idx, void **_ppsurface_level)
{
   if (!tex)
      return false;
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   if (SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(tex, idx, (IDirect3DSurface9**)_ppsurface_level)))
      return true;
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   if (SUCCEEDED(IDirect3DTexture8_GetSurfaceLevel(tex, idx, (LPDIRECT3DSURFACE**)_ppsurface_level)))
      return true;
#else
   if (SUCCEEDED(tex->GetSurfaceLevel(idx, (ID3DSURFACE**)_ppsurface_level)))
      return true;
#endif
   return false;
}

#ifdef HAVE_D3DX
static LPDIRECT3DTEXTURE d3d_texture_new_from_file(
      LPDIRECT3DDEVICE dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, void *src_info_data,
      PALETTEENTRY *palette)
{
   LPDIRECT3DTEXTURE buf;
   HRESULT hr = D3DCreateTextureFromFile(dev,
         path, width, height, miplevels, usage, format,
         pool, filter, mipfilter, color_key, src_info_data,
         palette, &buf);

   if (FAILED(hr))
	   return NULL;

   return buf;
}
#endif

LPDIRECT3DTEXTURE d3d_texture_new(LPDIRECT3DDEVICE dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, void *src_info_data,
      PALETTEENTRY *palette)
{
   HRESULT hr;
   LPDIRECT3DTEXTURE buf;

#ifdef HAVE_D3DX
   if (path)
      return d3d_texture_new_from_file(dev,
            path, width, height, miplevels,
            usage, format, pool, filter, mipfilter,
            color_key, src_info_data, palette);
#else
   if (path)
      return NULL;
#endif

#if defined(HAVE_D3D9)
#ifdef __cplusplus
   hr = dev->CreateTexture(
         width, height, miplevels, usage,
         format, pool, &buf, NULL);
#else
   hr = IDirect3DDevice9_CreateTexture(dev,
         width, height, miplevels, usage,
         format, pool, &buf, NULL);
#endif
#elif defined(HAVE_D3D8)
#ifdef __cplusplus
   hr = dev->CreateTexture(
         width, height, miplevels, usage,
         format, pool, &buf);
#else
   hr = IDirect3DDevice8_CreateTexture(dev,
         width, height, miplevels, usage,
         format, pool, &buf);
#endif
#endif

   if (FAILED(hr))
      return NULL;

   return buf;
}

void d3d_texture_free(LPDIRECT3DTEXTURE tex)
{
   if (tex)
   {
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3DTexture9_Release(tex);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
      IDirect3DTexture8_Release(tex);
#else
      tex->Release();
#endif
   }
}

bool d3d_surface_lock_rect(void *data, void *data2)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;
#if defined(HAVE_D3D9) && !defined(__cplusplus)
#if defined(_XBOX)
   IDirect3DSurface9_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY);
#else
   if (FAILED(IDirect3DSurface9_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
	   return false;
#endif
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   if (FAILED(IDirect3DSurface8_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
	   return false;
#elif defined(_XBOX)
   surf->LockRect((D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY);
#else
   if (FAILED(surf->LockRect((D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
	   return false;
#endif
   return true;
}

void d3d_surface_unlock_rect(void *data)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;
   if (surf)
   {
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3DSurface9_UnlockRect(surf);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
      IDirect3DSurface8_UnlockRect(surf);
#else
      surf->UnlockRect();
#endif
   }
}

void d3d_surface_free(void *data)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;
   if (!surf)
      return;
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DSurface9_Release(surf);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DSurface8_Release(surf);
#else
   surf->Release();
#endif
}

void d3d_vertex_declaration_free(void *data)
{
   if (!data)
      return;
#if defined(HAVE_D3D8)
   /* empty */
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DVertexDeclaration9_Release((LPDIRECT3DVERTEXDECLARATION)data);
#else
   {
      LPDIRECT3DVERTEXDECLARATION vertex_decl = 
         (LPDIRECT3DVERTEXDECLARATION)data;
      if (vertex_decl)
         vertex_decl->Release();
   }
#endif
}

bool d3d_vertex_declaration_new(LPDIRECT3DDEVICE dev,
      const void *vertex_data, void **decl_data)
{
#ifdef HAVE_D3D9
   const D3DVERTEXELEMENT   *vertex_elements = (const D3DVERTEXELEMENT*)vertex_data;
   LPDIRECT3DVERTEXDECLARATION **vertex_decl = (LPDIRECT3DVERTEXDECLARATION**)decl_data;

#if defined(__cplusplus)
   if (SUCCEEDED(dev->CreateVertexDeclaration(vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(dev, vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
      return true;
#endif

#endif
   return false;
}

LPDIRECT3DVERTEXBUFFER d3d_vertex_buffer_new(LPDIRECT3DDEVICE dev,
      unsigned length, unsigned usage,
      unsigned fvf, D3DPOOL pool, void *handle)
{
   HRESULT hr;
   LPDIRECT3DVERTEXBUFFER buf;

#ifndef _XBOX
   if (usage == 0)
   {
#if defined(HAVE_D3D9)
#ifdef __cplusplus
	  if (dev->GetSoftwareVertexProcessing())
         usage = D3DUSAGE_SOFTWAREPROCESSING;
#else
	  if (IDirect3DDevice9_GetSoftwareVertexProcessing(dev))
         usage = D3DUSAGE_SOFTWAREPROCESSING;
#endif
#endif         
   }
#endif

#if defined(HAVE_D3D9) && !defined(__cplusplus)
   hr = IDirect3DDevice9_CreateVertexBuffer(dev, length, usage, fvf, pool,
         &buf, NULL);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   hr = IDirect3DDevice8_CreateVertexBuffer(dev, length, usage, fvf, pool,
         &buf);
#else
   hr = dev->CreateVertexBuffer(length, usage, fvf, pool, &buf, NULL);
#endif

   if (FAILED(hr))
	   return NULL;

   return buf;
}

void d3d_vertex_buffer_unlock(void *vertbuf_ptr)
{
   LPDIRECT3DVERTEXBUFFER vertbuf = (LPDIRECT3DVERTEXBUFFER)vertbuf_ptr;

#ifdef _XBOX360
   D3DVertexBuffer_Unlock(vertbuf);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DVertexBuffer9_Unlock(vertbuf);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DVertexBuffer8_Unlock(vertbuf);
#else
   vertbuf->Unlock();
#endif
}

void *d3d_vertex_buffer_lock(void *vertbuf_ptr)
{
   void                      *buf = NULL;
   LPDIRECT3DVERTEXBUFFER vertbuf = (LPDIRECT3DVERTEXBUFFER)vertbuf_ptr;

#if defined(_XBOX1)
   buf = (void*)D3DVertexBuffer_Lock2(vertbuf, 0);
#elif defined(_XBOX360)
   buf = D3DVertexBuffer_Lock(vertbuf, 0, 0, 0);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DVertexBuffer9_Lock(vertbuf, 0, sizeof(buf), &buf, 0);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DVertexBuffer8_Lock(vertbuf, 0, sizeof(buf), &buf, 0);
#else
   vertbuf->Lock(0, sizeof(buf), &buf, 0);
#endif

   if (!buf)
      return NULL;

   return buf;
}

void d3d_vertex_buffer_free(void *vertex_data, void *vertex_declaration)
{
   if (vertex_data)
   {
      LPDIRECT3DVERTEXBUFFER buf = (LPDIRECT3DVERTEXBUFFER)vertex_data;
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3DVertexBuffer9_Release(buf);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
      IDirect3DVertexBuffer8_Release(buf);
#else
      buf->Release();
#endif
      buf = NULL;
   }

#ifdef HAVE_D3D9
   if (vertex_declaration)
   {
      LPDIRECT3DVERTEXDECLARATION vertex_decl = (LPDIRECT3DVERTEXDECLARATION)vertex_declaration;
      d3d_vertex_declaration_free(vertex_decl);
      vertex_decl = NULL;
   }
#endif
}

void d3d_set_stream_source(LPDIRECT3DDEVICE dev, unsigned stream_no,
      void *stream_vertbuf_ptr, unsigned offset_bytes,
      unsigned stride)
{
	LPDIRECT3DVERTEXBUFFER stream_vertbuf = (LPDIRECT3DVERTEXBUFFER)stream_vertbuf_ptr;
#if defined(_XBOX360)
   D3DDevice_SetStreamSource_Inline(dev, stream_no, stream_vertbuf,
         offset_bytes, stride);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetStreamSource(dev, stream_no, stream_vertbuf,
         offset_bytes,
         stride);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_SetStreamSource(dev, stream_no, stream_vertbuf, stride);
#else
   dev->SetStreamSource(stream_no, stream_vertbuf, offset_bytes, stride);
#endif
}

bool d3d_device_create_offscreen_plain_surface(
      LPDIRECT3DDEVICE dev,
      unsigned width,
      unsigned height,
      unsigned format,
      unsigned pool,
      void **surf_data,
      void *data)
{
#if defined(HAVE_D3D9) && !defined(_XBOX)
#ifdef __cplusplus
   if (SUCCEEDED(dev->CreateOffscreenPlainSurface(width, height,
         (D3DFORMAT)format, (D3DPOOL)pool,
         (LPDIRECT3DSURFACE*)surf_data,
         (HANDLE*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev,
               width, height,
         (D3DFORMAT)format, (D3DPOOL)pool,
         (LPDIRECT3DSURFACE*)surf_data,
         (HANDLE*)data)))
      return true;
#endif
#endif
   return false;
}

#ifndef _XBOX360
/* XBox 360 has no fixed-function pipeline. */
static void d3d_set_texture_stage_state(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value, unsigned type)
{
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetTextureStageState(dev, sampler, (D3DTEXTURESTAGESTATETYPE)type, value);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_SetTextureStageState(dev, sampler, (D3DTEXTURESTAGESTATETYPE)type, value);
#else
   dev->SetTextureStageState(sampler, (D3DTEXTURESTAGESTATETYPE)type, value);
#endif
}
#endif

void d3d_set_sampler_address_u(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_ADDRESSU] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_AddressU_Inline(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSU, value);
#elif defined(HAVE_D3D8)
   d3d_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSU, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, value);
#endif
}

void d3d_set_sampler_address_v(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_ADDRESSV] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_AddressV_Inline(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSV, value);
#elif defined(HAVE_D3D8)
   d3d_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSV, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, value);
#endif
}

void d3d_set_sampler_minfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_MINFILTER] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_MinFilter(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MINFILTER, value);
#elif defined(HAVE_D3D8)
   d3d_set_texture_stage_state(dev, sampler, D3DTSS_MINFILTER, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, value);
#endif
}

void d3d_set_sampler_magfilter(LPDIRECT3DDEVICE dev,
      unsigned sampler, unsigned value)
{
#if defined(_XBOX1)
   D3D__DirtyFlags |= (D3DDIRTYFLAG_TEXTURE_STATE_0 << sampler);
   D3D__TextureState[sampler][D3DTSS_MAGFILTER] = value;
#elif defined(_XBOX360)
   D3DDevice_SetSamplerState_MagFilter(dev, sampler, value);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MAGFILTER, value);
#elif defined(HAVE_D3D8)
   d3d_set_texture_stage_state(dev, sampler, D3DTSS_MAGFILTER, value);
#else
   dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, value);
#endif
}

bool d3d_begin_scene(LPDIRECT3DDEVICE dev)
{
#if defined(HAVE_D3D9) && !defined(__cplusplus)
#if defined(_XBOX)
   IDirect3DDevice9_BeginScene(dev);
#else
   if (FAILED(IDirect3DDevice9_BeginScene(dev)))
	   return false;
#endif
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   if (FAILED(IDirect3DDevice8_BeginScene(dev)))
	   return false;
#elif defined(_XBOX)
   dev->BeginScene();
#else
   if (FAILED(dev->BeginScene()))
      return false;
#endif

   return true;
}

void d3d_end_scene(LPDIRECT3DDEVICE dev)
{
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_EndScene(dev);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_EndScene(dev);
#else
   dev->EndScene();
#endif
}

static void d3d_draw_primitive_internal(LPDIRECT3DDEVICE dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_DrawPrimitive(dev, type, start, count);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_DrawPrimitive(dev, type, start, count);
#else
   dev->DrawPrimitive(type, start, count);
#endif
}

void d3d_draw_primitive(LPDIRECT3DDEVICE dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
#if defined(_XBOX1)
   D3DDevice_DrawVertices(type, start, D3DVERTEXCOUNT(type, count));
#elif defined(_XBOX360)
   D3DDevice_DrawVertices(dev, type, start, D3DVERTEXCOUNT(type, count));
#else
   if (d3d_begin_scene(dev))
   {
      d3d_draw_primitive_internal(dev, type, start, count);
      d3d_end_scene(dev);
   }
#endif
}

void d3d_clear(LPDIRECT3DDEVICE dev,
      unsigned count, const D3DRECT *rects, unsigned flags,
      D3DCOLOR color, float z, unsigned stencil)
{
#if defined(_XBOX1)
   D3DDevice_Clear(count, rects, flags, color, z, stencil);
#elif defined(_XBOX360)
   D3DDevice_Clear(dev, count, rects, flags, color, z,
         stencil, false);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_Clear(dev, count, rects, flags,
         color, z, stencil);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_Clear(dev, count, rects, flags,
         color, z, stencil);
#else
   dev->Clear(count, rects, flags, color, z, stencil);
#endif
}

bool d3d_device_get_render_target_data(LPDIRECT3DDEVICE dev,
      void *_src, void *_dst)
{
#if defined(HAVE_D3D9)
   LPDIRECT3DSURFACE src = (LPDIRECT3DSURFACE)_src;
   LPDIRECT3DSURFACE dst = (LPDIRECT3DSURFACE)_dst;

#ifndef _XBOX
#ifdef __cplusplus
   if (SUCCEEDED(dev->GetRenderTargetData(src, dst)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, src, dst)))
      return true;
#endif
#endif
#endif

   return false;
}

bool d3d_device_get_render_target(LPDIRECT3DDEVICE dev,
      unsigned idx, void **data)
{
   if (!dev)
	   return false;

#if defined(HAVE_D3D9)
#ifdef __cplusplus
   if (SUCCEEDED(dev->GetRenderTarget(idx,
               (LPDIRECT3DSURFACE*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,
	   idx, (LPDIRECT3DSURFACE*)data)))
      return true;
#endif
#elif defined(HAVE_D3D8)
#ifdef __cplusplus
   if (SUCCEEDED(dev->GetRenderTarget(
               (LPDIRECT3DSURFACE*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice8_GetRenderTarget(dev,
               (LPDIRECT3DSURFACE*)data)))
      return true;
#endif
#endif
   return false;
}


bool d3d_lock_rectangle(LPDIRECT3DTEXTURE tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
#if defined(_XBOX)
   D3DTexture_LockRect(tex, level, lock_rect, rect, flags);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   if (IDirect3DTexture9_LockRect(tex, level, lock_rect, (const RECT*)rect, flags) != D3D_OK)
      return false;
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   if (IDirect3DTexture8_LockRect(tex, level, lock_rect, rect, flags) != D3D_OK)
      return false;
#else
   if (FAILED(tex->LockRect(level, lock_rect, rect, flags)))
      return false;
#endif
   return true;
}

void d3d_unlock_rectangle(LPDIRECT3DTEXTURE tex)
{
#ifdef _XBOX
   D3DTexture_UnlockRect(tex, 0);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DTexture9_UnlockRect(tex, 0);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DTexture8_UnlockRect(tex, 0);
#else
   tex->UnlockRect(0);
#endif
}

void d3d_lock_rectangle_clear(LPDIRECT3DTEXTURE tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
#if defined(_XBOX)
   level = 0;
#endif
   memset(lock_rect->pBits, level, rectangle_height * lock_rect->Pitch);
   d3d_unlock_rectangle(tex);
}

void d3d_set_viewports(LPDIRECT3DDEVICE dev, D3DVIEWPORT *vp)
{
#if defined(_XBOX360)
   D3DDevice_SetViewport(dev, vp);
#elif defined(_XBOX1)
   D3DDevice_SetViewport(vp);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetViewport(dev, vp);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_SetViewport(dev, vp);
#else
   dev->SetViewport(vp);
#endif
}

void d3d_set_texture(LPDIRECT3DDEVICE dev, unsigned sampler,
      void *tex_data)
{
   LPDIRECT3DTEXTURE tex = (LPDIRECT3DTEXTURE)tex_data;
#if defined(_XBOX1)
   D3DDevice_SetTexture(sampler, tex);
#elif defined(_XBOX360)
   unsigned fetchConstant =
      GPU_CONVERT_D3D_TO_HARDWARE_TEXTUREFETCHCONSTANT(sampler);
   uint64_t pendingMask3 =
      D3DTAG_MASKENCODE(D3DTAG_START(D3DTAG_FETCHCONSTANTS)
            + fetchConstant, D3DTAG_START(D3DTAG_FETCHCONSTANTS)
            + fetchConstant);
#if defined(__cplusplus)
   D3DDevice_SetTexture(dev, sampler, tex, pendingMask3);
#else
   D3DDevice_SetTexture(dev, sampler, (D3DBaseTexture*)tex, pendingMask3);
#endif
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetTexture(dev, sampler, (IDirect3DBaseTexture9*)tex);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_SetTexture(dev, sampler, tex);
#else
   dev->SetTexture(sampler, tex);
#endif
}

HRESULT d3d_set_vertex_shader(LPDIRECT3DDEVICE dev, unsigned index,
      void *data)
{
#if defined(_XBOX360)
   LPDIRECT3DVERTEXSHADER shader = (LPDIRECT3DVERTEXSHADER)data;
   D3DDevice_SetVertexShader(dev, shader);
   return S_OK;
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   return IDirect3DDevice8_SetVertexShader(dev, index);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   LPDIRECT3DVERTEXSHADER shader = (LPDIRECT3DVERTEXSHADER)data;
   return IDirect3DDevice9_SetVertexShader(dev, shader);
#else
   LPDIRECT3DVERTEXSHADER shader = (LPDIRECT3DVERTEXSHADER)data;
   return dev->SetVertexShader(shader);
#endif
}


void d3d_texture_blit(unsigned pixel_size,
      LPDIRECT3DTEXTURE tex, D3DLOCKED_RECT *lr, const void *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   if (d3d_lock_rectangle(tex, 0, lr, NULL, 0, 0))
   {
#if defined(_XBOX360)
      D3DSURFACE_DESC desc;
      d3d_texture_get_level_desc(tex, 0, &desc);
      XGCopySurface(lr->pBits, lr->Pitch, width, height, desc.Format, NULL,
            frame, pitch, desc.Format, NULL, 0, 0);
#else
      unsigned y;
      for (y = 0; y < height; y++)
      {
         const uint8_t *in = (const uint8_t*)frame + y * pitch;
         uint8_t *out = (uint8_t*)lr->pBits + y * lr->Pitch;
         memcpy(out, in, width * pixel_size);
      }
#endif
      d3d_unlock_rectangle(tex);
   }
}

void d3d_set_render_state(void *data, D3DRENDERSTATETYPE state, DWORD value)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetRenderState(dev, state, value);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_SetRenderState(dev, state, value);
#else
   dev->SetRenderState(state, value);
#endif
}

void d3d_enable_blend_func(void *data)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

   d3d_set_render_state(dev, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d_set_render_state(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d_set_render_state(dev, D3DRS_ALPHABLENDENABLE, true);
}

void d3d_device_set_render_target(LPDIRECT3DDEVICE dev, unsigned idx,
      void *data)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetRenderTarget(dev, idx, surf);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   IDirect3DDevice8_SetRenderTarget(dev, surf, NULL);
#else
   dev->SetRenderTarget(idx, surf);
#endif
}

void d3d_enable_alpha_blend_texture_func(void *data)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

#ifndef _XBOX360
   /* Also blend the texture with the set alpha value. */
   d3d_set_texture_stage_state(dev, 0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
   d3d_set_texture_stage_state(dev, 0, D3DTSS_ALPHAARG1,   D3DTA_DIFFUSE);
   d3d_set_texture_stage_state(dev, 0, D3DTSS_ALPHAARG2,   D3DTA_TEXTURE);
#endif
}

void d3d_frame_postprocess(void *data)
{
#if defined(_XBOX1)
   global_t        *global = global_get_ptr();

#ifdef __cplusplus
   LPDIRECT3DDEVICE    dev = (LPDIRECT3DDEVICE)data;
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

void d3d_disable_blend_func(void *data)
{
   LPDIRECT3DDEVICE dev = (LPDIRECT3DDEVICE)data;

   if (!dev)
      return;

   d3d_set_render_state(dev, D3DRS_ALPHABLENDENABLE, false);
}

void d3d_set_vertex_declaration(void *data, void *vertex_data)
{
   LPDIRECT3DDEVICE dev             = (LPDIRECT3DDEVICE)data;
   if (!dev)
      return;

#ifdef _XBOX1
   d3d_set_vertex_shader(dev, D3DFVF_XYZ | D3DFVF_TEX1, NULL);
#elif defined(HAVE_D3D9) && !defined(__cplusplus)
   IDirect3DDevice9_SetVertexDeclaration(dev, (LPDIRECT3DVERTEXDECLARATION)vertex_data);
#elif defined(HAVE_D3D9)
   dev->SetVertexDeclaration((LPDIRECT3DVERTEXDECLARATION)vertex_data);
#endif
}

static bool d3d_reset_internal(LPDIRECT3DDEVICE dev,
      D3DPRESENT_PARAMETERS *d3dpp
      )
{
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   return (IDirect3DDevice9_Reset(dev, d3dpp) == D3D_OK);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   return (IDirect3DDevice8_Reset(dev, d3dpp) == D3D_OK);
#else
   return (dev->Reset(d3dpp) == D3D_OK);
#endif
}

static HRESULT d3d_test_cooperative_level(LPDIRECT3DDEVICE dev)
{
#if defined(HAVE_D3D9) && !defined(__cplusplus) && !defined(_XBOX)
   return IDirect3DDevice9_TestCooperativeLevel(dev);
#elif defined(HAVE_D3D8) && !defined(__cplusplus) && !defined(_XBOX)
   return IDirect3DDevice8_TestCooperativeLevel(dev);
#elif defined(_XBOX)
   return E_FAIL;
#else
   return dev->TestCooperativeLevel();
#endif
}

static bool d3d_create_device_internal(LPDIRECT3DDEVICE *dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      LPDIRECT3D d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
#if defined(HAVE_D3D9) && !defined(__cplusplus)
   if (SUCCEEDED(IDirect3D9_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               dev)))
      return true;
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
   if (SUCCEEDED(IDirect3D8_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               dev)))
      return true;
#else
   if (SUCCEEDED(d3d->CreateDevice(
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               dev)))
      return true;
#endif
   return false;
}

bool d3d_create_device(LPDIRECT3DDEVICE *dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      LPDIRECT3D d3d,
      HWND focus_window,
      unsigned cur_mon_id)
{
   if (!d3d_create_device_internal(dev,
            d3dpp,
            d3d,
            focus_window,
            cur_mon_id,
            D3DCREATE_HARDWARE_VERTEXPROCESSING))
      if (!d3d_create_device_internal(
               dev, d3dpp, d3d, focus_window,
               cur_mon_id,
               D3DCREATE_SOFTWARE_VERTEXPROCESSING))
         return false;
   return true;
}

bool d3d_reset(LPDIRECT3DDEVICE dev, D3DPRESENT_PARAMETERS *d3dpp)
{
   const char *err = NULL;

   if (d3d_reset_internal(dev, d3dpp))
      return true;

   RARCH_WARN("[D3D]: Attempting to recover from dead state...\n");

#ifndef _XBOX
   /* Try to recreate the device completely. */
   switch (d3d_test_cooperative_level(dev))
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

bool d3d_device_get_backbuffer(LPDIRECT3DDEVICE dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data)
{
   if (!dev)
      return false;

#if defined(HAVE_D3D9)
#ifdef __cplusplus
   if (SUCCEEDED(dev->GetBackBuffer( 
               swapchain_idx, idx, 
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 
               swapchain_idx, idx, 
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE*)data)))
      return true;
#endif
#elif defined(HAVE_D3D8)
#ifdef __cplusplus
   if (SUCCEEDED(dev->GetBackBuffer(idx,
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE*)data)))
      return true;
#else
   if (SUCCEEDED(IDirect3DDevice8_GetBackBuffer(dev, idx,
               (D3DBACKBUFFER_TYPE)backbuffer_type,
               (LPDIRECT3DSURFACE*)data)))
      return true;
#endif
#endif

   return false;
}


void d3d_device_free(LPDIRECT3DDEVICE dev, LPDIRECT3D pd3d)
{
   if (dev)
   {
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3DDevice9_Release(dev);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
      IDirect3DDevice8_Release(dev);
#else
      dev->Release();
#endif
   }
   if (pd3d)
   {
#if defined(HAVE_D3D9) && !defined(__cplusplus)
      IDirect3D9_Release(pd3d);
#elif defined(HAVE_D3D8) && !defined(__cplusplus)
      IDirect3D8_Release(pd3d);
#else
      pd3d->Release();
#endif
   }
}

D3DTEXTUREFILTERTYPE d3d_translate_filter(unsigned type)
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


void *d3d_matrix_transpose(void *_pout, const void *_pm)
{
   unsigned i,j;
   D3DMATRIX     *pout = (D3DMATRIX*)_pout;
   CONST D3DMATRIX *pm = (D3DMATRIX*)_pm;

   for (i = 0; i < 4; i++)
   {
      for (j = 0; j < 4; j++)
         pout->m[i][j] = pm->m[j][i];
   }
   return pout;
}


void *d3d_matrix_identity(void *_pout)
{
   D3DMATRIX *pout = (D3DMATRIX*)_pout;
   if ( !pout )
      return NULL;

   pout->m[0][1] = 0.0f;
   pout->m[0][2] = 0.0f;
   pout->m[0][3] = 0.0f;
   pout->m[1][0] = 0.0f;
   pout->m[1][2] = 0.0f;
   pout->m[1][3] = 0.0f;
   pout->m[2][0] = 0.0f;
   pout->m[2][1] = 0.0f;
   pout->m[2][3] = 0.0f;
   pout->m[3][0] = 0.0f;
   pout->m[3][1] = 0.0f;
   pout->m[3][2] = 0.0f;
   pout->m[0][0] = 1.0f;
   pout->m[1][1] = 1.0f;
   pout->m[2][2] = 1.0f;
   pout->m[3][3] = 1.0f;
   return pout;
}

void *d3d_matrix_ortho_off_center_lh(void *_pout, float l, float r, float b, float t, float zn, float zf)
{
   D3DMATRIX *pout = (D3DMATRIX*)_pout;

   d3d_matrix_identity(pout);

   pout->m[0][0] = 2.0f / (r - l);
   pout->m[1][1] = 2.0f / (t - b);
   pout->m[2][2] = 1.0f / (zf -zn);
   pout->m[3][0] = -1.0f -2.0f *l / (r - l);
   pout->m[3][1] = 1.0f + 2.0f * t / (b - t);
   pout->m[3][2] = zn / (zn -zf);
   return pout;
}

void *d3d_matrix_multiply(void *_pout, const void *_pm1, const void *_pm2)
{
   unsigned i,j;
   D3DMATRIX      *pout = (D3DMATRIX*)_pout;
   CONST D3DMATRIX *pm1 = (CONST D3DMATRIX*)_pm1;
   CONST D3DMATRIX *pm2 = (CONST D3DMATRIX*)_pm2;

   for (i=0; i<4; i++)
   {
      for (j=0; j<4; j++)
         pout->m[i][j] = pm1->m[i][0] * pm2->m[0][j] + pm1->m[i][1] * pm2->m[1][j] + pm1->m[i][2] * pm2->m[2][j] + pm1->m[i][3] * pm2->m[3][j];
   }
   return pout;
}

void *d3d_matrix_rotation_z(void *_pout, float angle)
{
   D3DMATRIX *pout = (D3DMATRIX*)_pout;
   d3d_matrix_identity(pout);
   pout->m[0][0] = cos(angle);
   pout->m[1][1] = cos(angle);
   pout->m[0][1] = sin(angle);
   pout->m[1][0] = -sin(angle);
   return pout;
}

bool d3dx_create_font_indirect(LPDIRECT3DDEVICE dev,
      void *desc, void **font_data)
{
#ifdef HAVE_D3DX

#if defined(HAVE_D3D9)
#ifdef __cplusplus
   if (FAILED(D3DCreateFontIndirect(
               dev, (D3DXFONT_DESC*)desc, font_data)))
      return false;
#else
   if (FAILED(D3DCreateFontIndirect(
               dev, (D3DXFONT_DESC*)desc,
               (struct ID3DXFont**)font_data)))
      return false;
#endif
#elif defined(HAVE_D3D8)
   if (FAILED(D3DCreateFontIndirect(
               dev, (CONST LOGFONT*)desc,
               (struct ID3DXFont**)font_data)))
      return false;
#endif

   return true;
#else
   return false;
#endif
}
