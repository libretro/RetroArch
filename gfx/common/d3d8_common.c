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

#define CINTERFACE

/* For Xbox we will just link statically
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
#endif

#include "../../verbosity.h"

#ifdef HAVE_D3DX
#include <d3dx8core.h>
#include <d3dx8tex.h>
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

#ifdef HAVE_D3DX
static void *d3d8_texture_new_from_file(
      LPDIRECT3DDEVICE8 dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, D3DFORMAT format,
      D3DPOOL pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette)
{
   void *buf  = NULL;
   if (FAILED(D3DCreateTextureFromFile(dev,
         path, width, height, miplevels, usage, format,
         pool, filter, mipfilter, color_key, src_info_data,
         palette, (struct IDirect3DTeture8**)&buf)))
      return NULL;
   return buf;
}
#endif

void *d3d8_texture_new(LPDIRECT3DDEVICE8 dev,
      const char *path, unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
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

   if (FAILED(IDirect3DDevice8_CreateTexture(dev,
               width, height, miplevels, usage,
               (D3DFORMAT)format, (D3DPOOL)pool,
               (struct IDirect3DTexture8**)&buf)))
      return NULL;

   return buf;
}

void d3d8_frame_postprocess(void *data)
{
#if defined(_XBOX)
   global_t        *global = global_get_ptr();
   D3DDevice_SetFlickerFilter(global->console.screen.flicker_filter_index);
   D3DDevice_SetSoftDisplayFilter(global->console.softfilter_enable);
#endif
}

static bool d3d8_reset_internal(LPDIRECT3DDEVICE8 dev,
      D3DPRESENT_PARAMETERS *d3dpp
      )
{
   if (dev &&
         IDirect3DDevice8_Reset(dev, d3dpp) == D3D_OK)
      return true;
   return false;
}

static HRESULT d3d8_test_cooperative_level(LPDIRECT3DDEVICE8 dev)
{
#ifndef _XBOX
   if (dev)
      return IDirect3DDevice8_TestCooperativeLevel(dev);
#endif
   return E_FAIL;
}

static bool d3d8_create_device_internal(
      LPDIRECT3DDEVICE8 dev,
      D3DPRESENT_PARAMETERS *d3dpp,
      LPDIRECT3D8 d3d,
      HWND focus_window,
      unsigned cur_mon_id,
      DWORD behavior_flags)
{
   if (dev &&
         SUCCEEDED(IDirect3D8_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice8**)dev)))
      return true;

   return false;
}

bool d3d8_create_device(void *dev,
      void *d3dpp,
      LPDIRECT3D8 d3d,
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

bool d3d8x_create_font_indirect(LPDIRECT3DDEVICE8 dev,
      void *desc, void **font_data)
{
#ifdef HAVE_D3DX
   if (SUCCEEDED(D3DCreateFontIndirect(
               dev, (CONST LOGFONT*)desc,
               (struct ID3DXFont**)font_data)))
      return true;
#endif
   return false;
}

void d3d8x_font_draw_text(void *data,
      void *sprite_data, void *string_data,
      unsigned count, void *rect_data,
      unsigned format, unsigned color)
{
#ifdef HAVE_D3DX
   ID3DXFont *font = (ID3DXFont*)data;
   if (font)
      font->lpVtbl->DrawText(font, (LPD3DXSPRITE)sprite_data,
            (LPCTSTR)string_data, count, (LPRECT)rect_data,
            (DWORD)format, (D3DCOLOR)color);
#endif
}

void d3d8x_font_release(void *data)
{
#ifdef HAVE_D3DX
   ID3DXFont *font = (ID3DXFont*)data;
   if (font)
      font->lpVtbl->Release(font);
#endif
}

void d3d8x_font_get_text_metrics(void *data, void *metrics)
{
#ifdef HAVE_D3DX
   ID3DXFont *font = (ID3DXFont*)data;
   if (font)
      font->lpVtbl->GetTextMetrics(font, (TEXTMETRICA*)metrics);
#endif
}
