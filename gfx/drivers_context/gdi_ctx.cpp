/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2016 - Brad Parker
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

/* Win32/GDI context. */

/* necessary for mingw32 multimon defines: */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <string.h>
#include <math.h>

#include <windows.h>
#include <commdlg.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../video_context_driver.h"

#include "../common/win32_common.h"

static HDC   win32_hdc;

static unsigned         win32_major       = 0;
static unsigned         win32_minor       = 0;
static unsigned         win32_interval    = 0;
static enum gfx_ctx_api win32_api         = GFX_CTX_NONE;

void *dinput_gdi;

/*static void setup_pixel_format(HDC hdc)
{
   PIXELFORMATDESCRIPTOR pfd = {0};
   pfd.nSize        = sizeof(PIXELFORMATDESCRIPTOR);
   pfd.nVersion     = 1;
   pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
   pfd.iPixelType   = PFD_TYPE_RGBA;
   pfd.cColorBits   = 32;
   pfd.cDepthBits   = 0;
   pfd.cStencilBits = 0;
   pfd.iLayerType   = PFD_MAIN_PLANE;

   SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
}*/

static void gfx_ctx_gdi_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   win32_check_window(quit, resize, width, height);
}

static bool gfx_ctx_gdi_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   switch (win32_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_gdi_update_window_title(void *data)
{
   char buf[128];
   char buf_fps[128];
   settings_t      *settings = config_get_ptr();
   const ui_window_t *window = ui_companion_driver_get_window_ptr();

   buf[0] = buf_fps[0] = '\0';

   if (window && video_monitor_get_fps(buf, sizeof(buf),
            buf_fps, sizeof(buf_fps)))
      window->set_title(&main_window, buf);
   if (settings->fps_show)
      runloop_msg_queue_push(buf_fps, 1, 1, false);
}

static void gfx_ctx_gdi_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   (void)data;
   HWND         window  = win32_get_window();

   if (!window)
   {
      RECT mon_rect;
      MONITORINFOEX current_mon;
      unsigned mon_id           = 0;
      HMONITOR hm_to_use        = NULL;

      win32_monitor_info(&current_mon, &hm_to_use, &mon_id);
      mon_rect = current_mon.rcMonitor;
      *width  = mon_rect.right - mon_rect.left;
      *height = mon_rect.bottom - mon_rect.top;
   }
   else
   {
      *width  = g_resize_width;
      *height = g_resize_height;
   }
}

static void *gfx_ctx_gdi_init(void *video_driver)
{
   WNDCLASSEX wndclass = {0};

   (void)video_driver;

   if (g_inited)
      return NULL;
   
   win32_window_reset();
   win32_monitor_init();

   wndclass.lpfnWndProc   = WndProcGDI;
   if (!win32_window_init(&wndclass, true, NULL))
           return NULL;

   switch (win32_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   return (void*)"gdi";
}

static void gfx_ctx_gdi_destroy(void *data)
{
   HWND     window  = win32_get_window();

   (void)data;

   switch (win32_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (window && win32_hdc)
   {
      ReleaseDC(window, win32_hdc);
      win32_hdc = NULL;
   }

   if (window)
   {
      win32_monitor_from_window();
      win32_destroy_window();
   }

   if (g_restore_desktop)
   {
      win32_monitor_get_info();
      g_restore_desktop     = false;
   }

   g_inited                     = false;
   win32_major                  = 0;
   win32_minor                  = 0;
}

static bool gfx_ctx_gdi_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (!win32_set_video_mode(NULL, width, height, fullscreen))
   {
      RARCH_ERR("[GDI]: win32_set_video_mode failed.\n");
      goto error;
   }

   switch (win32_api)
   {
      case GFX_CTX_NONE:
      default:
         break;
   }

   return true;

error:
   gfx_ctx_gdi_destroy(data);
   return false;
}


static void gfx_ctx_gdi_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;

   dinput_gdi   = input_dinput.init();

   *input       = dinput_gdi ? &input_dinput : NULL;
   *input_data  = dinput_gdi;
}

static bool gfx_ctx_gdi_has_focus(void *data)
{
   return win32_has_focus();
}

static bool gfx_ctx_gdi_suppress_screensaver(void *data, bool enable)
{
   return win32_suppress_screensaver(data, enable);
}

static bool gfx_ctx_gdi_has_windowed(void *data)
{
   (void)data;

   return true;
}

static bool gfx_ctx_gdi_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   return win32_get_metrics(data, type, value);
}

static bool gfx_ctx_gdi_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   win32_major = major;
   win32_minor = minor;
   win32_api   = api;

   return true;
}

static void gfx_ctx_gdi_show_mouse(void *data, bool state)
{
   (void)data;
   win32_show_cursor(state);
}

static void gfx_ctx_gdi_swap_interval(void *data, unsigned interval)
{
   (void)data;
   (void)interval;
}

static void gfx_ctx_gdi_set_flags(void *data, uint32_t flags)
{
   (void)data;
   (void)flags;
}

static uint32_t gfx_ctx_gdi_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);
   return flags;
}

static void gfx_ctx_gdi_swap_buffers(void *data)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_gdi = {
   gfx_ctx_gdi_init,
   gfx_ctx_gdi_destroy,
   gfx_ctx_gdi_bind_api,
   gfx_ctx_gdi_swap_interval,
   gfx_ctx_gdi_set_video_mode,
   gfx_ctx_gdi_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_gdi_get_metrics,
   NULL,
   gfx_ctx_gdi_update_window_title,
   gfx_ctx_gdi_check_window,
   gfx_ctx_gdi_set_resize,
   gfx_ctx_gdi_has_focus,
   gfx_ctx_gdi_suppress_screensaver,
   gfx_ctx_gdi_has_windowed,
   gfx_ctx_gdi_swap_buffers,
   gfx_ctx_gdi_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_gdi_show_mouse,
   "gdi",
   gfx_ctx_gdi_get_flags,
   gfx_ctx_gdi_set_flags,
   NULL,
   NULL,
   NULL
};

