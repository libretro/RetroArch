/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

static bool gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)major;
   (void)minor;
#ifdef IOS
   return api == GFX_CTX_OPENGL_ES_API;
#else
   return apple_create_gl_context((major << 12) | (minor << 8));
#endif
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   *quit = false;

   unsigned new_width, new_height;
   apple_get_game_view_size(&new_width, &new_height);
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

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol_name)
{
   return (gfx_ctx_proc_t)apple_get_proc_address(symbol_name);
}

// The apple_* functions are implemented in apple/RetroArch/RAGameView.m

const gfx_ctx_driver_t gfx_ctx_apple = {
   apple_init_game_view,
   apple_destroy_game_view,
   gfx_ctx_bind_api,
   apple_set_game_view_sync,
   apple_set_video_mode,
   apple_get_game_view_size,
   NULL,
   apple_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   apple_game_view_has_focus,
   apple_flip_game_view,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
   NULL,
   "ios",
};
