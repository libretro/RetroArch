/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7 */
#endif

#include "../video_display_server.h"
#include "../common/win32_common.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef __ITaskbarList3_INTERFACE_DEFINED__
#define HAS_TASKBAR_EXT

/* MSVC really doesn't want CINTERFACE to be used 
 * with shobjidl for some reason, but since we use C++ mode,
 * we need a workaround... so use the names of the 
 * COBJMACROS functions instead. */
#if defined(__cplusplus) && !defined(CINTERFACE)
#define ITaskbarList3_HrInit(x) (x)->HrInit()
#define ITaskbarList3_Release(x) (x)->Release()
#define ITaskbarList3_SetProgressState(a, b, c) (a)->SetProgressState(b, c)
#define ITaskbarList3_SetProgressValue(a, b, c, d) (a)->SetProgressValue(b, c, d)
#endif

#endif

typedef struct
{
   bool decorations;
   int progress;
   int crt_center;
   unsigned opacity;
   unsigned orig_width;
   unsigned orig_height;
   unsigned orig_refresh;
#ifdef HAS_TASKBAR_EXT
   ITaskbarList3 *taskbar_list;
#endif
} dispserv_win32_t;

/*
   NOTE: When an application displays a window, its taskbar button is created
   by the system. When the button is in place, the taskbar sends a
   TaskbarButtonCreated message to the window. Its value is computed by
   calling RegisterWindowMessage(L("TaskbarButtonCreated")). That message must
   be received by your application before it calls any ITaskbarList3 method.
 */

static void *win32_display_server_init(void)
{
   dispserv_win32_t *dispserv = (dispserv_win32_t*)calloc(1, sizeof(*dispserv));

   if (!dispserv)
      return NULL;

#ifdef HAS_TASKBAR_EXT
#ifdef __cplusplus
   /* When compiling in C++ mode, GUIDs are references instead of pointers */
   if (FAILED(CoCreateInstance(CLSID_TaskbarList, NULL,
         CLSCTX_INPROC_SERVER, IID_ITaskbarList3,
         (void**)&dispserv->taskbar_list)))
#else
   /* Mingw GUIDs are pointers instead of references since we're in C mode */
   if (FAILED(CoCreateInstance(&CLSID_TaskbarList, NULL,
         CLSCTX_INPROC_SERVER, &IID_ITaskbarList3,
         (void**)&dispserv->taskbar_list)))
#endif
   {
      dispserv->taskbar_list = NULL;
      RARCH_ERR("[dispserv]: CoCreateInstance of ITaskbarList3 failed.\n");
   }
   else
   {
      if (FAILED(ITaskbarList3_HrInit(dispserv->taskbar_list)))
         RARCH_ERR("[dispserv]: HrInit of ITaskbarList3 failed.\n");
   }
#endif

   return dispserv;
}

static void win32_display_server_destroy(void *data)
{
   dispserv_win32_t *dispserv = (dispserv_win32_t*)data;

   if (dispserv->orig_width > 0 && dispserv->orig_height > 0)
      video_display_server_set_resolution(
            dispserv->orig_width,
            dispserv->orig_height,
            dispserv->orig_refresh,
            (float)dispserv->orig_refresh,
            dispserv->crt_center, 0, 0, 0);

#ifdef HAS_TASKBAR_EXT
   if (dispserv->taskbar_list)
   {
      ITaskbarList3_Release(dispserv->taskbar_list);
      dispserv->taskbar_list = NULL;
   }
#endif

   if (dispserv)
      free(dispserv);
}

static bool win32_display_server_set_window_opacity(
      void *data, unsigned opacity)
{
   HWND              hwnd = win32_get_window();
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
      return SetLayeredWindowAttributes(hwnd, 0, (255 * opacity) / 100,
            LWA_ALPHA);
   }

   SetWindowLongPtr(hwnd,
         GWL_EXSTYLE,
         GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
   return true;
#else
   return false;
#endif
}

static bool win32_display_server_set_window_progress(
      void *data, int progress, bool finished)
{
   HWND              hwnd = win32_get_window();
   dispserv_win32_t *serv = (dispserv_win32_t*)data;

   if (serv)
      serv->progress      = progress;

#ifdef HAS_TASKBAR_EXT
   if (!serv->taskbar_list || !win32_taskbar_is_created())
      return false;

   if (progress == -1)
   {
      if (ITaskbarList3_SetProgressState(
            serv->taskbar_list, hwnd, TBPF_INDETERMINATE) != S_OK)
         return false;
   }
   else if (finished)
   {
      if (ITaskbarList3_SetProgressState(
            serv->taskbar_list, hwnd, TBPF_NOPROGRESS) != S_OK)
         return false;
   }
   else if (progress >= 0)
   {
      if (ITaskbarList3_SetProgressState(
            serv->taskbar_list, hwnd, TBPF_NORMAL) != S_OK)
         return false;

      if (ITaskbarList3_SetProgressValue(
            serv->taskbar_list, hwnd, progress, 100) != S_OK)
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
      unsigned width, unsigned height, int int_hz, float hz, int center, int monitor_index, int xoffset, int padjust)
{
   DEVMODE dm                = {0};
   LONG res                  = 0;
   unsigned i                = 0;
   unsigned curr_bpp         = 0;
#if _WIN32_WINNT >= 0x0500
   unsigned curr_orientation = 0;
#endif
   dispserv_win32_t *serv    = (dispserv_win32_t*)data;

   if (!serv)
      return false;

   win32_get_video_output(&dm, -1, sizeof(dm));

   if (serv->orig_width == 0)
      serv->orig_width  = GetSystemMetrics(SM_CXSCREEN);
   if (serv->orig_height == 0)
      serv->orig_height = GetSystemMetrics(SM_CYSCREEN);
   if (serv->orig_refresh == 0)
      serv->orig_refresh = video_driver_get_refresh_rate();

   /* Used to stop super resolution bug */
   if (width == dm.dmPelsWidth)
      width = 0;
   if (width == 0)
      width = dm.dmPelsWidth;
   if (height == 0)
      height = dm.dmPelsHeight;
   if (curr_bpp == 0)
      curr_bpp = dm.dmBitsPerPel;
   if (int_hz == 0)
      int_hz = dm.dmDisplayFrequency;
#if _WIN32_WINNT >= 0x0500
   if (curr_orientation == 0)
      curr_orientation = dm.dmDisplayOrientation;
#endif

   for (i = 0; win32_get_video_output(&dm, i, sizeof(dm)); i++)
   {
      if (dm.dmPelsWidth  != width)
         continue;
      if (dm.dmPelsHeight != height)
         continue;
      if (dm.dmBitsPerPel != curr_bpp)
         continue;
      if (dm.dmDisplayFrequency != int_hz)
         continue;
#if _WIN32_WINNT >= 0x0500
      if (dm.dmDisplayOrientation != curr_orientation)
         continue;
      if (dm.dmDisplayFixedOutput != DMDFO_DEFAULT)
         continue;
#endif

      dm.dmFields |= DM_PELSWIDTH | DM_PELSHEIGHT 
                  | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
#if _WIN32_WINNT >= 0x0500
      dm.dmFields |= DM_DISPLAYORIENTATION;
#endif

      res = win32_change_display_settings(NULL, &dm, CDS_TEST);

      switch (res)
      {
         case DISP_CHANGE_SUCCESSFUL:
            res = win32_change_display_settings(NULL, &dm, 0);
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

/* Display resolution list qsort helper function */
static int resolution_list_qsort_func(
      const video_display_config_t *a, const video_display_config_t *b)
{
   char str_a[64];
   char str_b[64];

   if (!a || !b)
      return 0;

   snprintf(str_a, sizeof(str_a), "%04dx%04d (%d Hz)",
         a->width,
         a->height,
         a->refreshrate);

   snprintf(str_b, sizeof(str_b), "%04dx%04d (%d Hz)",
         b->width,
         b->height,
         b->refreshrate);

   return strcasecmp(str_a, str_b);
}

static void *win32_display_server_get_resolution_list(
      void *data, unsigned *len)
{
   DEVMODE dm                        = {0};
   unsigned i, j, count              = 0;
   unsigned curr_width               = 0;
   unsigned curr_height              = 0;
   unsigned curr_bpp                 = 0;
   unsigned curr_refreshrate         = 0;
#if _WIN32_WINNT >= 0x0500
   unsigned curr_orientation         = 0;
#endif
   struct video_display_config *conf = NULL;

   if (win32_get_video_output(&dm, -1, sizeof(dm)))
   {
      curr_width                     = dm.dmPelsWidth;
      curr_height                    = dm.dmPelsHeight;
      curr_bpp                       = dm.dmBitsPerPel;
      curr_refreshrate               = dm.dmDisplayFrequency;
#if _WIN32_WINNT >= 0x0500
      curr_orientation               = dm.dmDisplayOrientation;
#endif
   }

   for (i = 0; win32_get_video_output(&dm, i, sizeof(dm)); i++)
   {
      if (dm.dmBitsPerPel != curr_bpp)
         continue;
#if _WIN32_WINNT >= 0x0500
      if (dm.dmDisplayOrientation != curr_orientation)
         continue;
      if (dm.dmDisplayFixedOutput != DMDFO_DEFAULT)
         continue;
#endif

      count++;
   }

   *len = count;
   conf = (struct video_display_config*)
      calloc(*len, sizeof(struct video_display_config));

   if (!conf)
      return NULL;

   for (i = 0, j = 0; win32_get_video_output(&dm, i, sizeof(dm)); i++)
   {
      if (dm.dmBitsPerPel != curr_bpp)
         continue;
#if _WIN32_WINNT >= 0x0500
      if (dm.dmDisplayOrientation != curr_orientation)
         continue;
      if (dm.dmDisplayFixedOutput != DMDFO_DEFAULT)
         continue;
#endif

      conf[j].width       = dm.dmPelsWidth;
      conf[j].height      = dm.dmPelsHeight;
      conf[j].bpp         = dm.dmBitsPerPel;
      conf[j].refreshrate = dm.dmDisplayFrequency;
      conf[j].idx         = j;
      conf[j].current     = false;

      if (     (conf[j].width       == curr_width)
            && (conf[j].height      == curr_height)
            && (conf[j].bpp         == curr_bpp)
            && (conf[j].refreshrate == curr_refreshrate)
         )
         conf[j].current  = true;

      j++;
   }

   qsort(
         conf, count,
         sizeof(video_display_config_t),
         (int (*)(const void *, const void *))
               resolution_list_qsort_func);

   return conf;
}

#if _WIN32_WINNT >= 0x0500
enum rotation win32_display_server_get_screen_orientation(void *data)
{
   DEVMODE dm = {0};
   enum rotation rotation;

   win32_get_video_output(&dm, -1, sizeof(dm));

   switch (dm.dmDisplayOrientation)
   {
      case DMDO_DEFAULT:
      default:
         rotation = ORIENTATION_NORMAL;
         break;
      case DMDO_90:
         rotation = ORIENTATION_FLIPPED_ROTATED;
         break;
      case DMDO_180:
         rotation = ORIENTATION_FLIPPED;
         break;
      case DMDO_270:
         rotation = ORIENTATION_VERTICAL;
         break;
   }

   return rotation;
}

void win32_display_server_set_screen_orientation(void *data,
      enum rotation rotation)
{
   DEVMODE dm = {0};

   win32_get_video_output(&dm, -1, sizeof(dm));

   switch (rotation)
   {
      case ORIENTATION_NORMAL:
      default:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_90 
                     || dm.dmDisplayOrientation == DMDO_270)
                  && width != dm.dmPelsHeight)
            {
               /* device is changing orientations, swap the aspect */
               dm.dmPelsWidth = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_DEFAULT;
            break;
         }
      case ORIENTATION_VERTICAL:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_DEFAULT 
                     || dm.dmDisplayOrientation == DMDO_180) 
                  && width != dm.dmPelsHeight)
            {
               /* device is changing orientations, swap the aspect */
               dm.dmPelsWidth = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_270;
            break;
         }
      case ORIENTATION_FLIPPED:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_90 
                     || dm.dmDisplayOrientation == DMDO_270)
                  && width != dm.dmPelsHeight)
            {
               /* device is changing orientations, swap the aspect */
               dm.dmPelsWidth = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_180;
            break;
         }
      case ORIENTATION_FLIPPED_ROTATED:
         {
            int width = dm.dmPelsWidth;

            if ((       dm.dmDisplayOrientation == DMDO_DEFAULT 
                     || dm.dmDisplayOrientation == DMDO_180)
                  && width != dm.dmPelsHeight)
            {
               /* device is changing orientations, swap the aspect */
               dm.dmPelsWidth = dm.dmPelsHeight;
               dm.dmPelsHeight = width;
            }

            dm.dmDisplayOrientation = DMDO_90;
            break;
         }
   }

   win32_change_display_settings(NULL, &dm, 0);
}
#endif

static uint32_t win32_display_server_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, DISPSERV_CTX_CRT_SWITCHRES);

   return flags;
}

const video_display_server_t dispserv_win32 = {
   win32_display_server_init,
   win32_display_server_destroy,
   win32_display_server_set_window_opacity,
   win32_display_server_set_window_progress,
   win32_display_server_set_window_decorations,
   win32_display_server_set_resolution,
   win32_display_server_get_resolution_list,
   NULL, /* get_output_options */
#if _WIN32_WINNT >= 0x0500
   win32_display_server_set_screen_orientation,
   win32_display_server_get_screen_orientation,
#else
   NULL,
   NULL,
#endif
   win32_display_server_get_flags,
   "win32"
};
