/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef MENU_CBS_H__
#define MENU_CBS_H__

#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

RETRO_BEGIN_DECLS

enum
{
   ACTION_OK_DL_DEFAULT = 0,
   ACTION_OK_DL_OPEN_ARCHIVE,
   ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE,
   ACTION_OK_DL_MUSIC,
   ACTION_OK_DL_SCAN_DIR_LIST,
   ACTION_OK_DL_HELP,
   ACTION_OK_DL_RPL_ENTRY,
   ACTION_OK_DL_RDB_ENTRY,
   ACTION_OK_DL_RDB_ENTRY_SUBMENU,
   ACTION_OK_DL_AUDIO_DSP_PLUGIN,
   ACTION_OK_DL_SHADER_PASS,
   ACTION_OK_DL_SHADER_PARAMETERS,
   ACTION_OK_DL_SHADER_PRESET,
   ACTION_OK_DL_GENERIC,
   ACTION_OK_DL_PUSH_DEFAULT,
   ACTION_OK_DL_FILE_BROWSER_SELECT_DIR,
   ACTION_OK_DL_INPUT_SETTINGS_LIST,
   ACTION_OK_DL_DRIVER_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_SETTINGS_LIST,
   ACTION_OK_DL_AUDIO_SETTINGS_LIST,
   ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST,
   ACTION_OK_DL_SAVING_SETTINGS_LIST,
   ACTION_OK_DL_LOGGING_SETTINGS_LIST,
   ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST,
   ACTION_OK_DL_REWIND_SETTINGS_LIST,
   ACTION_OK_DL_CORE_SETTINGS_LIST,
   ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST,
   ACTION_OK_DL_RECORDING_SETTINGS_LIST,
   ACTION_OK_DL_PLAYLIST_SETTINGS_LIST,
   ACTION_OK_DL_ACCOUNTS_LIST,
   ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST,
   ACTION_OK_DL_USER_BINDS_LIST,
   ACTION_OK_DL_CONTENT_LIST,
   ACTION_OK_DL_REMAP_FILE,
   ACTION_OK_DL_RECORD_CONFIGFILE,
   ACTION_OK_DL_DISK_IMAGE_APPEND_LIST,
   ACTION_OK_DL_PLAYLIST_COLLECTION,
   ACTION_OK_DL_CONTENT_COLLECTION_LIST,
   ACTION_OK_DL_CHEAT_FILE,
   ACTION_OK_DL_CORE_LIST,
   ACTION_OK_DL_LAKKA_LIST,
   ACTION_OK_DL_CONFIGURATIONS_LIST,
   ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH,
   ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE,
   ACTION_OK_DL_PARENT_DIRECTORY_PUSH,
   ACTION_OK_DL_DIRECTORY_PUSH,
   ACTION_OK_DL_DATABASE_MANAGER_LIST,
   ACTION_OK_DL_CURSOR_MANAGER_LIST,
   ACTION_OK_DL_CORE_UPDATER_LIST,
   ACTION_OK_DL_THUMBNAILS_UPDATER_LIST,
   ACTION_OK_DL_BROWSE_URL_LIST,
   ACTION_OK_DL_CORE_CONTENT_LIST,
   ACTION_OK_DL_CORE_CONTENT_DIRS_LIST,
   ACTION_OK_DL_CORE_CONTENT_DIRS_SUBDIR_LIST,
   ACTION_OK_DL_DEFERRED_CORE_LIST,
   ACTION_OK_DL_DEFERRED_CORE_LIST_SET,
   ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST,
   ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST,
   ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST,
   ACTION_OK_DL_MENU_SETTINGS_LIST,
   ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST,
   ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST,
   ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST,
   ACTION_OK_DL_UPDATER_SETTINGS_LIST,
   ACTION_OK_DL_WIFI_SETTINGS_LIST,
   ACTION_OK_DL_NETWORK_SETTINGS_LIST,
   ACTION_OK_DL_NETPLAY_LAN_SCAN_SETTINGS_LIST,
   ACTION_OK_DL_LAKKA_SERVICES_LIST,
   ACTION_OK_DL_USER_SETTINGS_LIST,
   ACTION_OK_DL_DIRECTORY_SETTINGS_LIST,
   ACTION_OK_DL_PRIVACY_SETTINGS_LIST,
   ACTION_OK_DL_BROWSE_URL_START,
   ACTION_OK_DL_CONTENT_SETTINGS
};

typedef struct
{
   enum msg_hash_enums enum_idx;
   char path[PATH_MAX_LENGTH];
} menu_file_transfer_t;

/* FIXME - Externs, refactor */
extern size_t hack_shader_pass;
extern unsigned rpl_entry_selection_ptr;

/* Function callbacks */

int action_refresh_default(file_list_t *list, file_list_t *menu_list);

int shader_action_parameter_right(unsigned type, const char *label, bool wraparound);

int shader_action_parameter_preset_right(unsigned type, const char *label,
      bool wraparound);

int generic_action_ok_displaylist_push(const char *path, const char *new_path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type);

int generic_action_cheat_toggle(size_t idx, unsigned type, const char *label,
      bool wraparound);

int action_ok_push_generic_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx);

int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx);

int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx);

int core_setting_right(unsigned type, const char *label,
      bool wraparound);

int action_right_input_desc(unsigned type, const char *label,
      bool wraparound);

int action_right_cheat(unsigned type, const char *label,
      bool wraparound);

/* End of function callbacks */

int menu_cbs_init_bind_left(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *menu_label,
      uint32_t label_hash);

int menu_cbs_init_bind_right(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *menu_label,
      uint32_t label_hash);

int menu_cbs_init_bind_refresh(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_label(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_sublabel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_up(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_down(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_info(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_content_list_switch(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      uint32_t label_hash, uint32_t menu_label_hash);

int menu_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      uint32_t label_hash);

int menu_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_scan(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_title(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      uint32_t label_hash);

#ifdef HAVE_LIBRETRODB
int action_scan_directory(const char *path,
      const char *label, unsigned type, size_t idx);

int action_scan_file(const char *path,
      const char *label, unsigned type, size_t idx);
#endif

int bind_right_generic(unsigned type, const char *label,
       bool wraparound);

void menu_cbs_init(void *data,
      menu_file_list_cbs_t *cbs,
      const char *path, const char *label,
      unsigned type, size_t idx);

int menu_cbs_exit(void);

RETRO_END_DECLS

#endif
