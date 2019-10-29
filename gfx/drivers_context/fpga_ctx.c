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

/* FPGA context. */

#include <string.h>
#include <math.h>

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../dynamic.h"
#include "../../verbosity.h"
#include "../video_driver.h"
#include "../common/fpga_common.h"

static unsigned g_resize_width  = FB_WIDTH;
static unsigned g_resize_height = FB_HEIGHT;

static void gfx_ctx_fpga_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
}

static bool gfx_ctx_fpga_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;

   return false;
}

static void gfx_ctx_fpga_update_title(void *data, void *data2)
{
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));
}

static void gfx_ctx_fpga_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   (void)data;

    *width  = g_resize_width;
    *height = g_resize_height;
}

static void *gfx_ctx_fpga_init(video_frame_info_t *video_info, void *video_driver)
{
   (void)video_driver;

   return (void*)"fpga";
}

static void gfx_ctx_fpga_destroy(void *data)
{
   (void)data;
}

static bool gfx_ctx_fpga_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (false)
      goto error;

   return true;

error:
   gfx_ctx_fpga_destroy(data);
   return false;
}


static void gfx_ctx_fpga_input_driver(void *data,
      const char *joypad_name,
      const input_driver_t **input, void **input_data)
{
   (void)data;
   settings_t *settings = config_get_ptr();
}

static bool gfx_ctx_fpga_has_focus(void *data)
{
   return true;
}

static bool gfx_ctx_fpga_suppress_screensaver(void *data, bool enable)
{
   return true;
}

static bool gfx_ctx_fpga_has_windowed(void *data)
{
   (void)data;

   return true;
}

static bool gfx_ctx_fpga_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   return true;
}

static bool gfx_ctx_fpga_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   return true;
}

static void gfx_ctx_fpga_show_mouse(void *data, bool state)
{
   (void)data;
}

static void gfx_ctx_fpga_swap_interval(void *data, unsigned interval)
{
   (void)data;
   (void)interval;
}

static void gfx_ctx_fpga_set_flags(void *data, uint32_t flags)
{
   (void)data;
   (void)flags;
}

static uint32_t gfx_ctx_fpga_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);
   return flags;
}

static void gfx_ctx_fpga_swap_buffers(void *data, void *data2)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_fpga = {
   gfx_ctx_fpga_init,
   gfx_ctx_fpga_destroy,
   gfx_ctx_fpga_bind_api,
   gfx_ctx_fpga_swap_interval,
   gfx_ctx_fpga_set_video_mode,
   gfx_ctx_fpga_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_fpga_get_metrics,
   NULL,
   gfx_ctx_fpga_update_title,
   gfx_ctx_fpga_check_window,
   gfx_ctx_fpga_set_resize,
   gfx_ctx_fpga_has_focus,
   gfx_ctx_fpga_suppress_screensaver,
   gfx_ctx_fpga_has_windowed,
   gfx_ctx_fpga_swap_buffers,
   gfx_ctx_fpga_input_driver,
   NULL,
   NULL,
   NULL,
   gfx_ctx_fpga_show_mouse,
   "fpga",
   gfx_ctx_fpga_get_flags,
   gfx_ctx_fpga_set_flags,
   NULL,
   NULL,
   NULL
};

