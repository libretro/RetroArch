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
#include "../frontend.h"

static void draw_frame(bool enable)
{
   if (driver.video_data && driver.video_poke &&
         driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data,
            enable, MENU_TEXTURE_FULLSCREEN);

   if (!enable)
      return;

   if (driver.video)
      rarch_render_cached_frame();
}

static void throttle_frame(void)
{
   if (!driver.menu)
      return;

   /* Throttle in case VSync is broken (avoid 1000+ FPS Menu). */
   driver.menu->time = rarch_get_time_usec();
   driver.menu->delta = (driver.menu->time - driver.menu->last_time) / 1000;
   driver.menu->target_msec = 750 / g_settings.video.refresh_rate;
   /* Try to sleep less, so we can hopefully rely on FPS logger. */
   driver.menu->sleep_msec = driver.menu->target_msec - driver.menu->delta;

   if (driver.menu->sleep_msec > 0)
      rarch_sleep((unsigned int)driver.menu->sleep_msec);
   driver.menu->last_time = rarch_get_time_usec();
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

   wrap_args->no_content    = driver.menu->load_no_content;
   wrap_args->verbose       = g_extern.verbosity;
   wrap_args->config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
   wrap_args->sram_path     = *g_extern.savefile_dir ? g_extern.savefile_dir : NULL;
   wrap_args->state_path    = *g_extern.savestate_dir ? g_extern.savestate_dir : NULL;
   wrap_args->content_path  = *g_extern.fullpath ? g_extern.fullpath : NULL;
   wrap_args->libretro_path = *g_settings.libretro ? g_settings.libretro : NULL;
   wrap_args->touched       = true;
}

bool load_menu_content(void)
{
   if (*g_extern.fullpath || (driver.menu && driver.menu->load_no_content))
   {
      if (*g_extern.fullpath)
      {
         char tmp[PATH_MAX];
         char str[PATH_MAX];

         fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
         snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
         msg_queue_push(g_extern.msg_queue, str, 1, 1);
      }

      content_playlist_push(g_extern.history,
            g_extern.fullpath,
            g_settings.libretro,
            g_extern.menu.info.library_name);
   }

   /* redraw menu frame */
   if (driver.menu)
   {
      driver.menu->old_input_state = driver.menu->trigger_state = 0;
      driver.menu->do_held = false;
      driver.menu->msg_force = true;
   }

   if (driver.menu_ctx && driver.menu_ctx->backend &&
         driver.menu_ctx->backend->iterate) 
      driver.menu_ctx->backend->iterate(MENU_ACTION_NOOP);

   draw_frame(true);
   draw_frame(false);

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

   rarch_main_command(RARCH_CMD_HISTORY_DEINIT);
   rarch_main_command(RARCH_CMD_HISTORY_INIT);

   if (driver.menu_ctx && driver.menu_ctx->backend
         && driver.menu_ctx->backend->shader_manager_init)
      driver.menu_ctx->backend->shader_manager_init(driver.menu);

   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   rarch_main_command(RARCH_CMD_RESUME);

   return true;
}

void *menu_init(const void *data)
{
   menu_handle_t *menu;
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
   file_list_push(menu->menu_stack, "", "mainmenu", MENU_SETTINGS, 0);
   menu_clear_navigation(menu);
   menu->push_start_screen = g_settings.menu_show_start_screen;
   g_settings.menu_show_start_screen = false;

   menu_entries_push_list(menu, menu->selection_buf,
         "", "mainmenu", 0);

   menu->trigger_state = 0;
   menu->old_input_state = 0;
   menu->do_held = false;
   menu->current_pad = 0;

   update_libretro_info(&g_extern.menu.info);

   if (menu_ctx->backend
         && menu_ctx->backend->shader_manager_init)
      menu_ctx->backend->shader_manager_init(menu);

   rarch_main_command(RARCH_CMD_HISTORY_INIT);
   menu->last_time = rarch_get_time_usec();

   return menu;
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

   file_list_free(menu->menu_stack);
   file_list_free(menu->selection_buf);

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
   }
   else
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

bool menu_iterate(retro_input_t input,
      retro_input_t old_input, retro_input_t trigger_input)
{
   unsigned action = MENU_ACTION_NOOP;
   static bool initial_held = true;
   static bool first_held = false;
   uint64_t input_state = 0;
   int32_t ret = 0;

   if (!driver.menu)
      return false;

#ifdef HAVE_OVERLAY
   if (BIND_PRESSED(trigger_input, RARCH_OVERLAY_NEXT))
         input_overlay_next(driver.overlay);
#endif
   check_fullscreen_func(trigger_input);

   if (check_enter_menu_func(trigger_input) &&
         g_extern.main_is_init && !g_extern.libretro_dummy)
   {
      rarch_main_command(RARCH_CMD_RESUME);
      return false;
   }

   rarch_input_poll();
   input_state = menu_input();

   if (driver.menu->do_held)
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
         driver.menu->trigger_state = input_state;
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
   driver.menu->old_input_state = input_state;

   if (driver.block_input)
      driver.menu->trigger_state = 0;

   /* don't run anything first frame, only capture held inputs
    * for old_input_state.
    */
   action = input_frame(driver.menu->trigger_state);

   if (driver.menu_ctx && driver.menu_ctx->backend
         && driver.menu_ctx->backend->iterate) 
      ret = driver.menu_ctx->backend->iterate(action);

   draw_frame(true);
   throttle_frame();
   draw_frame(false);

   if (driver.menu_ctx && driver.menu_ctx->input_postprocess)
      driver.menu_ctx->input_postprocess(driver.menu->old_input_state);

#if 0
   /* Go back to Main Menu when exiting */
   if (ret < 0)
      menu_flush_stack_type(driver.menu->menu_stack, MENU_SETTINGS);
#endif

   if (ret)
      return false;

   return true;
}

unsigned menu_common_type_is(const char *label, unsigned type)
{
   if (
         !strcmp(label, "video_shader_pass") ||
         !strcmp(label, "video_shader_filter_pass") ||
         !strcmp(label, "video_shader_scale_pass") ||
         !strcmp(label, "video_shader_default_filter") ||
         !strcmp(label, "video_shader_num_passes") ||
         !strcmp(label, "video_shader_preset")
         )
      return MENU_SETTINGS_SHADER_OPTIONS;

   if (
         type == MENU_SETTINGS ||
         !strcmp(label, "General Options") ||
         !strcmp(label, "core_options") ||
         !strcmp(label, "core_information") ||
         !strcmp(label, "Video Options") ||
         !strcmp(label, "Font Options") ||
         !strcmp(label, "Shader Options") ||
         !strcmp(label, "video_shader_parameters") ||
         !strcmp(label, "video_shader_preset_parameters") ||
         !strcmp(label, "Audio Options") ||
         !strcmp(label, "disk_options") ||
         !strcmp(label, "Path Options") ||
         !strcmp(label, "Privacy Options") ||
         !strcmp(label, "Overlay Options") ||
         !strcmp(label, "User Options") ||
         !strcmp(label, "Netplay Options") ||
         !strcmp(label, "settings") ||
         !strcmp(label, "Driver Options") ||
         !strcmp(label, "performance_counters") ||
         !strcmp(label, "frontend_counters") ||
         !strcmp(label, "core_counters") ||
         !strcmp(label, "Input Options")
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

int menu_common_core_setting_toggle(unsigned setting, unsigned action)
{
   unsigned index = setting - MENU_SETTINGS_CORE_OPTION_START;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         core_option_prev(g_extern.system.core_options, index);
         break;

      case MENU_ACTION_RIGHT:
      case MENU_ACTION_OK:
         core_option_next(g_extern.system.core_options, index);
         break;

      case MENU_ACTION_START:
         core_option_set_default(g_extern.system.core_options, index);
         break;

      default:
         break;
   }

   return 0;
}

int menu_common_setting_set_perf(unsigned setting, unsigned action,
      struct retro_perf_counter **counters, unsigned offset)
{
   if (counters[offset] && action == MENU_ACTION_START)
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}
