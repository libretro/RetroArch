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

#ifdef __ITaskbarList3_INTERFACE_DEFINED__
#define HAS_TASKBAR_EXT

static ITaskbarList3 *g_taskbarList = NULL;

/* MSVC really doesn't want CINTERFACE to be used with shobjidl for some reason, but since we use C++ mode,
 * we need a workaround... so use the names of the COBJMACROS functions instead. */
#ifdef __cplusplus
#define ITaskbarList3_Release(x) g_taskbarList->Release()
#define ITaskbarList3_SetProgressState(a, b, c) g_taskbarList->SetProgressState(b, c)
#define ITaskbarList3_SetProgressValue(a, b, c, d) g_taskbarList->SetProgressValue(b, c, d)
#endif

#endif

typedef struct
{
   unsigned opacity;
   int progress;
} dispserv_win32_t;

/*
NOTE: When an application displays a window, its taskbar button is created
by the system. When the button is in place, the taskbar sends a
TaskbarButtonCreated message to the window. Its value is computed by
calling RegisterWindowMessage(L("TaskbarButtonCreated")). That message must
be received by your application before it calls any ITaskbarList3 method.
*/

static void* win32_display_server_init(void)
{
   dispserv_win32_t *dispserv = (dispserv_win32_t*)calloc(1, sizeof(*dispserv));
   HRESULT hr;

   (void)hr;

   if (!dispserv)
      return NULL;

#ifdef HAS_TASKBAR_EXT
#ifdef __cplusplus
   /* when compiling in C++ mode, GUIDs are references instead of pointers */
   hr = CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (void**)&g_taskbarList);
#else
   /* mingw GUIDs are pointers instead of references since we're in C mode */
   hr = CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList3, (void**)&g_taskbarList);
#endif

   if (!SUCCEEDED(hr))
   {
      g_taskbarList = false;
      RARCH_ERR("[dispserv]: CoCreateInstance of ITaskbarList3 failed.\n");
   }
#endif

   return dispserv;
}

static void win32_display_server_destroy(void)
{
#ifdef HAS_TASKBAR_EXT
   if (g_taskbarList && win32_taskbar_is_created())
   {
      ITaskbarList3_Release(g_taskbarList);
      g_taskbarList = NULL;
   }
#endif
}

static bool win32_set_window_opacity(void *data, unsigned opacity)
{
   HWND              hwnd = win32_get_window();
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   serv->opacity          = opacity;

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500
   /* Set window transparency on Windows 2000 and above */
   if (SetLayeredWindowAttributes(hwnd, 0, (255 * opacity) / 100, LWA_ALPHA))
      return true;
#endif
   return false;
}

static bool win32_set_window_progress(void *data, int progress, bool finished)
{
   HWND              hwnd = win32_get_window();
   dispserv_win32_t *serv = (dispserv_win32_t*)data;
   bool               ret = false;

   serv->progress = progress;

#ifdef HAS_TASKBAR_EXT
   if (!g_taskbarList || !win32_taskbar_is_created())
      return false;

   if (progress == -1)
   {
      if (ITaskbarList3_SetProgressState(g_taskbarList, hwnd, TBPF_INDETERMINATE) == S_OK)
         ret = true;

      if (!ret)
         return false;
   }
   else if (finished)
   {
      if (ITaskbarList3_SetProgressState(g_taskbarList, hwnd, TBPF_NOPROGRESS) == S_OK)
         ret = true;

      if (!ret)
         return false;
   }
   else if (progress >= 0)
   {
      if (ITaskbarList3_SetProgressState(g_taskbarList, hwnd, TBPF_NORMAL) == S_OK)
         ret = true;

      if (!ret)
         return false;

      if (ITaskbarList3_SetProgressValue(g_taskbarList, hwnd, progress, 100) == S_OK)
         ret = true;
   }
#endif

   return ret;
}

const video_display_server_t dispserv_win32 = {
   win32_display_server_init,
   win32_display_server_destroy,
   win32_set_window_opacity,
   win32_set_window_progress,
   "win32"
};

