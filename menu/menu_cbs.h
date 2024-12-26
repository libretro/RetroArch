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

#include <queues/task_queue.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "menu_entries.h"

#include "../msg_hash.h"

RETRO_BEGIN_DECLS

enum
{
   ACTION_OK_DL_DEFAULT = 0,
   ACTION_OK_DL_DROPDOWN_BOX_LIST,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_SPECIAL,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_RESOLUTION,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_AUDIO_DEVICE,
#ifdef HAVE_MICROPHONE
   ACTION_OK_DL_DROPDOWN_BOX_LIST_MICROPHONE_DEVICE,
#endif
   ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PARAMETER,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PRESET_PARAMETER,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_DISK_INDEX,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION,
   ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD,
#ifdef ANDROID
    ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_SELECT_PHYSICAL_KEYBOARD,
#endif
#ifdef HAVE_NETWORKING
   ACTION_OK_DL_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER,
#endif
   ACTION_OK_DL_OPEN_ARCHIVE,
   ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE,
   ACTION_OK_DL_MUSIC,
   ACTION_OK_DL_NETPLAY,
   ACTION_OK_DL_SCAN_DIR_LIST,
   ACTION_OK_DL_MANUAL_SCAN_DIR_LIST,
   ACTION_OK_DL_HELP,
   ACTION_OK_DL_RPL_ENTRY,
   ACTION_OK_DL_RDB_ENTRY,
   ACTION_OK_DL_RDB_ENTRY_SUBMENU,
   ACTION_OK_DL_CDROM_INFO_LIST,
   ACTION_OK_DL_AUDIO_DSP_PLUGIN,
   ACTION_OK_DL_VIDEO_FILTER,
   ACTION_OK_DL_OVERLAY_PRESET,
   ACTION_OK_DL_OSK_OVERLAY_PRESET,
   ACTION_OK_DL_VIDEO_FONT,
   ACTION_OK_DL_SHADER_PASS,
   ACTION_OK_DL_FAVORITES_LIST,
   ACTION_OK_DL_IMAGES_LIST,
   ACTION_OK_DL_VIDEO_LIST,
   ACTION_OK_DL_EXPLORE_LIST,
   ACTION_OK_DL_CONTENTLESS_CORES_LIST,
   ACTION_OK_DL_MUSIC_LIST,
   ACTION_OK_DL_SHADER_PARAMETERS,
   ACTION_OK_DL_SHADER_PRESET,
   ACTION_OK_DL_SHADER_PRESET_PREPEND,
   ACTION_OK_DL_SHADER_PRESET_APPEND,
   ACTION_OK_DL_SHADER_PRESET_SAVE,
   ACTION_OK_DL_SHADER_PRESET_REMOVE,
   ACTION_OK_DL_GENERIC,
   ACTION_OK_DL_PUSH_DEFAULT,
   ACTION_OK_DL_FILE_BROWSER_SELECT_FILE,
   ACTION_OK_DL_FILE_BROWSER_SELECT_DIR,
   ACTION_OK_DL_INPUT_SETTINGS_LIST,
   ACTION_OK_DL_INPUT_TURBO_FIRE_SETTINGS_LIST,
   ACTION_OK_DL_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST,
   ACTION_OK_DL_REMAPPINGS_PORT_LIST,
   ACTION_OK_DL_INPUT_MENU_SETTINGS_LIST,
   ACTION_OK_DL_DRIVER_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_WINDOWED_MODE_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_SYNCHRONIZATION_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_OUTPUT_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_SCALING_SETTINGS_LIST,
   ACTION_OK_DL_VIDEO_HDR_SETTINGS_LIST,
   ACTION_OK_DL_CRT_SWITCHRES_SETTINGS_LIST,
   ACTION_OK_DL_AUDIO_SETTINGS_LIST,
   ACTION_OK_DL_AUDIO_OUTPUT_SETTINGS_LIST,
#ifdef HAVE_MICROPHONE
   ACTION_OK_DL_MICROPHONE_SETTINGS_LIST,
#endif
   ACTION_OK_DL_AUDIO_SYNCHRONIZATION_SETTINGS_LIST,
   ACTION_OK_DL_AUDIO_MIXER_SETTINGS_LIST,
   ACTION_OK_DL_LATENCY_SETTINGS_LIST,
   ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST,
   ACTION_OK_DL_SAVING_SETTINGS_LIST,
   ACTION_OK_DL_CLOUD_SYNC_SETTINGS_LIST,
   ACTION_OK_DL_LOGGING_SETTINGS_LIST,
   ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST,
   ACTION_OK_DL_FRAME_TIME_COUNTER_SETTINGS_LIST,
   ACTION_OK_DL_REWIND_SETTINGS_LIST,
   ACTION_OK_DL_CHEAT_DETAILS_SETTINGS_LIST,
   ACTION_OK_DL_CHEAT_SEARCH_SETTINGS_LIST,
   ACTION_OK_DL_CORE_SETTINGS_LIST,
   ACTION_OK_DL_CORE_INFORMATION_LIST,
#ifdef HAVE_MIST
   ACTION_OK_DL_CORE_INFORMATION_STEAM_LIST,
#endif
   ACTION_OK_DL_INPUT_RETROPAD_BINDS_LIST,
   ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST,
   ACTION_OK_DL_RECORDING_SETTINGS_LIST,
   ACTION_OK_DL_PLAYLIST_SETTINGS_LIST,
   ACTION_OK_DL_PLAYLIST_MANAGER_LIST,
   ACTION_OK_DL_PLAYLIST_MANAGER_SETTINGS,
   ACTION_OK_DL_ACCOUNTS_LIST,
   ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST,
   ACTION_OK_DL_ACCOUNTS_YOUTUBE_LIST,
   ACTION_OK_DL_ACCOUNTS_TWITCH_LIST,
   ACTION_OK_DL_ACCOUNTS_FACEBOOK_LIST,
   ACTION_OK_DL_USER_BINDS_LIST,
   ACTION_OK_DL_CONTENT_LIST,
   ACTION_OK_DL_REMAP_FILE,
   ACTION_OK_DL_OVERRIDE_FILE,
   ACTION_OK_DL_RECORD_CONFIGFILE,
   ACTION_OK_DL_STREAM_CONFIGFILE,
   ACTION_OK_DL_DISK_IMAGE_APPEND_LIST,
   ACTION_OK_DL_SUBSYSTEM_ADD_LIST,
   ACTION_OK_DL_SUBSYSTEM_LOAD,
   ACTION_OK_DL_PLAYLIST_COLLECTION,
   ACTION_OK_DL_CONTENT_COLLECTION_LIST,
   ACTION_OK_DL_CHEAT_FILE,
   ACTION_OK_DL_CHEAT_FILE_APPEND,
   ACTION_OK_DL_CORE_LIST,
   ACTION_OK_DL_SIDELOAD_CORE_LIST,
   ACTION_OK_DL_LAKKA_LIST,
   ACTION_OK_DL_CONFIGURATIONS_LIST,
   ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH,
   ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE,
   ACTION_OK_DL_PARENT_DIRECTORY_PUSH,
   ACTION_OK_DL_DIRECTORY_PUSH,
   ACTION_OK_DL_DATABASE_MANAGER_LIST,
   ACTION_OK_DL_CORE_UPDATER_LIST,
   ACTION_OK_DL_CORE_MANAGER_LIST,
#ifdef HAVE_MIST
   ACTION_OK_DL_STEAM_SETTINGS_LIST,
   ACTION_OK_DL_CORE_MANAGER_STEAM_LIST,
#endif
   ACTION_OK_DL_THUMBNAILS_UPDATER_LIST,
   ACTION_OK_DL_PL_THUMBNAILS_UPDATER_LIST,
   ACTION_OK_DL_BROWSE_URL_LIST,
   ACTION_OK_DL_CORE_CONTENT_LIST,
   ACTION_OK_DL_CORE_CONTENT_DIRS_LIST,
   ACTION_OK_DL_CORE_CONTENT_DIRS_SUBDIR_LIST,
   ACTION_OK_DL_CORE_SYSTEM_FILES_LIST,
   ACTION_OK_DL_DEFERRED_LOAD_DISC_LIST,
   ACTION_OK_DL_DEFERRED_DUMP_DISC_LIST,
   ACTION_OK_DL_DEFERRED_CORE_LIST,
   ACTION_OK_DL_DEFERRED_CORE_LIST_SET,
   ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST,
   ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST,
   ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST,
   ACTION_OK_DL_OSK_OVERLAY_SETTINGS_LIST,
   ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST,
   ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST,
   ACTION_OK_DL_MENU_VIEWS_SETTINGS_LIST,
   ACTION_OK_DL_SETTINGS_VIEWS_SETTINGS_LIST,
   ACTION_OK_DL_QUICK_MENU_VIEWS_SETTINGS_LIST,
   ACTION_OK_DL_QUICK_MENU_OVERRIDE_OPTIONS_LIST,
   ACTION_OK_DL_MENU_SETTINGS_LIST,
#ifdef _3DS
   ACTION_OK_DL_MENU_BOTTOM_SETTINGS_LIST,
#endif
   ACTION_OK_DL_AI_SERVICE_SETTINGS_LIST,
   ACTION_OK_DL_ACCESSIBILITY_SETTINGS_LIST,
   ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST,
   ACTION_OK_DL_POWER_MANAGEMENT_SETTINGS_LIST,
   ACTION_OK_DL_CPU_PERFPOWER_SETTINGS_LIST,
   ACTION_OK_DL_CPU_POLICY_SETTINGS_LIST,
   ACTION_OK_DL_MENU_SOUNDS_LIST,
   ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST,
   ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST,
   ACTION_OK_DL_CHEEVOS_APPEARANCE_SETTINGS_LIST,
   ACTION_OK_DL_CHEEVOS_VISIBILITY_SETTINGS_LIST,
   ACTION_OK_DL_ACHIEVEMENTS_HARDCORE_PAUSE_LIST,
   ACTION_OK_DL_UPDATER_SETTINGS_LIST,
   ACTION_OK_DL_BLUETOOTH_SETTINGS_LIST,
   ACTION_OK_DL_WIFI_SETTINGS_LIST,
   ACTION_OK_DL_WIFI_NETWORKS_LIST,
   ACTION_OK_DL_NETWORK_SETTINGS_LIST,
   ACTION_OK_DL_SUBSYSTEM_SETTINGS_LIST,
   ACTION_OK_DL_NETWORK_HOSTING_SETTINGS_LIST,
   ACTION_OK_DL_NETPLAY_KICK_LIST,
   ACTION_OK_DL_NETPLAY_BAN_LIST,
   ACTION_OK_DL_NETPLAY_LOBBY_FILTERS_LIST,
   ACTION_OK_DL_NETPLAY_LAN_SCAN_SETTINGS_LIST,
   ACTION_OK_DL_LAKKA_SERVICES_LIST,
   ACTION_OK_DL_LAKKA_SWITCH_OPTIONS_LIST,
   ACTION_OK_DL_USER_SETTINGS_LIST,
   ACTION_OK_DL_DIRECTORY_SETTINGS_LIST,
   ACTION_OK_DL_PRIVACY_SETTINGS_LIST,
   ACTION_OK_DL_MIDI_SETTINGS_LIST,
   ACTION_OK_DL_LOAD_DISC_LIST,
   ACTION_OK_DL_DUMP_DISC_LIST,
#ifdef HAVE_LAKKA
   ACTION_OK_DL_EJECT_DISC,
#endif
   ACTION_OK_DL_BROWSE_URL_START,
   ACTION_OK_DL_CONTENT_SETTINGS,
   ACTION_OK_DL_CDROM_INFO_DETAIL_LIST,
   ACTION_OK_DL_RGUI_MENU_THEME_PRESET,
   ACTION_OK_DL_MANUAL_CONTENT_SCAN_LIST,
   ACTION_OK_DL_MANUAL_CONTENT_SCAN_DAT_FILE,
   ACTION_OK_DL_CORE_RESTORE_BACKUP_LIST,
   ACTION_OK_DL_CORE_DELETE_BACKUP_LIST,
   ACTION_OK_DL_SAVESTATE_LIST,
   ACTION_OK_DL_CORE_OPTION_OVERRIDE_LIST,
   ACTION_OK_DL_CORE_OPTIONS_LIST,
   ACTION_OK_DL_REMAP_FILE_MANAGER_LIST
};

/* Function callbacks */
int action_cb_push_dropdown_item_resolution(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx);

int action_cancel_pop_default(const char *path,
      const char *label, unsigned type, size_t idx);

int action_cancel_pop_with_new_pos(const char *path,
      const char *label, unsigned type, size_t idx, size_t new_idx);

int generic_action_ok_displaylist_push(const char *path, const char *new_path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type);

int generic_action_cheat_toggle(size_t idx, unsigned type, const char *label,
      bool wraparound);

int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx);

void input_keyboard_mapping_bits(unsigned mode, unsigned key);

unsigned libretro_device_get_size(unsigned *devices, size_t devices_size, unsigned port);

/* End of function callbacks */

int menu_cbs_init_bind_left(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, size_t lbl_len,
      unsigned type, size_t idx,
      const char *menu_label, size_t menu_lbl_len);

int menu_cbs_init_bind_right(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, size_t lbl_len,
      unsigned type, size_t idx,
      const char *menu_label, size_t menu_lbl_len);

int menu_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, size_t lbl_len,
      unsigned type, size_t idx);

int menu_cbs_init_bind_label(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_sublabel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, size_t lbl_len,
      unsigned type, size_t idx);

int menu_cbs_init_bind_info(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, size_t lbl_len,
      unsigned type, size_t idx,
      const char *menu_label, size_t menu_lbl_len);

int menu_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_scan(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

int menu_cbs_init_bind_title(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx);

#ifdef HAVE_LIBRETRODB
int action_scan_directory(const char *path,
      const char *label, unsigned type, size_t idx);

int action_scan_file(const char *path,
      const char *label, unsigned type, size_t idx);
#endif

int action_ok_core_option_dropdown_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx);

void cb_generic_download(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err);

RETRO_END_DECLS

#endif
