/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

/* VC6 needs objbase included before initguid, but nothing else does */
#include <objbase.h>
#include <initguid.h>
#include <windows.h>
#include <ntverp.h>

#ifndef COBJMACROS
#define COBJMACROS
#define COBJMACROS_DEFINED
#endif
/* We really just want shobjidl.h, but there's no way to detect its existance at compile time (especially with mingw). however shlobj happens to include it for us when it's supported, which is easier. */
#include <shlobj.h>
#ifdef COBJMACROS_DEFINED
#undef COBJMACROS
#endif

#include "../video_display_server.h"
#include "../common/win32_common.h"
#include "../../verbosity.h"
#include "../video_driver.h" /* needed to set refresh rate in set resolution */

#ifdef __ITaskbarList3_INTERFACE_DEFINED__
#define HAS_TASKBAR_EXT

static ITaskbarList3 *g_taskbarList = NULL;

/* MSVC really doesn't want CINTERFACE to be used with shobjidl for some reason, but since we use C++ mode,
 * we need a workaround... so use the names of the COBJMACROS functions instead. */
#if defined(__cplusplus) && !defined(CINTERFACE)
#define ITaskbarList3_Release(x) g_taskbarList->Release()
#define ITaskbarList3_SetProgressState(a, b, c) g_taskbarList->SetProgressState(b, c)
#define ITaskbarList3_SetProgressValue(a, b, c, d) g_taskbarList->SetProgressValue(b, c, d)
#endif

#endif

typedef struct
{
   unsigned opacity;
   int progress;
   bool decorations;
} dispserv_win32_t;

/*
   NOTE: When an application displays a window, its taskbar button is created
   by the system. When the button is in place, the taskbar sends a
   TaskbarButtonCreated message to the window. Its value is computed by
   calling RegisterWindowMessage(L("TaskbarButtonCreated")). That message must
   be received by your application before it calls any ITaskbarList3 method.
 */

static unsigned win32_orig_width          = 0;
static unsigned win32_orig_height         = 0;
static unsigned win32_orig_refresh        = 0;
static int crt_center                     = 0;

static void* win32_display_server_init(void)
{
   HRESULT hr;
   dispserv_win32_t *dispserv = (dispserv_win32_t*)calloc(1, sizeof(*dispserv));

   (void)hr;

   if (!dispserv)
      return NULL;

#ifdef HAS_TASKBAR_EXT
#ifdef __cplusplus
   /* When compiling in C++ mode, GUIDs are references instead of pointers */
   hr = CoCreateInstance(CLSID_TaskbarList, NULL,
         CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (void**)&g_taskbarList);
#else
   /* Mingw GUIDs are pointers instead of references since we're in C mode */
   hr = CoCreateInstance(&CLSID_TaskbarList, NULL,
         CLSCTX_INPROC_SERVER, &IID_ITaskbarList3, (void**)&g_taskbarList);
#endif

   if (!SUCCEEDED(hr))
   {
      g_taskbarList = NULL;
      RARCH_ERR("[dispserv]: CoCreateInstance of ITaskbarList3 failed.\n");
   }
#endif

   return dispserv;
}

static void win32_display_server_destroy(void *data)
{
   dispserv_win32_t *dispserv = (dispserv_win32_t*)data;

   if (win32_orig_width > 0 && win32_orig_height > 0)
      video_display_server_switch_resolution(win32_orig_width, win32_orig_height,
            win32_orig_refresh, (float)win32_orig_refresh, crt_center );

#ifdef HAS_TASKBAR_EXT
   if (g_taskbarList && win32_taskbar_is_created())
   {
      ITaskbarList3_Release(g_taskbarList);
      g_taskbarList = NULL;
   }
#endif

   if (dispserv)
      free(dispserv);
}

static bool win32_display_server_set_window_opacity(void *data, unsigned opacity)
{
   HWND hwnd = win32_get_window();
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   if (serv)
      serv->opacity       = opacity;

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   /* Set window transparency on Windows 2000 and above */
   if (opacity < 100)
   {
      SetWindowLongPtr(hwnd,
            GWL_EXSTYLE,
            GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
      return SetLayeredWindowAttributes(hwnd, 0, (255 * opacity) / 100, LWA_ALPHA);
   }

   SetWindowLongPtr(hwnd,
         GWL_EXSTYLE,
         GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
   return true;
#else
   return false;
#endif
}

static bool win32_display_server_set_window_progress(void *data, int progress, bool finished)
{
   HWND hwnd = win32_get_window();
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   if (serv)
      serv->progress      = progress;

#ifdef HAS_TASKBAR_EXT
   if (!g_taskbarList || !win32_taskbar_is_created())
      return false;

   if (progress == -1)
   {
      if (ITaskbarList3_SetProgressState(
            g_taskbarList, hwnd, TBPF_INDETERMINATE) != S_OK)
         return false;
   }
   else if (finished)
   {
      if (ITaskbarList3_SetProgressState(
            g_taskbarList, hwnd, TBPF_NOPROGRESS) != S_OK)
         return false;
   }
   else if (progress >= 0)
   {
      if (ITaskbarList3_SetProgressState(
            g_taskbarList, hwnd, TBPF_NORMAL) != S_OK)
         return false;

      if (ITaskbarList3_SetProgressValue(
            g_taskbarList, hwnd, progress, 100) != S_OK)
         return false;
   }
#endif

   return true;
}

static bool win32_display_server_set_window_decorations(void *data, bool on)
{
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   if (serv)
      serv->decorations = on;

   /* menu_setting performs a reinit instead to properly
    * apply decoration changes */

   return true;
}

static bool win32_display_server_set_resolution(void *data,
      unsigned width, unsigned height, int int_hz, float hz, int center)
{
   LONG res;
   DEVMODE curDevmode;
   DEVMODE devmode;

   int iModeNum;
   int freq               = int_hz;
   DWORD flags            = 0;
   int depth              = 0;
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   if (!serv)
      return false;

   EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &curDevmode);

   if (win32_orig_width == 0)
      win32_orig_width          = GetSystemMetrics(SM_CXSCREEN);
   win32_orig_refresh        = curDevmode.dmDisplayFrequency;
   if (win32_orig_height == 0)
      win32_orig_height         = GetSystemMetrics(SM_CYSCREEN);

   /* Used to stop super resolution bug */
   if (width == curDevmode.dmPelsWidth)
      width  = 0;
   if (width == 0)
      width = curDevmode.dmPelsWidth;
   if (height == 0)
      height = curDevmode.dmPelsHeight;
   if (depth == 0)
      depth = curDevmode.dmBitsPerPel;
   if (freq == 0)
      freq = curDevmode.dmDisplayFrequency;

   for (iModeNum = 0;; iModeNum++)
   {
      if (!EnumDisplaySettings(NULL, iModeNum, &devmode))
         break;

      if (devmode.dmPelsWidth != width)
         continue;

      if (devmode.dmPelsHeight != height)
         continue;

      if (devmode.dmBitsPerPel != depth)
         continue;

      if (devmode.dmDisplayFrequency != freq)
         continue;

      devmode.dmFields |=
            DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
      res               =
            win32_change_display_settings(NULL, &devmode, CDS_TEST);

      switch (res)
      {
      case DISP_CHANGE_SUCCESSFUL:
         res = win32_change_display_settings(NULL, &devmode, flags);
         switch (res)
         {
         case DISP_CHANGE_SUCCESSFUL:
            return true;
         case DISP_CHANGE_NOTUPDATED:
            return true;
         default:
            break;
         }
         break;
      case DISP_CHANGE_RESTART:
         break;
      default:
         break;
      }
   }

   return true;
}

const video_display_server_t dispserv_win32 = {
   win32_display_server_init,
   win32_display_server_destroy,
   win32_display_server_set_window_opacity,
   win32_display_server_set_window_progress,
   win32_display_server_set_window_decorations,
   win32_display_server_set_resolution,
   NULL, /* get_output_options */
   "win32"
};
