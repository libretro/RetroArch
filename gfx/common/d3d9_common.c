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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* For Xbox we will just link statically
 * to Direct3D libraries instead. */

#if !defined(_XBOX) && defined(HAVE_DYLIB)
#define HAVE_DYNAMIC_D3D
#endif

#ifdef HAVE_DYNAMIC_D3D
#include <dynamic/dylib.h>
#endif
#include <string/stdstring.h>

#include "../../verbosity.h"

#include "d3d9_common.h"

#ifdef _XBOX
#include <xgraphics.h>
#endif

#include "win32_common.h"

#define FS_PRESENTINTERVAL(pp) ((pp)->PresentationInterval)

/* TODO/FIXME - static globals */
LPDIRECT3D9 g_pD3D9;
#ifdef HAVE_DYNAMIC_D3D
static dylib_t g_d3d9_dll;
static bool d3d9_dylib_initialized = false;
#endif

typedef IDirect3D9 *(__stdcall *D3D9Create_t)(UINT);
static D3D9Create_t D3D9Create;

void *d3d9_create(void)
{
#ifdef _XBOX
   UINT ver = 0;
#else
   UINT ver = 31;
#endif
   return D3D9Create(ver);
}

bool d3d9_initialize_symbols(enum gfx_ctx_api api)
{
#ifdef HAVE_DYNAMIC_D3D
   if (d3d9_dylib_initialized)
      return true;
#if defined(DEBUG) || defined(_DEBUG)
   if (!(g_d3d9_dll  = dylib_load("d3d9d.dll")))
#endif
   if (!(g_d3d9_dll  = dylib_load("d3d9.dll")))
      return false;
   D3D9Create                 = (D3D9Create_t)dylib_proc(g_d3d9_dll, "Direct3DCreate9");
#else
   D3D9Create                 = Direct3DCreate9;
#endif

   if (!D3D9Create)
   {
      d3d9_deinitialize_symbols();
      return false;
   }

#ifdef HAVE_DYNAMIC_D3D
   d3d9_dylib_initialized = true;
#endif

   return true;
}

void d3d9_deinitialize_symbols(void)
{
#ifdef HAVE_DYNAMIC_D3D
   if (g_d3d9_dll)
      dylib_close(g_d3d9_dll);
   g_d3d9_dll         = NULL;

   d3d9_dylib_initialized = false;
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
   return (dev &&
         SUCCEEDED(IDirect3D9_CreateDevice(d3d,
               cur_mon_id,
               D3DDEVTYPE_HAL,
               focus_window,
               behavior_flags,
               d3dpp,
               (IDirect3DDevice9**)dev)));
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

bool d3d9_reset(void *data, void *d3dpp)
{
   LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)data;
   if (dev)
   {
      const char *err = NULL;
      if (IDirect3DDevice9_Reset(dev, (D3DPRESENT_PARAMETERS*)d3dpp) == D3D_OK)
         return true;
#ifndef _XBOX
      RARCH_WARN("[D3D] Attempting to recover from dead state...\n");
      /* Try to recreate the device completely. */
      switch (IDirect3DDevice9_TestCooperativeLevel(dev))
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
      RARCH_WARN("[D3D] Recovering from dead state: (%s).\n", err);
#endif
   }
   return false;
}

#ifdef _XBOX
static bool d3d9_is_windowed_enable(bool info_fullscreen)
{
   return false;
}

static D3DFORMAT d3d9_get_color_format_backbuffer(bool rgb32)
{
   if (rgb32)
      return D3DFMT_X8R8G8B8;
   return D3D9_RGB565_FORMAT;
}

static void d3d9_get_video_size(d3d9_video_t *d3d,
      unsigned *width, unsigned *height)
{
   XVIDEO_MODE video_mode;

   XGetVideoMode(&video_mode);

   *width                       = video_mode.dwDisplayWidth;
   *height                      = video_mode.dwDisplayHeight;

   d3d->resolution_hd_enable    = false;

   if (video_mode.fIsHiDef)
   {
      *width                    = 1280;
      *height                   = 720;
      d3d->resolution_hd_enable = true;
   }
   else
   {
      *width                    = 640;
      *height                   = 480;
   }

   d3d->widescreen_mode         = video_mode.fIsWideScreen;
}

static D3DFORMAT d3d9_get_color_format_front_buffer(void)
{
   return D3DFMT_LE_X8R8G8B8;
}
#else
static bool d3d9_is_windowed_enable(bool info_fullscreen)
{
   settings_t *settings = config_get_ptr();
   if (!info_fullscreen)
      return true;
   if (settings)
      return settings->bools.video_windowed_fullscreen;
   return false;
}

static D3DFORMAT d3d9_get_color_format_backbuffer(
      bool rgb32, bool windowed)
{
   if (windowed)
   {
      D3DDISPLAYMODE display_mode;
      if (IDirect3D9_GetAdapterDisplayMode(g_pD3D9, 0, &display_mode))
         return display_mode.Format;
   }
   return D3DFMT_X8R8G8B8;
}
#endif

void d3d9_make_d3dpp(d3d9_video_t *d3d,
      const video_info_t *info, void *_d3dpp)
{
   D3DPRESENT_PARAMETERS *d3dpp   = (D3DPRESENT_PARAMETERS*)_d3dpp;
#ifdef _XBOX
   /* TODO/FIXME - get rid of global state dependencies. */
   global_t *global               = global_get_ptr();
   int gamma_enable               = global ?
      global->console.screen.gamma_correction : 0;
#endif
   bool windowed_enable           = d3d9_is_windowed_enable(info->fullscreen);

   memset(d3dpp, 0, sizeof(*d3dpp));

   d3dpp->Windowed                = windowed_enable;
   FS_PRESENTINTERVAL(d3dpp)      = D3DPRESENT_INTERVAL_IMMEDIATE;

   if (info->vsync)
   {
      settings_t *settings         = config_get_ptr();
      unsigned video_swap_interval = runloop_get_video_swap_interval(
            settings->uints.video_swap_interval);

      switch (video_swap_interval)
      {
         default:
         case 1:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_ONE;
            break;
         case 2:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_TWO;
            break;
         case 3:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_THREE;
            break;
         case 4:
            FS_PRESENTINTERVAL(d3dpp) = D3DPRESENT_INTERVAL_FOUR;
            break;
      }
   }

   d3dpp->SwapEffect              = D3DSWAPEFFECT_DISCARD;
   d3dpp->BackBufferCount         = 2;

#ifdef _XBOX
   d3dpp->BackBufferFormat        = d3d9_get_color_format_backbuffer(
         info->rgb32);
   d3dpp->FrontBufferFormat       = d3d9_get_color_format_front_buffer();

   if (gamma_enable)
   {
      d3dpp->BackBufferFormat     = (D3DFORMAT)MAKESRGBFMT(
            d3dpp->BackBufferFormat);
      d3dpp->FrontBufferFormat    = (D3DFORMAT)MAKESRGBFMT(
            d3dpp->FrontBufferFormat);
   }
#else
   d3dpp->BackBufferFormat        = d3d9_get_color_format_backbuffer(
         info->rgb32, windowed_enable);
   d3dpp->hDeviceWindow           = win32_get_window();
#endif

   if (!windowed_enable)
   {
#ifdef _XBOX
      unsigned width  = 0;
      unsigned height = 0;
      d3d9_get_video_size(d3d, &width, &height);
      video_driver_set_size(width, height);
#endif
      video_driver_get_size(&d3dpp->BackBufferWidth,
            &d3dpp->BackBufferHeight);
   }

#ifdef _XBOX
   d3dpp->MultiSampleType         = D3DMULTISAMPLE_NONE;
   d3dpp->EnableAutoDepthStencil  = FALSE;
   if (!d3d->widescreen_mode)
      d3dpp->Flags |= D3DPRESENTFLAG_NO_LETTERBOX;
   d3dpp->MultiSampleQuality      = 0;
#endif
}
