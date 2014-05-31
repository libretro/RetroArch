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

void menu_update_system_info(void *data, bool *load_no_rom)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
   if (*g_settings.libretro)
   {
      libretro_get_system_info(g_settings.libretro, &rgui->info, load_no_rom);
#endif
      // Keep track of info for the currently selected core.
      if (rgui->core_info)
      {
         if (core_info_list_get_info(rgui->core_info, rgui->core_info_current, g_settings.libretro))
         {
            const core_info_t *info = (const core_info_t*)rgui->core_info_current;

            RARCH_LOG("[Core Info]:\n");
            if (info->display_name)
               RARCH_LOG("  Display Name: %s\n", info->display_name);
            if (info->supported_extensions)
               RARCH_LOG("  Supported Extensions: %s\n", info->supported_extensions);
            if (info->authors)
               RARCH_LOG("  Authors: %s\n", info->authors);
            if (info->permissions)
               RARCH_LOG("  Permissions: %s\n", info->permissions);
         }
      }
#ifdef HAVE_DYNAMIC
   }
#endif
}

// When selection is presented back, returns 0. If it can make a decision right now, returns -1.
int menu_defer_core(void *info_, const char *dir, const char *path, char *deferred_path, size_t sizeof_deferred_path)
{
   core_info_list_t *core_info;
   const core_info_t *info;
   size_t supported;

   core_info = (core_info_list_t*)info_;
   info = NULL;
   supported = 0;

   fill_pathname_join(deferred_path, dir, path, sizeof_deferred_path);

   if (core_info)
      core_info_list_get_supported_cores(core_info, deferred_path, &info, &supported);

   if (supported == 1) // Can make a decision right now.
   {
      strlcpy(g_extern.fullpath, deferred_path, sizeof(g_extern.fullpath));
      strlcpy(g_settings.libretro, info->path, sizeof(g_settings.libretro));

#ifdef HAVE_DYNAMIC
      g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
#else
      rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)g_settings.libretro);
      rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)g_extern.fullpath);
#endif
      return -1;
   }

   return 0;
}

void menu_rom_history_push(const char *path,
      const char *core_path,
      const char *core_name)
{
   if (driver.menu && driver.menu->history)
      rom_history_push(driver.menu->history, path, core_path, core_name);
}

void menu_rom_history_push_current(void)
{
   // g_extern.fullpath can be relative here.
   // Ensure we're pushing absolute path.
   char tmp[PATH_MAX];

   if (!driver.menu)
      return;

   strlcpy(tmp, g_extern.fullpath, sizeof(tmp));

   if (*tmp)
      path_resolve_realpath(tmp, sizeof(tmp));

   if (g_extern.system.no_game || *tmp)
      menu_rom_history_push(*tmp ? tmp : NULL,
            g_settings.libretro,
            g_extern.system.info.library_name);
}

void load_menu_game_prepare(void)
{
   if (!driver.menu)
      return;

   if (*g_extern.fullpath || driver.menu->load_no_rom)
   {
      if (*g_extern.fullpath)
      {
         char tmp[PATH_MAX];
         char str[PATH_MAX];

         fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
         snprintf(str, sizeof(str), "INFO - Loading %s ...", tmp);
         msg_queue_push(g_extern.msg_queue, str, 1, 1);
      }

#ifdef RARCH_CONSOLE
      if (g_extern.system.no_game || *g_extern.fullpath)
#endif
      menu_rom_history_push(*g_extern.fullpath ? g_extern.fullpath : NULL,
            g_settings.libretro,
            driver.menu->info.library_name ? driver.menu->info.library_name : "");
   }

   // redraw RGUI frame
   driver.menu->old_input_state = driver.menu->trigger_state = 0;
   driver.menu->do_held = false;
   driver.menu->msg_force = true;

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->iterate) 
      driver.menu_ctx->backend->iterate(RGUI_ACTION_NOOP);

   // Draw frame for loading message
   if (driver.video_data && driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, driver.menu->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   if (driver.video)
      rarch_render_cached_frame();

   if (driver.video_data && driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);
}

void load_menu_game_history(unsigned game_index)
{
   const char *path = NULL;
   const char *core_path = NULL;
   const char *core_name = NULL;

   if (!driver.menu)
      return;

   rom_history_get_index(driver.menu->history,
         game_index, &path, &core_path, &core_name);

   // SET_LIBRETRO_PATH is unsafe here.
   // Risks booting different and wrong core if core doesn't exist anymore.
   strlcpy(g_settings.libretro, core_path, sizeof(g_settings.libretro));

   if (path)
      driver.menu->load_no_rom = false;
   else
      driver.menu->load_no_rom = true;

   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)path);

#if defined(HAVE_DYNAMIC)
   menu_update_system_info(driver.menu, NULL);
#endif
}

static void menu_init_history(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

   if (rgui->history)
   {
      rom_history_free(rgui->history);
      rgui->history = NULL;
   }

   if (*g_extern.config_path)
   {
      char history_path[PATH_MAX];
      if (*g_settings.game_history_path)
         strlcpy(history_path, g_settings.game_history_path, sizeof(history_path));
      else
      {
         fill_pathname_resolve_relative(history_path, g_extern.config_path,
               ".retroarch-game-history.txt", sizeof(history_path));
      }

      RARCH_LOG("[RGUI]: Opening history: %s.\n", history_path);
      rgui->history = rom_history_init(history_path, g_settings.game_history_size);
   }
}

static void menu_update_libretro_info(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

#ifndef HAVE_DYNAMIC
   retro_get_system_info(&rgui->info);
#endif

   core_info_list_free(rgui->core_info);
   rgui->core_info = NULL;
   if (*g_settings.libretro_directory)
      rgui->core_info = core_info_list_new(g_settings.libretro_directory);

   menu_update_system_info(rgui, NULL);
}

void load_menu_game_prepare_dummy(void)
{
   if (!driver.menu)
      return;

   // Starts dummy core.
   *g_extern.fullpath = '\0';
   driver.menu->load_no_rom = false;

   g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
   g_extern.lifecycle_state &= ~(1ULL << MODE_GAME);
   g_extern.system.shutdown = false;
}

bool load_menu_game(void)
{
   int ret;
   struct rarch_main_wrap args = {0};

   if (!driver.menu)
      return false;

   args.no_rom        = driver.menu->load_no_rom;

   if (g_extern.main_is_init)
      rarch_main_deinit();

   args.verbose       = g_extern.verbose;
   args.config_path   = *g_extern.config_path ? g_extern.config_path : NULL;
   args.sram_path     = *g_extern.savefile_dir ? g_extern.savefile_dir : NULL;
   args.state_path    = *g_extern.savestate_dir ? g_extern.savestate_dir : NULL;
   args.rom_path      = *g_extern.fullpath ? g_extern.fullpath : NULL;
   args.libretro_path = *g_settings.libretro ? g_settings.libretro : NULL;

   ret = rarch_main_init_wrap(&args);

   if (ret != 0)
   {
      char name[PATH_MAX], msg[PATH_MAX];

      fill_pathname_base(name, g_extern.fullpath, sizeof(name));
      snprintf(msg, sizeof(msg), "Failed to load %s.\n", name);
      msg_queue_push(g_extern.msg_queue, msg, 1, 90);
      driver.menu->msg_force = true;
      RARCH_ERR("rarch_main_init_wrap() failed.\n");
      return false;
   }

   RARCH_LOG("rarch_main_init_wrap() succeeded.\n");

   // Update menu state which depends on config.
   menu_update_libretro_info(driver.menu);
   menu_init_history(driver.menu);

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_init)
      driver.menu_ctx->backend->shader_manager_init(driver.menu);

   return true;
}

void *menu_init(const void *data)
{
   rgui_handle_t *rgui;
   menu_ctx_driver_t *menu_ctx = (menu_ctx_driver_t*)data;

   if (!menu_ctx)
      return NULL;

   rgui = (rgui_handle_t*)menu_ctx->init();

   if (!rgui)
      return NULL;

   strlcpy(g_settings.menu.driver, menu_ctx->ident, sizeof(g_settings.menu.driver));

   rgui->menu_stack = (file_list_t*)calloc(1, sizeof(file_list_t));
   rgui->selection_buf = (file_list_t*)calloc(1, sizeof(file_list_t));
   rgui->core_info_current = (core_info_t*)calloc(1, sizeof(core_info_t));
#ifdef HAVE_SHADER_MANAGER
   rgui->shader = (struct gfx_shader*)calloc(1, sizeof(struct gfx_shader));
#endif
   file_list_push(rgui->menu_stack, "", RGUI_SETTINGS, 0);
   menu_clear_navigation(rgui);
   rgui->push_start_screen = g_settings.rgui_show_start_screen;
   g_settings.rgui_show_start_screen = false;

   if (menu_ctx && menu_ctx->backend && menu_ctx->backend->entries_init) 
      menu_ctx->backend->entries_init(rgui, RGUI_SETTINGS);

   rgui->trigger_state = 0;
   rgui->old_input_state = 0;
   rgui->do_held = false;
   rgui->frame_buf_show = true;
   rgui->current_pad = 0;

   menu_update_libretro_info(rgui);

   if (menu_ctx && menu_ctx->backend && menu_ctx->backend->shader_manager_init)
      menu_ctx->backend->shader_manager_init(rgui);

   menu_init_history(rgui);
   rgui->last_time = rarch_get_time_usec();

   return rgui;
}

void menu_free(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;
  
#ifdef HAVE_SHADER_MANAGER
   if (rgui->shader)
      free(rgui->shader);
   rgui->shader = NULL;
#endif

   if (driver.menu_ctx && driver.menu_ctx->free)
      driver.menu_ctx->free(rgui);

#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&rgui->info);
#endif

   file_list_free(rgui->menu_stack);
   file_list_free(rgui->selection_buf);

   rom_history_free(rgui->history);
   core_info_list_free(rgui->core_info);

   if (rgui->core_info_current)
      free(rgui->core_info_current);

   free(data);
}

void menu_ticker_line(char *buf, size_t len, unsigned index, const char *str, bool selected)
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
      // Wrap long strings in options with some kind of ticker line.
      unsigned ticker_period = 2 * (str_len - len) + 4;
      unsigned phase = index % ticker_period;

      unsigned phase_left_stop = 2;
      unsigned phase_left_moving = phase_left_stop + (str_len - len);
      unsigned phase_right_stop = phase_left_moving + 2;

      unsigned left_offset = phase - phase_left_stop;
      unsigned right_offset = (str_len - len) - (phase - phase_right_stop);

      // Ticker period: [Wait at left (2 ticks), Progress to right (type_len - w), Wait at right (2 ticks), Progress to left].
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

uint64_t menu_input(void)
{
   unsigned i;
   uint64_t input_state;
   static const struct retro_keybind *binds[] = { g_settings.input.binds[0] };

   if (!driver.menu)
      return 0;

   input_state = 0;


   input_push_analog_dpad((struct retro_keybind*)binds[0], (g_settings.input.analog_dpad_mode[0] == ANALOG_DPAD_NONE) ? ANALOG_DPAD_LSTICK : g_settings.input.analog_dpad_mode[0]);
   for (i = 0; i < MAX_PLAYERS; i++)
      input_push_analog_dpad(g_settings.input.autoconf_binds[i], g_settings.input.analog_dpad_mode[i]);

   for (i = 0; i < RETRO_DEVICE_ID_JOYPAD_R2; i++)
   {
      input_state |= input_input_state_func(binds,
            0, RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;
#ifdef HAVE_OVERLAY
      input_state |= (driver.overlay_state.buttons & (UINT64_C(1) << i)) ? (1ULL << i) : 0;
#endif
   }

   input_state |= input_key_pressed_func(RARCH_MENU_TOGGLE) ? (1ULL << RARCH_MENU_TOGGLE) : 0;

   input_pop_analog_dpad((struct retro_keybind*)binds[0]);
   for (i = 0; i < MAX_PLAYERS; i++)
      input_pop_analog_dpad(g_settings.input.autoconf_binds[i]);

   driver.menu->trigger_state = input_state & ~driver.menu->old_input_state;

   driver.menu->do_held = (input_state & (
            (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_L)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_R)
            )) && !(input_state & (1ULL << RARCH_MENU_TOGGLE));

   return input_state;
}

bool menu_custom_bind_keyboard_cb(void *data, unsigned code)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return false;

   rgui->binds.target->key = (enum retro_key)code;
   rgui->binds.begin++;
   rgui->binds.target++;
   rgui->binds.timeout_end = rarch_get_time_usec() + RGUI_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
   return rgui->binds.begin <= rgui->binds.last;
}

void menu_flush_stack_type(unsigned final_type)
{
   unsigned type;

   if (!driver.menu)
      return;

   type = 0;
   driver.menu->need_refresh = true;
   file_list_get_last(driver.menu->menu_stack, NULL, &type);
   while (type != final_type)
   {
      file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
      file_list_get_last(driver.menu->menu_stack, NULL, &type);
   }
}

void load_menu_game_new_core(void)
{
   if (!driver.menu)
      return;

#ifdef HAVE_DYNAMIC
   menu_update_system_info(driver.menu, &driver.menu->load_no_rom);
   g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
#else
   rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH, (void*)g_settings.libretro);
   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)g_extern.fullpath);
#endif
}

bool menu_iterate(void)
{
   unsigned action;
   static bool initial_held = true;
   static bool first_held = false;
   uint64_t input_state;
   int32_t input_entry_ret, ret;

   input_state = 0;
   input_entry_ret = 0;
   ret = 0;

   if (!driver.menu)
      return false;

   if (g_extern.lifecycle_state & (1ULL << MODE_MENU_PREINIT))
   {
      driver.menu->need_refresh = true;
      g_extern.lifecycle_state &= ~(1ULL << MODE_MENU_PREINIT);
      driver.menu->old_input_state |= 1ULL << RARCH_MENU_TOGGLE;
   }

   rarch_input_poll();
   rarch_check_block_hotkey();
#ifdef HAVE_OVERLAY
   rarch_check_overlay();
#endif
   rarch_check_fullscreen();

   if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
   {
      g_extern.lifecycle_state |= (1ULL << MODE_GAME);
      return false;
   }

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

   action = RGUI_ACTION_NOOP;

   // don't run anything first frame, only capture held inputs for old_input_state
   if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
      action = RGUI_ACTION_UP;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = RGUI_ACTION_DOWN;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = RGUI_ACTION_LEFT;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = RGUI_ACTION_RIGHT;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L))
      action = RGUI_ACTION_SCROLL_UP;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R))
      action = RGUI_ACTION_SCROLL_DOWN;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B))
      action = RGUI_ACTION_CANCEL;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A))
      action = RGUI_ACTION_OK;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START))
      action = RGUI_ACTION_START;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT))
      action = RGUI_ACTION_SELECT;

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->iterate) 
      input_entry_ret = driver.menu_ctx->backend->iterate(action);

   if (driver.video_data && driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, driver.menu->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   rarch_render_cached_frame();

   // Throttle in case VSync is broken (avoid 1000+ FPS RGUI).
   driver.menu->time = rarch_get_time_usec();
   driver.menu->delta = (driver.menu->time - driver.menu->last_time) / 1000;
   driver.menu->target_msec = 750 / g_settings.video.refresh_rate; // Try to sleep less, so we can hopefully rely on FPS logger.
   driver.menu->sleep_msec = driver.menu->target_msec - driver.menu->delta;

   if (driver.menu->sleep_msec > 0)
      rarch_sleep((unsigned int)driver.menu->sleep_msec);
   driver.menu->last_time = rarch_get_time_usec();

   if (driver.video_data && driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);

   if (driver.menu_ctx && driver.menu_ctx->input_postprocess)
      ret = driver.menu_ctx->input_postprocess(driver.menu->old_input_state);

   if (ret < 0)
   {
      unsigned type = 0;
      file_list_get_last(driver.menu->menu_stack, NULL, &type);
      while (type != RGUI_SETTINGS)
      {
         file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
         file_list_get_last(driver.menu->menu_stack, NULL, &type);
      }
   }

   if (ret || input_entry_ret)
      return false;

   return true;
}

// Quite intrusive and error prone.
// Likely to have lots of small bugs.
// Cleanly exit the main loop to ensure that all the tiny details get set properly.
// This should mitigate most of the smaller bugs.
bool menu_replace_config(const char *path)
{
   if (!driver.menu)
      return false;

   if (strcmp(path, g_extern.config_path) == 0)
      return false;

   if (g_extern.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   strlcpy(g_extern.config_path, path, sizeof(g_extern.config_path));
   g_extern.block_config_read = false;

   // Load dummy core.
   *g_extern.fullpath = '\0';
   *g_settings.libretro = '\0'; // Load core in new config.
   g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
   driver.menu->load_no_rom = false;

   return true;
}

// Save a new config to a file. Filename is based on heuristics to avoid typing.
bool menu_save_new_config(void)
{
   char config_dir[PATH_MAX];
   *config_dir = '\0';

   if (*g_settings.rgui_config_directory)
      strlcpy(config_dir, g_settings.rgui_config_directory, sizeof(config_dir));
   else if (*g_extern.config_path) // Fallback
      fill_pathname_basedir(config_dir, g_extern.config_path, sizeof(config_dir));
   else
   {
      const char *msg = "Config directory not set. Cannot save new config.";
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_ERR("%s\n", msg);
      return false;
   }

   bool found_path = false;
   char config_name[PATH_MAX];
   char config_path[PATH_MAX];
   if (*g_settings.libretro && path_file_exists(g_settings.libretro)) // Infer file name based on libretro core.
   {
      unsigned i;
      // In case of collision, find an alternative name.
      for (i = 0; i < 16; i++)
      {
         fill_pathname_base(config_name, g_settings.libretro, sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name, sizeof(config_path));

         char tmp[64];
         *tmp = '\0';
         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));

         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   // Fallback to system time ...
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name, sizeof(config_path));
   }

   char msg[512];
   bool ret;
   if (config_save_file(config_path))
   {
      strlcpy(g_extern.config_path, config_path, sizeof(g_extern.config_path));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".", config_path);
      RARCH_LOG("%s\n", msg);
      ret = true;
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".", config_path);
      RARCH_ERR("%s\n", msg);
      ret = false;
   }

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   return ret;
}

void menu_poll_bind_state(void *data)
{
   struct rgui_bind_state *state = (struct rgui_bind_state*)data;

   if (!state)
      return;

   unsigned i, b, a, h;
   memset(state->state, 0, sizeof(state->state));
   state->skip = input_input_state_func(NULL, 0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);

   const rarch_joypad_driver_t *joypad = NULL;
   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   input_joypad_poll(joypad);
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      for (b = 0; b < RGUI_MAX_BUTTONS; b++)
         state->state[i].buttons[b] = input_joypad_button_raw(joypad, i, b);
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->state[i].axes[a] = input_joypad_axis_raw(joypad, i, a);
      for (h = 0; h < RGUI_MAX_HATS; h++)
      {
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_UP_MASK, h) ? HAT_UP_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_DOWN_MASK, h) ? HAT_DOWN_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_LEFT_MASK, h) ? HAT_LEFT_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_RIGHT_MASK, h) ? HAT_RIGHT_MASK : 0;
      }
   }
}

void menu_poll_bind_get_rested_axes(void *data)
{
   unsigned i, a;
   const rarch_joypad_driver_t *joypad = NULL;
   struct rgui_bind_state *state = (struct rgui_bind_state*)data;

   if (!state)
      return;

   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   for (i = 0; i < MAX_PLAYERS; i++)
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->axis_state[i].rested_axes[a] = input_joypad_axis_raw(joypad, i, a);
}

static bool menu_poll_find_trigger_pad(struct rgui_bind_state *state, struct rgui_bind_state *new_state, unsigned p)
{
   unsigned a, b, h;
   const struct rgui_bind_state_port *n = (const struct rgui_bind_state_port*)&new_state->state[p];
   const struct rgui_bind_state_port *o = (const struct rgui_bind_state_port*)&state->state[p];

   for (b = 0; b < RGUI_MAX_BUTTONS; b++)
   {
      if (n->buttons[b] && !o->buttons[b])
      {
         state->target->joykey = b;
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   // Axes are a bit tricky ...
   for (a = 0; a < RGUI_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] - new_state->axis_state[p].locked_axes[a]);
      int rested_distance = abs(n->axes[a] - new_state->axis_state[p].rested_axes[a]);

      if (abs(n->axes[a]) >= 20000 &&
            locked_distance >= 20000 &&
            rested_distance >= 20000) // Take care of case where axis rests on +/- 0x7fff (e.g. 360 controller on Linux)
      {
         state->target->joyaxis = n->axes[a] > 0 ? AXIS_POS(a) : AXIS_NEG(a);
         state->target->joykey = NO_BTN;

         // Lock the current axis.
         new_state->axis_state[p].locked_axes[a] = n->axes[a] > 0 ? 0x7fff : -0x7fff;
         return true;
      }

      if (locked_distance >= 20000) // Unlock the axis.
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < RGUI_MAX_HATS; h++)
   {
      uint16_t trigged = n->hats[h] & (~o->hats[h]);
      uint16_t sane_trigger = 0;
      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         state->target->joykey = HAT_MAP(h, sane_trigger);
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

bool menu_poll_find_trigger(void *data1, void *data2)
{
   unsigned i;
   struct rgui_bind_state *state, *new_state;
   state     = (struct rgui_bind_state*)data1;
   new_state = (struct rgui_bind_state*)data2;

   if (!state || !new_state)
      return false;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (menu_poll_find_trigger_pad(state, new_state, i))
      {
         g_settings.input.joypad_map[state->player] = i; // Update the joypad mapping automatically. More friendly that way.
         return true;
      }
   }
   return false;
}

static inline int menu_list_get_first_char(file_list_t *buf, unsigned offset)
{
   const char *path = NULL;
   file_list_get_alt_at_offset(buf, offset, &path);
   int ret = tolower(*path);

   // "Normalize" non-alphabetical entries so they are lumped together for purposes of jumping.
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

static inline bool menu_list_elem_is_dir(file_list_t *buf, unsigned offset)
{
   const char *path = NULL;
   unsigned type = 0;
   file_list_get_at_offset(buf, offset, &path, &type);
   return type != RGUI_FILE_PLAIN;
}

void menu_build_scroll_indices(file_list_t *buf)
{
   size_t i;
   int current;
   bool current_is_dir;

   if (!driver.menu)
      return;

   driver.menu->scroll_indices_size = 0;
   if (!buf->size)
      return;

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = 0;

   current = menu_list_get_first_char(buf, 0);
   current_is_dir = menu_list_elem_is_dir(buf, 0);

   for (i = 1; i < buf->size; i++)
   {
      int first;
      bool is_dir;

      first = menu_list_get_first_char(buf, i);
      is_dir = menu_list_elem_is_dir(buf, i);

      if ((current_is_dir && !is_dir) || (first > current))
         driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = i;

      current = first;
      current_is_dir = is_dir;
   }

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = buf->size - 1;
}
