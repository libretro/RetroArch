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

#include <file/file_path.h>
#include "menu.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"
#include "menu_entries.h"
#include "menu_shader.h"
#include "menu_navigation.h"

#include "../retroarch.h"
#include "../runloop.h"

#include "../input/input_remapping.h"

/* FIXME - Global variables, refactor */
char core_updater_list_path[PATH_MAX_LENGTH];
char core_updater_list_label[PATH_MAX_LENGTH];
unsigned core_updater_list_type;
unsigned rdb_entry_start_game_selection_ptr;
size_t hack_shader_pass = 0;
#ifdef HAVE_NETWORKING
char core_updater_path[PATH_MAX_LENGTH];
#endif

static int menu_action_setting_set_current_string_path(
      rarch_setting_t *setting, const char *dir, const char *path)
{
   fill_pathname_join(setting->value.string, dir, path, setting->size);
   return menu_setting_generic(setting);
}

static int action_ok_rdb_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   rarch_playlist_load_content(menu->db_playlist,
         rdb_entry_start_game_selection_ptr);
   return -1;
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   rarch_playlist_load_content(g_defaults.history,
         menu->navigation.selection_ptr);
   menu_list_flush_stack(menu->menu_list, MENU_SETTINGS);
   return -1;
}



static int action_ok_cheat_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx)
{
   cheat_manager_t *cheat = g_extern.cheat;

   if (!cheat)
      return -1;

   cheat_manager_apply_cheats(cheat);

   return 0;
}


static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   (void)menu_path;

#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(menu->shader->pass[hack_shader_pass].source.path,
         menu_path, path,
         sizeof(menu->shader->pass[hack_shader_pass].source.path));

   /* This will reset any changed parameters. */
   video_shader_resolve_parameters(NULL, menu->shader);
   menu_list_flush_stack_by_needle(menu->menu_list, "shader_options");
   return 0;
#else
   return -1;
#endif
}

#ifdef HAVE_SHADER_MANAGER
extern size_t hack_shader_pass;
#endif

static int action_ok_shader_pass(const char *path,
      const char *label, unsigned type, size_t idx)
{
   hack_shader_pass = type - MENU_SETTINGS_SHADER_PASS_0;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.video.shader_dir, 
         label,
         type,
         idx);
}

static int action_ok_shader_parameters(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   return menu_list_push_stack_refresh(
         menu->menu_list,
         "", label, MENU_SETTING_ACTION,
         idx);
}

static int action_ok_push_generic_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (path)
      strlcpy(menu->deferred_path, path,
            sizeof(menu->deferred_path));

   return menu_list_push_stack_refresh(
         menu->menu_list,
         "", label, type, idx);
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   return menu_list_push_stack_refresh(
         menu->menu_list,
         label, label, type, idx);
}

static int action_ok_shader_preset(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.video.shader_dir, 
         label, type, idx);
}

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.menu_content_directory,
         label, MENU_FILE_DIRECTORY, idx);
}

static int action_ok_disk_image_append_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.menu_content_directory, label, type,
         idx);
}

static int action_ok_configurations_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *dir = g_settings.menu_config_directory;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   return menu_list_push_stack_refresh(
         menu->menu_list,
         dir ? dir : label, label, type,
         idx);
}

static int action_ok_cheat_file(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.cheat_database,
         label, type, idx);
}

static int action_ok_audio_dsp_plugin(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return 0;
}

static int action_ok_video_filter(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.video.filter_dir,
         "deferred_video_filter",
         0, idx);
}

static int action_ok_core_updater_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char url_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   driver_t *driver = driver_get_ptr();
   if (!menu)
      return -1;

   driver->menu->nonblocking_refresh = true;

   (void)url_path;
#ifdef HAVE_NETWORKING
   strlcpy(core_updater_list_path, path, sizeof(core_updater_list_path));
   strlcpy(core_updater_list_label, label, sizeof(core_updater_list_label));
   core_updater_list_type = type;
#endif

   if (g_settings.network.buildbot_url[0] == '\0')
   {
      return -1;
   }

#ifdef HAVE_NETWORKING
   rarch_main_command(RARCH_CMD_NETWORK_INIT);

   fill_pathname_join(url_path, g_settings.network.buildbot_url,
         ".index", sizeof(url_path));

   rarch_main_data_msg_queue_push(DATA_TYPE_HTTP, url_path, "cb_core_updater_list", 0, 1,
         true);
#endif

   return menu_list_push_stack_refresh(
         menu->menu_list,
         path, "deferred_core_updater_list", type, idx);
}

static int action_ok_remap_file(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.input_remapping_directory,
         label, type, idx);
}

static int action_ok_core_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   return menu_list_push_stack_refresh(
         menu->menu_list,
         g_settings.libretro_directory,
         label, type, idx);
}

static int action_ok_remap_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char remap_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   (void)remap_path;
   (void)menu_path;

   menu_list_get_last_stack(menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(remap_path, menu_path, path, sizeof(remap_path));
   input_remapping_load_file(remap_path);

   menu_list_flush_stack_by_needle(menu->menu_list, "core_input_remapping_options");

   return 0;
}

static int action_ok_video_filter_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char filter_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   (void)filter_path;
   (void)menu_path;

   menu_list_get_last_stack(menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(filter_path, menu_path, path, sizeof(filter_path));

   strlcpy(g_settings.video.softfilter_plugin, filter_path,
         sizeof(g_settings.video.softfilter_plugin));

   rarch_main_command(RARCH_CMD_REINIT);

   menu_list_flush_stack_by_needle(menu->menu_list, "video_options");

   return 0;
}

static int action_ok_cheat_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char cheat_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   (void)cheat_path;
   (void)menu_path;
   menu_list_get_last_stack(menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(cheat_path, menu_path, path, sizeof(cheat_path));

   if (g_extern.cheat)
      cheat_manager_free(g_extern.cheat);

   g_extern.cheat = cheat_manager_load(cheat_path);

   if (!g_extern.cheat)
      return -1;

   menu_list_flush_stack_by_needle(menu->menu_list, "core_cheat_options");

   return 0;
}

static int action_ok_menu_wallpaper_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path = NULL;
   rarch_setting_t *setting = NULL;
   char wallpaper_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list, &menu_path, &menu_label,
         NULL);

   setting = menu_setting_find(menu_label);

   if (!setting)
      return -1;

   fill_pathname_join(wallpaper_path, menu_path, path, sizeof(wallpaper_path));

   if (path_file_exists(wallpaper_path))
   {
      strlcpy(g_settings.menu.wallpaper, wallpaper_path, sizeof(g_settings.menu.wallpaper));

      rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, wallpaper_path, "cb_menu_wallpaper", 0, 1,
            true);
   }

   menu_list_pop_stack_by_needle(menu->menu_list, setting->name);

   return 0;
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path = NULL;
   char shader_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   (void)shader_path;
   (void)menu_path;
#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(menu->menu_list, &menu_path, NULL,
         NULL);

   fill_pathname_join(shader_path, menu_path, path, sizeof(shader_path));
   menu_shader_manager_set_preset(menu->shader,
         video_shader_parse_type(shader_path, RARCH_SHADER_NONE),
         shader_path);
   menu_list_flush_stack_by_needle(menu->menu_list, "shader_options");
   return 0;
#else
   return -1;
#endif
}

static int action_ok_cheat(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line("Input Cheat",
         label, type, idx, menu_input_st_cheat_callback);
   return 0;
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line("Preset Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_cheat_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line("Cheat Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_remap_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_input_key_start_line("Remapping Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   setting = menu_setting_find(menu_label);

   if (!setting)
      return -1;

   if (setting->type != ST_DIR)
      return -1;

   menu_action_setting_set_current_string(setting, menu_path);
   menu_list_pop_stack_by_needle(menu->menu_list, setting->name);

   return 0;
}

static int action_ok_core_load_deferred(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (path)
      strlcpy(g_settings.libretro, path, sizeof(g_settings.libretro));
   strlcpy(g_extern.fullpath, menu->deferred_path,
         sizeof(g_extern.fullpath));

   menu_entries_common_load_content(false);

   return -1;
}

static int action_ok_database_manager_list_deferred(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return 0;
}

static int action_ok_rdb_entry(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char tmp[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   strlcpy(tmp, "deferred_rdb_entry_detail|", sizeof(tmp));
   strlcat(tmp, path, sizeof(tmp));

   return menu_list_push_stack_refresh(
         menu->menu_list,
         label,
         tmp,
         0, idx);
}

static int action_ok_cursor_manager_list_deferred(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return 0;
}

static int action_ok_core_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(g_settings.libretro, menu_path, path,
         sizeof(g_settings.libretro));
   rarch_main_command(RARCH_CMD_LOAD_CORE);
   menu_list_flush_stack(menu->menu_list, MENU_SETTINGS);
#if defined(HAVE_DYNAMIC)
   /* No content needed for this core, load core immediately. */
   if (menu->load_no_content)
   {
      *g_extern.fullpath = '\0';

      menu_entries_common_load_content(false);
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

static int action_ok_core_download(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return 0;
}

static int action_ok_compressed_archive_push(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   menu_list_push_stack(
         menu->menu_list,
         path,
         "load_open_zip",
         0,
         idx);

   return 0;
}

static int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path  = NULL;
   const char *menu_label = NULL;
   char cat_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   if (!path)
      return -1;

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));

   return menu_list_push_stack_refresh(menu->menu_list,
         cat_path, menu_label, type, idx);
}

static int action_ok_database_manager_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char rdb_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;
   if (!path)
      return -1;
   if (!label)
      return -1;

   fill_pathname_join(rdb_path, g_settings.content_database,
         path, sizeof(rdb_path));

   return menu_list_push_stack_refresh(
         menu->menu_list,
         rdb_path,
         "deferred_database_manager_list",
         0, idx);
}

static int action_ok_cursor_manager_list(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char cursor_path[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   fill_pathname_join(cursor_path, g_settings.cursor_directory,
         path, sizeof(cursor_path));

   return menu_list_push_stack_refresh(
         menu->menu_list,
         cursor_path,
         "deferred_cursor_manager_list",
         0, idx);
}

static int action_ok_config_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path  = NULL;
   char config[PATH_MAX_LENGTH];
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(config, menu_path, path, sizeof(config));
   menu_list_flush_stack(menu->menu_list, MENU_SETTINGS);
   menu->msg_force = true;
   if (rarch_replace_config(config))
   {
      menu_navigation_clear(&menu->navigation, false);
      return -1;
   }

   return 0;
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t idx)
{
   char image[PATH_MAX_LENGTH];
   const char *menu_path    = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, NULL, NULL);

   fill_pathname_join(image, menu_path, path, sizeof(image));
   rarch_disk_control_append_image(image);

   rarch_main_command(RARCH_CMD_RESUME);

   menu_list_flush_stack(menu->menu_list, MENU_SETTINGS);
   return -1;
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx)
{
   int ret;
   const char *menu_path    = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, NULL, NULL);

   ret = rarch_defer_core(g_extern.core_info,
         menu_path, path, label, menu->deferred_path,
         sizeof(menu->deferred_path));

   if (ret == -1)
   {
      rarch_main_command(RARCH_CMD_LOAD_CORE);
      menu_entries_common_load_content(false);
      return -1;
   }

   if (ret == 0)
      menu_list_push_stack_refresh(
            menu->menu_list,
            g_settings.libretro_directory,
            "deferred_core_list",
            0, idx);

   return ret;
}

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_get_last(menu->menu_list->menu_stack,
         &menu_path, &menu_label, NULL);

   setting = menu_setting_find(menu_label);

   if (setting && setting->type == ST_PATH)
   {
      menu_action_setting_set_current_string_path(setting, menu_path, path);
      menu_list_pop_stack_by_needle(menu->menu_list, setting->name);
   }
   else
   {
      if (type == MENU_FILE_IN_CARCHIVE)
         fill_pathname_join_delim(g_extern.fullpath, menu_path, path,
               '#',sizeof(g_extern.fullpath));
      else
         fill_pathname_join(g_extern.fullpath, menu_path, path,
               sizeof(g_extern.fullpath));

      menu_entries_common_load_content(true);

      return -1;
   }

   return 0;
}

static int action_ok_set_path(const char *path,
      const char *label, unsigned type, size_t idx)
{
   const char *menu_path    = NULL;
   const char *menu_label   = NULL;
   rarch_setting_t *setting = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   menu_list_get_last_stack(menu->menu_list,
         &menu_path, &menu_label, NULL);

   setting = menu_setting_find(menu_label);

   if (!setting)
      return -1;

   menu_action_setting_set_current_string_path(setting, menu_path, path);
   menu_list_pop_stack_by_needle(menu->menu_list, setting->name);

   return 0;
}

static int action_ok_custom_viewport(const char *path,
      const char *label, unsigned type, size_t idx)
{
   /* Start with something sane. */
   video_viewport_t *custom = &g_extern.console.screen.viewports.custom_vp;
   menu_handle_t *menu = menu_driver_resolve();
   driver_t *driver = driver_get_ptr();

   if (!menu)
      return -1;


   menu_list_push_stack(
         menu->menu_list,
         "",
         "custom_viewport_1",
         MENU_SETTINGS_CUSTOM_VIEWPORT,
         idx);

   if (driver->video_data && driver->video &&
         driver->video->viewport_info)
      driver->video->viewport_info(driver->video_data, custom);

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   g_settings.video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;

   rarch_main_command(RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   return 0;
}


static int generic_action_ok_command(unsigned cmd)
{

   if (!rarch_main_command(cmd))
      return -1;
   return 0;
}

static int action_ok_load_state(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (generic_action_ok_command(RARCH_CMD_LOAD_STATE) == -1)
      return -1;
   return generic_action_ok_command(RARCH_CMD_RESUME);
}


static int action_ok_save_state(const char *path,
      const char *label, unsigned type, size_t idx)
{
   if (generic_action_ok_command(RARCH_CMD_SAVE_STATE) == -1)
      return -1;
   return generic_action_ok_command(RARCH_CMD_RESUME);
}

static int action_ok_core_updater_download(const char *path,
      const char *label, unsigned type, size_t idx)
{
#ifdef HAVE_NETWORKING
   char core_path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];

   fill_pathname_join(core_path, g_settings.network.buildbot_url,
         path, sizeof(core_path));

   strlcpy(core_updater_path, path, sizeof(core_updater_path));
   snprintf(msg, sizeof(msg), "Starting download: %s.", path);

   rarch_main_msg_queue_push(msg, 1, 90, true);

   rarch_main_data_msg_queue_push(DATA_TYPE_HTTP, core_path,
         "cb_core_updater_download", 0, 1, true);
#endif
   return 0;
}

static int action_ok_disk_cycle_tray_status(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_DISK_EJECT_TOGGLE);
}

static int action_ok_quit(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_QUIT);
}

static int action_ok_save_new_config(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_MENU_SAVE_CONFIG);
}

static int action_ok_resume_content(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_RESUME);
}

static int action_ok_restart_content(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_RESET);
}

static int action_ok_screenshot(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_TAKE_SCREENSHOT);
}

static int action_ok_file_load_or_resume(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   if (!strcmp(menu->deferred_path, g_extern.fullpath))
      return generic_action_ok_command(RARCH_CMD_RESUME);
   else
   {
      strlcpy(g_extern.fullpath,
            menu->deferred_path, sizeof(g_extern.fullpath));
      rarch_main_command(RARCH_CMD_LOAD_CORE);
      rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
      return -1;
   }
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return generic_action_ok_command(RARCH_CMD_SHADERS_APPLY_CHANGES);
}

static int action_ok_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return menu_setting_set(type, label, MENU_ACTION_OK, false);
}

static int action_ok_rdb_entry_submenu(const char *path,
      const char *label, unsigned type, size_t idx)
{
   int ret;
   union string_list_elem_attr attr;
   char new_label[PATH_MAX_LENGTH];
   char *rdb = NULL;
   int len = 0;
   struct string_list *str_list  = NULL;
   struct string_list *str_list2 = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;

   if (!label)
      return -1;

   str_list = string_split(label, "|"); 

   if (!str_list)
      return -1;

   str_list2 = string_list_new();
   if (!str_list2)
   {
      string_list_free(str_list);
      return -1;
   }

   /* element 0 : label
    * element 1 : value
    * element 2 : database path
    */

   attr.i = 0;

   len += strlen(str_list->elems[1].data) + 1;
   string_list_append(str_list2, str_list->elems[1].data, attr);

   len += strlen(str_list->elems[2].data) + 1;
   string_list_append(str_list2, str_list->elems[2].data, attr);

   rdb = (char*)calloc(len, sizeof(char));

   if (!rdb)
   {
      string_list_free(str_list);
      string_list_free(str_list2);
      return -1;
   }

   string_list_join_concat(rdb, len, str_list2, "|");

   strlcpy(new_label, "deferred_cursor_manager_list_", sizeof(new_label));
   strlcat(new_label, str_list->elems[0].data, sizeof(new_label));

   ret = menu_list_push_stack_refresh(
         menu->menu_list,
         rdb,
         new_label,
         0, idx);

   string_list_free(str_list);
   string_list_free(str_list2);

   return ret;
}

static int action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx)
{
   menu_handle_t *menu = menu_driver_resolve();
   if (!menu)
      return -1;

   menu_list_push_stack(
         menu->menu_list,
         "",
         "help",
         0,
         0);
   menu->push_start_screen = false;

   return 0;
}

static int action_ok_video_resolution(const char *path,
      const char *label, unsigned type, size_t idx)
{

#ifdef __CELLOS_LV2__
   if (g_extern.console.screen.resolutions.list[
         g_extern.console.screen.resolutions.current.idx] == 
         CELL_VIDEO_OUT_RESOLUTION_576)
   {
      if (g_extern.console.screen.pal_enable)
         g_extern.console.screen.pal60_enable = true;
   }
   else
   {
      g_extern.console.screen.pal_enable = false;
      g_extern.console.screen.pal60_enable = false;
   }

   rarch_main_command(RARCH_CMD_REINIT);
#else
   driver_t *driver = driver_get_ptr();

   if (driver->video_data && driver->video_poke &&
         driver->video_poke->get_video_output_size)
   {
      unsigned width = 0, height = 0;
      driver->video_poke->get_video_output_size(driver->video_data,
            &width, &height);

      if (driver->video_data && driver->video_poke &&
            driver->video_poke->set_video_mode)
         driver->video_poke->set_video_mode(driver->video_data,
               width, height, true);
   }
#endif

   return 0;
}

static int is_rdb_entry(const char *label)
{
   return (
         !(strcmp(label, "rdb_entry_publisher")) ||
         !(strcmp(label, "rdb_entry_developer")) ||
         !(strcmp(label, "rdb_entry_origin")) ||
         !(strcmp(label, "rdb_entry_franchise")) ||
         !(strcmp(label, "rdb_entry_enhancement_hw")) ||
         !(strcmp(label, "rdb_entry_esrb_rating")) ||
         !(strcmp(label, "rdb_entry_bbfc_rating")) ||
         !(strcmp(label, "rdb_entry_elspa_rating")) ||
         !(strcmp(label, "rdb_entry_pegi_rating")) ||
         !(strcmp(label, "rdb_entry_cero_rating")) ||
         !(strcmp(label, "rdb_entry_edge_magazine_rating")) ||
         !(strcmp(label, "rdb_entry_edge_magazine_issue")) ||
         !(strcmp(label, "rdb_entry_famitsu_magazine_rating")) ||
         !(strcmp(label, "rdb_entry_releasemonth")) ||
         !(strcmp(label, "rdb_entry_releaseyear")) ||
         !(strcmp(label, "rdb_entry_max_users"))
         );
}

void menu_entries_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label)
{
   rarch_setting_t *setting = menu_setting_find(label);
   menu_handle_t *menu    = menu_driver_resolve();
   if (!menu)
      return;

   if (!cbs)
      return;
   if (!menu)
      return;

   cbs->action_ok = action_ok_lookup_setting;

   if (elem0[0] != '\0' && is_rdb_entry(elem0))
   {
      cbs->action_ok = action_ok_rdb_entry_submenu;
      return;
   }

   if (!strcmp(label, "custom_bind_all"))
      cbs->action_ok = action_ok_lookup_setting;
   else if (type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD ||
         type == MENU_SETTINGS_CUSTOM_BIND)
      cbs->action_ok = action_ok_lookup_setting;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_ok = NULL;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_ok = NULL;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_ok = action_ok_cheat;
   else if (!strcmp(label, "savestate"))
      cbs->action_ok = action_ok_save_state;
   else if (!strcmp(label, "loadstate"))
      cbs->action_ok = action_ok_load_state;
   else if (!strcmp(label, "resume_content"))
      cbs->action_ok = action_ok_resume_content;
   else if (!strcmp(label, "restart_content"))
      cbs->action_ok = action_ok_restart_content;
   else if (!strcmp(label, "take_screenshot"))
      cbs->action_ok = action_ok_screenshot;
   else if (!strcmp(label, "file_load_or_resume"))
      cbs->action_ok = action_ok_file_load_or_resume;
   else if (!strcmp(label, "quit_retroarch"))
      cbs->action_ok = action_ok_quit;
   else if (!strcmp(label, "save_new_config"))
      cbs->action_ok = action_ok_save_new_config;
   else if (!strcmp(label, "help"))
      cbs->action_ok = action_ok_help;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_ok = action_ok_shader_pass;
   else if (!strcmp(label, "video_shader_preset"))
      cbs->action_ok = action_ok_shader_preset;
   else if (!strcmp(label, "cheat_file_load"))
      cbs->action_ok = action_ok_cheat_file;
   else if (!strcmp(label, "audio_dsp_plugin"))
      cbs->action_ok = action_ok_audio_dsp_plugin;
   else if (!strcmp(label, "video_filter"))
      cbs->action_ok = action_ok_video_filter;
   else if (!strcmp(label, "remap_file_load"))
      cbs->action_ok = action_ok_remap_file;
   else if (!strcmp(label, "core_updater_list"))
      cbs->action_ok = action_ok_core_updater_list;
   else if (!strcmp(label, "video_shader_parameters") ||
         !strcmp(label, "video_shader_preset_parameters")
         )
      cbs->action_ok = action_ok_shader_parameters;
   else if (
         !strcmp(label, "shader_options") ||
         !strcmp(label, "video_options") ||
         !strcmp(label, "Input Settings") ||
         !strcmp(label, "core_options") ||
         !strcmp(label, "core_cheat_options") ||
         !strcmp(label, "core_input_remapping_options") ||
         !strcmp(label, "core_information") ||
         !strcmp(label, "disk_options") ||
         !strcmp(label, "settings") ||
         !strcmp(label, "performance_counters") ||
         !strcmp(label, "frontend_counters") ||
         !strcmp(label, "core_counters") ||
         !strcmp(label, "management") ||
         !strcmp(label, "options")
         )
      cbs->action_ok = action_ok_push_default;
   else if (
         !strcmp(label, "load_content") ||
         !strcmp(label, "detect_core_list")
         )
      cbs->action_ok = action_ok_push_content_list;
   else if (!strcmp(label, "history_list") ||
         !strcmp(label, "cursor_manager_list") ||
         !strcmp(label, "database_manager_list") ||
         (setting && setting->browser_selection_type == ST_DIR)
         )
      cbs->action_ok = action_ok_push_generic_list;
   else if (!strcmp(label, "shader_apply_changes"))
      cbs->action_ok = action_ok_shader_apply_changes;
   else if (!strcmp(label, "cheat_apply_changes"))
      cbs->action_ok = action_ok_cheat_apply_changes;
   else if (!strcmp(label, "video_shader_preset_save_as"))
      cbs->action_ok = action_ok_shader_preset_save_as;
   else if (!strcmp(label, "cheat_file_save_as"))
      cbs->action_ok = action_ok_cheat_file_save_as;
   else if (!strcmp(label, "remap_file_save_as"))
      cbs->action_ok = action_ok_remap_file_save_as;
   else if (!strcmp(label, "core_list"))
      cbs->action_ok = action_ok_core_list;
   else if (!strcmp(label, "disk_image_append"))
      cbs->action_ok = action_ok_disk_image_append_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_ok = action_ok_configurations_list;
   else
   switch (type)
   {
      case MENU_SETTINGS_VIDEO_RESOLUTION:
         cbs->action_ok = action_ok_video_resolution;
         break;
      case MENU_FILE_PLAYLIST_ENTRY:
         if (!strcmp(label, "rdb_entry_start_game"))
            cbs->action_ok = action_ok_rdb_playlist_entry;
         else
            cbs->action_ok = action_ok_playlist_entry;
         break;
      case MENU_FILE_CONTENTLIST_ENTRY:
         cbs->action_ok = action_ok_push_generic_list;
         break;
      case MENU_FILE_CHEAT:
         cbs->action_ok = action_ok_cheat_file_load;
         break;
      case MENU_FILE_REMAP:
         cbs->action_ok = action_ok_remap_file_load;
         break;
      case MENU_FILE_SHADER_PRESET:
         cbs->action_ok = action_ok_shader_preset_load;
         break;
      case MENU_FILE_SHADER:
         cbs->action_ok = action_ok_shader_pass_load;
         break;
      case MENU_FILE_IMAGE:
         cbs->action_ok = action_ok_menu_wallpaper_load;
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
         else if (!strcmp(menu_label, "core_updater_list"))
            cbs->action_ok = action_ok_core_download;
         else
            return;
         break;
      case MENU_FILE_DOWNLOAD_CORE:
         cbs->action_ok = action_ok_core_updater_download;
         break;
      case MENU_FILE_DOWNLOAD_CORE_INFO:
         break;
      case MENU_FILE_RDB:
         if (!strcmp(menu_label, "deferred_database_manager_list"))
            cbs->action_ok = action_ok_database_manager_list_deferred;
         else if (!strcmp(menu_label, "database_manager_list") 
               || !strcmp(menu_label, "Horizontal Menu"))
            cbs->action_ok = action_ok_database_manager_list;
         else
            return;
         break;
      case MENU_FILE_RDB_ENTRY:
         cbs->action_ok = action_ok_rdb_entry;
         break;
      case MENU_FILE_CURSOR:
         if (!strcmp(menu_label, "deferred_database_manager_list"))
            cbs->action_ok = action_ok_cursor_manager_list_deferred;
         else if (!strcmp(menu_label, "cursor_manager_list"))
            cbs->action_ok = action_ok_cursor_manager_list;
         break;
      case MENU_FILE_FONT:
      case MENU_FILE_OVERLAY:
      case MENU_FILE_AUDIOFILTER:
         cbs->action_ok = action_ok_set_path;
         break;
      case MENU_FILE_VIDEOFILTER:
         cbs->action_ok = action_ok_video_filter_file_load;
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
      case MENU_SETTINGS_CUSTOM_VIEWPORT:
         cbs->action_ok = action_ok_custom_viewport;
         break;
      case MENU_SETTINGS:
      case MENU_SETTING_GROUP:
      case MENU_SETTING_SUBGROUP:
         cbs->action_ok = action_ok_push_default;
         break;
      case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
         cbs->action_ok = action_ok_disk_cycle_tray_status;
         break;
      default:
         return;
   }
}
