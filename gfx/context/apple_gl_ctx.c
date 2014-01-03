/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2012-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../../driver.h"
#include "../gfx_common.h"
#include "../gl_common.h"
#include "../image.h"

#include "../fonts/gl_font.h"
#include <stdint.h>

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

#include "../../apple/common/rarch_wrapper.h"

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   *quit = false;

   unsigned new_width, new_height;
   apple_gfx_ctx_get_video_size(&new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
}

// The apple_* functions are implemented in apple/RetroArch/RAGameView.m
const gfx_ctx_driver_t gfx_ctx_apple = {
   apple_gfx_ctx_init,
   apple_gfx_ctx_destroy,
   apple_gfx_ctx_bind_api,
   apple_gfx_ctx_swap_interval,
   apple_gfx_ctx_set_video_mode,
   apple_gfx_ctx_get_video_size,
   NULL,
   apple_gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   apple_gfx_ctx_has_focus,
   apple_gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   apple_gfx_ctx_get_proc_address,
   NULL,
   "apple",
};
