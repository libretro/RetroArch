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
#include "menu_entries.h"
#include "menu_shader.h"
#include "../dynamic.h"
#include "../frontend/frontend.h"
#include "../../retroarch.h"
#include <file/file_path.h>

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
void menu_update_libretro_info(struct retro_system_info *info)
{
#ifndef HAVE_DYNAMIC
   retro_get_system_info(info);
#endif

   core_info_list_free(g_extern.core_info);
   g_extern.core_info = NULL;
   if (*g_settings.libretro_directory)
      g_extern.core_info = core_info_list_new(g_settings.libretro_directory);

   rarch_update_system_info(info, NULL);
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
      msg_queue_push(g_extern.msg_queue, str, 1, 1);
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
      msg_queue_push(g_extern.msg_queue, msg, 1, 90);

      if (driver.menu)
         driver.menu->msg_force = true;

      return false;
   }

   if (driver.menu && driver.menu_ctx
         && driver.menu_ctx->update_core_info)
      driver.menu_ctx->update_core_info(driver.menu);

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

   if (!(menu = (menu_handle_t*)menu_ctx->init()))
      return NULL;

   strlcpy(g_settings.menu.driver, menu_ctx->ident,
         sizeof(g_settings.menu.driver));

   if (!(menu->menu_list = (menu_list_t*)menu_list_new()))
      return NULL;

   g_extern.core_info_current = (core_info_t*)calloc(1, sizeof(core_info_t));
#ifdef HAVE_SHADER_MANAGER
   menu->shader = (struct video_shader*)calloc(1, sizeof(struct video_shader));
#endif
   menu->push_start_screen = g_settings.menu_show_start_screen;
   g_settings.menu_show_start_screen = false;

   menu_update_libretro_info(&g_extern.menu.info);

   menu_shader_manager_init(menu);

   return menu;
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
 * menu_init_list:
 * @data                     : Menu handle.
 *
 * Initialize menu lists.
 * Will be performed after menu_init().
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_init_list(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return false;

   if (!menu_entries_init(menu))
      return false;

   if (driver.menu_ctx && driver.menu_ctx->init_lists)
      if (!(driver.menu_ctx->init_lists(menu)))
            return false;

   return true;
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

   menu_database_free();

   if (driver.menu_ctx && driver.menu_ctx->free)
      driver.menu_ctx->free(menu);

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&g_extern.menu.info);
#endif

   menu_list_free(menu->menu_list);
   menu->menu_list = NULL;

   rarch_main_command(RARCH_CMD_HISTORY_DEINIT);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);

   if (g_extern.core_info_current)
      free(g_extern.core_info_current);

   free(data);
}

/**
 * menu_ticker_line:
 * @buf                      : buffer to write new message line to.
 * @len                      : length of buffer @input.
 * @idx                      : Index. Will be used for ticker logic.
 * @str                      : Input string.
 * @selected                 : Is the item currently selected in the menu?
 *
 * Take the contents of @str and apply a ticker effect to it,
 * and write the results in @buf.
 **/
void menu_ticker_line(char *buf, size_t len, unsigned idx,
      const char *str, bool selected)
{
   unsigned ticker_period, phase, phase_left_stop;
   unsigned phase_left_moving, phase_right_stop;
   unsigned left_offset, right_offset;
   size_t str_len = strlen(str);

   if (str_len <= len)
   {
      strlcpy(buf, str, len + 1);
      return;
   }

   if (!selected)
   {
      strlcpy(buf, str, len + 1 - 3);
      strlcat(buf, "...", len + 1);
      return;
   }

   /* Wrap long strings in options with some kind of ticker line. */
   ticker_period = 2 * (str_len - len) + 4;
   phase = idx % ticker_period;

   phase_left_stop = 2;
   phase_left_moving = phase_left_stop + (str_len - len);
   phase_right_stop = phase_left_moving + 2;

   left_offset = phase - phase_left_stop;
   right_offset = (str_len - len) - (phase - phase_right_stop);

   /* Ticker period:
    * [Wait at left (2 ticks),
    * Progress to right(type_len - w),
    * Wait at right (2 ticks),
    * Progress to left].
    */
   if (phase < phase_left_stop)
      strlcpy(buf, str, len + 1);
   else if (phase < phase_left_moving)
      strlcpy(buf, str + left_offset, len + 1);
   else if (phase < phase_right_stop)
      strlcpy(buf, str + str_len - len, len + 1);
   else
      strlcpy(buf, str + right_offset, len + 1);
}

void menu_apply_deferred_settings(void)
{
   rarch_setting_t *setting = NULL;
    
   if (!driver.menu)
      return;
    
   setting = (rarch_setting_t*)driver.menu->list_settings;
    
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
   int32_t ret     = 0;
   unsigned action = menu_input_frame(input, trigger_input);

   if (driver.menu_ctx && driver.menu_ctx->entry_iterate) 
      ret = driver.menu_ctx->entry_iterate(action);

   if (g_extern.is_menu)
      draw_frame();

   if (driver.menu_ctx && driver.menu_ctx->input_postprocess)
      driver.menu_ctx->input_postprocess(input, old_input);

   if (ret)
      return -1;

   return 0;
}
