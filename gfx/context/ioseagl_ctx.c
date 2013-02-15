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

#include "../../driver.h"
#include "../gfx_common.h"
#include "../gl_common.h"
#include "../image.h"

#include "../fonts/gl_font.h"
#include <stdint.h>

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

// C interface
static void gfx_ctx_set_swap_interval(unsigned interval)
{
   extern void ios_set_game_view_sync(bool on);
   ios_set_game_view_sync(interval ? true : false);
}

static void gfx_ctx_destroy(void)
{
   extern void ios_destroy_game_view();
   ios_destroy_game_view();
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   extern void ios_get_game_view_size(unsigned *, unsigned *);
   ios_get_game_view_size(width, height);
}

static bool gfx_ctx_init(void)
{
   extern bool ios_init_game_view();
   return ios_init_game_view();
}

static void gfx_ctx_swap_buffers(void)
{
   extern void ios_flip_game_view();
   ios_flip_game_view();
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   *quit = false;

   unsigned new_width, new_height;
   gfx_ctx_get_video_size(&new_width, &new_height);
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

static void gfx_ctx_update_window_title(bool reset)
{
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   *input = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api)
{
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_has_focus(void)
{
   return true;
}

static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}

const gfx_ctx_driver_t gfx_ctx_ios = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_set_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   NULL,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   NULL,
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
   NULL,
   "ios",
};
