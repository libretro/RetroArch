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

/* Vita context. */

#include "../../retroarch.h"

static void vita_swap_interval(void *data, int interval)
{
   (void)data;
   vglWaitVblankStart(interval);
}

static void vita_get_video_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;
   *width  = 960;
   *height = 544;
}

static void vita_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   unsigned new_width, new_height;

   vita_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *width = new_width;
      *height = new_height;
      *resize = true;
   }

   *quit = (bool)false;
}

static void vita_swap_buffers(void *data, void *data2)
{
   (void)data;
   vglStopRendering();
   vglStartRendering();
}

static bool vita_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;
   (void)width;
   (void)height;
   (void)fullscreen;

   return true;
}

static void vita_destroy(void *data)
{
   (void)data;
}

static void vita_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   (void)data;
   (void)input;
   (void)input_data;
}

static bool vita_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool vita_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static enum gfx_ctx_api vita_get_api(void *data)
{
   return GFX_CTX_NONE;
}

static bool vita_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)api;
   (void)major;
   (void)minor;

   return true;
}

static void vita_show_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static void vita_bind_hw_render(void *data, bool enable)
{
   (void)data;
   (void)enable;
}

static void *vita_init(video_frame_info_t *video_info, void *video_driver)
{
   (void)video_driver;

   return (void*)"vita";
}

static uint32_t vita_get_flags(void *data)
{
   uint32_t flags = 0;

   return flags;
}

static void vita_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t vita_ctx = {
   vita_init,
   vita_destroy,
   vita_get_api,
   vita_bind_api,
   vita_swap_interval,
   vita_set_video_mode,
   vita_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   vita_check_window,
   NULL, /* set_resize */
   vita_has_focus,
   vita_suppress_screensaver,
   false, /* has_windowed */
   vita_swap_buffers,
   vita_input_driver,
   NULL,
   NULL,
   NULL,
   vita_show_mouse,
   "vita",
   vita_get_flags,
   vita_set_flags,
   vita_bind_hw_render,
   NULL,
   NULL
};
