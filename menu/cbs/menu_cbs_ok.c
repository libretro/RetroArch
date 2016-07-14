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
#include "../menu_content.h"

#include "../../core_info.h"
#include "../../frontend/frontend_driver.h"
#include "../../defaults.h"
#include "../../managers/cheat_manager.h"
#include "../../general.h"
#include "../../tasks/tasks_internal.h"
#include "../../input/input_remapping.h"
#include "../../retroarch.h"
#include "../../system.h"
#include "../../lakka.h"

typedef struct
{
   enum msg_hash_enums enum_idx;
   char path[PATH_MAX_LENGTH];
} menu_file_transfer_t;

#ifndef BIND_ACTION_OK
#define BIND_ACTION_OK(cbs, name) \
   do { \
      cbs->action_ok = name; \
      cbs->action_ok_ident = #name; \
   } while(0)
#endif

/* FIXME - Global variables, refactor */
static char detect_content_path[PATH_MAX_LENGTH];
unsigned rpl_entry_selection_ptr;
unsigned rdb_entry_start_game_selection_ptr;
size_t hack_shader_pass = 0;

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
char *core_buf;
size_t core_len;

static void cb_net_generic_subdir(void *task_data, void *user_data, const char *err)
{
   char subdir_path[PATH_MAX_LENGTH] = {0};
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   menu_file_transfer_t *state       = (menu_file_transfer_t*)user_data;

   if (!data || err)
      goto finish;

   memcpy(subdir_path, data->data, data->len * sizeof(char));
   subdir_path[data->len] = '\0';

finish:
   if (!err && !strstr(subdir_path, file_path_str(FILE_PATH_INDEX_DIRS_URL)))
   {
      char parent_dir[PATH_MAX_LENGTH] = {0};

      fill_pathname_parent_dir(parent_dir,
            state->path, sizeof(parent_dir));

      generic_action_ok_displaylist_push(parent_dir, NULL,
            subdir_path, 0, 0, 0, ACTION_OK_DL_CORE_CONTENT_DIRS_SUBDIR_LIST);
   }

   if (err)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), err);

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }
}

/* defined in menu_cbs_deferred_push */
static void cb_net_generic(void *task_data, void *user_data, const char *err)
{
   bool refresh = false;
   http_transfer_data_t *data  = (http_transfer_data_t*)task_data;
   menu_file_transfer_t *state = (menu_file_transfer_t*)user_data;

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
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), err);

   if (data)
   {
      if (data->data)
         free(data->data);
      free(data);
   }

   if (!err && !strstr(state->path, file_path_str(FILE_PATH_INDEX_DIRS_URL)))
   {
      menu_file_transfer_t *transf     = NULL;
      char parent_dir[PATH_MAX_LENGTH] = {0};

      fill_pathname_parent_dir(parent_dir,
            state->path, sizeof(parent_dir));
      strlcat(parent_dir, file_path_str(FILE_PATH_INDEX_DIRS_URL), sizeof(parent_dir));

      transf           = (menu_file_transfer_t*)calloc(1, sizeof(*transf));
      strlcpy(transf->path, parent_dir, sizeof(transf->path));

      task_push_http_transfer(parent_dir, true, "index_dirs", cb_net_generic_subdir, transf);
   }
}
#endif

int generic_action_ok_displaylist_push(const char *path,
      const char *new_path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   enum menu_displaylist_ctl_state dl_type = DISPLAYLIST_NONE;
   char tmp[PATH_MAX_LENGTH]         = {0};
   char parent_dir[PATH_MAX_LENGTH]  = {0};
   char action_path[PATH_MAX_LENGTH] = {0};
   menu_displaylist_info_t      info = {0};
   const char           *menu_label  = NULL;
   const char            *menu_path  = NULL;
   const char          *content_path = NULL;
   const char          *info_label   = NULL;
   const char          *info_path    = NULL;
   menu_handle_t            *menu    = NULL;
   enum msg_hash_enums enum_idx      = MSG_UNKNOWN;
   settings_t            *settings   = config_get_ptr();
   file_list_t           *menu_stack = menu_entries_get_menu_stack_ptr(0);

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, &enum_idx, NULL);

   if (path && menu_path)
      fill_pathname_join(action_path, menu_path, path, sizeof(action_path));

   info.list          = menu_stack;

   switch (action_type)
   {
      case ACTION_OK_DL_USER_BINDS_LIST:
         info.type          = type;
         info.directory_ptr = idx; 
         info_path          = label;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE:
         if (menu)
         {
            menu_path    = menu->scratch2_buf;
            content_path = menu->scratch_buf;
         }
         if (content_path)
            fill_pathname_join(detect_content_path, menu_path, content_path,
                  sizeof(detect_content_path));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_OPEN_ARCHIVE:
         if (menu)
         {
            menu_path    = menu->scratch2_buf;
            content_path = menu->scratch_buf;
         }
         if (content_path)
            fill_pathname_join(detect_content_path, menu_path, content_path,
                  sizeof(detect_content_path));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_HELP:
         info_label = label;
         menu->push_help_screen = true;
         dl_type                = DISPLAYLIST_HELP;
         break;
      case ACTION_OK_DL_RPL_ENTRY:
         strlcpy(menu->deferred_path, label, sizeof(menu->deferred_path));
         info_label = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS);
         info.enum_idx           = MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS;
         info.directory_ptr      = idx;
         rpl_entry_selection_ptr = idx;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_AUDIO_DSP_PLUGIN:
         menu_displaylist_reset_filebrowser();
         info.directory_ptr = idx;
         info_path          = settings->directory.audio_filter;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN);
         info.enum_idx      = MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_SHADER_PASS:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.video_shader;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_SHADER_PARAMETERS:
         info.type          = MENU_SETTING_ACTION;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_GENERIC:
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_FILE_BROWSER_SELECT_DIR:
         menu_displaylist_reset_filebrowser();
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_DIR;
         break;
      case ACTION_OK_DL_PUSH_DEFAULT:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label; 
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_SHADER_PRESET:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.video_shader; 
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_CONTENT_LIST:
         menu_displaylist_reset_filebrowser();
         info.type          = FILE_TYPE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = new_path;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_SCAN_DIR_LIST:
         info.type          = FILE_TYPE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = new_path;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SCAN_DIR;
         break;
      case ACTION_OK_DL_REMAP_FILE:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.input_remapping;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_RECORD_CONFIGFILE:
         {
            global_t  *global  = global_get_ptr();
            menu_displaylist_reset_filebrowser();
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = global->record.config_dir;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         }
         break;
      case ACTION_OK_DL_DISK_IMAGE_APPEND_LIST:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.menu_content;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_PLAYLIST_COLLECTION:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CHEAT_FILE:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->path.cheat_database;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_CORE_LIST:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.libretro;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_CORE;
         break;
      case ACTION_OK_DL_CONTENT_COLLECTION_LIST:
         menu_displaylist_reset_filebrowser();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->directory.playlist;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_COLLECTION;
         break;
      case ACTION_OK_DL_RDB_ENTRY:
         menu_displaylist_reset_filebrowser();
         fill_pathname_join_delim(tmp,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL),
               path, '|', sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = label;
         info_label         = tmp;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_RDB_ENTRY_SUBMENU:
         info.directory_ptr = idx;
         info_label         = label;
         info_path          = path;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CONFIGURATIONS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         if (string_is_empty(settings->directory.menu_config))
            info_path        = label;
         else
            info_path        = settings->directory.menu_config;
         info_label = label;
         dl_type             = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE;

         if (!string_is_empty(path))
            strlcpy(menu->scratch_buf, path, sizeof(menu->scratch_buf));
         if (!string_is_empty(menu_path))
            strlcpy(menu->scratch2_buf, menu_path, sizeof(menu->scratch2_buf));
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION;

         if (!string_is_empty(path))
            strlcpy(menu->scratch_buf, path, sizeof(menu->scratch_buf));
         if (!string_is_empty(menu_path))
            strlcpy(menu->scratch2_buf, menu_path, sizeof(menu->scratch2_buf));
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_PARENT_DIRECTORY_PUSH:
         fill_pathname_parent_dir(parent_dir,
               action_path, sizeof(parent_dir));
         fill_pathname_parent_dir(parent_dir,
               parent_dir, sizeof(parent_dir));

         info.type          = type;
         info.directory_ptr = idx;
         info_path          = parent_dir;
         info_label         = menu_label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DIRECTORY_PUSH:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = action_path;
         info_label         = menu_label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DATABASE_MANAGER_LIST:
         menu_displaylist_reset_filebrowser();
         fill_pathname_join(tmp,
               settings->path.content_database,
               path, sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CURSOR_MANAGER_LIST:
         menu_displaylist_reset_filebrowser();
         fill_pathname_join(tmp, settings->directory.cursor,
               path, sizeof(tmp));

         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CORE_UPDATER_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_THUMBNAILS_UPDATER_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_CORE_CONTENT_DIRS_SUBDIR_LIST:
         {
            char new_path[PATH_MAX_LENGTH];
            snprintf(new_path, sizeof(new_path), "%s;%s", path, label);
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = new_path;
            info_label         = msg_hash_to_str(
                  MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST);
            info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST;
            dl_type            = DISPLAYLIST_GENERIC;
         }
         break;
      case ACTION_OK_DL_CORE_CONTENT_DIRS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_CORE_CONTENT_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_LAKKA_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST:
         info.directory_ptr = idx;
         info_path          = settings->directory.libretro;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST_SET:
         info.directory_ptr                 = idx;
         rdb_entry_start_game_selection_ptr = idx;
         info_path                          = settings->directory.libretro;
         info_label                         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET);
         info.enum_idx                      = MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_ACCOUNTS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_INPUT_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DRIVER_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CORE_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_VIDEO_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_SAVING_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_LOGGING_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_REWIND_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_MENU_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_UPDATER_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_NETWORK_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_USER_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_USER_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_USER_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DIRECTORY_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_PRIVACY_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PRIVACY_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_PRIVACY_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_AUDIO_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_PLAYLIST_SETTINGS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST:
         info.directory_ptr = idx;
         info.type          = type;
         info_path          = path;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST;
         dl_type                 = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CONTENT_SETTINGS:
         info.list          = menu_entries_get_selection_buf_ptr(0);
         info_path          = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS);
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS);
         info.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
         menu_entries_append_enum(menu_stack, info_path, info_label,
               MENU_ENUM_LABEL_CONTENT_SETTINGS,
               0, 0, 0);
         dl_type            = DISPLAYLIST_CONTENT_SETTINGS;
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

static int generic_action_ok_file_load(const char *corepath, const char *fullpath,
      enum rarch_core_type action_type, enum content_mode_load content_enum_idx)
{
   content_ctx_info_t content_info = {0};

   if (!task_push_content_load_default(
         corepath, fullpath,
         &content_info,
         action_type,
         content_enum_idx,
         NULL, NULL))
      return -1;

   return 0;
}

static int file_load_with_detect_core_wrapper(
      enum msg_hash_enums enum_label_idx,
      enum msg_hash_enums enum_idx,
      size_t idx, size_t entry_idx,
      const char *path, const char *label,
      unsigned type, bool is_carchive)
{
   menu_content_ctx_defer_info_t def_info;
   char new_core_path[PATH_MAX_LENGTH] = {0};
   char menu_path_new[PATH_MAX_LENGTH] = {0};
   int ret                             = 0;
   const char *menu_path               = NULL;
   const char *menu_label              = NULL;
   menu_handle_t *menu                 = NULL;
   core_info_list_t *list              = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, &enum_idx, NULL);

   if (!string_is_empty(menu_path))
      strlcpy(menu_path_new, menu_path, sizeof(menu_path_new));

   if (string_is_equal(menu_label,
            msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)))
      fill_pathname_join(menu_path_new, menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));
   else if (string_is_equal(menu_label,
            msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN)))
      fill_pathname_join(menu_path_new, menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));

   core_info_get_list(&list);

   def_info.data       = list;
   def_info.dir        = menu_path_new;
   def_info.path       = path;
   def_info.menu_label = menu_label;
   def_info.s          = menu->deferred_path;
   def_info.len        = sizeof(menu->deferred_path);

   if (menu_content_find_first_core(&def_info, false, new_core_path,
            sizeof(new_core_path)))
      ret = -1;

   if (     !is_carchive && !string_is_empty(path) 
         && !string_is_empty(menu_path_new))
      fill_pathname_join(detect_content_path, menu_path_new, path,
            sizeof(detect_content_path));

   if (enum_label_idx == MENU_ENUM_LABEL_COLLECTION)
      return generic_action_ok_displaylist_push(path, NULL,
            NULL, 0, idx, entry_idx, ACTION_OK_DL_DEFERRED_CORE_LIST_SET);

   switch (ret)
   {
      case -1:
         return generic_action_ok_file_load(new_core_path, def_info.s,
               CORE_TYPE_PLAIN, CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU);
      case 0:
         return generic_action_ok_displaylist_push(path, NULL, label, type,
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
   fill_pathname_join_delim(detect_content_path, detect_content_path, path,
         '#', sizeof(detect_content_path));

   type = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(MSG_UNKNOWN,
         MSG_UNKNOWN, idx, entry_idx,
         path, label, type, true);
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{

   type  = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(MSG_UNKNOWN,
         MSG_UNKNOWN, idx, entry_idx,
         path, label, type, false);
}

static int action_ok_file_load_with_detect_core_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   type  = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(
         MENU_ENUM_LABEL_COLLECTION,
         MSG_UNKNOWN, idx, entry_idx,
         path, label, type, false);
}

static int action_ok_playlist_entry_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t selection;
   menu_content_ctx_playlist_info_t playlist_info;
   char new_core_path[PATH_MAX_LENGTH]    = {0};
   size_t selection_ptr             = 0;
   playlist_t *playlist             = NULL;
   bool playlist_initialized        = false;
   const char *entry_path           = NULL;
   const char *entry_label          = NULL;
   const char *core_path            = NULL;
   const char *core_name            = NULL;
   playlist_t *tmp_playlist         = NULL;
   menu_handle_t *menu              = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return menu_cbs_exit();

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &tmp_playlist);

   if (!tmp_playlist)
   {
      tmp_playlist = playlist_init(
            menu->db_playlist_file, COLLECTION_SIZE);

      if (!tmp_playlist)
         return menu_cbs_exit();
      playlist_initialized = true;
   }

   playlist   = tmp_playlist;

   selection_ptr = entry_idx;

   playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL); 
   
   if (     string_is_equal(core_path, file_path_str(FILE_PATH_DETECT)) 
         && string_is_equal(core_name, file_path_str(FILE_PATH_DETECT)))
   {
      core_info_ctx_find_t core_info;
      char new_display_name[PATH_MAX_LENGTH] = {0};
      const char *entry_path                 = NULL;
      const char *entry_crc32                = NULL;
      const char *db_name                    = NULL;
      const char             *path_base      = 
         path_basename(menu->db_playlist_file);
      bool        found_associated_core      = 
         menu_content_playlist_find_associated_core(
            path_base, new_core_path, sizeof(new_core_path));

      core_info.inf  = NULL;
      core_info.path = new_core_path;

      if (!core_info_find(&core_info, new_core_path))
         found_associated_core = false;

      if (!found_associated_core)
      {
         int ret = action_ok_file_load_with_detect_core_collection(entry_path,
               label, type, selection_ptr, entry_idx);
         if (playlist_initialized)
            playlist_free(tmp_playlist);
         return ret;
      }

      menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &tmp_playlist);

      playlist_get_index(tmp_playlist, selection_ptr,
            &entry_path, &entry_label, NULL, NULL, &entry_crc32, &db_name);

      strlcpy(new_display_name,
            core_info.inf->display_name, sizeof(new_display_name));
      playlist_update(tmp_playlist,
            selection_ptr,
            entry_path,
            entry_label,
            new_core_path,
            new_display_name,
            entry_crc32,
            db_name);
      playlist_write_file(tmp_playlist);
   }
   else
      strlcpy(new_core_path, core_path, sizeof(new_core_path));

   playlist_info.data = playlist;
   playlist_info.idx  = selection_ptr;

   if (!menu_content_playlist_load(&playlist_info))
      return menu_cbs_exit();

   playlist_get_index(playlist,
         playlist_info.idx, &path, NULL, NULL, NULL, NULL, NULL);

   return generic_action_ok_file_load(new_core_path, path,
         CORE_TYPE_PLAIN, CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU);
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t selection;
   menu_content_ctx_playlist_info_t playlist_info;
   size_t selection_ptr             = 0;
   playlist_t *playlist             = g_defaults.history;
   const char *entry_path           = NULL;
   const char *entry_label          = NULL;
   const char *core_path            = NULL;
   const char *core_name            = NULL;
   playlist_t *tmp_playlist         = NULL;
   menu_handle_t *menu              = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return menu_cbs_exit();

   selection_ptr = entry_idx;

   playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL); 

   if (     string_is_equal(core_path, file_path_str(FILE_PATH_DETECT)) 
         && string_is_equal(core_name, file_path_str(FILE_PATH_DETECT)))
   {
      core_info_ctx_find_t core_info;
      char new_core_path[PATH_MAX_LENGTH]    = {0};
      char new_display_name[PATH_MAX_LENGTH] = {0};
      const char *entry_path                 = NULL;
      const char *entry_crc32                = NULL;
      const char *db_name                    = NULL;
      const char             *path_base      = 
         path_basename(menu->db_playlist_file);
      bool        found_associated_core      = 
         menu_content_playlist_find_associated_core(
            path_base, new_core_path, sizeof(new_core_path));

      core_info.inf                          = NULL;
      core_info.path                         = new_core_path;

      if (!core_info_find(&core_info, new_core_path))
         found_associated_core = false;

      if (!found_associated_core)
         return  action_ok_file_load_with_detect_core(entry_path,
               label, type, selection_ptr, entry_idx);

      menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &tmp_playlist);

      playlist_get_index(tmp_playlist, selection_ptr,
            &entry_path, &entry_label, NULL, NULL, &entry_crc32, &db_name);

      strlcpy(new_display_name,
            core_info.inf->display_name, sizeof(new_display_name));
      playlist_update(tmp_playlist,
            selection_ptr,
            entry_path,
            entry_label,
            new_core_path,
            new_display_name,
            entry_crc32,
            db_name);
      playlist_write_file(tmp_playlist);
   }

   playlist_info.data = playlist;
   playlist_info.idx  = selection_ptr;

   if (!menu_content_playlist_load(&playlist_info))
      return menu_cbs_exit();

   playlist_get_index(playlist,
         playlist_info.idx, &path, NULL, NULL, NULL, NULL, NULL);

   return generic_action_ok_file_load(core_path, path,
         CORE_TYPE_PLAIN, CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU);
}

static int action_ok_playlist_entry_start_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t selection;
   menu_content_ctx_playlist_info_t playlist_info;
   size_t selection_ptr             = 0;
   bool playlist_initialized        = false;
   playlist_t *playlist             = NULL;
   const char *entry_path           = NULL;
   const char *entry_label          = NULL;
   const char *core_path            = NULL;
   const char *core_name            = NULL;
   playlist_t *tmp_playlist         = NULL;
   menu_handle_t *menu              = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return menu_cbs_exit();

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &tmp_playlist);

   if (!tmp_playlist)
   {
      tmp_playlist = playlist_init(
            menu->db_playlist_file, COLLECTION_SIZE);

      if (!tmp_playlist)
         return menu_cbs_exit();
      playlist_initialized = true;
   }

   playlist      = tmp_playlist;
   selection_ptr = rdb_entry_start_game_selection_ptr;

   playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL); 

   if (     string_is_equal(core_path, file_path_str(FILE_PATH_DETECT)) 
         && string_is_equal(core_name, file_path_str(FILE_PATH_DETECT)))
   {
      core_info_ctx_find_t core_info;
      char new_core_path[PATH_MAX_LENGTH]    = {0};
      char new_display_name[PATH_MAX_LENGTH] = {0};
      const char *entry_path                 = NULL;
      const char *entry_crc32                = NULL;
      const char *db_name                    = NULL;
      const char             *path_base      = 
         path_basename(menu->db_playlist_file);
      bool        found_associated_core      = 
         menu_content_playlist_find_associated_core(
            path_base, new_core_path, sizeof(new_core_path));

      core_info.inf                          = NULL;
      core_info.path                         = new_core_path;

      if (!core_info_find(&core_info, new_core_path))
         found_associated_core = false;

      if (!found_associated_core)
      {
         int ret =  action_ok_file_load_with_detect_core(entry_path,
               label, type, selection_ptr, entry_idx);
         if (playlist_initialized)
            playlist_free(tmp_playlist);
         return ret;
      }

      menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &tmp_playlist);

      playlist_get_index(tmp_playlist, selection_ptr,
            &entry_path, &entry_label, NULL, NULL, &entry_crc32, &db_name);

      strlcpy(new_display_name,
            core_info.inf->display_name, sizeof(new_display_name));
      playlist_update(tmp_playlist,
            selection_ptr,
            entry_path,
            entry_label,
            new_core_path,
            new_display_name,
            entry_crc32,
            db_name);
      playlist_write_file(tmp_playlist);
   }

   playlist_info.data = playlist;
   playlist_info.idx  = selection_ptr;

   if (!menu_content_playlist_load(&playlist_info))
      return menu_cbs_exit();

   playlist_get_index(playlist,
         playlist_info.idx, &path, NULL, NULL, NULL, NULL, NULL);

   return generic_action_ok_file_load(core_path, path,
         CORE_TYPE_PLAIN, CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU);
}

static int action_ok_cheat_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   command_event(CMD_EVENT_CHEATS_APPLY, NULL);

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
   ACTION_OK_SET_PATH,
   ACTION_OK_SET_DIRECTORY
};

static int generic_action_ok(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned id, enum msg_hash_enums flush_id)
{
   char action_path[PATH_MAX_LENGTH] = {0};
   unsigned flush_type               = 0;
   int ret                           = 0;
   enum msg_hash_enums enum_idx      = MSG_UNKNOWN;
   const char             *menu_path = NULL;
   const char            *menu_label = NULL;
   const char *flush_char            = NULL;
   menu_handle_t               *menu = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      goto error;

   menu_entries_get_last_stack(&menu_path,
         &menu_label, NULL, &enum_idx, NULL);

   if (!string_is_empty(path))
      fill_pathname_join(action_path,
            menu_path, path, sizeof(action_path));
   else
      strlcpy(action_path, menu_path, sizeof(action_path));

   switch (id)
   {
      case ACTION_OK_LOAD_WALLPAPER:
         flush_type = MENU_SETTINGS;
         if (path_file_exists(action_path))
         {
            settings_t            *settings = config_get_ptr();

            strlcpy(settings->path.menu_wallpaper,
                  action_path, sizeof(settings->path.menu_wallpaper));
            task_push_image_load(action_path, 
                  MENU_ENUM_LABEL_CB_MENU_WALLPAPER,
                  menu_display_handle_wallpaper_upload, NULL);
         }
         break;
      case ACTION_OK_LOAD_CORE:
         flush_type = MENU_SETTINGS;
         
         if (generic_action_ok_file_load(action_path,
                  NULL, CORE_TYPE_PLAIN,
                  CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU) == 0)
         {
#ifndef HAVE_DYNAMIC
            ret = -1;
#endif
         }
         break;
      case ACTION_OK_LOAD_CONFIG_FILE:
         flush_type      = MENU_SETTINGS;
         menu_display_set_msg_force(true);

         if (config_replace(action_path))
         {
            bool pending_push = false;
            menu_navigation_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
            ret = -1;
         }
         break;
#ifdef HAVE_SHADER_MANAGER
      case ACTION_OK_LOAD_PRESET:
         {
            struct video_shader      *shader  = NULL;
            menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET, &shader);
            flush_char = msg_hash_to_str(flush_id);
            menu_shader_manager_set_preset(shader,
                  video_shader_parse_type(action_path, RARCH_SHADER_NONE),
                  action_path);
         }
         break;
      case ACTION_OK_LOAD_SHADER_PASS:
         {
            struct video_shader      *shader  = NULL;
            menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET, &shader);
            flush_char = msg_hash_to_str(flush_id);
            strlcpy(
                  shader->pass[hack_shader_pass].source.path,
                  action_path,
                  sizeof(shader->pass[hack_shader_pass].source.path));
            video_shader_resolve_parameters(NULL, shader);
         }
         break;
#endif
      case ACTION_OK_LOAD_RECORD_CONFIGFILE:
         {
            global_t *global = global_get_ptr();
            flush_char = msg_hash_to_str(flush_id);
            strlcpy(global->record.config, action_path,
                  sizeof(global->record.config));
         }
         break;
      case ACTION_OK_LOAD_REMAPPING_FILE:
         {
            config_file_t *conf = config_file_new(action_path);
            flush_char = msg_hash_to_str(flush_id);

            if (conf)
               input_remapping_load_file(conf, action_path);
         }
         break;
      case ACTION_OK_LOAD_CHEAT_FILE:
         flush_char = msg_hash_to_str(flush_id);
         cheat_manager_free();

         if (!cheat_manager_load(action_path))
            goto error;
         break;
      case ACTION_OK_APPEND_DISK_IMAGE:
         flush_type = MENU_SETTINGS;
         command_event(CMD_EVENT_DISK_APPEND_IMAGE, action_path);
         command_event(CMD_EVENT_RESUME, NULL);
         break;
      case ACTION_OK_SET_DIRECTORY:
      case ACTION_OK_SET_PATH:
         flush_type = MENU_SETTINGS;
         {
            rarch_setting_t *setting = menu_setting_find(menu_label);

            if (setting)
            {
               menu_setting_set_with_string_representation(
                     setting, action_path);
               ret = menu_setting_generic(setting, false);
            }
         }
         break;
      default:
         flush_char = msg_hash_to_str(flush_id);
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
         ACTION_OK_SET_PATH, MSG_UNKNOWN);
}

static int action_ok_menu_wallpaper_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{

   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_WALLPAPER, MSG_UNKNOWN);
}

static int action_ok_load_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_CORE, MSG_UNKNOWN);
}

static int action_ok_config_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_CONFIG_FILE, MSG_UNKNOWN);
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_APPEND_DISK_IMAGE, MSG_UNKNOWN);
}

static int action_ok_cheat_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_CHEAT_FILE,
         MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS);
}

static int action_ok_record_configfile_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_RECORD_CONFIGFILE,
         MENU_ENUM_LABEL_RECORDING_SETTINGS);
}

static int action_ok_remap_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_REMAPPING_FILE,
         MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS);
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_PRESET, MENU_ENUM_LABEL_SHADER_OPTIONS);
}

static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_SHADER_PASS, MENU_ENUM_LABEL_SHADER_OPTIONS);
}

static int  generic_action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums id, enum menu_help_type id2)
{
   const char               *lbl  = msg_hash_to_str(id);
   menu_handle_t            *menu = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu);
   menu->help_screen_type = id2;

   return generic_action_ok_displaylist_push(path, NULL, lbl, type, idx,
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
         MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION,
         MENU_HELP_CHEEVOS_DESCRIPTION);
}

static int action_ok_cheat(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;

   line.label         = msg_hash_to_str(MSG_INPUT_CHEAT);
   line.label_setting = label;
   line.type          = type;
   line.idx           = idx;
   line.cb            = menu_input_st_cheat_cb;

   if (!menu_input_ctl(MENU_INPUT_CTL_START_LINE, &line))
      return -1;
   return 0;
}

static void menu_input_st_string_cb_save_preset(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      rarch_setting_t         *setting = NULL;
      const char                *label = NULL;

      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL_SETTING, &label);

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         menu_setting_set_with_string_representation(setting, str);
         menu_setting_generic(setting, false);
      }
      else if (!string_is_empty(label))
         menu_shader_manager_save_preset(str, false);
   }

   menu_input_key_end_line();
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;

   line.label         = msg_hash_to_str(MSG_INPUT_PRESET_FILENAME);
   line.label_setting = label;
   line.type          = type;
   line.idx           = idx;
   line.cb            = menu_input_st_string_cb_save_preset;

   if (!menu_input_ctl(MENU_INPUT_CTL_START_LINE, &line))
      return -1;
   return 0;
}

static void menu_input_st_string_cb_cheat_file_save_as(
      void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t         *setting = NULL;
      const char                *label = NULL;

      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL_SETTING, &label);

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         menu_setting_set_with_string_representation(setting, str);
         menu_setting_generic(setting, false);
      }
      else if (!string_is_empty(label))
         cheat_manager_save(str);
   }

   menu_input_key_end_line();
}

static int action_ok_cheat_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;

   line.label         = msg_hash_to_str(MSG_INPUT_CHEAT_FILENAME);
   line.label_setting = label;
   line.type          = type;
   line.idx           = idx;
   line.cb            = menu_input_st_string_cb_cheat_file_save_as;

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
   char directory[PATH_MAX_LENGTH] = {0};
   char file[PATH_MAX_LENGTH]      = {0};
   settings_t *settings            = config_get_ptr();
   rarch_system_info_t *info       = NULL;
   const char *core_name           = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (info)
      core_name           = info->info.library_name;

   if (!string_is_empty(core_name))
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
         {
            global_t *global      = global_get_ptr();
            const char *game_name = path_basename(global->name.base);
            fill_pathname_join(file, core_name, game_name, sizeof(file));
         }
         break;
   }

   if(!path_file_exists(directory))
       path_mkdir(directory);

   if(input_remapping_save_file(file))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_REMAP_FILE_SAVED_SUCCESSFULLY),
            1, 100, true);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_SAVING_REMAP_FILE),
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
   return generic_action_ok(NULL, label, type, idx, entry_idx,
         ACTION_OK_SET_DIRECTORY, MSG_UNKNOWN);
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
   char core_display_name[PATH_MAX_LENGTH] = {0};
   const char            *entry_path       = NULL;
   const char           *entry_label       = NULL;
   const char           *entry_crc32       = NULL;
   const char               *db_name       = NULL;
   playlist_t               *playlist      = NULL;

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return menu_cbs_exit();

   menu_driver_ctl(RARCH_MENU_CTL_PLAYLIST_GET, &playlist);

   retro_assert(playlist != NULL);

   core_info_get_name(path, core_display_name, sizeof(core_display_name));

   idx = rdb_entry_start_game_selection_ptr;

   playlist_get_index(playlist, idx,
         &entry_path, &entry_label, NULL, NULL, &entry_crc32, &db_name);

   playlist_update(playlist, idx,
         entry_path, entry_label,
         path , core_display_name,
         entry_crc32,
         db_name);

   playlist_write_file(playlist);

   menu_entries_pop_stack(&selection, 0, 1);
   menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);

   return menu_cbs_exit();
}

static int action_ok_deferred_list_stub(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}

static int action_ok_load_core_deferred(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu                 = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   return generic_action_ok_file_load(path, menu->deferred_path,
         CORE_TYPE_PLAIN, CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU);
}

static int action_ok_start_net_retropad_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_file_load(NULL, NULL,
         CORE_TYPE_FFMPEG,
         CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU);
}

#ifdef HAVE_FFMPEG
static int action_ok_file_load_ffmpeg(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char new_path[PATH_MAX_LENGTH]  = {0};
   const char *menu_path           = NULL;
   file_list_t *menu_stack         = menu_entries_get_menu_stack_ptr(0);
   menu_entries_get_last(menu_stack, &menu_path, NULL, NULL, NULL);

   fill_pathname_join(new_path, menu_path, path,
         sizeof(new_path));
   return generic_action_ok_file_load(NULL, new_path,
         CORE_TYPE_FFMPEG,
         CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU);
}
#endif

static int action_ok_file_load_imageviewer(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char fullpath[PATH_MAX_LENGTH]  = {0};
   const char *menu_path           = NULL;
   file_list_t *menu_stack         = menu_entries_get_menu_stack_ptr(0);
   menu_entries_get_last(menu_stack, &menu_path, NULL, NULL, NULL);

   fill_pathname_join(fullpath, menu_path, path,
         sizeof(fullpath));
   return generic_action_ok_file_load(NULL, fullpath,
         CORE_TYPE_IMAGEVIEWER,
         CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU);
}

static int action_ok_file_load_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_file_load(path, detect_content_path,
         CORE_TYPE_FFMPEG, CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU);
}

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char menu_path_new[PATH_MAX_LENGTH] = {0};
   char full_path_new[PATH_MAX_LENGTH] = {0};
   const char *menu_label              = NULL;
   const char *menu_path               = NULL;
   rarch_setting_t *setting            = NULL;
   file_list_t  *menu_stack            = menu_entries_get_menu_stack_ptr(0);

   menu_entries_get_last(menu_stack, &menu_path, &menu_label, NULL, NULL);

   setting = menu_setting_find(menu_label);

   if (menu_setting_get_type(setting) == ST_PATH)
      return action_ok_set_path(path, label, type, idx, entry_idx);

   strlcpy(menu_path_new, menu_path, sizeof(menu_path_new));

   if (
         string_is_equal(menu_label,
            msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)) ||
         string_is_equal(menu_label,
            msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN))
      )
   {
      menu_handle_t *menu                 = NULL;
      if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
         return menu_cbs_exit();

      fill_pathname_join(menu_path_new,
            menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));
   }

   switch (type)
   {
      case FILE_TYPE_IN_CARCHIVE:
         fill_pathname_join_delim(full_path_new, menu_path_new, path,
               '#',sizeof(full_path_new));
         break;
      default:
         fill_pathname_join(full_path_new, menu_path_new, path,
               sizeof(full_path_new));
         break;
   }

   return generic_action_ok_file_load(NULL, full_path_new,
         CORE_TYPE_PLAIN,
         CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU);
}


static int generic_action_ok_command(enum event_command cmd)
{
   if (!command_event(cmd, NULL))
      return menu_cbs_exit();
   return 0;
}

static int action_ok_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_LOAD_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_SAVE_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_undo_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_UNDO_LOAD_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_undo_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_UNDO_SAVE_STATE) == -1)
      return menu_cbs_exit();
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

#ifdef HAVE_NETWORKING
#ifdef HAVE_ZLIB
static void cb_decompressed(void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (dec && !err)
   {
      unsigned type_hash = (uintptr_t)user_data;

      switch (type_hash)
      {
         case CB_CORE_UPDATER_DOWNLOAD:
            command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
            break;
         case CB_UPDATE_ASSETS:
            command_event(CMD_EVENT_REINIT, NULL);
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
#endif

#ifdef HAVE_NETWORKING
static int generic_action_ok_network(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums enum_idx)
{
   char url_path[PATH_MAX_LENGTH] = {0};
   settings_t *settings           = config_get_ptr();
   unsigned type_id2              = 0;
   menu_file_transfer_t *transf   = NULL;
   const char *url_label          = NULL;
   retro_task_callback_t callback = NULL;
   bool refresh                   = true;
   bool suppress_msg              = false;

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);

   if (string_is_empty(settings->network.buildbot_url))
      return menu_cbs_exit();

   command_event(CMD_EVENT_NETWORK_INIT, NULL);

   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DIRS_LIST:
         fill_pathname_join(url_path, settings->network.buildbot_assets_url,
               "cores/.index-dirs", sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_CORE_CONTENT_DIRS_LIST;
         callback  = cb_net_generic;
         suppress_msg = true;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST:
         fill_pathname_join(url_path, path,
               file_path_str(FILE_PATH_INDEX_URL), sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_CORE_CONTENT_LIST;
         callback  = cb_net_generic;
         suppress_msg = true;
         break;
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_LIST:
         fill_pathname_join(url_path, settings->network.buildbot_url,
               file_path_str(FILE_PATH_INDEX_EXTENDED_URL), sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_CORE_UPDATER_LIST;
         callback  = cb_net_generic;
         break;
      case MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST:
         fill_pathname_join(url_path,
               file_path_str(FILE_PATH_CORE_THUMBNAILS_URL),
               file_path_str(FILE_PATH_INDEX_URL), sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_THUMBNAILS_UPDATER_LIST;
         callback  = cb_net_generic;
         break;
#ifdef HAVE_LAKKA
      case MENU_ENUM_LABEL_CB_LAKKA_LIST:
         /* TODO unhardcode this path */
         fill_pathname_join(url_path, 
               file_path_str(FILE_PATH_LAKKA_URL),
               LAKKA_PROJECT, sizeof(url_path));
         fill_pathname_join(url_path, url_path,
               file_path_str(FILE_PATH_INDEX_URL),
               sizeof(url_path));
         url_label = msg_hash_to_str(enum_idx);
         type_id2  = ACTION_OK_DL_LAKKA_LIST;
         callback  = cb_net_generic;
         break;
#endif
      default:
         break;
   }

   transf           = (menu_file_transfer_t*)calloc(1, sizeof(*transf));
   strlcpy(transf->path, url_path, sizeof(transf->path));

   task_push_http_transfer(url_path, suppress_msg, url_label, callback, transf);

   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx, type_id2);
}

static int action_ok_core_content_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST);
} 

static int action_ok_core_content_dirs_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_CONTENT_DIRS_LIST);
} 

static int action_ok_core_updater_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_UPDATER_LIST);
}

static int action_ok_thumbnails_updater_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST);
}

static int action_ok_lakka_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_network(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_LAKKA_LIST);
}

static void cb_generic_dir_download(void *task_data,
      void *user_data, const char *err)
{
   menu_file_transfer_t     *transf      = (menu_file_transfer_t*)user_data;

   generic_action_ok_network(transf->path, transf->path, 0, 0, 0,
         MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST);
}
#endif

/* expects http_transfer_t*, menu_file_transfer_t* */
static void cb_generic_download(void *task_data,
      void *user_data, const char *err)
{
   char output_path[PATH_MAX_LENGTH]     = {0};
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
   switch (transf->enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         dir_path = settings->directory.thumbnails;
         break;
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD:
         dir_path = settings->directory.libretro;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         dir_path = settings->directory.core_assets;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         dir_path = settings->path.libretro_info;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         dir_path = settings->directory.assets;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         dir_path = settings->directory.autoconfig;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         dir_path = settings->path.content_database;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         dir_path = settings->directory.overlay;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         dir_path = settings->path.cheat_database;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
      {
         static char shaderdir[PATH_MAX_LENGTH]       = {0};
         const char *dirname                          = 
            transf->enum_idx == MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG ?
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
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
         dir_path = LAKKA_UPDATE_DIR;
         break;
      default:
         RARCH_WARN("Unknown transfer type '%s' bailing out.\n",
               msg_hash_to_str(transf->enum_idx));
         break;
   }

   if (!string_is_empty(dir_path))
      fill_pathname_join(output_path, dir_path,
            transf->path, sizeof(output_path));

   /* Make sure the directory exists */
   path_basedir(output_path);

   if (!path_mkdir(output_path))
   {
      err = msg_hash_to_str(MSG_FAILED_TO_CREATE_THE_DIRECTORY);
      goto finish;
   }

   if (!string_is_empty(dir_path))
      fill_pathname_join(output_path, dir_path,
            transf->path, sizeof(output_path));

#ifdef HAVE_ZLIB
   file_ext = path_get_extension(output_path);

   if (string_is_equal_noncase(file_ext, "zip"))
   {
      if (task_check_decompress(output_path))
      {
        err = msg_hash_to_str(MSG_DECOMPRESSION_ALREADY_IN_PROGRESS);
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
      if (!task_push_decompress(output_path, dir_path,
               NULL, NULL, NULL,
               cb_decompressed, (void*)(uintptr_t)
               msg_hash_calculate(msg_hash_to_str(transf->enum_idx))))
      {
        err = msg_hash_to_str(MSG_DECOMPRESSION_FAILED);
        goto finish;
      }
   }
#else
   switch (transf->enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD:
         command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
         break;
      default:
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
      const char *label, const char *menu_label,
      unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums enum_idx)
{
#ifdef HAVE_NETWORKING
   char s[PATH_MAX_LENGTH]      = {0};
   char s3[PATH_MAX_LENGTH]     = {0};
   menu_file_transfer_t *transf = NULL;
   settings_t *settings         = config_get_ptr();
   bool suppress_msg            = false;
   retro_task_callback_t cb     = cb_generic_download;

   fill_pathname_join(s,
         settings->network.buildbot_assets_url,
         "frontend", sizeof(s));

   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CB_DOWNLOAD_URL:
         suppress_msg = true;
         fill_pathname_join(s, label,
               path, sizeof(s));
         path = s;
         cb = cb_generic_dir_download;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         {
            struct string_list *str_list = string_split(menu_label, ";");
            strlcpy(s, str_list->elems[0].data, sizeof(s));
            string_list_free(str_list);
         }
         break;
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
#ifdef HAVE_LAKKA
         /* TODO unhardcode this path*/
         fill_pathname_join(s, file_path_str(FILE_PATH_LAKKA_URL),
               LAKKA_PROJECT, sizeof(s));
#endif
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         path = file_path_str(FILE_PATH_ASSETS_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         path = file_path_str(FILE_PATH_AUTOCONFIG_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         path = file_path_str(FILE_PATH_CORE_INFO_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         path = file_path_str(FILE_PATH_CHEATS_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         path = file_path_str(FILE_PATH_OVERLAYS_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         path = file_path_str(FILE_PATH_DATABASE_RDB_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
         path = file_path_str(FILE_PATH_SHADERS_GLSL_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
         path = file_path_str(FILE_PATH_SHADERS_CG_ZIP);
         break;
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         strlcpy(s, file_path_str(FILE_PATH_CORE_THUMBNAILS_URL), sizeof(s));
         break;
      default:
         strlcpy(s, settings->network.buildbot_url, sizeof(s));
         break;
   }

   fill_pathname_join(s3, s, path, sizeof(s3));

   transf           = (menu_file_transfer_t*)calloc(1, sizeof(*transf));
   transf->enum_idx = enum_idx;
   strlcpy(transf->path, path, sizeof(transf->path));

   task_push_http_transfer(s3, suppress_msg, msg_hash_to_str(enum_idx), cb, transf);
#endif
   return 0;
}

static int action_ok_core_content_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path   = NULL;
   const char *menu_label  = NULL;
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, &enum_idx, NULL);


   return action_ok_download_generic(path, label,
         menu_path, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD);
}

static int action_ok_core_content_thumbnails(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD);
}

static int action_ok_thumbnails_updater_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_DOWNLOAD);
}

static int action_ok_download_url(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_DOWNLOAD_URL);
}

static int action_ok_core_updater_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD);
}

static int action_ok_lakka_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD);
}

static int action_ok_update_assets(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_ASSETS);
}

static int action_ok_update_core_info_files(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES);
}

static int action_ok_update_overlays(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS);
}

static int action_ok_update_shaders_cg(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG);
}

static int action_ok_update_shaders_glsl(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL);
}

static int action_ok_update_databases(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_DATABASES);
}

static int action_ok_update_cheats(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_CHEATS);
}

static int action_ok_update_autoconfig_profiles(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES);
}

static int action_ok_disk_cycle_tray_status(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(CMD_EVENT_DISK_EJECT_TOGGLE);
}

/* creates folder and core options stub file for subsequent runs */
static int action_ok_option_create(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char game_path[PATH_MAX_LENGTH] = {0};
   config_file_t *conf             = NULL;

   if (!retroarch_validate_game_options(game_path, sizeof(game_path), true))
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE),
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

      runloop_msg_queue_push(
            msg_hash_to_str(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY),
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
   return generic_action_ok_command(CMD_EVENT_UNLOAD_CORE);
}

static int action_ok_quit(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(CMD_EVENT_QUIT);
}

static int action_ok_save_new_config(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(CMD_EVENT_MENU_SAVE_CONFIG);
}

static int action_ok_resume_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_restart_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(CMD_EVENT_RESET);
}

static int action_ok_screenshot(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(CMD_EVENT_TAKE_SCREENSHOT);
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(CMD_EVENT_SHADERS_APPLY_CHANGES);
}

static int action_ok_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_reset_filebrowser();
   return menu_setting_set(type, label, MENU_ACTION_OK, false);
}

static int action_ok_rdb_entry_submenu(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   union string_list_elem_attr attr;
   int ret                         = -1;
   char new_label[PATH_MAX_LENGTH] = {0};
   char new_path[PATH_MAX_LENGTH]  = {0};
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
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST),
         str_list->elems[0].data, '_',
         sizeof(new_label));

   ret = generic_action_ok_displaylist_push(new_path, NULL,
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
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_SHADER_PASS);
}

static int action_ok_shader_parameters(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_SHADER_PARAMETERS);
}

int action_ok_parent_directory_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_PARENT_DIRECTORY_PUSH);
}

int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx,
         entry_idx, ACTION_OK_DL_DIRECTORY_PUSH);
}

static int action_ok_database_manager_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx,
         entry_idx, ACTION_OK_DL_DATABASE_MANAGER_LIST);
}

static int action_ok_cursor_manager_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx,
         entry_idx, ACTION_OK_DL_CURSOR_MANAGER_LIST);
}

static int action_ok_compressed_archive_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx,
         entry_idx, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH);
}

static int action_ok_compressed_archive_push_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx,
         entry_idx, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE);
}

static int action_ok_configurations_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_CONFIGURATIONS_LIST);
}

static int action_ok_saving_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_SAVING_SETTINGS_LIST);
}

static int action_ok_logging_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_LOGGING_SETTINGS_LIST);
}

static int action_ok_frame_throttle_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST);
}

static int action_ok_rewind_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_REWIND_SETTINGS_LIST);
}

static int action_ok_onscreen_display_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST);
}

static int action_ok_onscreen_overlay_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST);
}

static int action_ok_menu_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_MENU_SETTINGS_LIST);
}

static int action_ok_user_interface_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST);
}

static int action_ok_menu_file_browser_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST);
}

static int action_ok_retro_achievements_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST);
}

static int action_ok_updater_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_UPDATER_SETTINGS_LIST);
}

static int action_ok_network_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_NETWORK_SETTINGS_LIST);
}

static int action_ok_user_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_USER_SETTINGS_LIST);
}

static int action_ok_directory_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_DIRECTORY_SETTINGS_LIST);
}

static int action_ok_privacy_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_PRIVACY_SETTINGS_LIST);
}

static int action_ok_rdb_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_RDB_ENTRY);
}

static int action_ok_content_collection_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_CONTENT_COLLECTION_LIST);
}

static int action_ok_core_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_CORE_LIST);
}

static int action_ok_cheat_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_CHEAT_FILE);
}

static int action_ok_playlist_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_PLAYLIST_COLLECTION);
}

static int action_ok_disk_image_append_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_DISK_IMAGE_APPEND_LIST);
}

static int action_ok_record_configfile(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_RECORD_CONFIGFILE);
}

static int action_ok_remap_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_REMAP_FILE);
}

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();
   return generic_action_ok_displaylist_push(path,
         settings->directory.menu_content, label, type, idx,
         entry_idx, ACTION_OK_DL_CONTENT_LIST);
}

static int action_ok_scan_directory_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();
   return generic_action_ok_displaylist_push(path,
         settings->directory.menu_content, label, type, idx,
         entry_idx, ACTION_OK_DL_SCAN_DIR_LIST);
}

static int action_ok_push_downloads_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();
   menu_displaylist_reset_filebrowser();
   return generic_action_ok_displaylist_push(path, settings->directory.core_assets, 
         msg_hash_to_str(MENU_ENUM_LABEL_DETECT_CORE_LIST),
         type, idx,
         entry_idx, ACTION_OK_DL_CONTENT_LIST);
}

static int action_ok_shader_preset(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_SHADER_PRESET);
}

int action_ok_push_generic_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_GENERIC);
}

int action_ok_push_filebrowser_list_dir_select(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_FILE_BROWSER_SELECT_DIR);
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_PUSH_DEFAULT);
}

static int action_ok_audio_dsp_plugin(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_AUDIO_DSP_PLUGIN);
}

static int action_ok_rpl_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx,
         entry_idx, ACTION_OK_DL_RPL_ENTRY);
}

static int action_ok_start_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_file_load(NULL, NULL,
         CORE_TYPE_PLAIN,
         CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU);
}


static int action_ok_open_archive_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE);
}

static int action_ok_push_accounts_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_ACCOUNTS_LIST);
}

static int action_ok_push_driver_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_DRIVER_SETTINGS_LIST);
}

static int action_ok_push_video_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_VIDEO_SETTINGS_LIST);
}

static int action_ok_push_configuration_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST);
}

static int action_ok_push_core_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_CORE_SETTINGS_LIST);
}

static int action_ok_push_audio_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_AUDIO_SETTINGS_LIST);
}

static int action_ok_push_input_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_INPUT_SETTINGS_LIST);
}

static int action_ok_push_playlist_settings_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_PLAYLIST_SETTINGS_LIST);
}

static int action_ok_push_input_hotkey_binds_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST);
}

static int action_ok_push_user_binds_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_USER_BINDS_LIST);
}

static int action_ok_push_accounts_cheevos_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx, entry_idx,
         ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST);
}

static int action_ok_open_archive(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(path, NULL,
         label, type, idx, entry_idx,
         ACTION_OK_DL_OPEN_ARCHIVE);
}

static int action_ok_load_archive(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu             = NULL;
   const char *menu_path           = NULL;
   const char *content_path        = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   menu_path    = menu->scratch2_buf;
   content_path = menu->scratch_buf;

   fill_pathname_join(detect_content_path, menu_path, content_path,
         sizeof(detect_content_path));

   command_event(CMD_EVENT_LOAD_CORE, NULL);

   return generic_action_ok_file_load(NULL, detect_content_path,
         CORE_TYPE_PLAIN,
         CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU);
}

static int action_ok_load_archive_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_content_ctx_defer_info_t def_info;
   char new_core_path[PATH_MAX_LENGTH] = {0};
   int ret                             = 0;
   core_info_list_t *list              = NULL;
   menu_handle_t *menu                 = NULL;
   const char *menu_path               = NULL;
   const char *content_path            = NULL;

   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return menu_cbs_exit();

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &idx))
      return false;

   menu_path    = menu->scratch2_buf;
   content_path = menu->scratch_buf;

   core_info_get_list(&list);

   def_info.data       = list;
   def_info.dir        = menu_path;
   def_info.path       = content_path;
   def_info.menu_label = label;
   def_info.s          = menu->deferred_path;
   def_info.len        = sizeof(menu->deferred_path);

   if (menu_content_find_first_core(&def_info, false,
            new_core_path, sizeof(new_core_path)))
      ret = -1;

   fill_pathname_join(detect_content_path, menu_path, content_path,
         sizeof(detect_content_path));

   switch (ret)
   {
      case -1:
         return generic_action_ok_file_load(new_core_path, def_info.s,
               CORE_TYPE_PLAIN,
               CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU);
      case 0:
         return generic_action_ok_displaylist_push(path, NULL,
               label, type,
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
         MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
         MENU_HELP_AUDIO_VIDEO_TROUBLESHOOTING);
}

static int action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_HELP, MENU_HELP_WELCOME);
}

static int action_ok_help_controls(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_HELP_CONTROLS, MENU_HELP_CONTROLS);
}

static int action_ok_help_what_is_a_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE, MENU_HELP_WHAT_IS_A_CORE);
}

static int action_ok_help_scanning_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_HELP_SCANNING_CONTENT, MENU_HELP_SCANNING_CONTENT);
}

static int action_ok_help_change_virtual_gamepad(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD,
         MENU_HELP_CHANGE_VIRTUAL_GAMEPAD);
}

static int action_ok_help_load_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_help(path, label, type, idx, entry_idx,
         MENU_ENUM_LABEL_HELP_LOADING_CONTENT, MENU_HELP_LOADING_CONTENT);
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
      command_event(CMD_EVENT_REINIT, NULL);
#endif
      video_driver_set_video_mode(width, height, true);
#ifdef GEKKO
      if (width == 0 || height == 0)
         strlcpy(msg, "Applying: DEFAULT", sizeof(msg));
      else
#endif
         snprintf(msg, sizeof(msg),
               "Applying: %dx%d\n START to reset",            
               width, height);
      runloop_msg_queue_push(msg, 1, 100, true);
   }

   return 0;
}

static int is_rdb_entry(enum msg_hash_enums enum_idx)
{
   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_RDB_ENTRY_PUBLISHER:
      case MENU_ENUM_LABEL_RDB_ENTRY_DEVELOPER:
      case MENU_ENUM_LABEL_RDB_ENTRY_ORIGIN:
      case MENU_ENUM_LABEL_RDB_ENTRY_FRANCHISE:
      case MENU_ENUM_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
      case MENU_ENUM_LABEL_RDB_ENTRY_ESRB_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_BBFC_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_ELSPA_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_PEGI_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_CERO_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
      case MENU_ENUM_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_MONTH:
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_YEAR:
      case MENU_ENUM_LABEL_RDB_ENTRY_MAX_USERS:
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_ok_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t hash)
{
   if (cbs->enum_idx != MSG_UNKNOWN && is_rdb_entry(cbs->enum_idx) == 0)
   {
      BIND_ACTION_OK(cbs, action_ok_rdb_entry_submenu);
      return 0;
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      unsigned i;

      for (i = 0; i < MAX_USERS; i++)
      {
         unsigned first_char = 0;
         const char *str = msg_hash_to_str(cbs->enum_idx);
         if (!str)
            continue;
         if (!strstr(str, "input_binds_list"))
            continue;
         first_char = atoi(&str[0]);
         if (first_char == (i+1))
         {
            BIND_ACTION_OK(cbs, action_ok_push_user_binds_list);
            return 0;
         }
      }
   }

   if (menu_setting_get_browser_selection_type(cbs->setting) == ST_DIR)
   {
      BIND_ACTION_OK(cbs, action_ok_push_filebrowser_list_dir_select);
      return 0;
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_START_CORE:
            BIND_ACTION_OK(cbs, action_ok_start_core);
            break;
         case MENU_ENUM_LABEL_START_NET_RETROPAD:
            BIND_ACTION_OK(cbs, action_ok_start_net_retropad_core);
            break;
         case MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE:
            BIND_ACTION_OK(cbs, action_ok_open_archive_detect_core);
            break;
         case MENU_ENUM_LABEL_OPEN_ARCHIVE:
            BIND_ACTION_OK(cbs, action_ok_open_archive);
            break;
         case MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE:
            BIND_ACTION_OK(cbs, action_ok_load_archive_detect_core);
            break;
         case MENU_ENUM_LABEL_LOAD_ARCHIVE:
            BIND_ACTION_OK(cbs, action_ok_load_archive);
            break;
         case MENU_ENUM_LABEL_CUSTOM_BIND_ALL:
            BIND_ACTION_OK(cbs, action_ok_lookup_setting);
            break;
         case MENU_ENUM_LABEL_SAVE_STATE:
            BIND_ACTION_OK(cbs, action_ok_save_state);
            break;
         case MENU_ENUM_LABEL_LOAD_STATE:
            BIND_ACTION_OK(cbs, action_ok_load_state);
            break;
         case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            BIND_ACTION_OK(cbs, action_ok_undo_load_state);
            break;
         case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            BIND_ACTION_OK(cbs, action_ok_undo_save_state);
            break;
         case MENU_ENUM_LABEL_RESUME_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_resume_content);
            break;
         case MENU_ENUM_LABEL_RESTART_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_restart_content);
            break;
         case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            BIND_ACTION_OK(cbs, action_ok_screenshot);
            break;
         case MENU_ENUM_LABEL_QUIT_RETROARCH:
            BIND_ACTION_OK(cbs, action_ok_quit);
            break;
         case MENU_ENUM_LABEL_CLOSE_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_close_content);
            break;
         case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_save_new_config);
            break;
         case MENU_ENUM_LABEL_HELP:
            BIND_ACTION_OK(cbs, action_ok_help);
            break;
         case MENU_ENUM_LABEL_HELP_CONTROLS:
            BIND_ACTION_OK(cbs, action_ok_help_controls);
            break;
         case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
            BIND_ACTION_OK(cbs, action_ok_help_what_is_a_core);
            break;
         case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
            BIND_ACTION_OK(cbs, action_ok_help_change_virtual_gamepad);
            break;
         case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
            BIND_ACTION_OK(cbs, action_ok_help_audio_video_troubleshooting);
            break;
         case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_help_scanning_content);
            break;
         case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_help_load_content);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            BIND_ACTION_OK(cbs, action_ok_shader_pass);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_OK(cbs, action_ok_shader_preset);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_OK(cbs, action_ok_cheat_file);
            break;
         case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_OK(cbs, action_ok_audio_dsp_plugin);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
            BIND_ACTION_OK(cbs, action_ok_remap_file);
            break;
         case MENU_ENUM_LABEL_RECORD_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_record_configfile);
            break;
#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_list);
            break;
         case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS:
            BIND_ACTION_OK(cbs, action_ok_core_content_dirs_list);
            break;
         case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
            BIND_ACTION_OK(cbs, action_ok_core_updater_list);
            break;
         case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_OK(cbs, action_ok_thumbnails_updater_list);
            break;
         case MENU_ENUM_LABEL_UPDATE_LAKKA:
            BIND_ACTION_OK(cbs, action_ok_lakka_list);
            break;
#endif
         case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_OK(cbs, action_ok_shader_parameters);
            break;
         case MENU_ENUM_LABEL_ACCOUNTS_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_accounts_list);
            break;
         case MENU_ENUM_LABEL_INPUT_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_input_settings_list);
            break;
         case MENU_ENUM_LABEL_DRIVER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_driver_settings_list);
            break;
         case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_video_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_audio_settings_list);
            break;
         case MENU_ENUM_LABEL_CORE_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_core_settings_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_configuration_settings_list);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_playlist_settings_list);
            break;
         case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS:
            BIND_ACTION_OK(cbs, action_ok_push_input_hotkey_binds_list);
            break;
         case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            BIND_ACTION_OK(cbs, action_ok_push_accounts_cheevos_list);
            break;
         case MENU_ENUM_LABEL_SHADER_OPTIONS:
         case MENU_ENUM_LABEL_CORE_OPTIONS:
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         case MENU_ENUM_LABEL_CORE_INFORMATION:
         case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
         case MENU_ENUM_LABEL_NETWORK_INFORMATION:
         case MENU_ENUM_LABEL_DEBUG_INFORMATION:
         case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         case MENU_ENUM_LABEL_DISK_OPTIONS:
         case MENU_ENUM_LABEL_SETTINGS:
         case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
         case MENU_ENUM_LABEL_CORE_COUNTERS:
         case MENU_ENUM_LABEL_MANAGEMENT:
         case MENU_ENUM_LABEL_ONLINE_UPDATER:
         case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
         case MENU_ENUM_LABEL_HELP_LIST:
         case MENU_ENUM_LABEL_INFORMATION_LIST:
         case MENU_ENUM_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case MENU_ENUM_LABEL_SCAN_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_scan_directory_list);
            break;
         case MENU_ENUM_LABEL_SCAN_FILE:
         case MENU_ENUM_LABEL_LOAD_CONTENT:
         case MENU_ENUM_LABEL_DETECT_CORE_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_content_list);
            break;
         case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_downloads_dir);
            break;
         case MENU_ENUM_LABEL_DETECT_CORE_LIST_OK:
            BIND_ACTION_OK(cbs, action_ok_file_load_detect_core);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
            BIND_ACTION_OK(cbs, action_ok_push_generic_list);
            break;
         case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            BIND_ACTION_OK(cbs, action_ok_shader_apply_changes);
            break;
         case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
            BIND_ACTION_OK(cbs, action_ok_cheat_apply_changes);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
            BIND_ACTION_OK(cbs, action_ok_shader_preset_save_as);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            BIND_ACTION_OK(cbs, action_ok_cheat_file_save_as);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_core);
            break;
         case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
            BIND_ACTION_OK(cbs, action_ok_remap_file_save_game);
            break;
         case MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST:
            BIND_ACTION_OK(cbs, action_ok_content_collection_list);
            break;
         case MENU_ENUM_LABEL_CORE_LIST:
            BIND_ACTION_OK(cbs, action_ok_core_list);
            break;
         case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
            BIND_ACTION_OK(cbs, action_ok_disk_image_append_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS:
            BIND_ACTION_OK(cbs, action_ok_configurations_list);
            break;
         case MENU_ENUM_LABEL_SAVING_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_saving_list);
            break;
         case MENU_ENUM_LABEL_LOGGING_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_logging_list);
            break;
         case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_frame_throttle_list);
            break;
         case MENU_ENUM_LABEL_REWIND_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_rewind_list);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_onscreen_display_list);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_onscreen_overlay_list);
            break;
         case MENU_ENUM_LABEL_MENU_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_menu_list);
            break;
         case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_user_interface_list);
            break;
         case MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_menu_file_browser_list);
            break;
         case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_retro_achievements_list);
            break;
         case MENU_ENUM_LABEL_UPDATER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_updater_list);
            break;
         case MENU_ENUM_LABEL_NETWORK_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_network_list);
            break;
         case MENU_ENUM_LABEL_USER_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_user_list);
            break;
         case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_directory_list);
            break;
         case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            BIND_ACTION_OK(cbs, action_ok_privacy_list);
            break;
         case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
            BIND_ACTION_OK(cbs, action_ok_video_resolution);
            break;
         case MENU_ENUM_LABEL_UPDATE_ASSETS:
            BIND_ACTION_OK(cbs, action_ok_update_assets);
            break;
         case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
            BIND_ACTION_OK(cbs, action_ok_update_core_info_files);
            break;
         case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
            BIND_ACTION_OK(cbs, action_ok_update_overlays);
            break;
         case MENU_ENUM_LABEL_UPDATE_DATABASES:
            BIND_ACTION_OK(cbs, action_ok_update_databases);
            break;
         case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
            BIND_ACTION_OK(cbs, action_ok_update_shaders_glsl);
            break;
         case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
            BIND_ACTION_OK(cbs, action_ok_update_shaders_cg);
            break;
         case MENU_ENUM_LABEL_UPDATE_CHEATS:
            BIND_ACTION_OK(cbs, action_ok_update_cheats);
            break;
         case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
            BIND_ACTION_OK(cbs, action_ok_update_autoconfig_profiles);
            break;
         default:
            return -1;
      }
   }
   else
   {
      switch (hash)
      {
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
         case MENU_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            BIND_ACTION_OK(cbs, action_ok_push_accounts_cheevos_list);
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
         case MENU_LABEL_DISK_IMAGE_APPEND:
            BIND_ACTION_OK(cbs, action_ok_disk_image_append_list);
            break;
         case MENU_LABEL_SCREEN_RESOLUTION:
            BIND_ACTION_OK(cbs, action_ok_video_resolution);
            break;
         default:
            return -1;
      }
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
         case FILE_TYPE_PLAYLIST_ENTRY:
            if (label_hash == MENU_LABEL_COLLECTION)
            {
               BIND_ACTION_OK(cbs, action_ok_playlist_entry_collection);
            }
            else if (label_hash == MENU_LABEL_RDB_ENTRY_START_CONTENT)
            {
               BIND_ACTION_OK(cbs, action_ok_playlist_entry_start_content);
            }
            else
            {
               BIND_ACTION_OK(cbs, action_ok_playlist_entry);
            }
            break;
         case FILE_TYPE_RPL_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rpl_entry);
            break;
         case FILE_TYPE_PLAYLIST_COLLECTION:
            BIND_ACTION_OK(cbs, action_ok_playlist_collection);
            break;
         case FILE_TYPE_CONTENTLIST_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_push_generic_list);
            break;
         case FILE_TYPE_CHEAT:
            BIND_ACTION_OK(cbs, action_ok_cheat_file_load);
            break;
         case FILE_TYPE_RECORD_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_record_configfile_load);
            break;
         case FILE_TYPE_REMAP:
            BIND_ACTION_OK(cbs, action_ok_remap_file_load);
            break;
         case FILE_TYPE_SHADER_PRESET:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_shader_preset_load);
            break;
         case FILE_TYPE_SHADER:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_shader_pass_load);
            break;
         case FILE_TYPE_IMAGE:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_menu_wallpaper_load);
            break;
         case FILE_TYPE_USE_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_path_use_directory);
            break;
#ifdef HAVE_LIBRETRODB
         case FILE_TYPE_SCAN_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_path_scan_directory);
            break;
#endif
         case FILE_TYPE_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_config_load);
            break;
         case FILE_TYPE_PARENT_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_parent_directory_push);
            break;
         case FILE_TYPE_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_directory_push);
            break;
         case FILE_TYPE_CARCHIVE:
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
         case FILE_TYPE_CORE:
            if (cbs->enum_idx != MSG_UNKNOWN)
            {
               switch (cbs->enum_idx)
               {
                  case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
                     BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
                     break;
                  case MSG_UNKNOWN:
                  default:
                     break;
               }
            }
            else
            {
               switch (menu_label_hash)
               {
                  case MENU_LABEL_DEFERRED_CORE_LIST:
                     BIND_ACTION_OK(cbs, action_ok_load_core_deferred);
                     break;
                  case MENU_LABEL_DEFERRED_CORE_LIST_SET:
                     BIND_ACTION_OK(cbs, action_ok_core_deferred_set);
                     break;
                  case MENU_LABEL_CORE_LIST:
                     BIND_ACTION_OK(cbs, action_ok_load_core);
                     break;
               }
            }
            break;
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_download);
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
            BIND_ACTION_OK(cbs, action_ok_core_content_thumbnails);
            break;
         case FILE_TYPE_DOWNLOAD_CORE:
            BIND_ACTION_OK(cbs, action_ok_core_updater_download);
            break;
         case FILE_TYPE_DOWNLOAD_URL:
            BIND_ACTION_OK(cbs, action_ok_download_url);
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL:
            BIND_ACTION_OK(cbs, action_ok_thumbnails_updater_download);
            break;
         case FILE_TYPE_DOWNLOAD_LAKKA:
            BIND_ACTION_OK(cbs, action_ok_lakka_download);
            break;
         case FILE_TYPE_DOWNLOAD_CORE_INFO:
            break;
         case FILE_TYPE_RDB:
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
         case FILE_TYPE_RDB_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rdb_entry);
            break;
         case FILE_TYPE_CURSOR:
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
         case FILE_TYPE_FONT:
         case FILE_TYPE_OVERLAY:
         case FILE_TYPE_AUDIOFILTER:
         case FILE_TYPE_VIDEOFILTER:
            BIND_ACTION_OK(cbs, action_ok_set_path);
            break;
#ifdef HAVE_COMPRESSION
         case FILE_TYPE_IN_CARCHIVE:
#endif
         case FILE_TYPE_PLAIN:
            if (cbs->enum_idx != MSG_UNKNOWN)
            {
               switch (cbs->enum_idx)
               {
#ifdef HAVE_LIBRETRODB
                  case MENU_ENUM_LABEL_SCAN_FILE:
                     BIND_ACTION_OK(cbs, action_ok_scan_file);
                     break;
#endif
                  case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
                  case MENU_ENUM_LABEL_DETECT_CORE_LIST:
                  case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
#ifdef HAVE_COMPRESSION
                     if (type == FILE_TYPE_IN_CARCHIVE)
                     {
                        BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core_carchive);
                     }
                     else
#endif
                     {
                        BIND_ACTION_OK(cbs, action_ok_file_load_with_detect_core);
                     }
                     break;
                  case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
                     BIND_ACTION_OK(cbs, action_ok_disk_image_append);
                     break;
                  default:
                     BIND_ACTION_OK(cbs, action_ok_file_load);
                     break;
               }
            }
            else
            {
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
                     if (type == FILE_TYPE_IN_CARCHIVE)
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
            }
            break;
         case FILE_TYPE_MOVIE:
         case FILE_TYPE_MUSIC:
#ifdef HAVE_FFMPEG
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_file_load_ffmpeg);
#endif
            break;
         case FILE_TYPE_IMAGEVIEWER:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_file_load_imageviewer);
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
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_OK(cbs, action_ok_lookup_setting);

   if (menu_cbs_init_bind_ok_compare_label(cbs, label, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_ok_compare_type(cbs, label_hash, menu_label_hash, type) == 0)
      return 0;

   return -1;
}
