/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "menu_common.h"
#include "menu_input_line_cb.h"
#include "menu_entries.h"
#include "menu_list.h"
#include "menu_shader.h"
#include "../frontend.h"

static void draw_frame(void)
{
   if (driver.video_data && driver.video_poke &&
         driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data,
            true, MENU_TEXTURE_FULLSCREEN);

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

/* Update menu state which depends on config. */

static void update_libretro_info(struct retro_system_info *info)
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
      char tmp[PATH_MAX];
      char str[PATH_MAX];

      fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
      snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
      msg_queue_push(g_extern.msg_queue, str, 1, 1);
   }

   content_playlist_push(g_defaults.history,
         g_extern.fullpath,
         g_settings.libretro,
         g_extern.menu.info.library_name);
}

bool load_menu_content(void)
{
   if (*g_extern.fullpath || (driver.menu && driver.menu->load_no_content))
      push_to_history_playlist();

   /* redraw menu frame */
   if (driver.menu)
      driver.menu->msg_force = true;

   if (driver.menu_ctx && driver.menu_ctx->backend &&
         driver.menu_ctx->backend->iterate) 
      driver.menu_ctx->backend->iterate(MENU_ACTION_NOOP);

   draw_frame();

   if (!(main_load_content(0, NULL, NULL, menu_environment_get,
         driver.frontend_ctx->process_args)))
   {
      char name[PATH_MAX], msg[PATH_MAX];

      fill_pathname_base(name, g_extern.fullpath, sizeof(name));
      snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
      msg_queue_push(g_extern.msg_queue, msg, 1, 90);

      if (driver.menu)
         driver.menu->msg_force = true;

      return false;
   }

   if (driver.menu)
      update_libretro_info(&g_extern.menu.info);

   menu_shader_manager_init(driver.menu);

   rarch_main_command(RARCH_CMD_HISTORY_INIT);
   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   rarch_main_command(RARCH_CMD_RESUME);

   return true;
}

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

   menu->menu_stack = (file_list_t*)calloc(1, sizeof(file_list_t));
   menu->selection_buf = (file_list_t*)calloc(1, sizeof(file_list_t));
   g_extern.core_info_current = (core_info_t*)calloc(1, sizeof(core_info_t));
#ifdef HAVE_SHADER_MANAGER
   menu->shader = (struct gfx_shader*)calloc(1, sizeof(struct gfx_shader));
#endif
   menu->push_start_screen = g_settings.menu_show_start_screen;
   g_settings.menu_show_start_screen = false;

   menu->current_pad = 0;

   update_libretro_info(&g_extern.menu.info);

   menu_shader_manager_init(menu);

   return menu;
}

void menu_free_list(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;
   if (!menu)
      return;

   settings_list_free(menu->list_mainmenu);
   settings_list_free(menu->list_settings);

   menu->list_mainmenu = NULL;
   menu->list_settings = NULL;
}

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

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&g_extern.menu.info);
#endif

   menu_list_destroy(menu->menu_stack);
   menu_list_destroy(menu->selection_buf);

   rarch_main_command(RARCH_CMD_HISTORY_DEINIT);

   if (g_extern.core_info)
      core_info_list_free(g_extern.core_info);

   if (g_extern.core_info_current)
      free(g_extern.core_info_current);

   free(data);
}

void menu_ticker_line(char *buf, size_t len, unsigned index,
      const char *str, bool selected)
{
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

   {
      /* Wrap long strings in options with some kind of ticker line. */
      unsigned ticker_period = 2 * (str_len - len) + 4;
      unsigned phase = index % ticker_period;

      unsigned phase_left_stop = 2;
      unsigned phase_left_moving = phase_left_stop + (str_len - len);
      unsigned phase_right_stop = phase_left_moving + 2;

      unsigned left_offset = phase - phase_left_stop;
      unsigned right_offset = (str_len - len) - (phase - phase_right_stop);

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
}

static unsigned input_frame(uint64_t trigger_state)
{
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
      return MENU_ACTION_UP;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
      return MENU_ACTION_DOWN;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
      return MENU_ACTION_LEFT;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      return MENU_ACTION_RIGHT;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L))
      return MENU_ACTION_SCROLL_UP;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R))
      return MENU_ACTION_SCROLL_DOWN;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B))
      return MENU_ACTION_CANCEL;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A))
      return MENU_ACTION_OK;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_Y))
      return MENU_ACTION_Y;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START))
      return MENU_ACTION_START;
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT))
      return MENU_ACTION_SELECT;
   if (trigger_state & (1ULL << RARCH_MENU_TOGGLE))
      return MENU_ACTION_TOGGLE;
   return MENU_ACTION_NOOP;
}

void apply_deferred_settings(void)
{
   rarch_setting_t *setting = NULL;
    
   if (!driver.menu)
      return;
    
   setting = (rarch_setting_t*)driver.menu->list_settings;
    
   if (!setting)
      return;

   for (; setting->type != ST_NONE; setting++)
   {
      if ((setting->type < ST_GROUP) && (setting->flags & SD_FLAG_IS_DEFERRED))
      {
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
               /* always run the deferred write handler */
               setting->deferred_handler(setting);
               break;
            default:
               break;
         }
      }
   }
}

/* Returns:
 *  0  -  Forcibly wake up the loop.
 * -1  -  Quit out of iteration loop.
 */

int menu_iterate(retro_input_t input,
      retro_input_t old_input, retro_input_t trigger_input)
{
   unsigned action = MENU_ACTION_NOOP;
   static bool initial_held = true;
   static bool first_held = false;
   int32_t ret = 0;
   static const retro_input_t input_repeat =
      (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_L)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_R);

   if (!driver.menu)
      return -1;

   if (BIT64_GET(trigger_input, RARCH_OVERLAY_NEXT))
      rarch_main_command(RARCH_CMD_OVERLAY_NEXT);
   if (BIT64_GET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY))
      rarch_main_command(RARCH_CMD_FULLSCREEN_TOGGLE);

   driver.retro_ctx.poll_cb();

   if (input & input_repeat)
   {
      if (!first_held)
      {
         first_held = true;
         driver.menu->delay_timer = initial_held ? 12 : 6;
         driver.menu->delay_count = 0;
      }

      if (driver.menu->delay_count >= driver.menu->delay_timer)
      {
         first_held = false;
         trigger_input |= input & input_repeat;
         driver.menu->scroll_accel = min(driver.menu->scroll_accel + 1, 64);
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
      driver.menu->scroll_accel = 0;
   }

   driver.menu->delay_count++;

   if (driver.block_input)
      trigger_input = 0;

   /* don't run anything first frame, only capture held inputs
    * for old_input_state.
    */
   action = input_frame(trigger_input);

   if (driver.menu_ctx && driver.menu_ctx->backend
         && driver.menu_ctx->backend->iterate) 
      ret = driver.menu_ctx->backend->iterate(action);

   draw_frame();

   if (driver.menu_ctx && driver.menu_ctx->input_postprocess)
      driver.menu_ctx->input_postprocess(input, old_input);

   if (ret)
      return -1;

   return 0;
}

unsigned menu_common_type_is(const char *label, unsigned type)
{
   if (
         type == MENU_SETTINGS ||
         type == MENU_FILE_CATEGORY ||
         !strcmp(label, "Shader Options") ||
         !strcmp(label, "Input Options") ||
         !strcmp(label, "core_options") ||
         !strcmp(label, "core_information") ||
         !strcmp(label, "video_shader_parameters") ||
         !strcmp(label, "video_shader_preset_parameters") ||
         !strcmp(label, "disk_options") ||
         !strcmp(label, "settings") ||
         !strcmp(label, "performance_counters") ||
         !strcmp(label, "frontend_counters") ||
         !strcmp(label, "core_counters")
         )
         return MENU_SETTINGS;


   if (
         !strcmp(label, "rgui_browser_directory") ||
         !strcmp(label, "content_directory") ||
         !strcmp(label, "assets_directory") ||
         !strcmp(label, "video_shader_dir") ||
         !strcmp(label, "video_filter_dir") ||
         !strcmp(label, "audio_filter_dir") ||
         !strcmp(label, "savestate_directory") ||
         !strcmp(label, "libretro_dir_path") ||
         !strcmp(label, "libretro_info_path") ||
         !strcmp(label, "rgui_config_directory") ||
         !strcmp(label, "savefile_directory") ||
         !strcmp(label, "overlay_directory") ||
         !strcmp(label, "screenshot_directory") ||
         !strcmp(label, "joypad_autoconfig_dir") ||
         !strcmp(label, "playlist_directory") ||
         !strcmp(label, "extraction_directory") ||
         !strcmp(label, "system_directory"))
      return MENU_FILE_DIRECTORY;

   return 0;
}
