/*  RetroArch - A frontend for libretro.
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

#include "menu_action.h"
#include "menu_common.h"
#include "menu_input_line_cb.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "backend/menu_backend.h"

#include "../../config.def.h"
#include "../../performance.h"

static void common_load_content(void)
{
   rarch_main_command(RARCH_CMD_LOAD_CONTENT);
   menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
   driver.menu->msg_force = true;
}

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         g_settings.menu_content_directory, label, MENU_FILE_DIRECTORY,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   rarch_playlist_load_content(g_defaults.history,
         driver.menu->selection_ptr);
   menu_flush_stack_type(driver.menu->menu_stack, MENU_SETTINGS);
   return -1;
}

static int action_ok_push_history_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         "", label, type, driver.menu->selection_ptr);
   menu_entries_push_list(driver.menu, driver.menu->selection_buf, 
         path, label, type);
   return 0;
}

static int action_ok_push_path_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         "", label, type, driver.menu->selection_ptr);
   return 0;
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t index)
{
   rarch_main_command(RARCH_CMD_SHADERS_APPLY_CHANGES);
   return 0;
}

// FIXME: Ugly hack, nees to be refactored badly
size_t hack_shader_pass = 0;

static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path = NULL;
   if (!driver.menu)
      return -1;
   (void)menu_path;

#ifdef HAVE_SHADER_MANAGER
   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL,
         NULL);
   fill_pathname_join(driver.menu->shader->pass[hack_shader_pass].source.path,
         menu_path, path,
         sizeof(driver.menu->shader->pass[hack_shader_pass].source.path));

   /* This will reset any changed parameters. */
   gfx_shader_resolve_parameters(NULL, driver.menu->shader);
   menu_flush_stack_label(driver.menu->menu_stack, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path = NULL;
   char shader_path[PATH_MAX];
   if (!driver.menu)
      return -1;

   (void)shader_path;
   (void)menu_path;
#ifdef HAVE_SHADER_MANAGER
   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL,
         NULL);
   fill_pathname_join(shader_path, menu_path, path, sizeof(shader_path));
   menu_shader_manager_set_preset(driver.menu->shader,
         gfx_shader_parse_type(shader_path, RARCH_SHADER_NONE),
         shader_path);
   menu_flush_stack_label(driver.menu->menu_stack, "Shader Options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_key_start_line(driver.menu, "Preset Filename",
         label, st_string_callback);
   return 0;
}

static int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, &menu_label, NULL);
   setting = (rarch_setting_t*)
      setting_data_find_setting(driver.menu->list_settings, menu_label);

   if (!setting)
      return -1;

   if (setting->type == ST_DIR)
   {
      menu_action_setting_set_current_string(setting, menu_path);
      menu_entries_pop_stack(driver.menu->menu_stack, setting->name);
   }

   return 0;
}

static int action_ok_core_load_deferred(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
   strlcpy(g_extern.fullpath, driver.menu->deferred_path,
         sizeof(g_extern.fullpath));

   rarch_main_command(RARCH_CMD_LOAD_CONTENT);
   menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
   driver.menu->msg_force = true;

   return -1;
}

static int action_ok_core_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path    = NULL;
   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL, NULL);

   fill_pathname_join(g_settings.libretro, menu_path, path,
         sizeof(g_settings.libretro));
   rarch_main_command(RARCH_CMD_LOAD_CORE);
   menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
#if defined(HAVE_DYNAMIC)
   /* No content needed for this core, load core immediately. */
   if (driver.menu->load_no_content)
   {
      *g_extern.fullpath = '\0';
      rarch_main_command(RARCH_CMD_LOAD_CONTENT);
      menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
      driver.menu->msg_force = true;
      return -1;
   }

   return 0;
   /* Core selection on non-console just updates directory listing.
    * Will take effect on new content load. */
#elif defined(RARCH_CONSOLE)
   rarch_main_command(RARCH_CMD_RESTART_RETROARCH);
   return -1;
#endif
}

static int action_ok_compressed_archive_push(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   char cat_path[PATH_MAX];

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, &menu_label, NULL);

   if (!strcmp(menu_label, "detect_core_list"))
   {
      file_list_push(driver.menu->menu_stack, path, "load_open_zip",
            0, driver.menu->selection_ptr);
      return 0;
   }

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_entries_push(driver.menu->menu_stack,
         cat_path, menu_label, type, driver.menu->selection_ptr);

   return 0;
}

static int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   char cat_path[PATH_MAX];

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, &menu_label, NULL);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));
   menu_entries_push(driver.menu->menu_stack,
         cat_path, menu_label, type, driver.menu->selection_ptr);

   return 0;
}

static int action_ok_config_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path  = NULL;
   char config[PATH_MAX];

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL, NULL);

   fill_pathname_join(config, menu_path, path, sizeof(config));
   menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
   driver.menu->msg_force = true;
   if (rarch_replace_config(config))
   {
      menu_clear_navigation(driver.menu, false);
      return -1;
   }

   return 0;
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path    = NULL;
   char image[PATH_MAX];

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL, NULL);

   fill_pathname_join(image, menu_path, path, sizeof(image));
   rarch_disk_control_append_image(image);

   rarch_main_command(RARCH_CMD_RESUME);

   menu_flush_stack_type(driver.menu->menu_stack, MENU_SETTINGS);
   return -1;
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path    = NULL;
   int ret;

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, NULL, NULL);

   ret = rarch_defer_core(g_extern.core_info,
         menu_path, path, driver.menu->deferred_path,
         sizeof(driver.menu->deferred_path));

   if (ret == -1)
   {
      rarch_main_command(RARCH_CMD_LOAD_CORE);
      common_load_content();
      return -1;
   }
   else if (ret == 0)
      menu_entries_push(
            driver.menu->menu_stack,
            g_settings.libretro_directory,
            "deferred_core_list",
            0, driver.menu->selection_ptr);

   return ret;
}

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, &menu_label, NULL);
   setting = (rarch_setting_t*)
      setting_data_find_setting(driver.menu->list_settings, menu_label);

   if (setting && setting->type == ST_PATH)
   {
      menu_action_setting_set_current_string_path(setting, menu_path, path);
      menu_entries_pop_stack(driver.menu->menu_stack, setting->name);
   }
   else
   {
      if (type == MENU_FILE_IN_CARCHIVE)
         fill_pathname_join_delim(g_extern.fullpath, menu_path, path,
               '#',sizeof(g_extern.fullpath));
      else
         fill_pathname_join(g_extern.fullpath, menu_path, path,
               sizeof(g_extern.fullpath));

      common_load_content();
      rarch_main_command(RARCH_CMD_LOAD_CONTENT_PERSIST);
      menu_flush_stack_type(driver.menu->menu_stack,MENU_SETTINGS);
      driver.menu->msg_force = true;

      return -1;
   }

   return 0;
}

static int action_ok_set_path(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *menu_path    = NULL;
   const char *menu_label   = NULL;
   rarch_setting_t *setting = NULL;

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, &menu_path, &menu_label, NULL);

   setting = (rarch_setting_t*)
      setting_data_find_setting(driver.menu->list_settings, menu_label);

   if (!setting)
      return -1;

   menu_action_setting_set_current_string_path(setting, menu_path, path);
   menu_entries_pop_stack(driver.menu->menu_stack, setting->name);

   return 0;
}

static int action_ok_bind_all(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   driver.menu->binds.target = &g_settings.input.binds
      [driver.menu->current_pad][0];
   driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
   driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;

   file_list_push(driver.menu->menu_stack, "", "",
         driver.menu->bind_mode_keyboard ?
         MENU_SETTINGS_CUSTOM_BIND_KEYBOARD :
         MENU_SETTINGS_CUSTOM_BIND,
         driver.menu->selection_ptr);
   if (driver.menu->bind_mode_keyboard)
   {
      driver.menu->binds.timeout_end =
         rarch_get_time_usec() + 
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      input_keyboard_wait_keys(driver.menu,
            menu_custom_bind_keyboard_cb);
   }
   else
   {
      menu_poll_bind_get_rested_axes(&driver.menu->binds);
      menu_poll_bind_state(&driver.menu->binds);
   }
   return 0;
}

static int action_ok_bind_default_all(const char *path,
      const char *label, unsigned type, size_t index)
{
   unsigned i;
   const struct retro_keybind *def_binds;
   struct retro_keybind *target = (struct retro_keybind*)
      &g_settings.input.binds[driver.menu->current_pad][0];

   def_binds = driver.menu->current_pad ? retro_keybinds_rest : retro_keybinds_1;

   driver.menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
   driver.menu->binds.last = MENU_SETTINGS_BIND_LAST;

   for (i = MENU_SETTINGS_BIND_BEGIN;
         i <= MENU_SETTINGS_BIND_LAST; i++, target++)
   {
      if (driver.menu->bind_mode_keyboard)
         target->key = def_binds[i - MENU_SETTINGS_BIND_BEGIN].key;
      else
      {
         target->joykey = NO_BTN;
         target->joyaxis = AXIS_NONE;
      }
   }
   return 0;
}

static int action_ok_bind_key(const char *path,
      const char *label, unsigned type, size_t index)
{
   struct retro_keybind *bind = NULL;

   if (!driver.menu)
      return -1;
   
   bind = (struct retro_keybind*)&g_settings.input.binds
      [driver.menu->current_pad][type - MENU_SETTINGS_BIND_BEGIN];

   driver.menu->binds.begin  = type;
   driver.menu->binds.last   = type;
   driver.menu->binds.target = bind;
   driver.menu->binds.player = driver.menu->current_pad;
   file_list_push(driver.menu->menu_stack, "", "",
         driver.menu->bind_mode_keyboard ?
         MENU_SETTINGS_CUSTOM_BIND_KEYBOARD : MENU_SETTINGS_CUSTOM_BIND,
         driver.menu->selection_ptr);

   if (driver.menu->bind_mode_keyboard)
   {
      driver.menu->binds.timeout_end = rarch_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      input_keyboard_wait_keys(driver.menu,
            menu_custom_bind_keyboard_cb);
   }
   else
   {
      menu_poll_bind_get_rested_axes(&driver.menu->binds);
      menu_poll_bind_state(&driver.menu->binds);
   }

   return 0;
}

static int action_ok_custom_viewport(const char *path,
      const char *label, unsigned type, size_t index)
{
   file_list_push(driver.menu->menu_stack, "", "",
         MENU_SETTINGS_CUSTOM_VIEWPORT,
         driver.menu->selection_ptr);

   /* Start with something sane. */
   rarch_viewport_t *custom = (rarch_viewport_t*)
      &g_extern.console.screen.viewports.custom_vp;

   if (driver.video_data && driver.video &&
         driver.video->viewport_info)
      driver.video->viewport_info(driver.video_data, custom);
   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;

   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   return 0;
}

static int action_ok_core_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *dir = g_settings.libretro_directory;

   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         dir, label, type,
         driver.menu->selection_ptr);

   return 0;
}

static int action_ok_disk_image_append_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *dir = g_settings.menu_content_directory;

   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         dir, label, type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_configurations_list(const char *path,
      const char *label, unsigned type, size_t index)
{
   const char *dir = g_settings.menu_config_directory;
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         dir ? dir : label, label, type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   menu_entries_push(driver.menu->menu_stack,
         label, label, type,
         driver.menu->selection_ptr);
   return 0;
}

static int action_ok_help(const char *path,
      const char *label, unsigned type, size_t index)
{
   if (!driver.menu)
      return -1;

   file_list_push(driver.menu->menu_stack, "", "help", 0, 0);
   driver.menu->push_start_screen = false;

   return 0;
}

static int performance_counters_core_toggle(unsigned type, const char *label,
      unsigned action)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_libretro;
   unsigned offset = type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN;

   (void)label;

   if (counters[offset] && action == MENU_ACTION_START)
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int performance_counters_frontend_toggle(unsigned type, const char *label,
      unsigned action)
{
   struct retro_perf_counter **counters = (struct retro_perf_counter**)
      perf_counters_rarch;
   unsigned offset = type - MENU_SETTINGS_PERF_COUNTERS_BEGIN;

   (void)label;

   if (counters[offset] && action == MENU_ACTION_START)
   {
      counters[offset]->total = 0;
      counters[offset]->call_cnt = 0;
   }

   return 0;
}

static int core_setting_toggle(unsigned type, const char *label,
      unsigned action)
{
   unsigned index = type - MENU_SETTINGS_CORE_OPTION_START;

   (void)label;

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

static int action_start_bind(unsigned type, const char *label,
      unsigned action)
{
   struct retro_keybind *def_binds = (struct retro_keybind *)retro_keybinds_1;
   struct retro_keybind *bind = (struct retro_keybind*)
      &g_settings.input.binds[driver.menu->current_pad]
      [type - MENU_SETTINGS_BIND_BEGIN];

   if (!bind)
      return -1;

   (void)label;
   (void)action;

   if (!driver.menu->bind_mode_keyboard)
   {
      bind->joykey = NO_BTN;
      bind->joyaxis = AXIS_NONE;
      return 0;
   }

   if (driver.menu->current_pad)
      def_binds = (struct retro_keybind*)retro_keybinds_rest;

   if (!def_binds)
      return -1;

   bind->key = def_binds[type - MENU_SETTINGS_BIND_BEGIN].key;

   return 0;
}

/* Bind the OK callback function */

static int menu_entries_cbs_init_bind_ok_first(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   const char *menu_label = NULL;

   if (!driver.menu)
      return -1;

   file_list_get_last(driver.menu->menu_stack, NULL, &menu_label, NULL);

   if (type >= MENU_SETTINGS_BIND_BEGIN &&
         type <= MENU_SETTINGS_BIND_ALL_LAST)
   {
      cbs->action_ok = action_ok_bind_key;
      return 0;
   }

   switch (type)
   {
      case MENU_FILE_PLAYLIST_ENTRY:
         cbs->action_ok = action_ok_playlist_entry;
         break;
      case MENU_FILE_SHADER_PRESET:
         cbs->action_ok = action_ok_shader_preset_load;
         break;
      case MENU_FILE_SHADER:
         cbs->action_ok = action_ok_shader_pass_load;
         break;
      case MENU_FILE_USE_DIRECTORY:
         cbs->action_ok = action_ok_path_use_directory;
         break;
      case MENU_FILE_CONFIG:
         cbs->action_ok = action_ok_config_load;
         break;
      case MENU_FILE_DIRECTORY:
         cbs->action_ok = action_ok_directory_push;
         break;
      case MENU_FILE_CARCHIVE:
         cbs->action_ok = action_ok_compressed_archive_push;
         break;
      case MENU_FILE_CORE:
         if (!strcmp(menu_label, "deferred_core_list"))
            cbs->action_ok = action_ok_core_load_deferred;
         else if (!strcmp(menu_label, "core_list"))
            cbs->action_ok = action_ok_core_load;
         else
            return -1;
         break;
      case MENU_FILE_FONT:
      case MENU_FILE_OVERLAY:
      case MENU_FILE_AUDIOFILTER:
      case MENU_FILE_VIDEOFILTER:
         cbs->action_ok = action_ok_set_path;
         break;
#ifdef HAVE_COMPRESSION
      case MENU_FILE_IN_CARCHIVE:
#endif
      case MENU_FILE_PLAIN:
         if (!strcmp(menu_label, "detect_core_list"))
            cbs->action_ok = action_ok_file_load_with_detect_core;
         else if (!strcmp(menu_label, "disk_image_append"))
            cbs->action_ok = action_ok_disk_image_append;
         else
            cbs->action_ok = action_ok_file_load;
         break;
      case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
         cbs->action_ok = action_ok_bind_default_all;
         break;
      case MENU_SETTINGS_CUSTOM_BIND_ALL:
         cbs->action_ok = action_ok_bind_all;
         break;
      case MENU_SETTINGS_CUSTOM_VIEWPORT:
         cbs->action_ok = action_ok_custom_viewport;
         break;
      case MENU_SETTINGS:
      case MENU_FILE_CATEGORY:
         cbs->action_ok = action_ok_push_default;
         break;
      default:
         return -1;
   }

   return 0;
}

static void menu_entries_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   if (!cbs)
      return;

   cbs->action_start = NULL;

   if (type >= MENU_SETTINGS_BIND_BEGIN &&
         type <= MENU_SETTINGS_BIND_ALL_LAST)
      cbs->action_start = action_start_bind;
}

static void menu_entries_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   if (!cbs)
      return;

   cbs->action_ok = NULL;

   if (menu_entries_cbs_init_bind_ok_first(cbs, path, label, type, index) == 0)
      return;
   else if (!strcmp(label, "help"))
      cbs->action_ok = action_ok_help;
   else if (
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
      cbs->action_ok = action_ok_push_default;
   else if (
         !strcmp(label, "load_content") ||
         !strcmp(label, "detect_core_list")
         )
      cbs->action_ok = action_ok_push_content_list;
   else if (!strcmp(label, "history_list"))
      cbs->action_ok = action_ok_push_history_list;
   else if (menu_common_type_is(label, type) == MENU_FILE_DIRECTORY)
      cbs->action_ok = action_ok_push_path_list;
   else if (!strcmp(label, "shader_apply_changes"))
      cbs->action_ok = action_ok_shader_apply_changes;
   else if (!strcmp(label, "video_shader_preset_save_as"))
      cbs->action_ok = action_ok_shader_preset_save_as;
   else if (!strcmp(label, "core_list"))
      cbs->action_ok = action_ok_core_list;
   else if (!strcmp(label, "disk_image_append"))
      cbs->action_ok = action_ok_disk_image_append_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_ok = action_ok_configurations_list;
}

static void menu_entries_cbs_init_bind_toggle(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t index)
{
   if (!cbs)
      return;

   cbs->action_toggle = menu_action_setting_set;

   if ((menu_common_type_is(label, type) == MENU_SETTINGS_SHADER_OPTIONS) ||
         !strcmp(label, "video_shader_parameters") ||
         !strcmp(label, "video_shader_preset_parameters")
      )
      cbs->action_toggle = menu_shader_manager_setting_toggle;
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
      cbs->action_toggle = core_setting_toggle;
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_PERF_COUNTERS_END)
      cbs->action_toggle = performance_counters_frontend_toggle;
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN &&
         type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      cbs->action_toggle = performance_counters_core_toggle;
}

void menu_entries_cbs_init(void *data,
      const char *path, const char *label,
      unsigned type, size_t index)
{
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list = (file_list_t*)data;

   if (!list)
      return;

   cbs = (menu_file_list_cbs_t*)list->list[index].actiondata;

   if (cbs)
   {
      menu_entries_cbs_init_bind_ok(cbs, path, label, type, index);
      menu_entries_cbs_init_bind_start(cbs, path, label, type, index);
      menu_entries_cbs_init_bind_toggle(cbs, path, label, type, index);
   }
}
