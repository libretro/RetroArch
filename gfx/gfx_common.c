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

#include "gfx_common.h"
#include "../general.h"
#include "../performance.h"

static float time_to_fps(rarch_time_t last_time, rarch_time_t new_time, int frames)
{
   return (1000000.0f * frames) / (new_time - last_time);
}

#define FPS_UPDATE_INTERVAL 256
bool gfx_get_fps(char *buf, size_t size, bool always_write)
{
   static rarch_time_t time;
   static rarch_time_t fps_time;
   static float last_fps;
   bool ret = false;
   *buf = '\0';

   rarch_time_t new_time = rarch_get_time_usec();
   if (g_extern.frame_count)
   {
      unsigned write_index = g_extern.measure_data.frame_time_samples_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
      g_extern.measure_data.frame_time_samples[write_index] = new_time - fps_time;
      fps_time = new_time;

      if ((g_extern.frame_count % FPS_UPDATE_INTERVAL) == 0)
      {
         last_fps = time_to_fps(time, new_time, FPS_UPDATE_INTERVAL);
         time = new_time;

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
   }
   else
   {
      time = fps_time = new_time;
      snprintf(buf, size, "%s", g_extern.title_buf);
      ret = true;
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
   {
      dylib_close(dwmlib);
      dwmlib = NULL;
   }
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

#if defined(HAVE_RMENU) || defined(HAVE_RGUI)

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "1:1",           1.0f },
   { "2:1",           2.0f },
   { "3:2",           1.5f },
   { "3:4",           0.75f },
   { "4:1",           4.0f },
   { "4:3",           1.3333f },
   { "4:4",           1.0f },
   { "5:4",           1.25f },
   { "6:5",           1.2f },
   { "7:9",           0.7777f },
   { "8:3",           2.6666f },
   { "8:7",           1.1428f },
   { "16:9",          1.7778f },
   { "16:10",         1.6f },
   { "16:15",         3.2f },
   { "19:12",         1.5833f },
   { "19:14",         1.3571f },
   { "30:17",         1.7647f },
   { "32:9",          3.5555f },
   { "Auto",          1.0f },
   { "Core Provided", 1.0f },
   { "Custom",        0.0f }
};

char rotation_lut[ASPECT_RATIO_END][32] =
{
   "Normal",
   "Vertical",
   "Flipped",
   "Flipped Rotated"
};

void gfx_set_auto_viewport(unsigned width, unsigned height)
{
   if(width == 0 || height == 0)
      return;

   unsigned aspect_x, aspect_y, len, highest, i;

   len = width < height ? width : height;
   highest = 1;
   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   aspect_x = width / highest;
   aspect_y = height / highest;

   snprintf(aspectratio_lut[ASPECT_RATIO_AUTO].name, sizeof(aspectratio_lut[ASPECT_RATIO_AUTO].name), "%d:%d (Auto)", aspect_x, aspect_y);
   aspectratio_lut[ASPECT_RATIO_AUTO].value = (float) aspect_x / aspect_y;
}

void gfx_set_core_viewport(void)
{
   if (!g_extern.main_is_init)
      return;

   // fallback to 1:1 pixel ratio if none provided
   if (g_extern.system.av_info.geometry.aspect_ratio == 0.0)
      aspectratio_lut[ASPECT_RATIO_CORE].value = (float) g_extern.system.av_info.geometry.base_width / g_extern.system.av_info.geometry.base_height;
   else
      aspectratio_lut[ASPECT_RATIO_CORE].value = g_extern.system.av_info.geometry.aspect_ratio;
}

#endif
