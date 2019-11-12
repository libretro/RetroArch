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

/* Null context. */

#include "../../retroarch.h"

static void gfx_ctx_null_swap_interval(void *data, int interval)
{
   (void)data;
   (void)interval;
}

static void gfx_ctx_null_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   (void)data;
   (void)quit;
   (void)width;
   (void)height;
   (void)resize;
}

static void gfx_ctx_null_swap_buffers(void *data, void *data2)
{
   (void)data;
}

static void gfx_ctx_null_get_video_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;
#ifdef VITA
   *width  = 960;
   *height = 544;
#else
   *width  = 320;
   *height = 240;
#endif
}

static bool gfx_ctx_null_set_video_mode(void *data,
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

static void gfx_ctx_null_destroy(void *data)
{
   (void)data;
}

static void gfx_ctx_null_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
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

static enum gfx_ctx_api gfx_ctx_null_get_api(void *data)
{
   return GFX_CTX_NONE;
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

static void *gfx_ctx_null_init(video_frame_info_t *video_info, void *video_driver)
{
   (void)video_driver;

   return (void*)"null";
}

static uint32_t gfx_ctx_null_get_flags(void *data)
{
   uint32_t flags = 0;

   return flags;
}

static void gfx_ctx_null_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_null = {
   gfx_ctx_null_init,
   gfx_ctx_null_destroy,
   gfx_ctx_null_get_api,
   gfx_ctx_null_bind_api,
   gfx_ctx_null_swap_interval,
   gfx_ctx_null_set_video_mode,
   gfx_ctx_null_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_null_check_window,
   NULL, /* set_resize */
   gfx_ctx_null_has_focus,
   gfx_ctx_null_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_null_swap_buffers,
   gfx_ctx_null_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_null_show_mouse,
   "null",
   gfx_ctx_null_get_flags,
   gfx_ctx_null_set_flags,
   gfx_ctx_null_bind_hw_render,
   NULL,
   NULL
};
