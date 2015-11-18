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

#include <string/string_list.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include <compat/strl.h>

#include "../menu.h"
#include "../menu_cbs.h"
#include "../menu_hash.h"

#include "../../general.h"

#ifndef BIND_ACTION_GET_TITLE
#define BIND_ACTION_GET_TITLE(cbs, name) \
   cbs->action_get_title = name; \
   cbs->action_get_title_ident = #name;
#endif

static INLINE void replace_chars(char *str, char c1, char c2)
{
   char *pos = NULL;
   while((pos = strchr(str, c1)))
      *pos = c2;
}

static INLINE void sanitize_to_string(char *s, const char *label, size_t len)
{
   char new_label[PATH_MAX_LENGTH] = {0};

   strlcpy(new_label, label, sizeof(new_label));
   strlcpy(s, new_label, len);
   replace_chars(s, '_', ' ');
}

static int fill_title(char *s, const char *title, const char *path, size_t len)
{
   fill_pathname_join_delim(s, title, path, ' ', len);
   return 0;
}

static int action_get_title_disk_image_append(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Disk Append", path, len);
}

static int action_get_title_cheat_file_load(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Cheat File", path, len);
}

static int action_get_title_remap_file_load(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Remap File", path, len);
}

static int action_get_title_help(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, menu_hash_to_str(MENU_LABEL_VALUE_HELP_LIST), len);
   return 0;
}

static int action_get_title_overlay(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Overlay", path, len);
}

static int action_get_title_video_filter(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_FILTER), path, len);
}

static int action_get_title_cheat_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Cheat Dir", path, len);
}

static int action_get_title_core_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_LIBRETRO_DIR_PATH), path, len);
}

static int action_get_title_core_info_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_LIBRETRO_INFO_PATH), path, len);
}

static int action_get_title_audio_filter(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_AUDIO_FILTER_DIR), path, len);
}

static int action_get_title_font_path(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Font", path, len);
}

static int action_get_title_video_shader_preset(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Shader Preset", path, len);
}

static int action_get_title_generic(char *s, size_t len, const char *path,
      const char *text)
{
   char elem0_path[PATH_MAX_LENGTH] = {0};
   struct string_list *list_path    = string_split(path, "|");

   if (list_path)
   {
      if (list_path->size > 0)
         strlcpy(elem0_path, list_path->elems[0].data, sizeof(elem0_path));
      string_list_free(list_path);
   }

   snprintf(s, len, "%s - %s", text,
         (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");

   return 0;
}

static int action_get_title_deferred_database_manager_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Selection");
}

static int action_get_title_deferred_cursor_manager_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List");
}

static int action_get_title_list_rdb_entry_developer(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Developer ");
}

static int action_get_title_list_rdb_entry_publisher(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Publisher ");
}

static int action_get_title_list_rdb_entry_origin(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Origin ");
}

static int action_get_title_list_rdb_entry_franchise(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Franchise ");
}

static int action_get_title_list_rdb_entry_edge_magazine_rating(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Edge Magazine Rating ");
}

static int action_get_title_list_rdb_entry_edge_magazine_issue(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Edge Magazine Issue ");
}

static int action_get_title_list_rdb_entry_releasedate_by_month(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Releasedate By Month ");
}

static int action_get_title_list_rdb_entry_releasedate_by_year(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Releasedate By Year ");
}

static int action_get_title_list_rdb_entry_esrb_rating(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: ESRB Rating ");
}

static int action_get_title_list_rdb_entry_database_info(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Info ");
}

static int action_get_title_list_rdb_entry_elspa_rating(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Databsae Cursor List - Filter: ELSPA Rating ");
}

static int action_get_title_list_rdb_entry_pegi_rating(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: PEGI Rating ");
}

static int action_get_title_list_rdb_entry_cero_rating(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: CERO Rating ");
}

static int action_get_title_list_rdb_entry_bbfc_rating(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: BBFC Rating ");
}

static int action_get_title_list_rdb_entry_max_users(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return action_get_title_generic(s, len, path, "Database Cursor List - Filter: Max Users ");
}

static int action_get_title_deferred_core_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Supported Cores", path, len);
}

static int action_get_title_default(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   snprintf(s, len, "Select File %s", path);
   return 0;
}

static int action_get_title_group_settings(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   char elem0[PATH_MAX_LENGTH]    = {0};
   char elem1[PATH_MAX_LENGTH]    = {0};
   struct string_list *list_label = string_split(label, "|");

   if (list_label)
   {
      if (list_label->size > 0)
      {
         strlcpy(elem0, list_label->elems[0].data, sizeof(elem0));
         if (list_label->size > 1)
            strlcpy(elem1, list_label->elems[1].data, sizeof(elem1));
      }
      string_list_free(list_label);
   }

   strlcpy(s, elem0, len);

   if (elem1[0] != '\0')
   {
      strlcat(s, " - ", len);
      strlcat(s, elem1, len);
   }

   return 0;
}

static int action_get_user_accounts_cheevos_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS), len);
   return 0;
}

static int action_get_download_core_content_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_DOWNLOAD_CORE_CONTENT), len);
   return 0;
}

static int action_get_user_accounts_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_ACCOUNTS_LIST), len);
   return 0;
}

static int action_get_core_information_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INFORMATION), len);
   return 0;
}

static int action_get_core_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CORE_LIST), len);
   return 0;
}

static int action_get_online_updater_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_ONLINE_UPDATER), len);
   return 0;
}

static int action_get_core_updater_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CORE_UPDATER_LIST), len);
   return 0;
}

static int action_get_add_content_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_ADD_CONTENT_LIST), len);
   return 0;
}

static int action_get_core_options_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CORE_OPTIONS), len);
   return 0;
}

static int action_get_load_recent_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY), len);
   return 0;
}

static int action_get_quick_menu_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CONTENT_SETTINGS), len);
   return 0;
}

static int action_get_input_remapping_options_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS), len);
   return 0;
}

static int action_get_shader_options_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_SHADER_OPTIONS), len);
   return 0;
}

static int action_get_disk_options_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_DISK_OPTIONS), len);
   return 0;
}

static int action_get_frontend_counters_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_FRONTEND_COUNTERS), len);
   return 0;
}

static int action_get_core_counters_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CORE_COUNTERS), len);
   return 0;
}

static int action_get_playlist_settings_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_PLAYLIST_SETTINGS), len);
   return 0;
}

static int action_get_input_hotkey_binds_settings_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_INPUT_HOTKEY_BINDS), len);
   return 0;
}

static int action_get_input_settings_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_INPUT_SETTINGS), len);
   return 0;
}

static int action_get_core_cheat_options_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS), len);
   return 0;
}

static int action_get_load_content_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT_LIST), len);
   return 0;
}

static int action_get_cursor_manager_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_CURSOR_MANAGER), len);
   return 0;
}

static int action_get_database_manager_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_DATABASE_MANAGER), len);
   return 0;
}

static int action_get_system_information_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_SYSTEM_INFORMATION), len);
   return 0;
}

static int action_get_settings_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_SETTINGS), len);
   return 0;
}

static int action_get_title_information_list(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, menu_hash_to_str(MENU_LABEL_VALUE_INFORMATION_LIST), len);
   return 0;
}

static int action_get_title_action_generic(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   sanitize_to_string(s, label, len);
   return 0;
}

static int action_get_title_configurations(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Config", path, len);
}

static int action_get_title_content_database_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Database Dir", path, len);
}

static int action_get_title_savestate_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Savestate Dir", path, len);
}

static int action_get_title_dynamic_wallpapers_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Dynamic Wallpapers Dir", path, len);
}

static int action_get_title_core_assets_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Core Assets Dir", path, len);
}

static int action_get_title_config_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Config Dir", path, len);
}

static int action_get_title_input_remapping_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Input Remapping Dir", path, len);
}

static int action_get_title_autoconfig_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Autoconfig Dir", path, len);
}

static int action_get_title_playlist_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Playlist Dir", path, len);
}

static int action_get_title_browser_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Browser Dir", path, len);
}

static int action_get_title_content_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Content Dir", path, len);
}

static int action_get_title_screenshot_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Screenshot Dir", path, len);
}

static int action_get_title_cursor_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Cursor Dir", path, len);
}

static int action_get_title_onscreen_overlay_keyboard_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "OSK Overlay Dir", path, len);
}

static int action_get_title_recording_config_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Recording Config Dir", path, len);
}

static int action_get_title_recording_output_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Recording Output Dir", path, len);
}

static int action_get_title_video_shader_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SHADER_DIR), path, len);
}

static int action_get_title_audio_filter_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_AUDIO_FILTER_DIR), path, len);
}

static int action_get_title_video_filter_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_FILTER_DIR), path, len);
}

static int action_get_title_savefile_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_SAVEFILE_DIRECTORY), path, len);
}

static int action_get_title_overlay_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_OVERLAY_DIRECTORY), path, len);
}

static int action_get_title_system_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "System Dir", path, len);
}

static int action_get_title_assets_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_ASSETS_DIRECTORY), path, len);
}

static int action_get_title_extraction_directory(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, menu_hash_to_str(MENU_LABEL_VALUE_CACHE_DIRECTORY), path, len);
}

static int action_get_title_menu(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   return fill_title(s, "Menu", path, len);
}

static int action_get_title_input_settings(const char *path, const char *label, 
      unsigned menu_type, char *s, size_t len)
{
   strlcpy(s, "Input Settings", len);
   return 0;
}

static int menu_cbs_init_bind_title_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash, const char *elem1)
{
   if (cbs->setting)
   {
      const char *parent_group   = menu_setting_get_parent_group(cbs->setting);
      uint32_t parent_group_hash = menu_hash_calculate(parent_group);

      if ((parent_group_hash == MENU_VALUE_MAIN_MENU) && menu_setting_get_type(cbs->setting) == ST_GROUP)
      {
         BIND_ACTION_GET_TITLE(cbs, action_get_title_group_settings);
         return 0;
      }
   }

   switch (label_hash)
   {
      case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_database_manager_list);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_cursor_manager_list);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_developer);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_publisher);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_origin);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_franchise);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_edge_magazine_rating);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_edge_magazine_issue);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_releasedate_by_month);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_releasedate_by_year);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_esrb_rating);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_elspa_rating);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_pegi_rating);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_cero_rating);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_bbfc_rating);
         break;
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_max_users);
         break;
      case MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_list_rdb_entry_database_info);
         break;
      case MENU_LABEL_DEFERRED_CORE_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_deferred_core_list);
         break;
      case MENU_LABEL_CONFIGURATIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_configurations);
         break;
      case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_autoconfig_directory);
         break;
      case MENU_LABEL_CACHE_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_extraction_directory);
         break;
      case MENU_LABEL_SYSTEM_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_system_directory);
         break;
      case MENU_LABEL_ASSETS_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_assets_directory);
         break;
      case MENU_LABEL_SAVEFILE_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_savefile_directory);
         break;
      case MENU_LABEL_OVERLAY_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_overlay_directory);
         break;
      case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_browser_directory);
         break;
      case MENU_LABEL_PLAYLIST_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_playlist_directory);
         break;
      case MENU_LABEL_CONTENT_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_content_directory);
         break;
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_screenshot_directory);
         break;
      case MENU_LABEL_VIDEO_SHADER_DIR:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_directory);
         break;
      case MENU_LABEL_VIDEO_FILTER_DIR:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_video_filter_directory);
         break;
      case MENU_LABEL_AUDIO_FILTER_DIR:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_audio_filter_directory);
         break;
      case MENU_LABEL_CURSOR_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_cursor_directory);
         break;
      case MENU_LABEL_RECORDING_CONFIG_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_recording_config_directory);
         break;
      case MENU_LABEL_RECORDING_OUTPUT_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_recording_output_directory);
         break;
      case MENU_LABEL_OSK_OVERLAY_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_onscreen_overlay_keyboard_directory);
         break;
      case MENU_LABEL_INPUT_REMAPPING_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_input_remapping_directory);
         break;
      case MENU_LABEL_CONTENT_DATABASE_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_content_database_directory);
         break;
      case MENU_LABEL_SAVESTATE_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_savestate_directory);
         break;
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_dynamic_wallpapers_directory);
         break;
      case MENU_LABEL_CORE_ASSETS_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_core_assets_directory);
         break;
      case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_config_directory);
         break;
      case MENU_LABEL_INFORMATION_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_information_list);
         break;
      case MENU_LABEL_SETTINGS:
         BIND_ACTION_GET_TITLE(cbs, action_get_settings_list);
         break;
      case MENU_LABEL_DATABASE_MANAGER_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_database_manager_list);
         break;
      case MENU_LABEL_SYSTEM_INFORMATION:
         BIND_ACTION_GET_TITLE(cbs, action_get_system_information_list);
         break;
      case MENU_LABEL_CURSOR_MANAGER_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_cursor_manager_list);
         break;
      case MENU_LABEL_CORE_INFORMATION:
         BIND_ACTION_GET_TITLE(cbs, action_get_core_information_list);
         break;
      case MENU_LABEL_CORE_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_core_list);
         break;
      case MENU_LABEL_LOAD_CONTENT_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_load_content_list);
         break;
      case MENU_LABEL_ONLINE_UPDATER:
         BIND_ACTION_GET_TITLE(cbs, action_get_online_updater_list);
         break;
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_core_updater_list);
         break;
      case MENU_LABEL_ADD_CONTENT_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_add_content_list);
         break;
      case MENU_LABEL_CORE_OPTIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_core_options_list);
         break;
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         BIND_ACTION_GET_TITLE(cbs, action_get_load_recent_list);
         break;
      case MENU_LABEL_CONTENT_SETTINGS:
         BIND_ACTION_GET_TITLE(cbs, action_get_quick_menu_list);
         break;
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_input_remapping_options_list);
         break;
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_core_cheat_options_list);
         break;
      case MENU_LABEL_SHADER_OPTIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_shader_options_list);
         break;
      case MENU_LABEL_DISK_OPTIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_disk_options_list);
         break;
      case MENU_LABEL_FRONTEND_COUNTERS:
         BIND_ACTION_GET_TITLE(cbs, action_get_frontend_counters_list);
         break;
      case MENU_LABEL_CORE_COUNTERS:
         BIND_ACTION_GET_TITLE(cbs, action_get_core_counters_list);
         break;
      case MENU_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_input_hotkey_binds_settings_list);
         break;
      case MENU_LABEL_DEFERRED_INPUT_SETTINGS_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_input_settings_list);
         break;
      case MENU_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_playlist_settings_list);
         break;
      case MENU_LABEL_MANAGEMENT:
      case MENU_LABEL_DEBUG_INFORMATION:
      case MENU_LABEL_ACHIEVEMENT_LIST:
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_action_generic);
         break;
      case MENU_LABEL_DISK_IMAGE_APPEND:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_disk_image_append);
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_video_shader_preset);
         break;
      case MENU_LABEL_CHEAT_FILE_LOAD:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_file_load);
         break;
      case MENU_LABEL_REMAP_FILE_LOAD:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_remap_file_load);
         break;
      case MENU_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_cheevos_list);
         break;
      case MENU_LABEL_DOWNLOAD_CORE_CONTENT:
         BIND_ACTION_GET_TITLE(cbs, action_get_download_core_content_list);
         break;
      case MENU_LABEL_DEFERRED_ACCOUNTS_LIST:
         BIND_ACTION_GET_TITLE(cbs, action_get_user_accounts_list);
         break;
      case MENU_LABEL_HELP_LIST:
      case MENU_LABEL_HELP:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_help);
         break;
      case MENU_LABEL_INPUT_OVERLAY:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_overlay);
         break;
      case MENU_LABEL_VIDEO_FONT_PATH:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_font_path);
         break;
      case MENU_LABEL_VIDEO_FILTER:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_video_filter);
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_audio_filter);
         break;
      case MENU_LABEL_CHEAT_DATABASE_PATH:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_cheat_directory);
         break;
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_core_directory);
         break;
      case MENU_LABEL_LIBRETRO_INFO_PATH:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_core_info_directory);
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_title_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type)
{
   switch (type)
   {
      case MENU_SETTINGS:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_menu);
         break;
      case MENU_SETTINGS_CUSTOM_BIND:
      case MENU_SETTINGS_CUSTOM_BIND_KEYBOARD:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_input_settings);
         break;
      case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
         BIND_ACTION_GET_TITLE(cbs, action_get_title_action_generic);
         break;
      default:
         return -1;
   }

   return 0;
}

int menu_cbs_init_bind_title(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_GET_TITLE(cbs, action_get_title_default);

   if (menu_cbs_init_bind_title_compare_label(cbs, label, label_hash, elem1) == 0)
      return 0;

   if (menu_cbs_init_bind_title_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
