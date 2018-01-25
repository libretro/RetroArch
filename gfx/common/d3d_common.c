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

#ifdef HAVE_D3DX

#if defined(HAVE_D3D9)

#ifdef _XBOX
#include <d3dx9core.h>
#include <d3dx9tex.h>
#else
#include "../include/d3d9/d3dx9tex.h"
#endif

#elif defined(HAVE_D3D8)

#ifdef _XBOX
#include <d3dx8core.h>
#include <d3dx8tex.h>
#else
#include "../include/d3d8/d3dx8tex.h"
#endif

#endif

#endif

static enum gfx_ctx_api d3d_common_api = GFX_CTX_NONE;

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
#ifdef HAVE_D3D9
static D3DCompileShaderFromFile_t D3DCompileShaderFromFile;
static D3DCompileShader_t         D3DCompileShader;
#endif
#endif
static D3DCreate_t D3DCreate;

void *d3d_create(void)
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

static dylib_t dylib_load_d3dx(void)
{
   dylib_t dll           = NULL;

#if defined(HAVE_D3D9)
   const char **dll_name = d3dx9_dll_list;

   while (!dll && *dll_name)
      dll = dylib_load(*dll_name++);
#endif

   return dll;
}
#endif

#endif

bool d3d_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (dylib_initialized)
      return true;

   switch (api)
   {
      case GFX_CTX_DIRECT3D9_API:
#if defined(HAVE_D3D9)
#if defined(DEBUG) || defined(_DEBUG)
         g_d3d_dll     = dylib_load("d3d9d.dll");
         if(!g_d3d_dll)
#endif
            g_d3d_dll  = dylib_load("d3d9.dll");
#ifdef HAVE_D3DX
         g_d3dx_dll    = dylib_load_d3dx();

         if (!g_d3dx_dll)
            return false;
#endif
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#if defined(HAVE_D3D8)
#if defined(DEBUG) || defined(_DEBUG)
         g_d3d_dll     = dylib_load("d3d8d.dll");
         if(!g_d3d_dll)
#endif
            g_d3d_dll  = dylib_load("d3d8.dll");
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (!g_d3d_dll)
      return false;
#endif
   
   d3d_common_api           = api;

#ifdef HAVE_DYNAMIC_D3D
#ifdef HAVE_D3DX
#ifdef UNICODE
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3dx_dll, "D3DXCreateFontIndirectW");
#else
   D3DCreateFontIndirect    = (D3DXCreateFontIndirect_t)dylib_proc(g_d3dx_dll, "D3DXCreateFontIndirectA");
#endif
   D3DCreateTextureFromFile = (D3DCreateTextureFromFile_t)dylib_proc(g_d3dx_dll, "D3DXCreateTextureFromFileExA");
   D3DCompileShaderFromFile = (D3DCompileShaderFromFile_t)dylib_proc(g_d3dx_dll, "D3DXCompileShaderFromFile");
   D3DCompileShader         = (D3DCompileShader_t)dylib_proc(g_d3dx_dll, "D3DXCompileShader");
#endif
#else
#ifdef HAVE_D3DX
   D3DCreateFontIndirect    = D3DXCreateFontIndirect;
   D3DCreateTextureFromFile = D3DXCreateTextureFromFileExA;
   D3DCompileShaderFromFile = D3DXCompileShaderFromFile;
   D3DCompileShader         = D3DXCompileShader;
#endif
#endif

   switch (api)
   {
      case GFX_CTX_DIRECT3D9_API:
         SDKVersion               = 31;
#ifdef HAVE_D3D9 
#ifdef HAVE_DYNAMIC_D3D
         D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d_dll, "Direct3DCreate9");
#else
         D3DCreate                = Direct3DCreate9;
#endif
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
         SDKVersion = 220;
#ifdef HAVE_D3D8
#ifdef HAVE_DYNAMIC_D3D
         D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d_dll, "Direct3DCreate8");
#ifdef HAVE_D3DX
         D3DCreateFontIndirect    = D3DXCreateFontIndirect;
         D3DCreateTextureFromFile = D3DXCreateTextureFromFileExA;
#endif
#else
         D3DCreate                = Direct3DCreate8;
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

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
   g_d3dx_dll        = NULL;
#endif
   g_d3d_dll         = NULL;

   dylib_initialized = false;
#endif
   d3d_common_api    = GFX_CTX_NONE;
}

bool d3d_check_device_type(void *_d3d,
      unsigned idx,
      D3DFORMAT disp_format,
      D3DFORMAT backbuffer_format,
      bool windowed_mode)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3D9 d3d = (LPDIRECT3D9)_d3d;
            if (!d3d)
               return false;
#ifdef __cplusplus
            if (FAILED(d3d->CheckDeviceType(
                        0,
                        D3DDEVTYPE_HAL,
                        disp_format,
                        backbuffer_format,
                        windowed_mode)))
               return false;
#else
            if (FAILED(IDirect3D9_CheckDeviceType(d3d,
                        0,
                        D3DDEVTYPE_HAL,
                        disp_format,
                        backbuffer_format,
                        windowed_mode)))
               return false;
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3D8 d3d = (LPDIRECT3D8)_d3d;
            if (!d3d)
               return false;
#ifdef __cplusplus
            if (FAILED(d3d->CheckDeviceType(
                        0,
                        D3DDEVTYPE_HAL,
                        disp_format,
                        backbuffer_format,
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
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         return false;
   }

   return true;
}

bool d3d_get_adapter_display_mode(
      void *_d3d,
      unsigned idx,
      D3DDISPLAYMODE *display_mode)
{
   if (!display_mode)
      return false;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3D9 d3d = (LPDIRECT3D9)_d3d;
            if (!d3d)
               return false;
#ifdef _XBOX
            return true;
#elif defined(__cplusplus)
            if (FAILED(d3d->GetAdapterDisplayMode(idx, display_mode)))
               return false;
#else
            if (FAILED(IDirect3D9_GetAdapterDisplayMode(d3d, idx, display_mode)))
               return false;
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3D8 d3d = (LPDIRECT3D8)_d3d;
            if (!d3d)
               return false;
#ifdef __cplusplus
            if (FAILED(d3d->GetAdapterDisplayMode(idx, display_mode)))
               return false;
#else
            if (FAILED(IDirect3D8_GetAdapterDisplayMode(d3d, idx, display_mode)))
               return false;
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         return false;
   }

   return true;
}

bool d3d_swap(void *data, void *_dev)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
            if (dev->Present(NULL, NULL, NULL, NULL) != D3D_OK)
               return false;
#else
            if (IDirect3DDevice8_Present(dev, NULL, NULL, NULL, NULL) 
                  == D3DERR_DEVICELOST)
               return false;
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
   return true;
}

void d3d_set_transform(void *_dev,
      D3DTRANSFORMSTATETYPE state, CONST D3DMATRIX *matrix)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
            /* XBox 360 D3D9 does not support fixed-function pipeline. */
#ifdef HAVE_D3D9
#ifndef _XBOX
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
            dev->SetTransform(state, matrix);
#else
            IDirect3DDevice9_SetTransform(dev, state, matrix);
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
            dev->SetTransform(state, matrix);
#else
            IDirect3DDevice8_SetTransform(dev, state, matrix);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

bool d3d_texture_get_level_desc(void *_tex,
      unsigned idx, void *_ppsurface_level)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
#ifdef __cplusplus
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
#ifdef __cplusplus
            if (SUCCEEDED(tex->GetLevelDesc(idx, (D3DSURFACE_DESC*)_ppsurface_level)))
               return true;
#else
            if (SUCCEEDED(IDirect3DTexture8_GetLevelDesc(tex, idx, (D3DSURFACE_DESC*)_ppsurface_level)))
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

bool d3d_texture_get_surface_level(void *_tex,
      unsigned idx, void **_ppsurface_level)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
            if (!tex)
               return false;
#ifdef __cplusplus
            if (SUCCEEDED(tex->GetSurfaceLevel(idx, (IDirect3DSurface9**)_ppsurface_level)))
               return true;
#else
            if (SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(tex, idx, (IDirect3DSurface9**)_ppsurface_level)))
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
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
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

#ifdef HAVE_D3DX
static void *d3d_texture_new_from_file(
      void *dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, void *src_info_data,
      PALETTEENTRY *palette)
{
   void *buf  = NULL;
   HRESULT hr = E_FAIL;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#if defined(HAVE_D3D9)
         hr = D3DCreateTextureFromFile((LPDIRECT3DDEVICE9)dev,
               path, width, height, miplevels, usage, format,
               pool, filter, mipfilter, color_key, src_info_data,
               palette, (struct IDirect3DTexture9**)&buf);
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#if defined(HAVE_D3D8)
         hr = D3DCreateTextureFromFile((LPDIRECT3DDEVICE8)dev,
               path, width, height, miplevels, usage, format,
               pool, filter, mipfilter, color_key, src_info_data,
               palette, (struct IDirect3DTeture8**)&buf);
#endif
         break;
      default:
         break;
   }

   if (FAILED(hr))
      return NULL;

   return buf;
}
#endif

void *d3d_texture_new(void *_dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      D3DCOLOR color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   HRESULT hr            = S_OK;
   void *buf             = NULL;

   if (path)
   {
#ifdef HAVE_D3DX
      return d3d_texture_new_from_file(_dev,
            path, width, height, miplevels,
            usage, format, pool, filter, mipfilter,
            color_key, src_info_data, palette);
#else
      return NULL;
#endif
   }

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#ifndef _XBOX
            if (want_mipmap)
               usage |= D3DUSAGE_AUTOGENMIPMAP;
#endif
#ifdef __cplusplus
            hr = dev->CreateTexture(
                  width, height, miplevels, usage,
                  format, pool, (LPDIRECT3DTEXTURE9)&buf, NULL);
#else
            hr = IDirect3DDevice9_CreateTexture(dev,
                  width, height, miplevels, usage,
                  format, pool, (struct IDirect3DTexture9**)&buf, NULL);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
            hr = dev->CreateTexture(
                  width, height, miplevels, usage,
                  format, pool, (LPDIRECT3DTEXTURE8)&buf);
#else
            hr = IDirect3DDevice8_CreateTexture(dev,
                  width, height, miplevels, usage,
                  format, pool, (LPDIRECT3DTEXTURE8)&buf);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (FAILED(hr))
      return NULL;

   return buf;
}

void d3d_texture_free(void *_tex)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
            if (!tex)
               return;
#ifdef __cplusplus
            tex->Release();
#else
            IDirect3DTexture9_Release(tex);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
            if (!tex)
               return;
#ifdef __cplusplus
            tex->Release();
#else
            IDirect3DTexture8_Release(tex);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

bool d3d_surface_lock_rect(void *data, void *data2)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;

   if (!surf)
      return false;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
#ifdef __cplusplus
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
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
#ifdef __cplusplus
         if (FAILED(surf->LockRect((D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
            return false;
#else
         if (FAILED(IDirect3DSurface8_LockRect(surf, (D3DLOCKED_RECT*)data2, NULL, D3DLOCK_READONLY)))
            return false;
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;
}

void d3d_surface_unlock_rect(void *data)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;
   if (!surf)
      return;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
#ifdef __cplusplus
         surf->UnlockRect();
#else
         IDirect3DSurface9_UnlockRect(surf);
#endif
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
#ifdef __cplusplus
         surf->UnlockRect();
#else
         IDirect3DSurface8_UnlockRect(surf);
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_surface_free(void *data)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;
   if (!surf)
      return;
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
#ifdef __cplusplus
         surf->Release();
#else
         IDirect3DSurface9_Release(surf);
#endif
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
#ifdef __cplusplus
         surf->Release();
#else
         IDirect3DSurface8_Release(surf);
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_vertex_declaration_free(void *data)
{
   if (!data)
      return;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
#ifdef __cplusplus
         {
            LPDIRECT3DVERTEXDECLARATION vertex_decl = 
               (LPDIRECT3DVERTEXDECLARATION)data;
            if (vertex_decl)
               vertex_decl->Release();
         }
#else
         IDirect3DVertexDeclaration9_Release((LPDIRECT3DVERTEXDECLARATION)data);
#endif
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
}

bool d3d_vertex_declaration_new(void *_dev,
      const void *vertex_data, void **decl_data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
         {
            LPDIRECT3DDEVICE9                    dev  = (LPDIRECT3DDEVICE9)_dev;
            const D3DVERTEXELEMENT   *vertex_elements = (const D3DVERTEXELEMENT*)vertex_data;
            LPDIRECT3DVERTEXDECLARATION **vertex_decl = (LPDIRECT3DVERTEXDECLARATION**)decl_data;

#if defined(__cplusplus)
            if (SUCCEEDED(dev->CreateVertexDeclaration(vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
               return true;
#else
            if (SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(dev, vertex_elements, (IDirect3DVertexDeclaration9**)vertex_decl)))
               return true;
#endif
         }
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

LPDIRECT3DVERTEXBUFFER d3d_vertex_buffer_new(void *_dev,
      unsigned length, unsigned usage,
      unsigned fvf, D3DPOOL pool, void *handle)
{
   HRESULT hr                 = S_OK;
   LPDIRECT3DVERTEXBUFFER buf = NULL;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
            if (usage == 0)
            {
#ifndef _XBOX
#ifdef __cplusplus
               if (dev->GetSoftwareVertexProcessing())
                  usage = D3DUSAGE_SOFTWAREPROCESSING;
#else
               if (IDirect3DDevice9_GetSoftwareVertexProcessing(dev))
                  usage = D3DUSAGE_SOFTWAREPROCESSING;
#endif
#endif         
            }

#ifdef __cplusplus
            hr = dev->CreateVertexBuffer(length, usage, fvf, pool, &buf, NULL);
#else
            hr = IDirect3DDevice9_CreateVertexBuffer(dev, length, usage, fvf, pool,
                  &buf, NULL);
#endif

#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
            hr = dev->CreateVertexBuffer(length, usage, fvf, pool, &buf, NULL);
#else
            hr = IDirect3DDevice8_CreateVertexBuffer(dev, length, usage, fvf, pool,
                  &buf);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (FAILED(hr))
	   return NULL;

   return buf;
}

void d3d_vertex_buffer_unlock(void *vertbuf_ptr)
{
   LPDIRECT3DVERTEXBUFFER vertbuf = (LPDIRECT3DVERTEXBUFFER)vertbuf_ptr;

   if (!vertbuf)
      return;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
#ifdef __cplusplus
         vertbuf->Unlock();
#else
         IDirect3DVertexBuffer9_Unlock(vertbuf);
#endif
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
#ifdef __cplusplus
         vertbuf->Unlock();
#else
         IDirect3DVertexBuffer8_Unlock(vertbuf);
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

}

void *d3d_vertex_buffer_lock(void *vertbuf_ptr)
{
   void                      *buf = NULL;
   LPDIRECT3DVERTEXBUFFER vertbuf = (LPDIRECT3DVERTEXBUFFER)vertbuf_ptr;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
#ifdef __cplusplus
         vertbuf->Lock(0, 0, &buf, 0);
#else
         IDirect3DVertexBuffer9_Lock(vertbuf, 0, 0, &buf, 0);
#endif
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
#ifdef __cplusplus
         vertbuf->Lock(0, 0, &buf, 0);
#else
         IDirect3DVertexBuffer8_Lock(vertbuf, 0, 0, (BYTE**)&buf, 0);
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (!buf)
      return NULL;

   return buf;
}

void d3d_vertex_buffer_free(void *vertex_data, void *vertex_declaration)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
         if (vertex_data)
         {
            LPDIRECT3DVERTEXBUFFER buf = (LPDIRECT3DVERTEXBUFFER)vertex_data;
#ifdef __cplusplus
            buf->Release();
#else
            IDirect3DVertexBuffer9_Release(buf);
#endif
            buf = NULL;
         }

         if (vertex_declaration)
         {
            LPDIRECT3DVERTEXDECLARATION vertex_decl = (LPDIRECT3DVERTEXDECLARATION)vertex_declaration;
            d3d_vertex_declaration_free(vertex_decl);
            vertex_decl = NULL;
         }
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
         if (vertex_data)
         {
            LPDIRECT3DVERTEXBUFFER buf = (LPDIRECT3DVERTEXBUFFER)vertex_data;
#ifdef __cplusplus
            buf->Release();
#else
            IDirect3DVertexBuffer8_Release(buf);
#endif
            buf = NULL;
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_set_stream_source(void *_dev, unsigned stream_no,
      void *stream_vertbuf_ptr, unsigned offset_bytes,
      unsigned stride)
{
	LPDIRECT3DVERTEXBUFFER stream_vertbuf = (LPDIRECT3DVERTEXBUFFER)stream_vertbuf_ptr;

   if (!stream_vertbuf)
      return;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
            dev->SetStreamSource(stream_no, stream_vertbuf, offset_bytes, stride);
#else
            IDirect3DDevice9_SetStreamSource(dev, stream_no, stream_vertbuf,
                  offset_bytes,
                  stride);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
            dev->SetStreamSource(stream_no, stream_vertbuf, offset_bytes, stride);
#else
            IDirect3DDevice8_SetStreamSource(dev, stream_no, stream_vertbuf, stride);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

bool d3d_device_create_offscreen_plain_surface(
      void *_dev,
      unsigned width,
      unsigned height,
      unsigned format,
      unsigned pool,
      void **surf_data,
      void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifndef _XBOX
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void d3d_set_texture_stage_state(void *_dev,
      unsigned sampler, unsigned type, unsigned value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
            /* XBox 360 has no fixed-function pipeline. */
#ifndef _XBOX
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
            if (dev->SetTextureStageState(sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
               RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#else
            if (IDirect3DDevice9_SetTextureStageState(dev, sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
               RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
            if (dev->SetTextureStageState(sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
               RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#else
            if (IDirect3DDevice8_SetTextureStageState(dev, sampler, (D3DTEXTURESTAGESTATETYPE)type, value) != D3D_OK)
               RARCH_ERR("SetTextureStageState call failed, sampler: %d, value: %d, type: %d\n", sampler, value, type);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_set_sampler_address_u(void *_dev,
      unsigned sampler, unsigned value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
            dev->SetSamplerState(sampler, D3DSAMP_ADDRESSU, value);
#else
            IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSU, value);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
            d3d_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSU, value);
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_set_sampler_address_v(void *_dev,
      unsigned sampler, unsigned value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
            dev->SetSamplerState(sampler, D3DSAMP_ADDRESSV, value);
#else
            IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_ADDRESSV, value);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev  = (LPDIRECT3DDEVICE8)_dev;
            d3d_set_texture_stage_state(dev, sampler, D3DTSS_ADDRESSV, value);
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_set_sampler_minfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetSamplerState(sampler, D3DSAMP_MINFILTER, value);
#else
            IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MINFILTER, value);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
         d3d_set_texture_stage_state(_dev, sampler, D3DTSS_MINFILTER, value);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_set_sampler_magfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetSamplerState(sampler, D3DSAMP_MAGFILTER, value);
#else
            IDirect3DDevice9_SetSamplerState(dev, sampler, D3DSAMP_MAGFILTER, value);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
         d3d_set_texture_stage_state(_dev, sampler, D3DTSS_MAGFILTER, value);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_set_sampler_mipfilter(void *_dev,
      unsigned sampler, unsigned value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return;
            IDirect3DDevice9_SetSamplerState(dev, sampler,
                  D3DSAMP_MIPFILTER, value);
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
}

bool d3d_begin_scene(void *_dev)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return false;
#ifdef __cplusplus
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
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
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;
}

void d3d_end_scene(void *_dev)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->EndScene();
#else
            IDirect3DDevice9_EndScene(dev);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->EndScene();
#else
            IDirect3DDevice8_EndScene(dev);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static void d3d_draw_primitive_internal(void *_dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->DrawPrimitive(type, start, count);
#else
            IDirect3DDevice9_DrawPrimitive(dev, type, start, count);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->DrawPrimitive(type, start, count);
#else
            IDirect3DDevice8_DrawPrimitive(dev, type, start, count);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_draw_primitive(void *dev,
      D3DPRIMITIVETYPE type, unsigned start, unsigned count)
{
   if (!d3d_begin_scene(dev))
      return;

   d3d_draw_primitive_internal(dev, type, start, count);
   d3d_end_scene(dev);
}

void d3d_clear(void *_dev,
      unsigned count, const D3DRECT *rects, unsigned flags,
      D3DCOLOR color, float z, unsigned stencil)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->Clear(count, rects, flags, color, z, stencil);
#else
            IDirect3DDevice9_Clear(dev, count, rects, flags,
                  color, z, stencil);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->Clear(count, rects, flags, color, z, stencil);
#else
            IDirect3DDevice8_Clear(dev, count, rects, flags,
                  color, z, stencil);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

bool d3d_device_get_render_target_data(void *_dev,
      void *_src, void *_dst)
{
   LPDIRECT3DSURFACE src = (LPDIRECT3DSURFACE)_src;
   LPDIRECT3DSURFACE dst = (LPDIRECT3DSURFACE)_dst;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifndef _XBOX
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return false;
#ifdef __cplusplus
            if (SUCCEEDED(dev->GetRenderTargetData(src, dst)))
               return true;
#else
            if (SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(
                        dev, src, dst)))
               return true;
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

bool d3d_device_get_render_target(void *_dev,
      unsigned idx, void **data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return false;
#ifdef __cplusplus
            if (SUCCEEDED(dev->GetRenderTarget(idx,
                        (LPDIRECT3DSURFACE*)data)))
               return true;
#else
            if (SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,
                        idx, (LPDIRECT3DSURFACE*)data)))
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            if (!dev)
               return false;
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
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}


bool d3d_lock_rectangle(void *_tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
            if (!tex)
               return false;
#ifdef __cplusplus
            if (FAILED(tex->LockRect(level, lock_rect, rect, flags)))
               return false;
#else
#ifdef _XBOX
            IDirect3DTexture9_LockRect(tex, level, lock_rect, (const RECT*)rect, flags);
#else
            if (IDirect3DTexture9_LockRect(tex, level, lock_rect, (const RECT*)rect, flags) != D3D_OK)
               return false;
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
            if (!tex)
               return false;
#ifdef __cplusplus
            if (FAILED(tex->LockRect(level, lock_rect, rect, flags)))
               return false;
#else
            if (IDirect3DTexture8_LockRect(tex, level, lock_rect, rect, flags) != D3D_OK)
               return false;
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;
}

void d3d_unlock_rectangle(void *_tex)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)_tex;
            if (!tex)
               return;
#ifdef __cplusplus
            tex->UnlockRect(0);
#else
            IDirect3DTexture9_UnlockRect(tex, 0);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DTEXTURE8 tex = (LPDIRECT3DTEXTURE8)_tex;
            if (!tex)
               return;
#ifdef __cplusplus
            tex->UnlockRect(0);
#else
            IDirect3DTexture8_UnlockRect(tex, 0);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_lock_rectangle_clear(void *tex,
      unsigned level, D3DLOCKED_RECT *lock_rect, RECT *rect,
      unsigned rectangle_height, unsigned flags)
{
#if defined(_XBOX)
   level = 0;
#endif
   memset(lock_rect->pBits, level, rectangle_height * lock_rect->Pitch);
   d3d_unlock_rectangle(tex);
}

void d3d_set_viewports(void *_dev, void *_vp)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            D3DVIEWPORT9      *vp = (D3DVIEWPORT9*)_vp;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetViewport(vp);
#else
            IDirect3DDevice9_SetViewport(dev, vp);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            D3DVIEWPORT8      *vp = (D3DVIEWPORT8*)_vp;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetViewport(vp);
#else
            IDirect3DDevice8_SetViewport(dev, vp);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_set_texture(void *_dev, unsigned sampler,
      void *tex_data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DTEXTURE9 tex = (LPDIRECT3DTEXTURE9)tex_data;
            LPDIRECT3DDEVICE9 dev  = (LPDIRECT3DDEVICE9)_dev;
            if (!dev || !tex)
               return;
#ifdef __cplusplus
            dev->SetTexture(sampler, tex);
#else
            IDirect3DDevice9_SetTexture(dev, sampler,
                  (IDirect3DBaseTexture9*)tex);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
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
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_free_vertex_shader(void *_dev, void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
         {
            LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
            IDirect3DVertexShader9 *vs = (IDirect3DVertexShader9*)data;
            if (!dev || !vs)
               return;
#ifdef __cplusplus
            vs->Release();
#else
            IDirect3DVertexShader9_Release(vs);
#endif
         }
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_free_pixel_shader(void *_dev, void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
         {
            LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
            IDirect3DPixelShader9 *ps  = (IDirect3DPixelShader9*)data;
            if (!dev || !ps)
               return;
#ifdef __cplusplus
            ps->Release();
#else
            IDirect3DPixelShader9_Release(ps);
#endif
         }
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
}

bool d3d_create_vertex_shader(void *_dev, const DWORD *a, void **b)
{
   if (!_dev)
      return false;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
#if defined(__cplusplus)
            if (dev->CreateVertexShader(a, (IDirect3DVertexShader9**)b) == D3D_OK)
               return true;
#else
            if (IDirect3DDevice9_CreateVertexShader(dev, a,
                     (LPDIRECT3DVERTEXSHADER*)b) == D3D_OK)
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

bool d3d_create_pixel_shader(void *_dev, const DWORD *a, void **b)
{
   if (!_dev)
      return false;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9      dev = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
            if (dev->CreatePixelShader(a, (IDirect3DPixelShader9**)b) == D3D_OK)
               return true;
#else
            if (IDirect3DDevice9_CreatePixelShader(dev, a,
                     (LPDIRECT3DPIXELSHADER*)b) == D3D_OK)
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

bool d3d_set_pixel_shader(void *_dev, void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
         {
            LPDIRECT3DDEVICE9      dev  = (LPDIRECT3DDEVICE9)_dev;
            LPDIRECT3DPIXELSHADER d3dps = (LPDIRECT3DPIXELSHADER)data;
            if (!dev || !d3dps)
               return false;
#if defined(__cplusplus)
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
         }
#endif
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

bool d3d_set_vertex_shader(void *_dev, unsigned index,
      void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9      dev    = (LPDIRECT3DDEVICE9)_dev;
            LPDIRECT3DVERTEXSHADER shader = (LPDIRECT3DVERTEXSHADER)data;
#ifdef __cplusplus
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
         {
            LPDIRECT3DDEVICE8      dev    = (LPDIRECT3DDEVICE8)_dev;
#ifdef __cplusplus
            LPDIRECT3DVERTEXSHADER shader = (LPDIRECT3DVERTEXSHADER)data;

            if (dev->SetVertexShader(shader) != D3D_OK)
               return false;
#else
            if (IDirect3DDevice8_SetVertexShader(dev, index) != D3D_OK)
               return false;
#endif
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;
}

bool d3d_set_vertex_shader_constantf(void *_dev,
      UINT start_register,const float* constant_data,
      unsigned vector4f_count)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#if defined(HAVE_D3D9)
            LPDIRECT3DDEVICE9      dev    = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

void d3d_texture_blit(unsigned pixel_size,
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

bool d3d_get_render_state(void *data, D3DRENDERSTATETYPE state, DWORD *value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
#ifdef __cplusplus
            if (dev && dev->GetRenderState(state, value) == D3D_OK)
               return true;
#else
#ifdef _XBOX
            if (!dev)
               return false;
            IDirect3DDevice9_GetRenderState(dev, state, value);
            return true;
#else
            if (dev && IDirect3DDevice9_GetRenderState(dev, state, value) == D3D_OK)
               return true;
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
#ifdef __cplusplus
            if (dev && dev->GetRenderState(state, value) == D3D_OK)
               return true;
#else
            if (dev && IDirect3DDevice8_GetRenderState(dev, state, value) == D3D_OK)
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

void d3d_set_render_state(void *data, D3DRENDERSTATETYPE state, DWORD value)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetRenderState(state, value);
#else
            IDirect3DDevice9_SetRenderState(dev, state, value);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetRenderState(state, value);
#else
            IDirect3DDevice8_SetRenderState(dev, state, value);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_enable_blend_func(void *data)
{
   if (!data)
      return;

   d3d_set_render_state(data, D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
   d3d_set_render_state(data, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
   d3d_set_render_state(data, D3DRS_ALPHABLENDENABLE, true);
}

void d3d_device_set_render_target(void *_dev, unsigned idx,
      void *data)
{
   LPDIRECT3DSURFACE surf = (LPDIRECT3DSURFACE)data;

   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetRenderTarget(idx, surf);
#else
            IDirect3DDevice9_SetRenderTarget(dev, idx, surf);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetRenderTarget(idx, surf);
#else
            IDirect3DDevice8_SetRenderTarget(dev, surf, NULL);
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_enable_alpha_blend_texture_func(void *data)
{
   /* Also blend the texture with the set alpha value. */
   d3d_set_texture_stage_state(data, 0, D3DTSS_ALPHAOP,     D3DTOP_MODULATE);
   d3d_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG1,   D3DTA_DIFFUSE);
   d3d_set_texture_stage_state(data, 0, D3DTSS_ALPHAARG2,   D3DTA_TEXTURE);
}

void d3d_frame_postprocess(void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D8_API:
#ifdef HAVE_D3D8
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
#endif
         break;
      case GFX_CTX_DIRECT3D9_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
}

void d3d_disable_blend_func(void *data)
{
   d3d_set_render_state(data, D3DRS_ALPHABLENDENABLE, false);
}

void d3d_set_vertex_declaration(void *data, void *vertex_data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#if defined(HAVE_D3D9)
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
            if (!dev)
               return;
#ifdef __cplusplus
            dev->SetVertexDeclaration((LPDIRECT3DVERTEXDECLARATION)vertex_data);
#else
            IDirect3DDevice9_SetVertexDeclaration(dev, (LPDIRECT3DVERTEXDECLARATION)vertex_data);
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static bool d3d_reset_internal(void *data,
      D3DPRESENT_PARAMETERS *d3dpp
      )
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
            if (!dev)
               return false;
#ifdef __cplusplus
            if ((dev->Reset(d3dpp) == D3D_OK))
               return true;
#else
            if (IDirect3DDevice9_Reset(dev, d3dpp) == D3D_OK)
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
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
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static HRESULT d3d_test_cooperative_level(void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifndef _XBOX
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
            if (!dev)
               return E_FAIL;
#ifdef __cplusplus
            return dev->TestCooperativeLevel();
#else
            return IDirect3DDevice9_TestCooperativeLevel(dev);
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifndef _XBOX
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
            if (!dev)
               return E_FAIL;
#ifdef __cplusplus
            return dev->TestCooperativeLevel();
#else
            return IDirect3DDevice8_TestCooperativeLevel(dev);
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return E_FAIL;
}

static bool d3d_create_device_internal(
      void *data,
      D3DPRESENT_PARAMETERS *d3dpp,
      void *_d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3D9       d3d = (LPDIRECT3D9)_d3d;
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
            if (!dev)
               return false;
#ifdef __cplusplus
            if (SUCCEEDED(d3d->CreateDevice(
                        cur_mon_id,
                        D3DDEVTYPE_HAL,
                        focus_window,
                        behavior_flags,
                        d3dpp,
                        dev)))
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
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
                        dev)))
               return true;
#else
            if (SUCCEEDED(IDirect3D8_CreateDevice(d3d,
                        cur_mon_id,
                        D3DDEVTYPE_HAL,
                        focus_window,
                        behavior_flags,
                        d3dpp,
                        dev)))
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

bool d3d_create_device(void *dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      void *d3d,
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

bool d3d_reset(void *dev, D3DPRESENT_PARAMETERS *d3dpp)
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

bool d3d_device_get_backbuffer(void *_dev, 
      unsigned idx, unsigned swapchain_idx, 
      unsigned backbuffer_type, void **data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (!dev)
               return false;
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
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            if (!dev)
               return false;
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
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}


void d3d_device_free(void *_dev, void *_pd3d)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
            LPDIRECT3D9      pd3d = (LPDIRECT3D9)_pd3d;
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
            if (dev)
            {
#ifdef __cplusplus
               dev->Release();
#else
               IDirect3DDevice9_Release(dev);
#endif
            }

            if (pd3d)
            {
#ifdef __cplusplus
               pd3d->Release();
#else
               IDirect3D9_Release(pd3d);
#endif
            }
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3D8
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
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
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

bool d3dx_create_font_indirect(void *_dev,
      void *desc, void **font_data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3DX
#ifdef HAVE_D3D9
            LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)_dev;
#ifdef __cplusplus
            if (SUCCEEDED(D3DCreateFontIndirect(
                        dev, (D3DXFONT_DESC*)desc, font_data)))
               return true;
#else
            if (SUCCEEDED(D3DCreateFontIndirect(
                        dev, (D3DXFONT_DESC*)desc,
                        (struct ID3DXFont**)font_data)))
               return true;
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
         {
#ifdef HAVE_D3DX
#ifdef HAVE_D3D8
            LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)_dev;
            if (SUCCEEDED(D3DCreateFontIndirect(
                        dev, (CONST LOGFONT*)desc,
                        (struct ID3DXFont**)font_data)))
               return true;
#endif
#endif
         }
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

void d3dxbuffer_release(void *data)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
         {
#ifdef HAVE_D3D9
#ifdef HAVE_D3DX
#ifdef __cplusplus
            ID3DXBuffer *p = (ID3DXBuffer*)data;
#else
            LPD3DXBUFFER p = (LPD3DXBUFFER)data;
#endif
            if (!p)
               return;

#ifdef __cplusplus
            p->Release();
#else
            p->lpVtbl->Release(p);
#endif
#endif
#endif
         }
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
}

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
      void *ppconstanttable)
{
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#if defined(HAVE_D3DX) && defined(HAVE_D3D9)
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
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
   return false;
}

void d3dx_font_release(void *data)
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

void d3dx_font_get_text_metrics(void *data, void *metrics)
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

bool d3dx_compile_shader_from_file(
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
   switch (d3d_common_api)
   {
      case GFX_CTX_DIRECT3D9_API:
#if defined(HAVE_D3DX) && defined(HAVE_D3D9)
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
         break;
      case GFX_CTX_DIRECT3D8_API:
      case GFX_CTX_NONE:
      default:
         break;
   }
   return false;
}

D3DFORMAT d3d_get_rgb565_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_R5G6B5;
#else
   return D3DFMT_R5G6B5;
#endif
}

D3DFORMAT d3d_get_argb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_A8R8G8B8;
#else
   return D3DFMT_A8R8G8B8;
#endif
}

D3DFORMAT d3d_get_xrgb8888_format(void)
{
#ifdef _XBOX
   return D3DFMT_LIN_X8R8G8B8;
#else
   return D3DFMT_X8R8G8B8;
#endif
}
