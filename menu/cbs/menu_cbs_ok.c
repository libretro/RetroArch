/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <retro_assert.h>
#include <retro_stat.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <lists/string_list.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_display.h"
#include "../menu_setting.h"
#include "../menu_shader.h"
#include "../menu_navigation.h"
#include "../menu_hash.h"
#include "../menu_content.h"

#include "../../core_info.h"
#include "../../frontend/frontend_driver.h"
#include "../../defaults.h"
#include "../../cheats.h"
#include "../../general.h"
#include "../../tasks/tasks_internal.h"
#include "../../input/input_remapping.h"
#include "../../retroarch.h"
#include "../../system.h"
#include "../../lakka.h"

enum
{
   ACTION_OK_FFMPEG = 0,
   ACTION_OK_IMAGEVIEWER
};

typedef struct
{
   uint32_t type_hash;
   char path[PATH_MAX_LENGTH];
} menu_file_transfer_t;

#ifndef BIND_ACTION_OK
#define BIND_ACTION_OK(cbs, name) \
   cbs->action_ok = name; \
   cbs->action_ok_ident = #name;
#endif

/* FIXME - Global variables, refactor */
char detect_content_path[PATH_MAX_LENGTH];
unsigned rdb_entry_start_game_selection_ptr, rpl_entry_selection_ptr;
size_t hack_shader_pass = 0;

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
char *core_buf;
size_t core_len;

/* defined in menu_cbs_deferred_push */
static void cb_net_generic(void *task_data, void *user_data, const char *err)
{
   bool refresh = false;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;

   if (core_buf)
      free(core_buf);

   core_buf = NULL;
   core_len = 0;

   if (!data || err)
      goto finish;

   core_buf = (char*)malloc((data->len+1) * sizeof(char));

   if (!core_buf)
      goto finish;

   memcpy(core_buf, data->data, data->len * sizeof(char));
   core_buf[data->len] = '\0';
   core_len      = data->len;

finish:
   refresh = true;
   menu_entries_ctl(MENU_ENTRIES_CTL_UNSET_REFRESH, &refresh);

   if (err)
      RARCH_ERR("Download failed: %s\n", err);

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }
}
#endif

int generic_action_ok_displaylist_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   char tmp[PATH_MAX_LENGTH];
   char action_path[PATH_MAX_LENGTH];
   enum menu_displaylist_ctl_state dl_type = DISPLAYLIST_GENERIC;
   menu_displaylist_info_t      info = {0};
   const char           *menu_label  = NULL;
   const char            *menu_path  = NULL;
   const char          *content_path = NULL;
   const char          *info_label   = NULL;
   const char          *info_path    = NULL;
   menu_handle_t            *menu    = NULL;
   global_t                 *global  = global_get_ptr();
   settings_t            *settings   = config_get_ptr();
   file_list_t        *selection_buf = menu_entries_get_selection_buf_ptr(0);
   file_list_t           *menu_stack = menu_entries_get_menu_stack_ptr(0);

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, NULL);

   if (path && menu_path)
      fill_pathname_join(action_path, menu_path, path, sizeof(action_path));

   info.list          = menu_stack;

   switch (action_type)
   {
      case ACTION_OK_DL_USER_BINDS_LIST:
         info.type          = type;
         info.directory_ptr = idx; 
         info_path          = label;
         info_label         = menu_hash_to_str(
               MENU_LABEL_DEFERRED_USER_BINDS_LIST);
         break;
      case ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE:
      case ACTION_OK_DL_OPEN_ARCHIVE:
         if (menu)
         {
            menu_path    = menu->scratch2_buf;
            content_path = menu->scratch_buf;
         }
         fill_pathname_join(detect_content_path, menu_path, content_path,
               sizeof(detect_content_path));

         switch (action_type)
         {
            case ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE:
               info_label = menu_hash_to_str(
                     MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE);
               break;
            case ACTION_OK_DL_OPEN_ARCHIVE:
               info_label = menu_hash_to_str(
                     MENU_LABEL_DEFERRED_ARCHIVE_OPEN);
               break;
         }
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         break;
      case ACTION_OK_DL_HELP:
         info_label = label;
         menu->push_help_screen = true;
         dl_type                = DISPLAYLIST_HELP;
         break;
      case ACTION_OK_DL_RPL_ENTRY:
         strlcpy(menu->deferred_path, label, sizeof(menu->deferred_path));
         info_label = menu_hash_to_str(MENU_LABEL_DEFERRED_RPL_ENTRY_ACTIONS);
         info.directory_ptr      = idx;
         rpl_entry_selection_ptr = idx;
         break;
      case ACTION_OK_DL_AUDIO_DSP_PLUGIN:
         info.directory_ptr = idx;
         info_path          = settings->directory.audio_filter;
         info_label         = menu_hash_to_str(MENU_LABEL_AUDIO_DSP_PLUGIN);
         break;
      case ACTION_OK_DL_SHADER_PASS:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.video_shader;
         info_label         = label;
         break;
      case ACTION_OK_DL_SHADER_PARAMETERS:
         info.type          = MENU_SETTING_ACTION;
         info.directory_ptr = idx;
         info_label = label;
         break;
      case ACTION_OK_DL_GENERIC:
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label = label;
         break;
      case ACTION_OK_DL_PUSH_DEFAULT:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label; 
         info_label = label;
         break;
      case ACTION_OK_DL_SHADER_PRESET:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.video_shader; 
         info_label = label;
         break;
      case ACTION_OK_DL_DOWNLOADS_DIR:
         info.type          = MENU_FILE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = settings->directory.core_assets;
         info_label         = label;
         break;
      case ACTION_OK_DL_CONTENT_LIST:
         info.type          = MENU_FILE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = settings->directory.menu_content;
         info_label         = label;
         break;
      case ACTION_OK_DL_REMAP_FILE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.input_remapping;
         info_label         = label;
         break;
      case ACTION_OK_DL_RECORD_CONFIGFILE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = global->record.config_dir;
         info_label         = label;
         break;
      case ACTION_OK_DL_DISK_IMAGE_APPEND_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.menu_content;
         info_label         = label;
         break;
      case ACTION_OK_DL_PLAYLIST_COLLECTION:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = label;
         break;
      case ACTION_OK_DL_CHEAT_FILE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->path.cheat_database;
         info_label         = label;
         break;
      case ACTION_OK_DL_CORE_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.libretro;
         info_label         = label;
         break;
      case ACTION_OK_DL_CONTENT_COLLECTION_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.playlist;
         info_label         = label;
         break;
      case ACTION_OK_DL_RDB_ENTRY:
         fill_pathname_join_delim(tmp,
               menu_hash_to_str(MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL),
               path, '|', sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = label;
         info_label         = tmp;
         break;
      case ACTION_OK_DL_RDB_ENTRY_SUBMENU:
         info.directory_ptr = idx;
         info_label         = label;
         info_path          = path;
         break;
      case ACTION_OK_DL_CONFIGURATIONS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         if (string_is_empty(settings->directory.menu_config))
            info_path        = label;
         else
            info_path        = settings->directory.menu_config;
         info_label = label;
         break;
      case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE:
      case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         switch (action_type)
         {
            case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE:
               info_label = menu_hash_to_str(
                     MENU_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE);
               break;
            case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH:
               info_label = menu_hash_to_str(
                     MENU_LABEL_DEFERRED_ARCHIVE_ACTION);
               break;
         }

         strlcpy(menu->scratch_buf, path, sizeof(menu->scratch_buf));
         strlcpy(menu->scratch2_buf, menu_path, sizeof(menu->scratch2_buf));
         break;
      case ACTION_OK_DL_PARENT_DIRECTORY_PUSH:
         {
            char parent_dir[PATH_MAX_LENGTH];

            fill_pathname_parent_dir(parent_dir,
                  action_path, sizeof(parent_dir));
            fill_pathname_parent_dir(parent_dir,
                  parent_dir, sizeof(parent_dir));

            info.type          = type;
            info.directory_ptr = idx;
            info_path          = parent_dir;
            info_label         = menu_label;
         }
         break;
      case ACTION_OK_DL_DIRECTORY_PUSH:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = action_path;
         info_label         = menu_label;
         break;
      case ACTION_OK_DL_DATABASE_MANAGER_LIST:
         fill_pathname_join(tmp,
               settings->path.content_database,
               path, sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = menu_hash_to_str(
               MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST);
         break;
      case ACTION_OK_DL_CURSOR_MANAGER_LIST:
         fill_pathname_join(tmp, settings->directory.cursor,
               path, sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = menu_hash_to_str(
               MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST);
         break;
      case ACTION_OK_DL_CORE_UPDATER_LIST:
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = menu_hash_to_str(
               MENU_LABEL_DEFERRED_CORE_UPDATER_LIST);
         break;
      case ACTION_OK_DL_THUMBNAILS_UPDATER_LIST:
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = menu_hash_to_str(
               MENU_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST);
         break;
      case ACTION_OK_DL_CORE_CONTENT_LIST:
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = menu_hash_to_str(
               MENU_LABEL_DEFERRED_CORE_CONTENT_LIST);
         break;
      case ACTION_OK_DL_LAKKA_LIST:
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = menu_hash_to_str(MENU_LABEL_DEFERRED_LAKKA_LIST);
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST:
         info.directory_ptr = idx;
         info_path          = settings->directory.libretro;
         info_label         = menu_hash_to_str(MENU_LABEL_DEFERRED_CORE_LIST);
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST_SET:
         info.directory_ptr                 = idx;
         rdb_entry_start_game_selection_ptr = idx;
         info_path                          = settings->directory.libretro;
         info_label                         = menu_hash_to_str(
               MENU_LABEL_DEFERRED_CORE_LIST_SET);
         break;
      case ACTION_OK_DL_ACCOUNTS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = menu_hash_to_str(MENU_LABEL_DEFERRED_ACCOUNTS_LIST);
         break;
      case ACTION_OK_DL_INPUT_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = menu_hash_to_str(MENU_LABEL_DEFERRED_INPUT_SETTINGS_LIST);
         break;
      case ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = menu_hash_to_str(MENU_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST);
         break;
      case ACTION_OK_DL_PLAYLIST_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = menu_hash_to_str(MENU_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST);
         break;
      case ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = menu_hash_to_str(MENU_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST);
         break;
      case ACTION_OK_DL_CONTENT_SETTINGS:
         dl_type            = DISPLAYLIST_CONTENT_SETTINGS;
         info.list          = selection_buf;
         info_path          = menu_hash_to_str(MENU_LABEL_VALUE_CONTENT_SETTINGS);
         info_label         = menu_hash_to_str(MENU_LABEL_CONTENT_SETTINGS);
         menu_entries_add(menu_stack, info_path, info_label, 0, 0, 0);
         break;
   }

   if (info_label)
      strlcpy(info.label, info_label, sizeof(info.label));
   if (info_path)
      strlcpy(info.path, info_path, sizeof(info.path));

   if (menu_displaylist_ctl(dl_type, &info))
      if (menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info))
         return 0;

   return menu_cbs_exit();
}

static int file_load_with_detect_core_wrapper(size_t idx, size_t entry_idx,
      const char *path, const char *label,
      uint32_t hash_label,
      unsigned type, bool is_carchive)
{
   menu_content_ctx_defer_info_t def_info;
   char menu_path_new[PATH_MAX_LENGTH];
   int ret                  = 0;
   const char *menu_path    = NULL;
   const char *menu_label   = NULL;
   menu_handle_t *menu      = NULL;
   core_info_list_t *list   = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, NULL);

   if (!string_is_empty(menu_path))
      strlcpy(menu_path_new, menu_path, sizeof(menu_path_new));

   if (string_is_equal(menu_label,
            menu_hash_to_str(MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)))
      fill_pathname_join(menu_path_new, menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));
   else if (string_is_equal(menu_label,
            menu_hash_to_str(MENU_LABEL_DEFERRED_ARCHIVE_OPEN)))
      fill_pathname_join(menu_path_new, menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));

   core_info_ctl(CORE_INFO_CTL_LIST_GET, &list);

   def_info.data       = list;
   def_info.dir        = menu_path_new;
   def_info.path       = path;
   def_info.menu_label = menu_label;
   def_info.s          = menu->deferred_path;
   def_info.len        = sizeof(menu->deferred_path);

   if (menu_content_ctl(MENU_CONTENT_CTL_FIND_FIRST_CORE, &def_info))
      ret = -1;

   if (     !is_carchive && !string_is_empty(path) 
         && !string_is_empty(menu_path_new))
      fill_pathname_join(detect_content_path, menu_path_new, path,
            sizeof(detect_content_path));

   if (hash_label == MENU_LABEL_COLLECTION)
      return generic_action_ok_displaylist_push(path,
            NULL, 0, idx, entry_idx, ACTION_OK_DL_DEFERRED_CORE_LIST_SET);

   switch (ret)
   {
      case -1:
         event_cmd_ctl(EVENT_CMD_LOAD_CORE, NULL);

         rarch_task_push_content_load_default(NULL, NULL,
                  false, CORE_TYPE_PLAIN, NULL, NULL);
         return 0;
      case 0:
         return generic_action_ok_displaylist_push(path, label, type,
               idx, entry_idx, ACTION_OK_DL_DEFERRED_CORE_LIST);
      default:
         break;
   }

   return ret;
}

static int action_ok_file_load_with_detect_core_carchive(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   uint32_t hash_label      = menu_hash_calculate(label);

   fill_pathname_join_delim(detect_content_path, detect_content_path, path,
         '#', sizeof(detect_content_path));

   type = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(idx, entry_idx,
         path, label, hash_label, type, true);
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   uint32_t hash_label      = menu_hash_calculate(label);

   type  = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(idx, entry_idx,
         path, label, hash_label, type, false);
}


static int action_ok_file_load_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   rarch_task_push_content_load_default(path, detect_content_path,
         false, CORE_TYPE_PLAIN, NULL, NULL);

   return 0;
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t selection;
   menu_content_ctx_playlist_info_t playlist_info;
   uint32_t core_name_hash, core_path_hash;
   size_t selection_ptr             = 0;
   content_playlist_t *playlist     = g_defaults.history;
   bool is_history                  = true;
   const char *entry_path           = NULL;
   const char *entry_label          = NULL;
   const char *core_path            = NULL;
   const char *core_name            = NULL;
   content_playlist_t *tmp_playlist = NULL;
   menu_handle_t *menu              = NULL;
   uint32_t hash_label              = menu_hash_calculate(label);

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return menu_cbs_exit();

   switch (hash_label)
   {
      case MENU_LABEL_COLLECTION:
      case MENU_LABEL_RDB_ENTRY_START_CONTENT:
         menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &tmp_playlist);

         if (!tmp_playlist)
         {
            tmp_playlist = content_playlist_init(
                  menu->db_playlist_file, COLLECTION_SIZE);

            if (!tmp_playlist)
               return menu_cbs_exit();
         }

         playlist   = tmp_playlist;
         is_history = false;
         break;
   }

   selection_ptr = entry_idx;

   switch (hash_label)
   {
      case MENU_LABEL_RDB_ENTRY_START_CONTENT:
         selection_ptr = rdb_entry_start_game_selection_ptr;
         break;
   }

   content_playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL); 

#if 0
   RARCH_LOG("path: %s, label: %s, core path: %s, core name: %s, idx: %d\n",
         entry_path, entry_label,
         core_path, core_name, selection_ptr);
   RARCH_LOG("playlist file: %s\n", menu->db_playlist_file);
#endif

   core_path_hash = core_path ? menu_hash_calculate(core_path) : 0;
   core_name_hash = core_name ? menu_hash_calculate(core_name) : 0;

   if (
         (core_path_hash == MENU_VALUE_DETECT) &&
         (core_name_hash == MENU_VALUE_DETECT)
      )
   {
      char new_core_path[PATH_MAX_LENGTH];
      char new_display_name[PATH_MAX_LENGTH];
      core_info_ctx_find_t core_info;
      const char *entry_path            = NULL;
      const char *entry_crc32           = NULL;
      const char *db_name               = NULL;
      const char             *path_base = path_basename(menu->db_playlist_file);
      bool        found_associated_core = menu_playlist_find_associated_core(
            path_base, new_core_path, sizeof(new_core_path));

      core_info.inf  = NULL;
      core_info.path = new_core_path;

      if (!core_info_ctl(CORE_INFO_CTL_FIND, &core_info))
         found_associated_core = false;

      if (!found_associated_core)
         return action_ok_file_load_with_detect_core(entry_path,
               label, type, selection_ptr, entry_idx);

      menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &tmp_playlist);

      content_playlist_get_index(tmp_playlist, selection_ptr,
            &entry_path, &entry_label, NULL, NULL, &entry_crc32, &db_name);

      strlcpy(new_display_name, core_info.inf->display_name, sizeof(new_display_name));
      content_playlist_update(tmp_playlist,
            selection_ptr,
            entry_path,
            entry_label,
            new_core_path,
            new_display_name,
            entry_crc32,
            db_name);
      content_playlist_write_file(tmp_playlist);
   }

   playlist_info.data = playlist;
   playlist_info.idx  = selection_ptr;

   menu_content_ctl(MENU_CONTENT_CTL_LOAD_PLAYLIST, &playlist_info);

   if (is_history)
   {
      switch (hash_label)
      {
         case MENU_LABEL_COLLECTION:
         case MENU_LABEL_RDB_ENTRY_START_CONTENT:
            menu_entries_pop_stack(&selection, 0, 1);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION,
                  &selection);
            break;
         default:
            menu_entries_flush_stack(NULL, MENU_SETTINGS);
            break;
      }

      generic_action_ok_displaylist_push("",
            "", 0, 0, 0, ACTION_OK_DL_CONTENT_SETTINGS);
   }

   return menu_cbs_exit();
}

static int action_ok_cheat_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   event_cmd_ctl(EVENT_CMD_CHEATS_APPLY, NULL);

   return 0;
}

enum
{
   ACTION_OK_LOAD_PRESET = 0,
   ACTION_OK_LOAD_SHADER_PASS,
   ACTION_OK_LOAD_RECORD_CONFIGFILE,
   ACTION_OK_LOAD_REMAPPING_FILE,
   ACTION_OK_LOAD_CHEAT_FILE,
   ACTION_OK_APPEND_DISK_IMAGE,
   ACTION_OK_LOAD_CONFIG_FILE,
   ACTION_OK_LOAD_CORE,
   ACTION_OK_LOAD_WALLPAPER,
   ACTION_OK_SET_PATH
};


static int generic_action_ok(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned id, unsigned flush_id)
{
   char action_path[PATH_MAX_LENGTH];
   unsigned flush_type               = 0;
   int ret                           = 0;
   const char             *menu_path = NULL;
   const char *flush_char            = NULL;
   struct video_shader      *shader  = NULL;
   menu_handle_t               *menu = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      goto error;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);

   menu_entries_get_last_stack(&menu_path, NULL,
         NULL, NULL);

   fill_pathname_join(action_path, menu_path, path, sizeof(action_path));
   flush_char = menu_hash_to_str(flush_id);

   switch (id)
   {
      case ACTION_OK_LOAD_WALLPAPER:
         flush_char = NULL;
         flush_type = 49;
         if (path_file_exists(action_path))
         {
            settings_t *settings = config_get_ptr();

            strlcpy(settings->path.menu_wallpaper,
                  action_path, sizeof(settings->path.menu_wallpaper));
            rarch_task_push_image_load(action_path, "cb_menu_wallpaper",
                  menu_display_handle_wallpaper_upload, NULL);
         }
         break;
      case ACTION_OK_LOAD_CORE:
         flush_char = NULL;
         flush_type = MENU_SETTINGS;
         runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, action_path);
         if (event_cmd_ctl(EVENT_CMD_LOAD_CORE, NULL))
         {
#ifndef HAVE_DYNAMIC
            if (frontend_driver_set_fork(FRONTEND_FORK_CORE))
               ret = -1;
#endif
         }
         break;
      case ACTION_OK_LOAD_CONFIG_FILE:
         {
            bool msg_force  = true;
            flush_char      = NULL;
            flush_type      = MENU_SETTINGS;
            menu_display_ctl(MENU_DISPLAY_CTL_SET_MSG_FORCE, &msg_force);

            if (rarch_ctl(RARCH_CTL_REPLACE_CONFIG, action_path))
            {
               bool pending_push = false;
               menu_navigation_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
               ret = -1;
            }
         }
         break;
#ifdef HAVE_SHADER_MANAGER
      case ACTION_OK_LOAD_PRESET:
         menu_shader_manager_set_preset(shader,
               video_shader_parse_type(action_path, RARCH_SHADER_NONE),
               action_path);
         break;
      case ACTION_OK_LOAD_SHADER_PASS:
         strlcpy(
               shader->pass[hack_shader_pass].source.path,
               action_path,
               sizeof(shader->pass[hack_shader_pass].source.path));
         video_shader_resolve_parameters(NULL, shader);
         break;
#endif
      case ACTION_OK_LOAD_RECORD_CONFIGFILE:
         {
            global_t *global = global_get_ptr();
            strlcpy(global->record.config, action_path,
                  sizeof(global->record.config));
         }
         break;
      case ACTION_OK_LOAD_REMAPPING_FILE:
         {
            config_file_t *conf = config_file_new(action_path);

            if (conf)
               input_remapping_load_file(conf, action_path);
         }
         break;
      case ACTION_OK_LOAD_CHEAT_FILE:
         cheat_manager_free();

         if (!cheat_manager_load(action_path))
            goto error;
         break;
      case ACTION_OK_APPEND_DISK_IMAGE:
         flush_char = NULL;
         flush_type = 49;
         event_cmd_ctl(EVENT_CMD_DISK_APPEND_IMAGE, action_path);
         event_cmd_ctl(EVENT_CMD_RESUME, NULL);
         break;
      case ACTION_OK_SET_PATH:
         flush_char = NULL;
         flush_type = 49;
         {
            menu_file_list_cbs_t *cbs = 
               menu_entries_get_last_stack_actiondata();

            if (cbs)
            {
               menu_setting_set_with_string_representation(
                     cbs->setting, action_path);
               ret = menu_setting_generic(cbs->setting, false);
            }
         }
         break;
      default:
         break;
   }

   menu_entries_flush_stack(flush_char, flush_type);

   return ret;

error:
   return menu_cbs_exit();
}

static int action_ok_set_path(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_SET_PATH, MENU_SETTINGS);
}

static int action_ok_menu_wallpaper_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{

   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_WALLPAPER, MENU_SETTINGS);
}

static int action_ok_core_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_CORE, MENU_SETTINGS);
}

static int action_ok_config_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_CONFIG_FILE, MENU_SETTINGS);
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_APPEND_DISK_IMAGE, MENU_SETTINGS);
}

static int action_ok_cheat_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_CHEAT_FILE, MENU_LABEL_CORE_CHEAT_OPTIONS);
}

static int action_ok_record_configfile_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_RECORD_CONFIGFILE, MENU_LABEL_RECORDING_SETTINGS);
}

static int action_ok_remap_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_REMAPPING_FILE,
         MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS);
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_PRESET, MENU_LABEL_SHADER_OPTIONS);
}

static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_SHADER_PASS, MENU_LABEL_SHADER_OPTIONS);
}

static int  generic_action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned id, enum menu_help_type id2)
{
   const char               *lbl  = menu_hash_to_str(id);
   menu_handle_t            *menu = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu);
   menu->help_screen_type = id2;

   return generic_action_ok_displaylist_push(path, lbl, type, idx,
         entry_idx, ACTION_OK_DL_HELP);
}

static int action_ok_cheevos(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu    = NULL;
   unsigned new_id        = type - MENU_SETTINGS_CHEEVOS_START;

   menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu);

   menu->help_screen_id   = new_id;
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_CHEEVOS_DESCRIPTION, MENU_HELP_CHEEVOS_DESCRIPTION);
}

static int action_ok_cheat(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;

   line.label         = "Input Cheat";
   line.label_setting = label;
   line.type          = type;
   line.idx           = idx;
   line.cb            = menu_input_st_cheat_cb;

   if (!menu_input_ctl(MENU_INPUT_CTL_START_LINE, &line))
      return -1;
   return 0;
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;

   line.label         = "Preset Filename";
   line.label_setting = label;
   line.type          = type;
   line.idx           = idx;
   line.cb            = menu_input_st_string_cb;

   if (!menu_input_ctl(MENU_INPUT_CTL_START_LINE, &line))
      return -1;
   return 0;
}

static int action_ok_cheat_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;

   line.label         = "Cheat Filename";
   line.label_setting = label;
   line.type          = type;
   line.idx           = idx;
   line.cb            = menu_input_st_string_cb;

   if (!menu_input_ctl(MENU_INPUT_CTL_START_LINE, &line))
      return -1;
   return 0;
}

enum
{
   ACTION_OK_REMAP_FILE_SAVE_CORE = 0,
   ACTION_OK_REMAP_FILE_SAVE_GAME
};

static int generic_action_ok_remap_file_save(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   char directory[PATH_MAX_LENGTH];
   char file[PATH_MAX_LENGTH];
   global_t *global                = global_get_ptr();
   settings_t *settings            = config_get_ptr();
   rarch_system_info_t *info       = NULL;
   const char *game_name           = NULL;
   const char *core_name           = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (info)
      core_name           = info->info.library_name;

   fill_pathname_join(
         directory,
         settings->directory.input_remapping,
         core_name,
         sizeof(directory));

   switch (action_type)
   {
      case ACTION_OK_REMAP_FILE_SAVE_CORE:
         fill_pathname_join(file, core_name, core_name, sizeof(file));
         break;
      case ACTION_OK_REMAP_FILE_SAVE_GAME:
         if (global)
            game_name           = path_basename(global->name.base);
         fill_pathname_join(file, core_name, game_name, sizeof(file));
         break;
   }

   if(!path_file_exists(directory))
       path_mkdir(directory);

   if(input_remapping_save_file(file))
      runloop_msg_queue_push("Remap file saved successfully",
            1, 100, true);
   else
      runloop_msg_queue_push("Error saving remap file",
            1, 100, true);

   return 0;
}

static int action_ok_remap_file_save_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_save(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_CORE);
}

static int action_ok_remap_file_save_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_save(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_GAME);
}

int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return menu_entry_pathdir_set_value(0, NULL);
}

#ifdef HAVE_LIBRETRODB
static int action_ok_scan_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_scan_file(path, label, type, idx);
}

static int action_ok_path_scan_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_scan_directory(NULL, label, type, idx);
}
#endif

static int action_ok_core_deferred_set(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t selection;
   char core_display_name[PATH_MAX_LENGTH];
   const char            *entry_path = NULL;
   const char           *entry_label = NULL;
   const char           *entry_crc32 = NULL;
   const char               *db_name = NULL;
   content_playlist_t *playlist      = NULL;

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return menu_cbs_exit();

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   retro_assert(playlist != NULL);

   core_info_get_name(path, core_display_name, sizeof(core_display_name));

   idx = rdb_entry_start_game_selection_ptr;

   content_playlist_get_index(playlist, idx,
         &entry_path, &entry_label, NULL, NULL, &entry_crc32, &db_name);

   content_playlist_update(playlist, idx,
         entry_path, entry_label,
         path , core_display_name,
         entry_crc32,
         db_name);

   content_playlist_write_file(playlist);

   menu_entries_pop_stack(&selection, 0, 1);
   menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);

   return menu_cbs_exit();
}

static int action_ok_core_load_deferred(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu      = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   rarch_task_push_content_load_default(path, menu->deferred_path,
            false, CORE_TYPE_PLAIN, NULL, NULL);

   return 0;
}

static int action_ok_deferred_list_stub(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}


static int generic_action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum rarch_core_type action_type, unsigned id)
{
   char new_path[PATH_MAX_LENGTH];
   const char *menu_path    = NULL;
   file_list_t *menu_stack  = menu_entries_get_menu_stack_ptr(0);

   menu_entries_get_last(menu_stack, &menu_path, NULL, NULL, NULL);

   fill_pathname_join(new_path, menu_path, path,
         sizeof(new_path));

   switch (id)
   {
      case ACTION_OK_FFMPEG:
      case ACTION_OK_IMAGEVIEWER:
         rarch_task_push_content_load_default(
               NULL, new_path, true, action_type, NULL, NULL);
         break;
      default:
         break;
   }

   return 0;
}

#ifdef HAVE_FFMPEG
static int action_ok_file_load_ffmpeg(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_file_load(path, label, type, idx,
         entry_idx, CORE_TYPE_FFMPEG, ACTION_OK_FFMPEG);
}
#endif

static int action_ok_file_load_imageviewer(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_file_load(path, label, type, idx,
         entry_idx, CORE_TYPE_IMAGEVIEWER, ACTION_OK_IMAGEVIEWER);
}

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char menu_path_new[PATH_MAX_LENGTH];
   char full_path_new[PATH_MAX_LENGTH];
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;
   menu_handle_t *menu      = NULL;
   file_list_t  *menu_stack = menu_entries_get_menu_stack_ptr(0);

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_entries_get_last(menu_stack, &menu_path, &menu_label, NULL, NULL);

   strlcpy(menu_path_new, menu_path, sizeof(menu_path_new));

   if (
         string_is_equal(menu_label,
            menu_hash_to_str(MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)) ||
         string_is_equal(menu_label,
            menu_hash_to_str(MENU_LABEL_DEFERRED_ARCHIVE_OPEN))
      )
   {
      fill_pathname_join(menu_path_new, menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));
   }

   setting = menu_setting_find(menu_label);

   if (menu_setting_get_type(setting) == ST_PATH)
      return action_ok_set_path(path, label, type, idx, entry_idx);

   if (type == MENU_FILE_IN_CARCHIVE)
      fill_pathname_join_delim(full_path_new, menu_path_new, path,
            '#',sizeof(full_path_new));
   else
      fill_pathname_join(full_path_new, menu_path_new, path,
            sizeof(full_path_new));

   rarch_task_push_content_load_default(NULL, full_path_new,
            true, CORE_TYPE_PLAIN, NULL, NULL);

   return 0;
}


static int generic_action_ok_command(enum event_command cmd)
{
   if (!event_cmd_ctl(cmd, NULL))
      return menu_cbs_exit();
   return 0;
}

static int action_ok_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(EVENT_CMD_LOAD_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(EVENT_CMD_RESUME);
 }


static int action_ok_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(EVENT_CMD_SAVE_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(EVENT_CMD_RESUME);
}

#ifdef HAVE_NETWORKING
static void cb_decompressed(void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;
   unsigned type_hash = (uintptr_t)user_data;

   if (dec && !err)
   {
      switch (type_hash)
      {
         case CB_CORE_UPDATER_DOWNLOAD:
            event_cmd_ctl(EVENT_CMD_CORE_INFO_INIT, NULL);
            break;
         case CB_UPDATE_ASSETS:
            event_cmd_ctl(EVENT_CMD_REINIT, NULL);
            break;
      }
   }

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      if (path_file_exists(dec->source_file))
         remove(dec->source_file);

      free(dec->source_file);
      free(dec);
   }
}

/* expects http_transfer_t*, menu_file_transfer_t* */
static void cb_generic_download(void *task_data,
      void *user_data, const char *err)
{
   char output_path[PATH_MAX_LENGTH];
   char shaderdir[PATH_MAX_LENGTH];
   const char             *file_ext      = NULL;
   const char             *dir_path      = NULL;
   menu_file_transfer_t     *transf      = (menu_file_transfer_t*)user_data;
   settings_t              *settings     = config_get_ptr();
   http_transfer_data_t        *data     = (http_transfer_data_t*)task_data;

   if (!data || !data->data | !transf)
      goto finish;

   /* we have to determine dir_path at the time of writting or else
    * we'd run into races when the user changes the setting during an
    * http transfer. */
   switch (transf->type_hash)
   {
      case CB_CORE_THUMBNAILS_DOWNLOAD:
         dir_path = settings->directory.thumbnails;
         break;
      case CB_CORE_UPDATER_DOWNLOAD:
         dir_path = settings->directory.libretro;
         break;
      case CB_CORE_CONTENT_DOWNLOAD:
         dir_path = settings->directory.core_assets;
         break;
      case CB_UPDATE_CORE_INFO_FILES:
         dir_path = settings->path.libretro_info;
         break;
      case CB_UPDATE_ASSETS:
         dir_path = settings->directory.assets;
         break;
      case CB_UPDATE_AUTOCONFIG_PROFILES:
         dir_path = settings->directory.autoconfig;
         break;
      case CB_UPDATE_DATABASES:
         dir_path = settings->path.content_database;
         break;
      case CB_UPDATE_OVERLAYS:
         dir_path = settings->directory.overlay;
         break;
      case CB_UPDATE_CHEATS:
         dir_path = settings->path.cheat_database;
         break;
      case CB_UPDATE_SHADERS_CG:
      case CB_UPDATE_SHADERS_GLSL:
      {
         const char *dirname = transf->type_hash == CB_UPDATE_SHADERS_CG ?
                  "shaders_cg" : "shaders_glsl";

         fill_pathname_join(shaderdir,
               settings->directory.video_shader,
               dirname,
               sizeof(shaderdir));
         if (!path_file_exists(shaderdir))
            if (!path_mkdir(shaderdir))
               goto finish;

         dir_path = shaderdir;
         break;
      }
      case CB_LAKKA_DOWNLOAD:
         dir_path = LAKKA_UPDATE_DIR;
         break;
      default:
         RARCH_WARN("Unknown transfer type '%u' bailing out.\n",
               transf->type_hash);
         break;
   }

   fill_pathname_join(output_path, dir_path,
         transf->path, sizeof(output_path));
   RARCH_LOG("output_path: %s\n", output_path);

   /* Make sure the directory exists */
   path_basedir(output_path);
   if (!path_mkdir(output_path))
   {
      err = "Failed to create the directory.";
      goto finish;
   }

   fill_pathname_join(output_path, dir_path,
         transf->path, sizeof(output_path));

#ifdef HAVE_ZLIB
   file_ext = path_get_extension(output_path);

   if (string_is_equal_noncase(file_ext, "zip"))
   {
      if (rarch_task_check_decompress(output_path))
      {
        err = "Decompression already in progress.";
        goto finish;
      }
   }
#endif

   if (!filestream_write_file(output_path, data->data, data->len))
   {
      err = "Write failed.";
      goto finish;
   }

#ifdef HAVE_ZLIB
   if (!settings->network.buildbot_auto_extract_archive)
      goto finish;

   if (string_is_equal_noncase(file_ext, "zip"))
   {
      if (!rarch_task_push_decompress(output_path, dir_path, NULL, NULL, NULL,
            cb_decompressed, (void*)(uintptr_t)transf->type_hash))
      {
        err = "Decompression failed.";
        goto finish;
      }
   }
#else
   switch (transf->type_hash)
   {
      case CB_CORE_UPDATER_DOWNLOAD:
         event_cmd_ctl(EVENT_CMD_CORE_INFO_INIT, NULL);
         break;
   }
#endif

finish:
   if (err)
   {
      RARCH_ERR("Download of '%s' failed: %s\n",
            (transf ? transf->path: "unknown"), err);
   }

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (transf)
      free(transf);
}
#endif

static int action_ok_download_generic(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      const char *type_msg)
{
#ifdef HAVE_NETWORKING
   char s[PATH_MAX_LENGTH];
   char s3[PATH_MAX_LENGTH];
   menu_file_transfer_t *transf = NULL;
   settings_t *settings         = config_get_ptr();

   fill_pathname_join(s, settings->network.buildbot_assets_url,
         "frontend", sizeof(s));
   if (string_is_equal(type_msg, "cb_core_content_download"))
      fill_pathname_join(s, settings->network.buildbot_assets_url,
            "cores/gw", sizeof(s));
#ifdef HAVE_LAKKA
   /* TODO unhardcode this path*/
   else if (string_is_equal(type_msg, "cb_lakka_download"))
      fill_pathname_join(s, "http://mirror.lakka.tv/nightly",
            LAKKA_PROJECT, sizeof(s));
#endif
   else if (string_is_equal(type_msg, "cb_update_assets"))
      path = "assets.zip";
   else if (string_is_equal(type_msg, "cb_update_autoconfig_profiles"))
      path = "autoconfig.zip";
   else if (string_is_equal(type_msg, "cb_update_core_info_files"))
      path = "info.zip";
   else if (string_is_equal(type_msg, "cb_update_cheats"))
      path = "cheats.zip";
   else if (string_is_equal(type_msg, "cb_update_overlays"))
      path = "overlays.zip";
   else if (string_is_equal(type_msg, "cb_update_databases"))
      path = "database-rdb.zip";
   else if (string_is_equal(type_msg, "cb_update_shaders_glsl"))
      path = "shaders_glsl.zip";
   else if (string_is_equal(type_msg, "cb_update_shaders_cg"))
      path = "shaders_cg.zip";
   else if (string_is_equal(type_msg, "cb_core_thumbnails_download"))
      strlcpy(s, "http://thumbnailpacks.libretro.com", sizeof(s));
   else
      strlcpy(s, settings->network.buildbot_url, sizeof(s));

   fill_pathname_join(s3, s, path, sizeof(s3));

   transf = (menu_file_transfer_t*)calloc(1, sizeof(*transf));
   transf->type_hash = menu_hash_calculate(type_msg);
   strlcpy(transf->path, path, sizeof(transf->path));

   rarch_task_push_http_transfer(s3, type_msg, cb_generic_download, 0, transf);
#endif
   return 0;
}

static int action_ok_core_content_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_core_content_download");
}

static int action_ok_core_content_thumbnails(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_core_thumbnails_download");
}

static int action_ok_thumbnails_updater_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_thumbnails_updater_download");
}

static int action_ok_core_updater_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_core_updater_download");
}

static int action_ok_lakka_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_lakka_download");
}

static int action_ok_update_assets(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_assets");
}

static int action_ok_update_core_info_files(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_core_info_files");
}

static int action_ok_update_overlays(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_overlays");
}

static int action_ok_update_shaders_cg(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_shaders_cg");
}

static int action_ok_update_shaders_glsl(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_shaders_glsl");
}

static int action_ok_update_databases(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_databases");
}

static int action_ok_update_cheats(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_cheats");
}

static int action_ok_update_autoconfig_profiles(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_autoconfig_profiles");
}

static int action_ok_update_autoconfig_profiles_hid(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, type, idx, entry_idx,
         "cb_update_autoconfig_profiles_hid");
}

static int action_ok_disk_cycle_tray_status(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_DISK_EJECT_TOGGLE);
}

/* creates folder and core options stub file for subsequent runs */
static int action_ok_option_create(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char game_path[PATH_MAX_LENGTH];
   config_file_t *conf                    = NULL;

   if (!rarch_game_options_validate(game_path, sizeof(game_path), true))
   {
      runloop_msg_queue_push("Error saving core options file",
            1, 100, true);
      return 0;
   }

   conf = config_file_new(game_path);

   if (!conf)
   {
      conf = config_file_new(NULL);
      if (!conf)
         return false;
   }

   if(config_file_write(conf, game_path))
   {
      global_t                 *global  = global_get_ptr();

      runloop_msg_queue_push("Core options file created successfully",
            1, 100, true);

      strlcpy(global->path.core_options_path,
            game_path, sizeof(global->path.core_options_path));
   }
   config_file_free(conf);

   return 0;
}


static int action_ok_close_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_UNLOAD_CORE);
}

static int action_ok_quit(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_QUIT);
}

static int action_ok_save_new_config(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_MENU_SAVE_CONFIG);
}

static int action_ok_resume_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_RESUME);
}

static int action_ok_restart_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_RESET);
}

static int action_ok_screenshot(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_TAKE_SCREENSHOT);
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_SHADERS_APPLY_CHANGES);
}

static int action_ok_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return menu_setting_set(type, label, MENU_ACTION_OK, false);
}


#ifdef HAVE_NETWORKING
enum
{
   ACTION_OK_NETWORK_CORE_CONTENT_LIST = 0,
   ACTION_OK_NETWORK_CORE_UPDATER_LIST,
   ACTION_OK_NETWORK_THUMBNAILS_UPDATER_LIST,
   ACTION_OK_NETWORK_LAKKA_LIST
};

static int generic_action_ok_network(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned type_id)
{
   char url_path[PATH_MAX_LENGTH];
   settings_t *settings           = config_get_ptr();
   unsigned type_id2              = 0;
   const char *url_label          = NULL;
   retro_task_callback_t callback = NULL;
   bool refresh                   = true;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   if (string_is_empty(settings->network.buildbot_url))
      return menu_cbs_exit();

   event_cmd_ctl(EVENT_CMD_NETWORK_INIT, NULL);

   switch (type_id)
   {
      case ACTION_OK_NETWORK_CORE_CONTENT_LIST:
         fill_pathname_join(url_path, settings->network.buildbot_assets_url,
               "cores/gw/.index", sizeof(url_path));
         url_label = "cb_core_content_list";
         type_id2 = ACTION_OK_DL_CORE_CONTENT_LIST;
         callback = cb_net_generic;
         break;
      case ACTION_OK_NETWORK_CORE_UPDATER_LIST:
         fill_pathname_join(url_path, settings->network.buildbot_url,
               ".index-extended", sizeof(url_path));
         url_label = "cb_core_updater_list";
         type_id2  = ACTION_OK_DL_CORE_UPDATER_LIST;
         callback = cb_net_generic;
         break;
      case ACTION_OK_NETWORK_THUMBNAILS_UPDATER_LIST:
         fill_pathname_join(url_path,
               "http://thumbnailpacks.libretro.com",
               ".index", sizeof(url_path));
         url_label = "cb_thumbnails_updater_list";
         type_id2  = ACTION_OK_DL_THUMBNAILS_UPDATER_LIST;
         callback = cb_net_generic;
         break;
#ifdef HAVE_LAKKA
      case ACTION_OK_NETWORK_LAKKA_LIST:
         /* TODO unhardcode this path */
         fill_pathname_join(url_path, "http://mirror.lakka.tv/nightly",
               LAKKA_PROJECT, sizeof(url_path));
         fill_pathname_join(url_path, url_path,
               ".index", sizeof(url_path));
         url_label = "cb_lakka_list";
         type_id2  = ACTION_OK_DL_LAKKA_LIST;
         callback = cb_net_generic;
         break;
#endif
   }

   rarch_task_push_http_transfer(url_path, url_label, callback, 0, NULL);

   return generic_action_ok_displaylist_push(path,
         label, type, idx, entry_idx, type_id2);
}

static int action_ok_core_content_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         ACTION_OK_NETWORK_CORE_CONTENT_LIST);
} 

static int action_ok_core_updater_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         ACTION_OK_NETWORK_CORE_UPDATER_LIST);
}

typedef struct
{
   size_t i, j, k;
   int count;
   char url[PATH_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
}
update_state_t;

static const char* thumbnail_folders[] =
{
   "Named_Snaps", "Named_Titles", "Named_Boxarts"
};

static void sanitize(char* dest, const char* source)
{
   const char* end = dest + PATH_MAX_LENGTH - 1;
   
   while (*source && dest < end)
   {
      const char* found = strchr("/<>&?|:*\\\"", *source);
      
      if (found || *source < 32 || *source == 127)
         *dest = '_';
      else
         *dest = *source;
      
      dest++, source++;
   }
   
   *dest = 0;
}

static bool get_next_entry(update_state_t* us)
{
   settings_t *settings = config_get_ptr();
   
   file_list_t *playlists;
   menu_displaylist_info_t info = {0};
   size_t list_size;
   
   menu_entry_t entry;
   char path_playlist[PATH_MAX_LENGTH];
   char folder_name[PATH_MAX_LENGTH];
   char *system_name;
   content_playlist_t *playlist;
   const char *entry_label;
   char file_name[PATH_MAX_LENGTH];
   
   playlists = (file_list_t*)calloc(1, sizeof(*playlists));
   
   if (!playlists)
      return false;
   
   info.list         = playlists;
   info.menu_list    = NULL;
   info.type         = 0;
   info.type_default = MENU_FILE_PLAIN;
   info.flags        = SL_FLAG_ALLOW_EMPTY_LIST;
   
   strlcpy(info.label,
         menu_hash_to_str(MENU_LABEL_CONTENT_COLLECTION_LIST),
         sizeof(info.label));
         
   strlcpy(info.path,
         settings->directory.playlist,
         sizeof(info.path));
         
   strlcpy(info.exts, "lpl", sizeof(info.exts));

   if (menu_displaylist_ctl(DISPLAYLIST_DATABASE_PLAYLISTS_HORIZONTAL, &info))
      menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);

   list_size = file_list_get_size(playlists);
   
again:
   if (us->i >= list_size)
   {
      free(playlists);
      return false;
   }
   
   menu_entry_get(&entry, 0, us->i, playlists, true);
   
   fill_pathname_join(path_playlist, settings->directory.playlist, entry.path,
         sizeof(path_playlist));

   strlcpy(folder_name, path_playlist, sizeof(folder_name));
   path_remove_extension(folder_name);
   system_name = find_last_slash(folder_name);
   
   if (!system_name++)
   {
      free(playlists);
      return false;
   }
   
   playlist = content_playlist_init(path_playlist, COLLECTION_SIZE);
   
   if (!playlist)
   {
      free(playlists);
      return false;
   }
   
   if (us->j >= content_playlist_size(playlist))
   {
      content_playlist_free(playlist);
      us->j = 0;
      us->i++;
      goto again;
   }
   
   content_playlist_get_index(playlist, us->j,
         NULL, &entry_label, NULL, NULL,
         NULL, NULL);
   
   sanitize(file_name, entry_label);

   snprintf(us->url, sizeof(us->url), "http://thumbnails.libretro.com/%s/%s/%s.png", 
      system_name, thumbnail_folders[ us->k ], file_name);

   snprintf(us->path, sizeof(us->path), "%s/%s/%s/%s.png", settings->directory.thumbnails,
      system_name, thumbnail_folders[ us->k ], file_name);
   
   if (++us->k >= sizeof(thumbnail_folders) / sizeof(thumbnail_folders[0]))
   {
      us->k = 0;
      us->j++;
   }
   
   content_playlist_free(playlist);
   free(playlists);
   return true;
}

static void cb_thumbnail_downloaded(void *task_data, void *user_data, const char *error)
{
   update_state_t *us = user_data;
   http_transfer_data_t *data = (http_transfer_data_t*)task_data;
   char output_path[PATH_MAX_LENGTH];
   
   if (!data || !data->data)
   {
      RARCH_ERR("Error during download of %s: %s\n", us->url, error);
      goto finished;
   }
   
   strncpy(output_path, us->path, sizeof(output_path));
   output_path[sizeof(output_path) - 1] = 0;
   path_basedir(output_path);
   
   if (!path_mkdir(output_path))
   {
      RARCH_ERR("Error creating folders for file %s\n", us->path);
      goto finished;
   }
   
   if (!filestream_write_file(us->path, data->data, data->len))
   {
      RARCH_ERR("Error writing file %s\n", us->path);
      goto finished;
   }
   
finished:
   if (get_next_entry(us))
   {
      snprintf(output_path, sizeof(output_path), "Updating thumbnail %d", ++us->count);
      runloop_msg_queue_push(output_path, 1, 60, true);
      rarch_task_push_http_transfer(us->url, "", cb_thumbnail_downloaded, 1, us);
   }
   else
      free(us);
}

static int action_ok_thumbnails_updater_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   update_state_t *us = malloc(sizeof(*us));
   us->i = us->j = us->k = 0;
   us->count = 0;
   
   if (get_next_entry(us))
   {
      runloop_msg_queue_push("Updating thumbnails", 1, 60, true);
      rarch_task_push_http_transfer(us->url, "", cb_thumbnail_downloaded, 1, us);
   }
   
   return 0;
}

static int action_ok_lakka_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         ACTION_OK_NETWORK_LAKKA_LIST);
}

#endif


static int action_ok_rdb_entry_submenu(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   union string_list_elem_attr attr;
   char new_label[PATH_MAX_LENGTH];
   char new_path[PATH_MAX_LENGTH];
   int ret = -1;
   char *rdb                       = NULL;
   int len                         = 0;
   struct string_list *str_list    = NULL;
   struct string_list *str_list2   = NULL;

   if (!label)
      return menu_cbs_exit();

   str_list = string_split(label, "|");

   if (!str_list)
      goto end;

   str_list2 = string_list_new();
   if (!str_list2)
      goto end;

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
      goto end;

   string_list_join_concat(rdb, len, str_list2, "|");
   strlcpy(new_path, rdb, sizeof(new_path));

   fill_pathname_join_delim(new_label, 
         menu_hash_to_str(MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST),
         str_list->elems[0].data, '_',
         sizeof(new_label));

   ret = generic_action_ok_displaylist_push(new_path,
         new_label, type, idx, entry_idx,
         ACTION_OK_DL_RDB_ENTRY_SUBMENU);

end:
   if (rdb)
      free(rdb);
   if (str_list)
      string_list_free(str_list);
   if (str_list2)
      string_list_free(str_list2);

   return ret;
}

#ifdef HAVE_SHADER_MANAGER
extern size_t hack_shader_pass;
#endif

static int action_ok_shader_pass(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   hack_shader_pass             = type - MENU_SETTINGS_SHADER_PASS_0;
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_SHADER_PASS);
}

static int action_ok_shader_parameters(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_SHADER_PARAMETERS);
}

int action_ok_parent_directory_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_PARENT_DIRECTORY_PUSH);
}

int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_DIRECTORY_PUSH);
}

static int action_ok_database_manager_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_DATABASE_MANAGER_LIST);
}

static int action_ok_cursor_manager_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_CURSOR_MANAGER_LIST);
}

static int action_ok_compressed_archive_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH);
}

static int action_ok_compressed_archive_push_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE);
}

static int action_ok_configurations_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_CONFIGURATIONS_LIST);
}

static int action_ok_rdb_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_RDB_ENTRY);
}

static int action_ok_content_collection_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_CONTENT_COLLECTION_LIST);
}

static int action_ok_core_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_CORE_LIST);
}

static int action_ok_cheat_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_CHEAT_FILE);
}

static int action_ok_playlist_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_PLAYLIST_COLLECTION);
}

static int action_ok_disk_image_append_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_DISK_IMAGE_APPEND_LIST);
}

static int action_ok_record_configfile(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_RECORD_CONFIGFILE);
}

static int action_ok_remap_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_REMAP_FILE);
}

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_CONTENT_LIST);
}

static int action_ok_push_downloads_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_DOWNLOADS_DIR);
}

static int action_ok_shader_preset(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_SHADER_PRESET);
}

int action_ok_push_generic_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_GENERIC);
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_PUSH_DEFAULT);
}

static int action_ok_audio_dsp_plugin(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_AUDIO_DSP_PLUGIN);
}

static int action_ok_rpl_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, type, idx,
         entry_idx, ACTION_OK_DL_RPL_ENTRY);
}

static int action_ok_start_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   /* No content needed for this core, load core immediately. */
   if (menu_driver_ctl(RARCH_MENU_CTL_HAS_LOAD_NO_CONTENT, NULL))
   {
      runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
      rarch_task_push_content_load_default(NULL, NULL,
               false, CORE_TYPE_PLAIN, NULL, NULL);
   }

   return 0;
}

static int action_ok_open_archive_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE);
}

static int action_ok_push_accounts_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_ACCOUNTS_LIST);
}

static int action_ok_push_input_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_INPUT_SETTINGS_LIST);
}

static int action_ok_push_playlist_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_PLAYLIST_SETTINGS_LIST);
}

static int action_ok_push_input_hotkey_binds_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST);
}

static int action_ok_push_user_binds_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_USER_BINDS_LIST);
}

static int action_ok_push_accounts_cheevos_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST);
}

static int action_ok_open_archive(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, label, 0, 0, entry_idx,
         ACTION_OK_DL_OPEN_ARCHIVE);
}

static int action_ok_load_archive(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu      = NULL;
   const char *menu_path    = NULL;
   const char *content_path = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_path    = menu->scratch2_buf;
   content_path = menu->scratch_buf;

   fill_pathname_join(detect_content_path, menu_path, content_path,
         sizeof(detect_content_path));

   event_cmd_ctl(EVENT_CMD_LOAD_CORE, NULL);
   rarch_task_push_content_load_default(
            NULL, detect_content_path, false, CORE_TYPE_PLAIN, NULL, NULL);

   return 0;
}

static int action_ok_load_archive_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_content_ctx_defer_info_t def_info;
   int ret                  = 0;
   core_info_list_t *list   = NULL;
   menu_handle_t *menu      = NULL;
   const char *menu_path    = NULL;
   const char *content_path = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &idx))
      return false;

   menu_path    = menu->scratch2_buf;
   content_path = menu->scratch_buf;

   core_info_ctl(CORE_INFO_CTL_LIST_GET, &list);

   def_info.data       = list;
   def_info.dir        = menu_path;
   def_info.path       = content_path;
   def_info.menu_label = label;
   def_info.s          = menu->deferred_path;
   def_info.len        = sizeof(menu->deferred_path);

   if (menu_content_ctl(MENU_CONTENT_CTL_FIND_FIRST_CORE, &def_info))
      ret = -1;

   fill_pathname_join(detect_content_path, menu_path, content_path,
         sizeof(detect_content_path));

   switch (ret)
   {
      case -1:
         event_cmd_ctl(EVENT_CMD_LOAD_CORE, NULL);
         rarch_task_push_content_load_default(NULL, NULL,
                  false, CORE_TYPE_PLAIN, NULL, NULL);
         return 0;
      case 0:
         return generic_action_ok_displaylist_push(path, label, type,
               idx, entry_idx, ACTION_OK_DL_DEFERRED_CORE_LIST);
      default:
         break;
   }

   return ret;
}

static int action_ok_help_audio_video_troubleshooting(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
         MENU_HELP_AUDIO_VIDEO_TROUBLESHOOTING);
}

static int action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_HELP, MENU_HELP_WELCOME);
}

static int action_ok_help_controls(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_HELP_CONTROLS, MENU_HELP_CONTROLS);
}

static int action_ok_help_what_is_a_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_HELP_WHAT_IS_A_CORE, MENU_HELP_WHAT_IS_A_CORE);
}

static int action_ok_help_scanning_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_HELP_SCANNING_CONTENT, MENU_HELP_SCANNING_CONTENT);
}

static int action_ok_help_change_virtual_gamepad(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD,
         MENU_HELP_CHANGE_VIRTUAL_GAMEPAD);
}

static int action_ok_help_load_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_LABEL_HELP_LOADING_CONTENT, MENU_HELP_LOADING_CONTENT);
}

static int action_ok_video_resolution(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned width   = 0;
   unsigned  height = 0;

   if (video_driver_get_video_output_size(&width, &height))
   {
      char msg[PATH_MAX_LENGTH] = {0};
#ifdef __CELLOS_LV2__
      event_cmd_ctl(EVENT_CMD_REINIT, NULL);
#endif
      video_driver_set_video_mode(width, height, true);
#ifdef GEKKO
      if (width == 0 || height == 0)
         strlcpy(msg, "Applying: DEFAULT", sizeof(msg));
      else
#endif
         snprintf(msg, sizeof(msg),"Applying: %dx%d\n START to reset",            
            width, height);
      runloop_msg_queue_push(msg, 1, 100, true);
   }

   return 0;
}

static int is_rdb_entry(uint32_t label_hash)
{
   switch (label_hash)
   {
      case MENU_LABEL_RDB_ENTRY_PUBLISHER:
      case MENU_LABEL_RDB_ENTRY_DEVELOPER:
      case MENU_LABEL_RDB_ENTRY_ORIGIN:
      case MENU_LABEL_RDB_ENTRY_FRANCHISE:
      case MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
      case MENU_LABEL_RDB_ENTRY_ESRB_RATING:
      case MENU_LABEL_RDB_ENTRY_BBFC_RATING:
      case MENU_LABEL_RDB_ENTRY_ELSPA_RATING:
      case MENU_LABEL_RDB_ENTRY_PEGI_RATING:
      case MENU_LABEL_RDB_ENTRY_CERO_RATING:
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
      case MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
      case MENU_LABEL_RDB_ENTRY_RELEASE_MONTH:
      case MENU_LABEL_RDB_ENTRY_RELEASE_YEAR:
      case MENU_LABEL_RDB_ENTRY_MAX_USERS:
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_ok_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t hash, const char *elem0)
{
   uint32_t elem0_hash      = menu_hash_calculate(elem0);

   if (!string_is_empty(elem0) && (is_rdb_entry(elem0_hash) == 0))
   {
      BIND_ACTION_OK(cbs, action_ok_rdb_entry_submenu);
      return 0;
   }

   if (strstr(elem0, "_input_binds_list"))
   {
      BIND_ACTION_OK(cbs, action_ok_push_user_binds_list);
      return 0;
   }

   if (menu_setting_get_browser_selection_type(cbs->setting) == ST_DIR)
   {
      BIND_ACTION_OK(cbs, action_ok_push_generic_list);
      return 0;
   }

   switch (hash)
   {
      case MENU_LABEL_START_CORE:
         BIND_ACTION_OK(cbs, action_ok_start_core);
         break;
      case MENU_LABEL_OPEN_ARCHIVE_DETECT_CORE:
         BIND_ACTION_OK(cbs, action_ok_open_archive_detect_core);
         break;
      case MENU_LABEL_OPEN_ARCHIVE:
         BIND_ACTION_OK(cbs, action_ok_open_archive);
         break;
      case MENU_LABEL_LOAD_ARCHIVE_DETECT_CORE:
         BIND_ACTION_OK(cbs, action_ok_load_archive_detect_core);
         break;
      case MENU_LABEL_LOAD_ARCHIVE:
         BIND_ACTION_OK(cbs, action_ok_load_archive);
         break;
      case MENU_LABEL_CUSTOM_BIND_ALL:
         BIND_ACTION_OK(cbs, action_ok_lookup_setting);
         break;
      case MENU_LABEL_SAVESTATE:
         BIND_ACTION_OK(cbs, action_ok_save_state);
         break;
      case MENU_LABEL_LOADSTATE:
         BIND_ACTION_OK(cbs, action_ok_load_state);
         break;
      case MENU_LABEL_RESUME_CONTENT:
         BIND_ACTION_OK(cbs, action_ok_resume_content);
         break;
      case MENU_LABEL_RESTART_CONTENT:
         BIND_ACTION_OK(cbs, action_ok_restart_content);
         break;
      case MENU_LABEL_TAKE_SCREENSHOT:
         BIND_ACTION_OK(cbs, action_ok_screenshot);
         break;
      case MENU_LABEL_QUIT_RETROARCH:
         BIND_ACTION_OK(cbs, action_ok_quit);
         break;
      case MENU_LABEL_CLOSE_CONTENT:
         BIND_ACTION_OK(cbs, action_ok_close_content);
         break;
      case MENU_LABEL_SAVE_NEW_CONFIG:
         BIND_ACTION_OK(cbs, action_ok_save_new_config);
         break;
      case MENU_LABEL_HELP:
         BIND_ACTION_OK(cbs, action_ok_help);
         break;
      case MENU_LABEL_HELP_CONTROLS:
         BIND_ACTION_OK(cbs, action_ok_help_controls);
         break;
      case MENU_LABEL_HELP_WHAT_IS_A_CORE:
         BIND_ACTION_OK(cbs, action_ok_help_what_is_a_core);
         break;
      case MENU_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
         BIND_ACTION_OK(cbs, action_ok_help_change_virtual_gamepad);
         break;
      case MENU_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         BIND_ACTION_OK(cbs, action_ok_help_audio_video_troubleshooting);
         break;
      case MENU_LABEL_HELP_SCANNING_CONTENT:
         BIND_ACTION_OK(cbs, action_ok_help_scanning_content);
         break;
      case MENU_LABEL_HELP_LOADING_CONTENT:
         BIND_ACTION_OK(cbs, action_ok_help_load_content);
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         BIND_ACTION_OK(cbs, action_ok_shader_pass);
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         BIND_ACTION_OK(cbs, action_ok_shader_preset);
         break;
      case MENU_LABEL_CHEAT_FILE_LOAD:
         BIND_ACTION_OK(cbs, action_ok_cheat_file);
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         BIND_ACTION_OK(cbs, action_ok_audio_dsp_plugin);
         break;
      case MENU_LABEL_REMAP_FILE_LOAD:
         BIND_ACTION_OK(cbs, action_ok_remap_file);
         break;
      case MENU_LABEL_RECORD_CONFIG:
         BIND_ACTION_OK(cbs, action_ok_record_configfile);
         break;
#ifdef HAVE_NETWORKING
      case MENU_LABEL_DOWNLOAD_CORE_CONTENT:
         BIND_ACTION_OK(cbs, action_ok_core_content_list);
         break;
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         BIND_ACTION_OK(cbs, action_ok_core_updater_list);
         break;
      case MENU_LABEL_THUMBNAILS_UPDATER_LIST:
         BIND_ACTION_OK(cbs, action_ok_thumbnails_updater_list);
         break;
      case MENU_LABEL_UPDATE_LAKKA:
         BIND_ACTION_OK(cbs, action_ok_lakka_list);
         break;
#endif
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         BIND_ACTION_OK(cbs, action_ok_shader_parameters);
         break;
      case MENU_LABEL_ACCOUNTS_LIST:
         BIND_ACTION_OK(cbs, action_ok_push_accounts_list);
         break;
      case MENU_LABEL_INPUT_SETTINGS:
         BIND_ACTION_OK(cbs, action_ok_push_input_settings_list);
         break;
      case MENU_LABEL_PLAYLIST_SETTINGS:
         BIND_ACTION_OK(cbs, action_ok_push_playlist_settings_list);
         break;
      case MENU_LABEL_INPUT_HOTKEY_BINDS:
         BIND_ACTION_OK(cbs, action_ok_push_input_hotkey_binds_list);
         break;
      case MENU_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
         BIND_ACTION_OK(cbs, action_ok_push_accounts_cheevos_list);
         break;
      case MENU_LABEL_SHADER_OPTIONS:
      case MENU_VALUE_INPUT_SETTINGS:
      case MENU_LABEL_CORE_OPTIONS:
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
      case MENU_LABEL_CORE_INFORMATION:
      case MENU_LABEL_SYSTEM_INFORMATION:
      case MENU_LABEL_NETWORK_INFORMATION:
      case MENU_LABEL_DEBUG_INFORMATION:
      case MENU_LABEL_ACHIEVEMENT_LIST:
      case MENU_LABEL_DISK_OPTIONS:
      case MENU_LABEL_SETTINGS:
      case MENU_LABEL_FRONTEND_COUNTERS:
      case MENU_LABEL_CORE_COUNTERS:
      case MENU_LABEL_MANAGEMENT:
      case MENU_LABEL_ONLINE_UPDATER:
      case MENU_LABEL_LOAD_CONTENT_LIST:
      case MENU_LABEL_ADD_CONTENT_LIST:
      case MENU_LABEL_HELP_LIST:
      case MENU_LABEL_INFORMATION_LIST:
      case MENU_LABEL_CONTENT_SETTINGS:
         BIND_ACTION_OK(cbs, action_ok_push_default);
         break;
      case MENU_LABEL_SCAN_FILE:
      case MENU_LABEL_SCAN_DIRECTORY:
      case MENU_LABEL_LOAD_CONTENT:
      case MENU_LABEL_DETECT_CORE_LIST:
         BIND_ACTION_OK(cbs, action_ok_push_content_list);
         break;
      case MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         BIND_ACTION_OK(cbs, action_ok_push_downloads_dir);
         break;
      case MENU_LABEL_DETECT_CORE_LIST_OK:
         BIND_ACTION_OK(cbs, action_ok_file_load_detect_core);
         break;
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
      case MENU_LABEL_CURSOR_MANAGER_LIST:
      case MENU_LABEL_DATABASE_MANAGER_LIST:
         BIND_ACTION_OK(cbs, action_ok_push_generic_list);
         break;
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         BIND_ACTION_OK(cbs, action_ok_shader_apply_changes);
         break;
      case MENU_LABEL_CHEAT_APPLY_CHANGES:
         BIND_ACTION_OK(cbs, action_ok_cheat_apply_changes);
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         BIND_ACTION_OK(cbs, action_ok_shader_preset_save_as);
         break;
      case MENU_LABEL_CHEAT_FILE_SAVE_AS:
         BIND_ACTION_OK(cbs, action_ok_cheat_file_save_as);
         break;
      case MENU_LABEL_REMAP_FILE_SAVE_CORE:
         BIND_ACTION_OK(cbs, action_ok_remap_file_save_core);
         break;
      case MENU_LABEL_REMAP_FILE_SAVE_GAME:
         BIND_ACTION_OK(cbs, action_ok_remap_file_save_game);
         break;
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
         BIND_ACTION_OK(cbs, action_ok_content_collection_list);
         break;
      case MENU_LABEL_CORE_LIST:
         BIND_ACTION_OK(cbs, action_ok_core_list);
         break;
      case MENU_LABEL_DISK_IMAGE_APPEND:
         BIND_ACTION_OK(cbs, action_ok_disk_image_append_list);
         break;
      case MENU_LABEL_CONFIGURATIONS:
         BIND_ACTION_OK(cbs, action_ok_configurations_list);
         break;
      case MENU_LABEL_SCREEN_RESOLUTION:
         BIND_ACTION_OK(cbs, action_ok_video_resolution);
         break;
      case MENU_LABEL_UPDATE_ASSETS:
         BIND_ACTION_OK(cbs, action_ok_update_assets);
         break;
      case MENU_LABEL_UPDATE_CORE_INFO_FILES:
         BIND_ACTION_OK(cbs, action_ok_update_core_info_files);
         break;
      case MENU_LABEL_UPDATE_OVERLAYS:
         BIND_ACTION_OK(cbs, action_ok_update_overlays);
         break;
      case MENU_LABEL_UPDATE_DATABASES:
         BIND_ACTION_OK(cbs, action_ok_update_databases);
         break;
      case MENU_LABEL_UPDATE_GLSL_SHADERS:
         BIND_ACTION_OK(cbs, action_ok_update_shaders_glsl);
         break;
      case MENU_LABEL_UPDATE_CG_SHADERS:
         BIND_ACTION_OK(cbs, action_ok_update_shaders_cg);
         break;
      case MENU_LABEL_UPDATE_CHEATS:
         BIND_ACTION_OK(cbs, action_ok_update_cheats);
         break;
      case MENU_LABEL_UPDATE_AUTOCONFIG_PROFILES:
         BIND_ACTION_OK(cbs, action_ok_update_autoconfig_profiles);
         break;
      case MENU_LABEL_UPDATE_AUTOCONFIG_PROFILES_HID:
         BIND_ACTION_OK(cbs, action_ok_update_autoconfig_profiles_hid);
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_ok_compare_type(menu_file_list_cbs_t *cbs,
      uint32_t label_hash, uint32_t menu_label_hash, unsigned type)
{
   if (type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD ||
         type == MENU_SETTINGS_CUSTOM_BIND)
   {
      BIND_ACTION_OK(cbs, action_ok_lookup_setting);
   }
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_OK(cbs, NULL);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_OK(cbs, NULL);
   }
   else if ((type >= MENU_SETTINGS_CHEEVOS_START))
   {
      BIND_ACTION_OK(cbs, action_ok_cheevos);
   }
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_OK(cbs, action_ok_cheat);
   }
   else
   {
      switch (type)
      {
         case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case MENU_FILE_PLAYLIST_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_playlist_entry);
            break;
         case MENU_FILE_RPL_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rpl_entry);
            break;
         case MENU_FILE_PLAYLIST_COLLECTION:
            BIND_ACTION_OK(cbs, action_ok_playlist_collection);
            break;
         case MENU_FILE_CONTENTLIST_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_push_generic_list);
            break;
         case MENU_FILE_CHEAT:
            BIND_ACTION_OK(cbs, action_ok_cheat_file_load);
            break;
         case MENU_FILE_RECORD_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_record_configfile_load);
            break;
         case MENU_FILE_REMAP:
            BIND_ACTION_OK(cbs, action_ok_remap_file_load);
            break;
         case MENU_FILE_SHADER_PRESET:
            switch (menu_label_hash)
            {
               case MENU_LABEL_SCAN_FILE:
                  break;
               default:
                  BIND_ACTION_OK(cbs, action_ok_shader_preset_load);
                  break;
            }
            break;
         case MENU_FILE_SHADER:
            switch (menu_label_hash)
            {
               case MENU_LABEL_SCAN_FILE:
                  break;
               default:
                  BIND_ACTION_OK(cbs, action_ok_shader_pass_load);
                  break;
            }
            break;
         case MENU_FILE_IMAGE:
            switch (menu_label_hash)
            {
               case MENU_LABEL_SCAN_FILE:
                  break;
               default:
                  BIND_ACTION_OK(cbs, action_ok_menu_wallpaper_load);
                  break;
            }
            break;
         case MENU_FILE_USE_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_path_use_directory);
            break;
#ifdef HAVE_LIBRETRODB
         case MENU_FILE_SCAN_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_path_scan_directory);
            break;
#endif
         case MENU_FILE_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_config_load);
            break;
         case MENU_FILE_PARENT_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_parent_directory_push);
            break;
         case MENU_FILE_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_directory_push);
            break;
         case MENU_FILE_CARCHIVE:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DETECT_CORE_LIST:
                  BIND_ACTION_OK(cbs, action_ok_compressed_archive_push_detect_core);
                  break;
               case MENU_LABEL_SCAN_FILE:
                  break;
               default:
                  BIND_ACTION_OK(cbs, action_ok_compressed_archive_push);
                  break;
            }
            break;
         case MENU_FILE_CORE:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DEFERRED_CORE_LIST:
                  BIND_ACTION_OK(cbs, action_ok_core_load_deferred);
                  break;
               case MENU_LABEL_DEFERRED_CORE_LIST_SET:
                  BIND_ACTION_OK(cbs, action_ok_core_deferred_set);
                  break;
               case MENU_LABEL_CORE_LIST:
                  BIND_ACTION_OK(cbs, action_ok_core_load);
                  break;
               case MENU_LABEL_CORE_UPDATER_LIST:
                  BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
                  break;
            }
            break;
         case MENU_FILE_DOWNLOAD_CORE_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_download);
            break;
         case MENU_FILE_DOWNLOAD_THUMBNAIL_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_thumbnails);
            break;
         case MENU_FILE_DOWNLOAD_CORE:
            BIND_ACTION_OK(cbs, action_ok_core_updater_download);
            break;
         case MENU_FILE_DOWNLOAD_THUMBNAIL:
            BIND_ACTION_OK(cbs, action_ok_thumbnails_updater_download);
            break;
         case MENU_FILE_DOWNLOAD_LAKKA:
            BIND_ACTION_OK(cbs, action_ok_lakka_download);
            break;
         case MENU_FILE_DOWNLOAD_CORE_INFO:
            break;
         case MENU_FILE_RDB:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
                  BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
                  break;
               case MENU_LABEL_DATABASE_MANAGER_LIST:
               case MENU_VALUE_HORIZONTAL_MENU:
                  BIND_ACTION_OK(cbs, action_ok_database_manager_list);
                  break;
            }
            break;
         case MENU_FILE_RDB_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rdb_entry);
            break;
         case MENU_FILE_CURSOR:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
                  BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
                  break;
               case MENU_LABEL_CURSOR_MANAGER_LIST:
                  BIND_ACTION_OK(cbs, action_ok_cursor_manager_list);
                  break;
            }
            break;
         case MENU_FILE_FONT:
         case MENU_FILE_OVERLAY:
         case MENU_FILE_AUDIOFILTER:
         case MENU_FILE_VIDEOFILTER:
            BIND_ACTION_OK(cbs, action_ok_set_path);
            break;
#ifdef HAVE_COMPRESSION
         case MENU_FILE_IN_CARCHIVE:
#endif
         case MENU_FILE_PLAIN:
            switch (menu_label_hash)
            {
#ifdef HAVE_LIBRETRODB
               case MENU_LABEL_SCAN_FILE:
                  BIND_ACTION_OK(cbs, action_ok_scan_file);
                  break;
#endif
               case MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
               case MENU_LABEL_DETECT_CORE_LIST:
               case MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
#ifdef HAVE_COMPRESSION
                  if (type == MENU_FILE_IN_CARCHIVE)
                  {
                     BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core_carchive);
                  }
                  else
#endif
                  {
                     BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core);
                  }
                  break;
               case MENU_LABEL_DISK_IMAGE_APPEND:
                  BIND_ACTION_OK(cbs, action_ok_disk_image_append);
                  break;
               default:
                  BIND_ACTION_OK(cbs, action_ok_file_load);
                  break;
            }
            break;
         case MENU_FILE_MOVIE:
         case MENU_FILE_MUSIC:
#ifdef HAVE_FFMPEG
            switch (menu_label_hash)
            {
               case MENU_LABEL_SCAN_FILE:
                  break;
               default:
                  BIND_ACTION_OK(cbs, action_ok_file_load_ffmpeg);
                  break;
            }
#endif
            break;
         case MENU_FILE_IMAGEVIEWER:
            switch (menu_label_hash)
            {
               case MENU_LABEL_SCAN_FILE:
                  break;
               default:
                  BIND_ACTION_OK(cbs, action_ok_file_load_imageviewer);
                  break;
            }
            break;
         case MENU_SETTINGS:
         case MENU_SETTING_GROUP:
         case MENU_SETTING_SUBGROUP:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
            BIND_ACTION_OK(cbs, action_ok_disk_cycle_tray_status);
            break;
         case MENU_SETTINGS_CORE_OPTION_CREATE:
            BIND_ACTION_OK(cbs, action_ok_option_create);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_OK(cbs, action_ok_lookup_setting);

   if (menu_cbs_init_bind_ok_compare_label(cbs, label, label_hash, elem0) == 0)
      return 0;

   if (menu_cbs_init_bind_ok_compare_type(cbs, label_hash, menu_label_hash, type) == 0)
      return 0;

   return -1;
}
