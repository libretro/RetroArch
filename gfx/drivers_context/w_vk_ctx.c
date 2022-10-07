/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <math.h>

/* Win32/Vulkan context. */

/* necessary for mingw32 multimon defines: */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <tchar.h>
#include <wchar.h>

#include <string.h>
#include <math.h>

#include <windows.h>
#include <commdlg.h>

#include <dynamic/dylib.h>
#include <string/stdstring.h>
#include <retro_timers.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"

#include "../common/win32_common.h"

#include "../common/vulkan_common.h"

typedef struct gfx_ctx_w_vk_data
{
   void *empty;
} gfx_ctx_w_vk_data_t;

/* TODO/FIXME - static globals */
static gfx_ctx_vulkan_data_t win32_vk;
static void             *dinput_vk        = NULL;
static int              win32_vk_interval = 0;

void create_vk_context(HWND hwnd, bool *quit)
{
   RECT rect;
   HINSTANCE instance;
   unsigned width  = 0;
   unsigned height = 0;

   GetClientRect(hwnd, &rect);

   instance = GetModuleHandle(NULL);
   width    = rect.right - rect.left;
   height   = rect.bottom - rect.top;

   if (!vulkan_surface_create(&win32_vk, VULKAN_WSI_WIN32,
            &instance, &hwnd,
            width, height, win32_vk_interval))
      *quit = true;

   g_win32_inited = true;
}

static void gfx_ctx_w_vk_swap_interval(void *data, int interval)
{
   if (win32_vk_interval != interval)
   {
      win32_vk_interval = interval;
      if (win32_vk.swapchain)
         win32_vk.need_new_swapchain = true;
   }
}

static void gfx_ctx_w_vk_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   settings_t *settings     = config_get_ptr();
   float refresh_rate       = settings->floats.video_refresh_rate;

   win32_check_window(NULL, quit, resize, width, height);

   if (win32_vk.need_new_swapchain)
      *resize = true;

   /* Trigger video driver init when changing refresh rate
    * in fullscreen while dimensions stay the same.
    * Otherwise display refresh rate will not stay at the designated rate.
    * All other change combinations work however:
    *  - windowed works always
    *  - fullscreen works when dimensions and rate changes
    * Bigger than zero difference required in order to prevent
    * constant reinit when adjusting rate option in 0.001 increments.
    */
   if (win32_vk.fullscreen && g_win32_refresh_rate &&
         g_win32_refresh_rate  != refresh_rate &&
         abs(g_win32_refresh_rate - refresh_rate) > 0 &&
         g_win32_resize_width  == *width &&
         g_win32_resize_height == *height)
   {
      g_win32_refresh_rate = settings->floats.video_refresh_rate;
      command_event(CMD_EVENT_REINIT, NULL);
   }
}

static void gfx_ctx_w_vk_swap_buffers(void *data)
{
   if (win32_vk.context.has_acquired_swapchain)
   {
      win32_vk.context.has_acquired_swapchain = false;
      /* We're still waiting for a proper swapchain, so just fake it. */
      if (win32_vk.swapchain == VK_NULL_HANDLE)
         retro_sleep(10);
      else
         vulkan_present(&win32_vk, win32_vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&win32_vk);
}

static bool gfx_ctx_w_vk_set_resize(void *data,
      unsigned width, unsigned height)
{
   if (vulkan_create_swapchain(&win32_vk, width, height, win32_vk_interval))
   {
      if (win32_vk.created_new_swapchain)
         vulkan_acquire_next_image(&win32_vk);
      win32_vk.context.invalid_swapchain = true;
      win32_vk.need_new_swapchain        = false;

      return true;
   }

   RARCH_ERR("[Vulkan]: Failed to update swapchain.\n");
   return false;
}

static void gfx_ctx_w_vk_update_title(void *data)
{
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (title[0])
   {
      const ui_window_t *window = ui_companion_driver_get_window_ptr();

      if (window)
         window->set_title(&main_window, title);
   }
}

static void gfx_ctx_w_vk_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
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
      *width  = g_win32_resize_width;
      *height = g_win32_resize_height;
   }
}

static void gfx_ctx_w_vk_destroy(void *data)
{
   HWND            window  = win32_get_window();
   gfx_ctx_w_vk_data_t *vk = (gfx_ctx_w_vk_data_t*)data;

   vulkan_context_destroy(&win32_vk, win32_vk.vk_surface != VK_NULL_HANDLE);
   if (win32_vk.context.queue_lock)
      slock_free(win32_vk.context.queue_lock);
   memset(&win32_vk, 0, sizeof(win32_vk));

   if (window)
   {
      win32_monitor_from_window();
      win32_destroy_window();
   }

   if (g_win32_restore_desktop)
   {
      win32_monitor_get_info();
      g_win32_restore_desktop     = false;
   }

   if (vk)
      free(vk);

   g_win32_inited               = false;
}

static void *gfx_ctx_w_vk_init(void *video_driver)
{
   WNDCLASSEX wndclass     = {0};
   gfx_ctx_w_vk_data_t *vk = (gfx_ctx_w_vk_data_t*)calloc(1, sizeof(*vk));

   if (!vk)
      return NULL;

   if (g_win32_inited)
      gfx_ctx_w_vk_destroy(NULL);

   win32_window_reset();
   win32_monitor_init();

   {
      settings_t *settings     = config_get_ptr();
      wndclass.lpfnWndProc     = wnd_proc_vk_common;
#ifdef HAVE_DINPUT
      if (string_is_equal(settings->arrays.input_driver, "dinput"))
         wndclass.lpfnWndProc   = wnd_proc_vk_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
      if (string_is_equal(settings->arrays.input_driver, "raw"))
         wndclass.lpfnWndProc   = wnd_proc_vk_winraw;
#endif
   }
   if (!win32_window_init(&wndclass, true, NULL))
      goto error;

   if (!vulkan_context_init(&win32_vk, VULKAN_WSI_WIN32))
      goto error;

   return vk;

error:
   if (vk)
      free(vk);
   return NULL;
}

static bool gfx_ctx_w_vk_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   win32_vk.fullscreen = fullscreen;

   if (win32_set_video_mode(NULL, width, height, fullscreen))
   {
      /* Create a new swapchain in order to prevent fullscreen
       * emulated mailbox crash caused by refresh rate change */
      vulkan_create_swapchain(&win32_vk, width, height, win32_vk_interval);

      gfx_ctx_w_vk_swap_interval(data, win32_vk_interval);
      return true;
   }

   RARCH_ERR("[Vulkan]: win32_set_video_mode failed.\n");
   gfx_ctx_w_vk_destroy(data);
   return false;
}

static void gfx_ctx_w_vk_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   settings_t *settings     = config_get_ptr();

#if _WIN32_WINNT >= 0x0501
#ifdef HAVE_WINRAWINPUT
   const char *input_driver = settings->arrays.input_driver;

   /* winraw only available since XP */
   if (string_is_equal(input_driver, "raw"))
   {
      *input_data = input_driver_init_wrap(&input_winraw, joypad_name);
      if (*input_data)
      {
         *input        = &input_winraw;
         dinput_vk     = NULL;
         return;
      }
   }
#endif
#endif

#ifdef HAVE_DINPUT
   dinput_vk      = input_driver_init_wrap(&input_dinput, joypad_name);
   *input         = dinput_vk ? &input_dinput : NULL;
   *input_data    = dinput_vk;
#endif
}

static enum gfx_ctx_api gfx_ctx_w_vk_get_api(void *data) { return GFX_CTX_VULKAN_API; }

static bool gfx_ctx_w_vk_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   if (api == GFX_CTX_VULKAN_API)
      return true;
   return false;
}

static void gfx_ctx_w_vk_bind_hw_render(void *data, bool enable) { }

static void *gfx_ctx_w_vk_get_context_data(void *data) { return &win32_vk.context; }

static uint32_t gfx_ctx_w_vk_get_flags(void *data)
{
   uint32_t flags = 0;
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   return flags;
}

static void gfx_ctx_w_vk_set_flags(void *data, uint32_t flags) { }

static void gfx_ctx_w_vk_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   win32_get_video_output_size(width, height, desc, desc_len);
}

static void gfx_ctx_w_vk_get_video_output_prev(void *data) { }

static void gfx_ctx_w_vk_get_video_output_next(void *data) { }

const gfx_ctx_driver_t gfx_ctx_w_vk = {
   gfx_ctx_w_vk_init,
   gfx_ctx_w_vk_destroy,
   gfx_ctx_w_vk_get_api,
   gfx_ctx_w_vk_bind_api,
   gfx_ctx_w_vk_swap_interval,
   gfx_ctx_w_vk_set_video_mode,
   gfx_ctx_w_vk_get_video_size,
   win32_get_refresh_rate,
   gfx_ctx_w_vk_get_video_output_size,
   gfx_ctx_w_vk_get_video_output_prev,
   gfx_ctx_w_vk_get_video_output_next,
   win32_get_metrics,
   NULL,
   gfx_ctx_w_vk_update_title,
   gfx_ctx_w_vk_check_window,
   gfx_ctx_w_vk_set_resize,
   win32_has_focus,
   win32_suppress_screensaver,
   true,                            /* has_windowed */
   gfx_ctx_w_vk_swap_buffers,
   gfx_ctx_w_vk_input_driver,
   NULL,
   NULL,
   NULL,
   win32_show_cursor,
   "vk_w",
   gfx_ctx_w_vk_get_flags,
   gfx_ctx_w_vk_set_flags,
   gfx_ctx_w_vk_bind_hw_render,
   gfx_ctx_w_vk_get_context_data,
   NULL                             /* make_current */
};
