/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - OV2
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

#ifdef _XBOX
#include <xtl.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../drivers/d3d.h"
#include "../common/win32_common.h"

#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../ui/ui_companion_driver.h"

#ifdef _MSC_VER
#ifndef _XBOX
#pragma comment( lib, "d3d9" )
#pragma comment( lib, "d3dx9" )
#ifdef HAVE_CG
#pragma comment( lib, "cgd3d9" )
#endif
#pragma comment( lib, "dxguid" )
#endif
#endif

#if defined(_XBOX1)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTATIONINTERVAL
#define PresentationInterval FullScreen_PresentationInterval
#elif defined(_XBOX360)
#define XBOX_PRESENTATIONINTERVAL D3DRS_PRESENTINTERVAL
#endif

#ifdef _XBOX
static bool widescreen_mode = false;
#endif

void *dinput;

static bool gfx_ctx_d3d_set_resize(void *data, unsigned new_width, unsigned new_height)
{
   d3d_video_t *d3d      = (d3d_video_t*)video_driver_get_ptr(false);

   if (!d3d || !d3d->dev)
      return false;

   /* No changes? */
   if (new_width == d3d->video_info.width && new_height == d3d->video_info.height)
      return false;

   RARCH_LOG("[D3D]: Resize %ux%u.\n", new_width, new_height);
   d3d->video_info.width  = new_width;
   d3d->video_info.height = new_height;
   video_driver_set_size(&new_width, &new_height);

   return true;
}

static void gfx_ctx_d3d_swap_buffers(void *data, void *data2)
{
   d3d_video_t      *d3d = (d3d_video_t*)data;
   LPDIRECT3DDEVICE d3dr = (LPDIRECT3DDEVICE)d3d->dev;

   d3d_swap(d3d, d3dr);
}

static void gfx_ctx_d3d_update_title(void *data, void *data2)
{
   video_frame_info_t *video_info = (video_frame_info_t*)data2;
#ifdef _XBOX
   const ui_window_t *window      = NULL;
#else
   const ui_window_t *window      = ui_companion_driver_get_window_ptr();
#endif

   if (video_info->fps_show)
   {
      MEMORYSTATUS stat;
      char mem[128];

      mem[0] = '\0';

      GlobalMemoryStatus(&stat);
      snprintf(mem, sizeof(mem), "|| MEM: %.2f/%.2fMB",
            stat.dwAvailPhys/(1024.0f*1024.0f), stat.dwTotalPhys/(1024.0f*1024.0f));
      strlcat(video_info->fps_text, mem, sizeof(video_info->fps_text));
   }

   if (window)
   {
      char title[128];

      title[0] = '\0';

      video_driver_get_window_title(title, sizeof(title));

      if (title[0])
         window->set_title(&main_window, title);
   }
}

static void gfx_ctx_d3d_show_mouse(void *data, bool state)
{
   (void)data;

   win32_show_cursor(state);
}

static void gfx_ctx_d3d_check_window(void *data, bool *quit,
      bool *resize, unsigned *width,
      unsigned *height, bool is_shutdown)
{
   win32_check_window(quit, resize, width, height);
}

static bool gfx_ctx_d3d_has_focus(void *data)
{
   return win32_has_focus();
}

static bool gfx_ctx_d3d_suppress_screensaver(void *data, bool enable)
{
   return win32_suppress_screensaver(data, enable);
}

static bool gfx_ctx_d3d_has_windowed(void *data)
{
   (void)data;

#ifdef _XBOX
   return false;
#else
   return true;
#endif
}

static bool gfx_ctx_d3d_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;
   (void)api;

#if defined(HAVE_D3D8)
   return api == GFX_CTX_DIRECT3D8_API;
#else
   /* As long as we don't have a D3D11 implementation, we default to this */
   return api == GFX_CTX_DIRECT3D9_API;
#endif
}

static void *gfx_ctx_d3d_init(video_frame_info_t *video_info, void *video_driver)
{
   win32_monitor_init();

   return video_driver;
}

static void gfx_ctx_d3d_destroy(void *data)
{
   (void)data;
}

static void gfx_ctx_d3d_input_driver(void *data,
      const char *name,
      const input_driver_t **input, void **input_data)
{
#ifdef _XBOX
   void *xinput         = input_xinput.init(name);
   *input               = xinput ? (const input_driver_t*)&input_xinput : NULL;
   *input_data          = xinput;
#else
   settings_t *settings = config_get_ptr();

#if _WIN32_WINNT >= 0x0501
   /* winraw only available since XP */
   if (string_is_equal_fast(settings->arrays.input_driver, "raw", 4))
   {
      *input_data = input_winraw.init(name);
      if (*input_data)
      {
         *input = &input_winraw;
         dinput = NULL;
         return;
      }
   }
#endif

   dinput               = input_dinput.init(name);
   *input               = dinput ? &input_dinput : NULL;
   *input_data          = dinput;
#endif
   (void)data;
}

static bool gfx_ctx_d3d_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   win32_show_cursor(!fullscreen);

   return true;
}

static void gfx_ctx_d3d_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
#ifdef _XBOX
   d3d_video_t *d3d = (d3d_video_t*)data;

   (void)width;
   (void)height;
#if defined(_XBOX360)
   XVIDEO_MODE video_mode;

   XGetVideoMode(&video_mode);

   *width  = video_mode.dwDisplayWidth;
   *height = video_mode.dwDisplayHeight;

   d3d->resolution_hd_enable = false;

   if(video_mode.fIsHiDef)
   {
      *width = 1280;
      *height = 720;
      d3d->resolution_hd_enable = true;
   }
   else
   {
      *width = 640;
      *height = 480;
   }

   widescreen_mode = video_mode.fIsWideScreen;
#elif defined(_XBOX1)
   DWORD video_mode = XGetVideoFlags();

   *width  = 640;
   *height = 480;

   widescreen_mode = false;

   /* Only valid in PAL mode, not valid for HDTV modes! */

   if(XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I)
   {
      /* Check for 16:9 mode (PAL REGION) */
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         //60 Hz, 720x480i
         if(video_mode & XC_VIDEO_FLAGS_PAL_60Hz)
            *height = 480;
         else //50 Hz, 720x576i
            *height = 576;
         widescreen_mode = true;
      }
   }
   else
   {
      /* Check for 16:9 mode (NTSC REGIONS) */
      if(video_mode & XC_VIDEO_FLAGS_WIDESCREEN)
      {
         *width = 720;
         *height = 480;
         widescreen_mode = true;
      }
   }

   if(XGetAVPack() == XC_AV_PACK_HDTV)
   {
      if(video_mode & XC_VIDEO_FLAGS_HDTV_480p)
      {
         *width = 640;
         *height  = 480;
         widescreen_mode = false;
         d3d->resolution_hd_enable = true;
      }
      else if(video_mode & XC_VIDEO_FLAGS_HDTV_720p)
      {
         *width = 1280;
         *height  = 720;
         widescreen_mode = true;
         d3d->resolution_hd_enable = true;
      }
      else if(video_mode & XC_VIDEO_FLAGS_HDTV_1080i)
      {
         *width = 1920;
         *height  = 1080;
         widescreen_mode = true;
         d3d->resolution_hd_enable = true;
      }
   }
#endif
#endif
}

static void gfx_ctx_d3d_swap_interval(void *data, unsigned interval)
{
   d3d_video_t      *d3d = (d3d_video_t*)data;
#ifdef _XBOX
   unsigned d3d_interval = interval ? 
      D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

   d3d_set_render_state(d3d->dev, XBOX_PRESENTATIONINTERVAL, d3d_interval);
#endif
}

static bool gfx_ctx_d3d_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   return win32_get_metrics(data, type, value);
}

static uint32_t gfx_ctx_d3d_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);

   return flags;
}

static void gfx_ctx_d3d_set_flags(void *data, uint32_t flags)
{
   (void)flags;
}

const gfx_ctx_driver_t gfx_ctx_d3d = {
   gfx_ctx_d3d_init,
   gfx_ctx_d3d_destroy,
   gfx_ctx_d3d_bind_api,
   gfx_ctx_d3d_swap_interval,
   gfx_ctx_d3d_set_video_mode,
   gfx_ctx_d3d_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_d3d_get_metrics,
   NULL,
   gfx_ctx_d3d_update_title,
   gfx_ctx_d3d_check_window,
   gfx_ctx_d3d_set_resize,
   gfx_ctx_d3d_has_focus,
   gfx_ctx_d3d_suppress_screensaver,
   gfx_ctx_d3d_has_windowed,
   gfx_ctx_d3d_swap_buffers,
   gfx_ctx_d3d_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_d3d_show_mouse,
   "d3d",
   gfx_ctx_d3d_get_flags,
   gfx_ctx_d3d_set_flags,
   NULL,
   NULL
};
