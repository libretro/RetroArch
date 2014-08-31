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
#include "menu_entries.h"
#include "../frontend.h"

void menu_update_system_info(menu_handle_t *menu, bool *load_no_content)
{
#ifdef HAVE_DYNAMIC
   libretro_free_system_info(&menu->info);
   if (*g_settings.libretro)
   {
      libretro_get_system_info(g_settings.libretro, &menu->info,
            load_no_content);
#endif
      /* Keep track of info for the currently selected core. */
      if (menu->core_info)
      {
         if (core_info_list_get_info(menu->core_info,
                  menu->core_info_current, g_settings.libretro))
         {
            const core_info_t *info = (const core_info_t*)menu->core_info_current;

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

/* When selection is presented back, returns 0.
 * If it can make a decision right now, returns -1.
 */
int menu_defer_core(core_info_list_t *core_info, const char *dir,
      const char *path, char *deferred_path, size_t sizeof_deferred_path)
{
   const core_info_t *info = NULL;
   size_t supported = 0;

   fill_pathname_join(deferred_path, dir, path, sizeof_deferred_path);

   if (core_info)
      core_info_list_get_supported_cores(core_info, deferred_path, &info,
            &supported);

   /* Can make a decision right now. */
   if (supported == 1)
   {
      strlcpy(g_extern.fullpath, deferred_path, sizeof(g_extern.fullpath));

      if (path_file_exists(info->path))
         strlcpy(g_settings.libretro, info->path, sizeof(g_settings.libretro));

#ifdef HAVE_DYNAMIC
      g_extern.lifecycle_state |= (1ULL << MODE_LOAD_GAME);
#else
      rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)g_extern.fullpath);
#endif
      return -1;
   }

   return 0;
}

void menu_content_history_push_current(void)
{
   char tmp[PATH_MAX];

   /* g_extern.fullpath can be relative here.
    * Ensure we're pushing absolute path.
    */

   if (!g_extern.history)
      return;

   strlcpy(tmp, g_extern.fullpath, sizeof(tmp));

   if (*tmp)
      path_resolve_realpath(tmp, sizeof(tmp));

   if (g_extern.system.no_content || *tmp)
      if (g_extern.history)
         content_playlist_push(g_extern.history,
               *tmp ? tmp : NULL,
               g_settings.libretro,
               g_extern.system.info.library_name);
}

static void load_menu_content_prepare(void)
{
   if (!driver.menu)
      return;

   if (*g_extern.fullpath || driver.menu->load_no_content)
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
            *g_extern.fullpath ? g_extern.fullpath : NULL,
            g_settings.libretro,
            driver.menu->info.library_name ? driver.menu->info.library_name : "");
   }

   /* redraw menu frame */
   driver.menu->old_input_state = driver.menu->trigger_state = 0;
   driver.menu->do_held = false;
   driver.menu->msg_force = true;

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->iterate) 
      driver.menu_ctx->backend->iterate(MENU_ACTION_NOOP);

   /* Draw frame for loading message */
   if (driver.video_data && driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, driver.menu->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   if (driver.video)
      rarch_render_cached_frame();

   if (driver.video_data && driver.video_poke && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);
}

void load_menu_content_history(unsigned game_index)
{
   const char *path      = NULL;
   const char *core_path = NULL;
   const char *core_name = NULL;

   if (!driver.menu)
      return;

   content_playlist_get_index(g_extern.history,
         game_index, &path, &core_path, &core_name);

   strlcpy(g_settings.libretro, core_path, sizeof(g_settings.libretro));

   driver.menu->load_no_content = (path) ? false : true;

   rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)path);

#if defined(HAVE_DYNAMIC)
   menu_update_system_info(driver.menu, NULL);
#endif
}

static void menu_update_libretro_info(menu_handle_t *menu)
{
#ifndef HAVE_DYNAMIC
   retro_get_system_info(&menu->info);
#endif

   core_info_list_free(menu->core_info);
   menu->core_info = NULL;
   if (*g_settings.libretro_directory)
      menu->core_info = core_info_list_new(g_settings.libretro_directory);

   menu_update_system_info(menu, NULL);
}

static void menu_environment_get(int *argc, char *argv[], void *args, void *params_data)
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
   load_menu_content_prepare();

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

   /* Update menu state which depends on config. */
   if (driver.menu)
      menu_update_libretro_info(driver.menu);

   rarch_main_command(RARCH_CMD_HISTORY_DEINIT);
   rarch_main_command(RARCH_CMD_HISTORY_INIT);

   if (driver.menu_ctx && driver.menu_ctx->backend
         && driver.menu_ctx->backend->shader_manager_init)
      driver.menu_ctx->backend->shader_manager_init(driver.menu);

   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);

   g_extern.lifecycle_state |= (1ULL << MODE_GAME);

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
   menu->core_info_current = (core_info_t*)calloc(1, sizeof(core_info_t));
#ifdef HAVE_SHADER_MANAGER
   menu->shader = (struct gfx_shader*)calloc(1, sizeof(struct gfx_shader));
#endif
   file_list_push(menu->menu_stack, "", "", MENU_SETTINGS, 0);
   menu_clear_navigation(menu);
   menu->push_start_screen = g_settings.menu_show_start_screen;
   g_settings.menu_show_start_screen = false;

   menu_entries_push(menu, MENU_SETTINGS);

   menu->trigger_state = 0;
   menu->old_input_state = 0;
   menu->do_held = false;
   menu->frame_buf_show = true;
   menu->current_pad = 0;

   menu_update_libretro_info(menu);

   if (menu_ctx && menu_ctx->backend
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
   libretro_free_system_info(&menu->info);
#endif

   file_list_free(menu->menu_stack);
   file_list_free(menu->selection_buf);

   rarch_main_command(RARCH_CMD_HISTORY_DEINIT);

   if (menu->core_info)
      core_info_list_free(menu->core_info);

   if (menu->core_info_current)
      free(menu->core_info_current);

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

void menu_flush_stack_type(unsigned final_type)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   if (!driver.menu)
      return;

   driver.menu->need_refresh = true;
   file_list_get_last(driver.menu->menu_stack, &path, &label, &type);
   while (type != final_type)
   {
      file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
      file_list_get_last(driver.menu->menu_stack, &path, &label, &type);
   }
}

bool menu_iterate(void)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned action = MENU_ACTION_NOOP;
   static bool initial_held = true;
   static bool first_held = false;
   uint64_t input_state = 0;
   int32_t input_entry_ret = 0;
   int32_t ret = 0;

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

   if (input_key_pressed_func(RARCH_QUIT_KEY)
         || !driver.video->alive(driver.video_data))
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

   /* don't run anything first frame, only capture held inputs
    * for old_input_state.
    */
   if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
      action = MENU_ACTION_UP;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = MENU_ACTION_DOWN;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = MENU_ACTION_LEFT;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = MENU_ACTION_RIGHT;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_L))
      action = MENU_ACTION_SCROLL_UP;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_R))
      action = MENU_ACTION_SCROLL_DOWN;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B))
      action = MENU_ACTION_CANCEL;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A))
      action = MENU_ACTION_OK;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START))
      action = MENU_ACTION_START;
   else if (driver.menu->trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT))
      action = MENU_ACTION_SELECT;

   if (driver.menu_ctx && driver.menu_ctx->backend
         && driver.menu_ctx->backend->iterate) 
      input_entry_ret = driver.menu_ctx->backend->iterate(action);

   if (driver.video_data && driver.video_poke &&
         driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data,
            driver.menu->frame_buf_show, MENU_TEXTURE_FULLSCREEN);

   rarch_render_cached_frame();

   /* Throttle in case VSync is broken (avoid 1000+ FPS Menu). */
   driver.menu->time = rarch_get_time_usec();
   driver.menu->delta = (driver.menu->time - driver.menu->last_time) / 1000;
   driver.menu->target_msec = 750 / g_settings.video.refresh_rate;
   /* Try to sleep less, so we can hopefully rely on FPS logger. */
   driver.menu->sleep_msec = driver.menu->target_msec - driver.menu->delta;

   if (driver.menu->sleep_msec > 0)
      rarch_sleep((unsigned int)driver.menu->sleep_msec);
   driver.menu->last_time = rarch_get_time_usec();

   if (driver.video_data && driver.video_poke
         && driver.video_poke->set_texture_enable)
      driver.video_poke->set_texture_enable(driver.video_data, false,
            MENU_TEXTURE_FULLSCREEN);

   if (driver.menu_ctx && driver.menu_ctx->input_postprocess)
      ret = driver.menu_ctx->input_postprocess(driver.menu->old_input_state);

   if (ret < 0)
   {
      unsigned type = 0;
      file_list_get_last(driver.menu->menu_stack, &path, &label, &type);

      while (type != MENU_SETTINGS)
      {
         file_list_pop(driver.menu->menu_stack, &driver.menu->selection_ptr);
         file_list_get_last(driver.menu->menu_stack, &path, &label, &type);
      }
   }

   if (ret || input_entry_ret)
      return false;

   return true;
}

/* Quite intrusive and error prone.
 * Likely to have lots of small bugs.
 * Cleanly exit the main loop to ensure that all the tiny details
 * get set properly.
 * This should mitigate most of the smaller bugs.
 */
bool menu_replace_config(const char *path)
{
   if (strcmp(path, g_extern.config_path) == 0)
      return false;

   if (g_settings.config_save_on_exit && *g_extern.config_path)
      config_save_file(g_extern.config_path);

   strlcpy(g_extern.config_path, path, sizeof(g_extern.config_path));
   g_extern.block_config_read = false;
   *g_settings.libretro = '\0'; /* Load core in new config. */

   rarch_main_command(RARCH_CMD_PREPARE_DUMMY);

   return true;
}

/* Save a new config to a file. Filename is based
 * on heuristics to avoid typing.
 */
bool menu_save_new_config(void)
{
   char config_dir[PATH_MAX], config_name[PATH_MAX],
        config_path[PATH_MAX], msg[PATH_MAX];
   bool ret = false;
   bool found_path = false;

   *config_dir = '\0';

   if (*g_settings.menu_config_directory)
      strlcpy(config_dir, g_settings.menu_config_directory,
            sizeof(config_dir));
   else if (*g_extern.config_path) /* Fallback */
      fill_pathname_basedir(config_dir, g_extern.config_path,
            sizeof(config_dir));
   else
   {
      const char *msg = "Config directory not set. Cannot save new config.";
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_ERR("%s\n", msg);
      return false;
   }

   /* Infer file name based on libretro core. */
   if (*g_settings.libretro && path_file_exists(g_settings.libretro))
   {
      unsigned i;

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64];
         fill_pathname_base(config_name, g_settings.libretro, sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name, sizeof(config_path));

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

   /* Fallback to system time... */
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   if (config_save_file(config_path))
   {
      strlcpy(g_extern.config_path, config_path, sizeof(g_extern.config_path));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".", config_path);
      RARCH_LOG("%s\n", msg);
      ret = true;
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
            config_path);
      RARCH_ERR("%s\n", msg);
   }

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   return ret;
}

static inline int menu_list_get_first_char(file_list_t *buf, unsigned offset)
{
   int ret;
   const char *path = NULL;

   file_list_get_alt_at_offset(buf, offset, &path);
   ret = tolower(*path);

   /* "Normalize" non-alphabetical entries so they are lumped together
    * for purposes of jumping.
    */
   if (ret < 'a')
      ret = 'a' - 1;
   else if (ret > 'z')
      ret = 'z' + 1;
   return ret;
}

static inline bool menu_list_elem_is_dir(file_list_t *buf,
      unsigned offset)
{
   const char *path = NULL;
   const char *label = NULL;
   unsigned type = 0;

   file_list_get_at_offset(buf, offset, &path, &label, &type);

   return type != MENU_FILE_PLAIN;
}

void menu_build_scroll_indices(file_list_t *buf)
{
   size_t i;
   int current;
   bool current_is_dir;

   if (!driver.menu || !buf)
      return;

   driver.menu->scroll_indices_size = 0;
   if (!buf->size)
      return;

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = 0;

   current = menu_list_get_first_char(buf, 0);
   current_is_dir = menu_list_elem_is_dir(buf, 0);

   for (i = 1; i < buf->size; i++)
   {
      int first = menu_list_get_first_char(buf, i);
      bool is_dir = menu_list_elem_is_dir(buf, i);

      if ((current_is_dir && !is_dir) || (first > current))
         driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = i;

      current = first;
      current_is_dir = is_dir;
   }

   driver.menu->scroll_indices[driver.menu->scroll_indices_size++] = buf->size - 1;
}

void menu_push_info_screen(void)
{
   file_list_push(driver.menu->menu_stack, "", "help", 0, 0);
}
