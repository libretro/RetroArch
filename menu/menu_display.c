/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "menu.h"
#include "menu_display.h"
#include "menu_animation.h"
#include "../dynamic.h"
#include "../../retroarch.h"
#include "../gfx/video_context_driver.h"

bool menu_display_update_pending(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop)
      return false;
   if (runloop->frames.video.current.menu.animation.is_active ||
         runloop->frames.video.current.menu.label.is_updated ||
         runloop->frames.video.current.menu.framebuf.dirty)
      return true;
   return false;
}

/**
 ** menu_display_fb:
 *
 * Draws menu graphics onscreen.
 **/
void menu_display_fb(void)
{
   driver_t *driver     = driver_get_ptr();
   global_t *global     = global_get_ptr();
   settings_t *settings = config_get_ptr();

   video_driver_set_texture_enable(true, false);

   if (!settings->menu.pause_libretro)
   {
      if (global->main_is_init && !global->libretro_dummy)
      {
         bool block_libretro_input = driver->block_libretro_input;
         driver->block_libretro_input = true;
         pretro_run();
         driver->block_libretro_input = block_libretro_input;
         return;
      }
   }
    
   rarch_render_cached_frame();
}

void menu_display_free(menu_handle_t *menu)
{
   if (!menu)
      return;

   menu_animation_free(menu->animation);
   menu->animation = NULL;
}

bool menu_display_init(menu_handle_t *menu)
{
   if (!menu)
      return false;

   menu->animation = (animation_t*)calloc(1, sizeof(animation_t));

   if (!menu->animation)
      return false;

   return true;
}

float menu_display_get_dpi(menu_handle_t *menu)
{
   float dpi, dpi_orig = 128;

   if (!menu)
      return dpi_orig;

   if (!gfx_ctx_get_metrics(DISPLAY_METRIC_DPI, &dpi))
      dpi = dpi_orig;

   return dpi;
}
