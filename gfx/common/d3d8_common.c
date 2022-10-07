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

#include "d3d8_common.h"

/* TODO/FIXME - static globals */
#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d8_dll;
static bool dylib_initialized = false;
#endif

typedef IDirect3D8 *(__stdcall *D3DCreate_t)(UINT);

static D3DCreate_t D3DCreate;

void *d3d8_create(void)
{
#ifdef _XBOX
   UINT ver = 0;
#else
   UINT ver = 220;
#endif
   return D3DCreate(ver);
}

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

#ifdef HAVE_DYNAMIC_D3D
   D3DCreate                = (D3DCreate_t)dylib_proc(g_d3d8_dll, "Direct3DCreate8");
#else
   D3DCreate                = Direct3DCreate8;
#endif

   if (!D3DCreate)
      goto error;

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
   g_d3d8_dll         = NULL;
   dylib_initialized = false;
#endif
}

void *d3d8_texture_new(LPDIRECT3DDEVICE8 dev,
      unsigned width, unsigned height,
      unsigned miplevels, unsigned usage, INT32 format,
      INT32 pool, unsigned filter, unsigned mipfilter,
      INT32 color_key, void *src_info_data,
      PALETTEENTRY *palette, bool want_mipmap)
{
   void *buf             = NULL;
   if (SUCCEEDED(IDirect3DDevice8_CreateTexture(dev,
               width, height, miplevels, usage,
               (D3DFORMAT)format, (D3DPOOL)pool,
               (struct IDirect3DTexture8**)&buf)))
      return buf;
   return NULL;
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

bool d3d8_reset(void *data, void *d3dpp)
{
   const char       *err = NULL;
   LPDIRECT3DDEVICE8 dev = (LPDIRECT3DDEVICE8)data;
   if (dev && IDirect3DDevice8_Reset(dev, (D3DPRESENT_PARAMETERS*)d3dpp) ==
         D3D_OK)
      return true;
#ifndef _XBOX
   RARCH_WARN("[D3D]: Attempting to recover from dead state...\n");
   /* Try to recreate the device completely. */
   switch (IDirect3DDevice8_TestCooperativeLevel(dev))
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
