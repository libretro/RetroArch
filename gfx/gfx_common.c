/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "winmm")
#endif

#include "gfx_common.h"
#include "../general.h"

#ifndef _MSC_VER
#include <sys/time.h>
#else
#ifndef _XBOX
#include <winsock2.h>
#include <mmsystem.h>
#endif
#endif

#if defined(__PSL1GHT__)
#include <sys/time.h>
#elif defined(__CELLOS_LV2__)
#include <sys/sys_time.h>
#endif

#ifdef GEKKO
#include <ogc/lwp_watchdog.h>
#endif

#ifdef __linux__
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#endif

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(_MSC_VER) || defined(GEKKO)
static int gettimeofday2(struct timeval *val, void *dummy)
{
   (void)dummy;
#if defined(_MSC_VER) && !defined(_XBOX360)
   DWORD msec = timeGetTime();
#elif defined(_XBOX360)
   DWORD msec = GetTickCount();
#endif

#if defined(__CELLOS_LV2__)
   uint64_t usec = sys_time_get_system_time();
#elif defined(GEKKO)
   uint64_t usec = ticks_to_microsecs(gettime());
#else
   uint64_t usec = msec * 1000;
#endif

   val->tv_sec  = usec / 1000000;
   val->tv_usec = usec % 1000000;
   return 0;
}

// GEKKO has gettimeofday, but it's not accurate enough for calculating FPS, so hack around it
#define gettimeofday gettimeofday2
#endif

static float tv_to_fps(const struct timeval *tv, const struct timeval *new_tv, int frames)
{
   float time = new_tv->tv_sec - tv->tv_sec + (new_tv->tv_usec - tv->tv_usec) / 1000000.0;
   return frames/time;
}

bool gfx_get_fps(char *buf, size_t size, bool always_write)
{
   static struct timeval tv;
   static float last_fps;
   struct timeval new_tv;
   bool ret = false;

   if (g_extern.frame_count == 0)
   {
      gettimeofday(&tv, NULL);
      snprintf(buf, size, "%s", g_extern.title_buf);
      ret = true;
   }
   else if ((g_extern.frame_count % 180) == 0)
   {
      gettimeofday(&new_tv, NULL);
      struct timeval tmp_tv = tv;
      tv = new_tv;

      last_fps = tv_to_fps(&tmp_tv, &new_tv, 180);

#ifdef RARCH_CONSOLE
      snprintf(buf, size, "FPS: %6.1f || Frames: %d", last_fps, g_extern.frame_count);
#else
      snprintf(buf, size, "%s || FPS: %6.1f || Frames: %d", g_extern.title_buf, last_fps, g_extern.frame_count);
#endif
      ret = true;
   }
   else if (always_write)
   {
#ifdef RARCH_CONSOLE
      snprintf(buf, size, "FPS: %6.1f || Frames: %d", last_fps, g_extern.frame_count);
#else
      snprintf(buf, size, "%s || FPS: %6.1f || Frames: %d", g_extern.title_buf, last_fps, g_extern.frame_count);
#endif
   }

   return ret;
}

void gfx_window_title_reset(void)
{
   g_extern.frame_count = 0;
}

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#include "../dynamic.h"
// We only load this library once, so we let it be unloaded at application shutdown,
// since unloading it early seems to cause issues on some systems.

static dylib_t dwmlib = NULL;

static void gfx_dwm_shutdown(void)
{
   if (dwmlib)
      dylib_close(dwmlib);
}

void gfx_set_dwm(void)
{
   static bool inited = false;
   if (inited)
      return;
   inited = true;

   dwmlib = dylib_load("dwmapi.dll");
   if (!dwmlib)
   {
      RARCH_LOG("Did not find dwmapi.dll.\n");
      return;
   }
   atexit(gfx_dwm_shutdown);

   HRESULT (WINAPI *mmcss)(BOOL) = (HRESULT (WINAPI*)(BOOL))dylib_proc(dwmlib, "DwmEnableMMCSS");
   if (mmcss)
   {
      RARCH_LOG("Setting multimedia scheduling for DWM.\n");
      mmcss(TRUE);
   }

   if (!g_settings.video.disable_composition)
      return;

   HRESULT (WINAPI *composition_enable)(UINT) = (HRESULT (WINAPI*)(UINT))dylib_proc(dwmlib, "DwmEnableComposition");
   if (!composition_enable)
   {
      RARCH_ERR("Did not find DwmEnableComposition ...\n");
      return;
   }

   HRESULT ret = composition_enable(0);
   if (FAILED(ret))
      RARCH_ERR("Failed to set composition state ...\n");
}
#endif

void gfx_scale_integer(struct rarch_viewport *vp, unsigned width, unsigned height, float aspect_ratio, bool keep_aspect)
{
   // Use system reported sizes as these define the geometry for the "normal" case.
   unsigned base_height = g_extern.system.av_info.geometry.base_height;
   // Account for non-square pixels.
   // This is sort of contradictory with the goal of integer scale,
   // but it is desirable in some cases.
   // If square pixels are used, base_height will be equal to g_extern.system.av_info.base_height.
   unsigned base_width = (unsigned)roundf(base_height * aspect_ratio);

   unsigned padding_x = 0;
   unsigned padding_y = 0;

   // Make sure that we don't get 0x scale ...
   if (width >= base_width && height >= base_height)
   {
      if (keep_aspect) // X/Y scale must be same.
      {
         unsigned max_scale = min(width / base_width, height / base_height);
         padding_x = width - base_width * max_scale;
         padding_y = height - base_height * max_scale;
      }
      else // X/Y can be independent, each scaled as much as possible.
      {
         padding_x = width % base_width;
         padding_y = height % base_height;
      }
   }

   width     -= padding_x;
   height    -= padding_y;

   vp->width  = width;
   vp->height = height;
   vp->x      = padding_x >> 1;
   vp->y      = padding_y >> 1;
}

