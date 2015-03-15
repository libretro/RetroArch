/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include "menu_animation.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "../dynamic.h"
#include "../frontend/frontend.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../performance.h"
#include <file/file_path.h>

bool menu_display_update_pending(void)
{
   if (g_runloop.frames.video.current.menu.animation.is_active ||
         g_runloop.frames.video.current.menu.label.is_updated ||
         g_runloop.frames.video.current.menu.framebuf.dirty)
      return true;
   return false;
}

/**
 ** draw_frame:
 *
 * Draws menu graphics onscreen.
 **/
static void draw_frame(void)
{
   if (driver.video_data && driver.video_poke &&
         driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data,
            true, false);

   if (!g_settings.menu.pause_libretro)
   {
      if (g_extern.main_is_init && !g_extern.libretro_dummy)
      {
         bool block_libretro_input = driver.block_libretro_input;
         driver.block_libretro_input = true;
         pretro_run();
         driver.block_libretro_input = block_libretro_input;
         return;
      }
   }
    
   rarch_render_cached_frame();
}

/**
 * menu_update_libretro_info:
 * @info                     : Pointer to system info
 *
 * Update menu state which depends on config.
 **/
static void menu_update_libretro_info(struct retro_system_info *info)
{
#ifndef HAVE_DYNAMIC
   retro_get_system_info(info);
#endif

   rarch_main_command(RARCH_CMD_CORE_INFO_INIT);
   if (driver.menu_ctx && driver.menu_ctx->context_reset)
      driver.menu_ctx->context_reset();

   rarch_main_command(RARCH_CMD_LOAD_CORE_PERSIST);
}

static void menu_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
    
   if (!wrap_args)
      return;

   wrap_args->no_content    = driver.menu->load_no_content;
   if (!g_extern.has_set_verbosity)
      wrap_args->verbose       = g_extern.verbosity;
   wrap_args->config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
   wrap_args->sram_path     = *g_extern.savefile_dir ? g_extern.savefile_dir : NULL;
   wrap_args->state_path    = *g_extern.savestate_dir ? g_extern.savestate_dir : NULL;
   wrap_args->content_path  = *g_extern.fullpath ? g_extern.fullpath : NULL;
   if (!g_extern.has_set_libretro)
      wrap_args->libretro_path = *g_settings.libretro ? g_settings.libretro : NULL;
   wrap_args->touched       = true;
}

static void push_to_history_playlist(void)
{
   if (!g_settings.history_list_enable)
      return;

   if (*g_extern.fullpath)
   {
      char tmp[PATH_MAX_LENGTH];
      char str[PATH_MAX_LENGTH];

      fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
      snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
      rarch_main_msg_queue_push(str, 1, 1, false);
   }

   content_playlist_push(g_defaults.history,
         g_extern.fullpath,
         g_settings.libretro,
         g_extern.menu.info.library_name);
}

/**
 * menu_load_content:
 *
 * Loads content into currently selected core.
 * Will also optionally push the content entry to the history playlist.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_load_content(void)
{
   if (*g_extern.fullpath || (driver.menu && driver.menu->load_no_content))
      push_to_history_playlist();

   /* redraw menu frame */
   if (driver.menu)
      driver.menu->msg_force = true;

   if (driver.menu_ctx && driver.menu_ctx->entry_iterate) 
      driver.menu_ctx->entry_iterate(MENU_ACTION_NOOP);

   draw_frame();

   if (!(main_load_content(0, NULL, NULL, menu_environment_get,
         driver.frontend_ctx->process_args)))
   {
      char name[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];

      fill_pathname_base(name, g_extern.fullpath, sizeof(name));
      snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
      msg_queue_push(g_runloop.msg_queue, msg, 1, 90);

      if (driver.menu)
         driver.menu->msg_force = true;

      return false;
   }

   menu_update_libretro_info(&g_extern.menu.info);

   menu_shader_manager_init(driver.menu);

   rarch_main_command(RARCH_CMD_HISTORY_INIT);
   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   rarch_main_command(RARCH_CMD_RESUME);

   return true;
}

/**
 * menu_init:
 * @data                     : Menu context handle.
 *
 * Create and initialize menu handle.
 *
 * Returns: menu handle on success, otherwise NULL.
 **/
void *menu_init(const void *data)
{
   menu_handle_t *menu = NULL;
   menu_ctx_driver_t *menu_ctx = (menu_ctx_driver_t*)data;

   if (!menu_ctx)
      return NULL;

   menu_update_libretro_info(&g_extern.menu.info);

   if (!(menu = (menu_handle_t*)menu_ctx->init()))
      return NULL;

   strlcpy(g_settings.menu.driver, menu_ctx->ident,
         sizeof(g_settings.menu.driver));

   if (!(menu->menu_list = (menu_list_t*)menu_list_new()))
      goto error;

   g_extern.core_info_current = (core_info_t*)calloc(1, sizeof(core_info_t));
   if (!g_extern.core_info_current)
      goto error;

#ifdef HAVE_SHADER_MANAGER
   menu->shader = (struct video_shader*)calloc(1, sizeof(struct video_shader));
   if (!menu->shader)
      goto error;
#endif
   menu->push_start_screen = g_settings.menu_show_start_screen;
   g_settings.menu_show_start_screen = false;

   menu_shader_manager_init(menu);

   menu->animation = (animation_t*)calloc(1, sizeof(animation_t));

   if (!menu->animation)
      goto error;

   rarch_assert(menu->msg_queue = msg_queue_new(8));

   g_runloop.frames.video.current.menu.framebuf.dirty = true;

   return menu;
error:
   if (menu->menu_list)
      menu_list_free(menu->menu_list);
   menu->menu_list = NULL;
   if (g_extern.core_info_current)
      free(g_extern.core_info_current);
   g_extern.core_info_current = NULL;
   if (menu->shader)
      free(menu->shader);
   menu->shader = NULL;
   if (menu)
      free(menu);
   return NULL;
}

/**
 * menu_free_list:
 * @data                     : Menu handle.
 *
 * Frees menu lists.
 **/
void menu_free_list(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   if (!menu)
      return;

   settings_list_free(menu->list_settings);
   menu->list_settings = NULL;
}

/**
 * menu_free:
 * @info                     : Menu handle.
 *
 * Frees a menu handle
 **/
void menu_free(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;
  
#ifdef HAVE_SHADER_MANAGER
   if (menu->shader)
      free(menu->shader);
   menu->shader = NULL;
#endif

   if (driver.menu_ctx && driver.menu_ctx->free)
      driver.menu_ctx->free(menu);

#ifdef HAVE_LIBRETRODB
   menu_database_free(menu);
#endif

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&g_extern.menu.info);
#endif

   if (menu->msg_queue)
      msg_queue_free(menu->msg_queue);
   menu->msg_queue = NULL;

   menu_animation_free(menu->animation);
   menu->animation = NULL;

   if (menu->frame_buf.data)
      free(menu->frame_buf.data);
   menu->frame_buf.data = NULL;

   menu_list_free(menu->menu_list);
   menu->menu_list = NULL;

   rarch_main_command(RARCH_CMD_HISTORY_DEINIT);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);

   if (g_extern.core_info_current)
      free(g_extern.core_info_current);

   free(data);
}

void menu_apply_deferred_settings(void)
{
   rarch_setting_t *setting = NULL;
   menu_handle_t   *menu = menu_driver_resolve();
    
   if (!menu)
      return;
    
   setting = (rarch_setting_t*)menu->list_settings;
    
   if (!setting)
      return;

   for (; setting->type != ST_NONE; setting++)
   {
      if (setting->type >= ST_GROUP)
         continue;

      if (!(setting->flags & SD_FLAG_IS_DEFERRED))
         continue;

      switch (setting->type)
      {
         case ST_BOOL:
            if (*setting->value.boolean != setting->original_value.boolean)
            {
               setting->original_value.boolean = *setting->value.boolean;
               setting->deferred_handler(setting);
            }
            break;
         case ST_INT:
            if (*setting->value.integer != setting->original_value.integer)
            {
               setting->original_value.integer = *setting->value.integer;
               setting->deferred_handler(setting);
            }
            break;
         case ST_UINT:
            if (*setting->value.unsigned_integer != setting->original_value.unsigned_integer)
            {
               setting->original_value.unsigned_integer = *setting->value.unsigned_integer;
               setting->deferred_handler(setting);
            }
            break;
         case ST_FLOAT:
            if (*setting->value.fraction != setting->original_value.fraction)
            {
               setting->original_value.fraction = *setting->value.fraction;
               setting->deferred_handler(setting);
            }
            break;
         case ST_PATH:
         case ST_DIR:
         case ST_STRING:
         case ST_BIND:
            /* Always run the deferred write handler */
            setting->deferred_handler(setting);
            break;
         default:
            break;
      }
   }
}

/**
 * menu_iterate:
 * @input                    : input sample for this frame
 * @old_input                : input sample of the previous frame
 * @trigger_input            : difference' input sample - difference
 *                             between 'input' and 'old_input'
 *
 * Runs RetroArch menu for one frame.
 *
 * Returns: 0 on success, -1 if we need to quit out of the loop. 
 **/
int menu_iterate(retro_input_t input,
      retro_input_t old_input, retro_input_t trigger_input)
{
   static retro_time_t last_clock_update = 0;
   int32_t ret     = 0;
   unsigned action = menu_input_frame(input, trigger_input);

   menu_handle_t *menu = menu_driver_resolve();

   menu->cur_time = rarch_get_time_usec();
   menu->dt = menu->cur_time - menu->old_time;
   if (menu->dt >= IDEAL_DT * 4)
      menu->dt = IDEAL_DT * 4;
   if (menu->dt <= IDEAL_DT / 4)
      menu->dt = IDEAL_DT / 4;
   menu->old_time = menu->cur_time;

   if (menu->cur_time - last_clock_update > 1000000 && g_settings.menu.timedate_enable)
   {
      g_runloop.frames.video.current.menu.label.is_updated = true;
      last_clock_update = menu->cur_time;
   }

   if (driver.menu_ctx && driver.menu_ctx->entry_iterate)
      ret = driver.menu_ctx->entry_iterate(action);

   if (g_runloop.is_menu && !g_runloop.is_idle)
      draw_frame();

   if (driver.menu_ctx && driver.menu_ctx->set_texture)
      driver.menu_ctx->set_texture();

   if (ret)
      return -1;

   return 0;
}
