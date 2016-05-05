/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

/* Null context. */

#include "../../driver.h"
#include "../video_context_driver.h"

static void gfx_ctx_null_swap_interval(void *data, unsigned interval)
{
   (void)data;
   (void)interval;
}

static void gfx_ctx_null_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;
   (void)data;
   (void)quit;
   (void)width;
   (void)height;
   (void)resize;
}

static void gfx_ctx_null_swap_buffers(void *data)
{
   (void)data;
}

static bool gfx_ctx_null_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
   return false;
}

static void gfx_ctx_null_update_window_title(void *data)
{
   (void)data;
}

static void gfx_ctx_null_get_video_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;
   *width  = 320;
   *height = 240;
}

static bool gfx_ctx_null_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)data;
   (void)width;
   (void)height;
   (void)fullscreen;

   return true;
}

static void gfx_ctx_null_destroy(void *data)
{
   (void)data;
}

static void gfx_ctx_null_input_driver(void *data, const input_driver_t **input, void **input_data)
{
   (void)data;
   (void)input;
   (void)input_data;
}

static bool gfx_ctx_null_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_null_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool gfx_ctx_null_has_windowed(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_null_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)api;
   (void)major;
   (void)minor;

   return true;
}

static void gfx_ctx_null_show_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static void gfx_ctx_null_bind_hw_render(void *data, bool enable)
{
   (void)data;
   (void)enable;
}

static void *gfx_ctx_null_init(void *video_driver)
{
   (void)video_driver;

   return (void*)"null";
}

static uint32_t gfx_ctx_null_get_flags(void *data)
{
   (void)data;
   return 1UL << GFX_CTX_FLAGS_NONE;
}

const gfx_ctx_driver_t gfx_ctx_null = {
   gfx_ctx_null_init,
   gfx_ctx_null_destroy,
   gfx_ctx_null_bind_api,
   gfx_ctx_null_swap_interval,
   gfx_ctx_null_set_video_mode,
   gfx_ctx_null_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   gfx_ctx_null_update_window_title,
   gfx_ctx_null_check_window,
   gfx_ctx_null_set_resize,
   gfx_ctx_null_has_focus,
   gfx_ctx_null_suppress_screensaver,
   gfx_ctx_null_has_windowed,
   gfx_ctx_null_swap_buffers,
   gfx_ctx_null_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_null_show_mouse,
   "null",
   gfx_ctx_null_get_flags,
   gfx_ctx_null_bind_hw_render
};

