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
#include "../../config.def.h"
#include "../gfx/video_context_driver.h"
#include "menu_list.h"

bool menu_display_update_pending(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (runloop)
   {
      if (runloop->frames.video.current.menu.animation.is_active ||
            runloop->frames.video.current.menu.label.is_updated ||
            runloop->frames.video.current.menu.framebuf.dirty)
         return true;
   }
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
   float dpi = menu_dpi_override_value;
   settings_t *settings = config_get_ptr();

   if (!menu || !settings)
      return dpi;

   if (settings->menu.dpi.override_enable)
      dpi = settings->menu.dpi.override_value;
#if defined(HAVE_OPENGL) || defined(HAVE_GLES)
   else if (!gfx_ctx_get_metrics(DISPLAY_METRIC_DPI, &dpi))
      dpi = menu_dpi_override_value;
#endif

   return dpi;
}

bool menu_display_font_init_first(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (settings->video.threaded
         && !global->system.hw_render_callback.context_type)
   {
      driver_t *driver    = driver_get_ptr();
      thread_video_t *thr = (thread_video_t*)driver->video_data;

      if (!thr)
         return false;

      thr->cmd_data.font_init.method      = font_init_first;
      thr->cmd_data.font_init.font_driver = (const void**)font_driver;
      thr->cmd_data.font_init.font_handle = font_handle;
      thr->cmd_data.font_init.video_data  = video_data;
      thr->cmd_data.font_init.font_path   = font_path;
      thr->cmd_data.font_init.font_size   = font_size;
      thr->cmd_data.font_init.api         = FONT_DRIVER_RENDER_OPENGL_API;

      thr->send_cmd_func(thr,   CMD_FONT_INIT);
      thr->wait_reply_func(thr, CMD_FONT_INIT);

      return thr->cmd_data.font_init.return_value;
   }

   return font_init_first(font_driver, font_handle, video_data,
         font_path, font_size, FONT_DRIVER_RENDER_OPENGL_API);
}

bool menu_display_font_bind_block(menu_handle_t *menu,
      const struct font_renderer *font_driver, void *userdata)
{
   if (!font_driver || !font_driver->bind_block)
      return false;

   font_driver->bind_block(menu->font.buf, userdata);

   return true;
}

bool menu_display_font_flush_block(menu_handle_t *menu,
      const struct font_renderer *font_driver)
{
   if (!font_driver || !font_driver->flush)
      return false;

   font_driver->flush(menu->font.buf);

   return menu_display_font_bind_block(menu,
         font_driver, NULL);
}

void menu_display_free_main_font(menu_handle_t *menu)
{
   driver_t *driver = driver_get_ptr();
   if (menu->font.buf)
   {
      driver->font_osd_driver->free(menu->font.buf);
      menu->font.buf = NULL;
   }
}

bool menu_display_init_main_font(menu_handle_t *menu,
      const char *font_path, float font_size)
{
   driver_t *driver = driver_get_ptr();
   void     *video  = video_driver_get_ptr(NULL);
   bool      result;

   if (menu->font.buf)
      menu_display_free_main_font(menu);

   result = menu_display_font_init_first(
         (const void**)&driver->font_osd_driver, &menu->font.buf, video,
         font_path, font_size);

   if (result)
      menu->font.size = font_size;
   else
      menu->font.buf = NULL;

   return result;
}

void menu_display_set_viewport(menu_handle_t *menu)
{
   global_t *global    = global_get_ptr();

   video_driver_set_viewport(global->video_data.width,
         global->video_data.height, true, false);
}

void menu_display_unset_viewport(menu_handle_t *menu)
{
   global_t *global    = global_get_ptr();

   video_driver_set_viewport(global->video_data.width,
         global->video_data.height, false, true);
}

bool menu_display_setting_label(
      menu_entry_t *entry,
      unsigned i,
      const char *label, 
      void *userdata)
{
   menu_file_list_cbs_t *cbs = NULL;
   const char *entry_label   = NULL;
   const char *path          = NULL;
   menu_handle_t *menu       = menu_driver_get_ptr();
   file_list_t *list         = userdata ? (file_list_t*)userdata 
      : menu->menu_list->selection_buf;

   if (!entry)
      return false;

   menu_list_get_at_offset(list, i, &path, &entry_label, &entry->type);

   cbs = (menu_file_list_cbs_t*)
      menu_list_get_actiondata_at_offset(list,
            i);

   if (cbs && cbs->action_get_representation)
      cbs->action_get_representation(list,
            &entry->spacing, entry->type, i, label,
            entry->value,  sizeof(entry->value), 
            entry_label, path,
            entry->path, sizeof(entry->path));

   if (entry_label)
      strlcpy(entry->label, entry_label, sizeof(entry->label));

#if 0
   RARCH_LOG("Entry label : %s\n", entry->label ? entry->label : "N/A");
   RARCH_LOG("Entry path  : %s\n", entry->path);
   RARCH_LOG("Entry value : %s\n", entry->value);
#endif

   return true;
}
