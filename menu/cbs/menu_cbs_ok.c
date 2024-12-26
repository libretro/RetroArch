/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#include <compat/strl.h>
#include <array/rbuf.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <lists/string_list.h>

#ifdef HAVE_NETWORKING
#include <net/net_http.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <vfs/vfs_implementation.h>
#ifdef HAVE_CDROM
#include <vfs/vfs_implementation_cdrom.h>
#endif

#ifdef HAVE_DISCORD
#include "../../network/discord.h"
#endif

#include "../../config.def.h"
#include "../../driver.h"
#include "../../file_path_special.h"

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_entries.h"
#include "../menu_setting.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "../menu_shader.h"
#endif
#include "../menu_input.h"

#include "../../core.h"
#include "../../configuration.h"
#include "../../core_info.h"
#include "../../audio/audio_driver.h"
#include "../../record/record_driver.h"
#include "../../frontend/frontend_driver.h"
#include "../../defaults.h"
#include "../../core_option_manager.h"
#ifdef HAVE_CHEATS
#include "../../cheat_manager.h"
#endif
#ifdef HAVE_AUDIOMIXER
#include "../../tasks/task_audio_mixer.h"
#endif
#include "../../tasks/task_content.h"
#include "../../tasks/task_file_transfer.h"
#include "../../tasks/tasks_internal.h"
#include "../../input/input_remapping.h"
#include "../../paths.h"
#include "../../playlist.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../../lakka.h"
#ifdef HAVE_BLUETOOTH
#include "../../bluetooth/bluetooth_driver.h"
#endif
#include "../../gfx/video_display_server.h"
#include "../../manual_content_scan.h"

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#ifdef HAVE_WIFI
#include "../../network/wifi_driver.h"
#endif
#endif

#ifdef __WINRT__
#include "../../uwp/uwp_func.h"
#endif

#if defined(ANDROID)
#include "../../file_path_special.h"
#include "../../play_feature_delivery/play_feature_delivery.h"
#endif

#if defined(HAVE_LIBNX)
#include "../../switch_performance_profiles.h"
#endif

#ifdef HAVE_MIST
#include "../../steam/steam.h"
#endif

enum
{
   ACTION_OK_LOAD_PRESET = 0,
   ACTION_OK_LOAD_SHADER_PASS,
   ACTION_OK_LOAD_STREAM_CONFIGFILE,
   ACTION_OK_LOAD_RECORD_CONFIGFILE,
   ACTION_OK_LOAD_REMAPPING_FILE,
   ACTION_OK_LOAD_OVERRIDE_FILE,
   ACTION_OK_LOAD_CHEAT_FILE,
   ACTION_OK_SUBSYSTEM_ADD,
   ACTION_OK_LOAD_CONFIG_FILE,
   ACTION_OK_LOAD_CORE,
   ACTION_OK_LOAD_WALLPAPER,
   ACTION_OK_SET_PATH,
   ACTION_OK_SET_PATH_AUDIO_FILTER,
   ACTION_OK_SET_PATH_VIDEO_FILTER,
   ACTION_OK_SET_PATH_OVERLAY,
   ACTION_OK_SET_PATH_OSK_OVERLAY,
   ACTION_OK_SET_PATH_VIDEO_FONT,
   ACTION_OK_SET_DIRECTORY,
   ACTION_OK_SHOW_WIMP,
   ACTION_OK_LOAD_CHEAT_FILE_APPEND,
   ACTION_OK_LOAD_RGUI_MENU_THEME_PRESET,
   ACTION_OK_SET_MANUAL_CONTENT_SCAN_DAT_FILE
};

enum
{
   ACTION_OK_REMAP_FILE_SAVE_AS = 0,
   ACTION_OK_REMAP_FILE_SAVE_CORE,
   ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR,
   ACTION_OK_REMAP_FILE_SAVE_GAME,
   ACTION_OK_REMAP_FILE_REMOVE_CORE,
   ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR,
   ACTION_OK_REMAP_FILE_REMOVE_GAME
};

#ifndef BIND_ACTION_OK
#define BIND_ACTION_OK(cbs, name) (cbs)->action_ok = (name)
#endif

#ifdef HAVE_NETWORKING

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#endif

#define ACTION_OK_DL_LBL(a, b) \
   info.directory_ptr = idx; \
   info.type          = type; \
   info_path          = path; \
   info_label         = msg_hash_to_str(a); \
   info.enum_idx      = a; \
   dl_type            = b;


#define DEFAULT_ACTION_OK_SET(funcname, _id, _flush) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok(path, label, type, idx, entry_idx, _id, _flush); \
}

#define DEFAULT_ACTION_DIALOG_START(funcname, _label, _idx, _cb) \
static int (funcname)(const char *path, const char *label_setting, unsigned type, size_t idx, size_t entry_idx) \
{ \
   menu_input_ctx_line_t line; \
   line.label         = _label; \
   line.label_setting = label_setting; \
   line.type          = type; \
   line.idx           = (_idx); \
   line.cb            = _cb; \
   if (!menu_input_dialog_start(&line)) \
      return -1; \
   return 0; \
}


#define DEFAULT_ACTION_OK_START_BUILTIN_CORE(funcname, _id) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   content_ctx_info_t content_info; \
   content_info.argc                   = 0; \
   content_info.argv                   = NULL; \
   content_info.args                   = NULL; \
   content_info.environ_get            = NULL; \
   if (!task_push_start_builtin_core(&content_info, _id, NULL, NULL)) \
      return -1; \
   return 0; \
}

#define DEFAULT_ACTION_OK_LIST(funcname, _id) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_network(path, label, type, idx, entry_idx, _id); \
}

#define DEFAULT_ACTION_OK_DOWNLOAD(funcname, _id) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return action_ok_download_generic(path, label, NULL, type, idx, entry_idx,_id); \
}

#define STATIC_DEFAULT_ACTION_OK_CMD_FUNC(func_name, cmd) \
static int (func_name)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_command(cmd); \
}

#define STATIC_DEFAULT_ACTION_OK_FUNC(func_name, lbl) \
static int (func_name)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx, entry_idx, lbl); \
}

#define DEFAULT_ACTION_OK_FUNC(func_name, lbl) \
int (func_name)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_displaylist_push(path, NULL, label, type, idx, entry_idx, lbl); \
}

#define DEFAULT_ACTION_OK_DL_PUSH(funcname, _fbid, _id, _path) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   settings_t            *settings   = config_get_ptr(); \
   (void)settings; \
   filebrowser_set_type(_fbid); \
   return generic_action_ok_displaylist_push(path, _path, label, type, idx, entry_idx, _id); \
}

#define DEFAULT_ACTION_OK_HELP(funcname, _id, _id2) \
static int (funcname)(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx) \
{ \
   return generic_action_ok_help(path, label, type, idx, entry_idx, _id, _id2); \
}

#ifdef HAVE_NETWORKING
#ifdef HAVE_LAKKA
static char *lakka_get_project(void)
{
#ifndef HAVE_LAKKA_CANARY
   size_t len;
   static char lakka_project[128];
   FILE *command_file = popen("cat /etc/release | cut -d - -f 1", "r");

   fgets(lakka_project, sizeof(lakka_project), command_file);
   len = strlen(lakka_project);

   if (len > 0 && lakka_project[len-1] == '\n')
      lakka_project[--len] = '\0';

   pclose(command_file);
   return lakka_project;
#else
   return "/";
#endif
}
#endif
#endif

static enum msg_hash_enums action_ok_dl_to_enum(unsigned lbl)
{
   switch (lbl)
   {
      case ACTION_OK_DL_REMAPPINGS_PORT_LIST:
         return MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PARAMETER:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PARAMETER;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PRESET_PARAMETER:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PRESET_PARAMETER;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_SPECIAL:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_RESOLUTION:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_AUDIO_DEVICE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_AUDIO_DEVICE;
#ifdef HAVE_MICROPHONE
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_MICROPHONE_DEVICE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MICROPHONE_DEVICE;
#endif
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_DISK_INDEX:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD;
#ifdef ANDROID
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_SELECT_PHYSICAL_KEYBOARD:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_SELECT_PHYSICAL_KEYBOARD;
#endif
#ifdef HAVE_NETWORKING
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER:
         return MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER;
#endif
      case ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MIXER_STREAM_SETTINGS_LIST;
      case ACTION_OK_DL_ACCOUNTS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST;
      case ACTION_OK_DL_ACHIEVEMENTS_HARDCORE_PAUSE_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST;
      case ACTION_OK_DL_INPUT_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST;
      case ACTION_OK_DL_INPUT_MENU_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_MENU_SETTINGS_LIST;
      case ACTION_OK_DL_INPUT_TURBO_FIRE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_TURBO_FIRE_SETTINGS_LIST;
      case ACTION_OK_DL_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST;
      case ACTION_OK_DL_LATENCY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LATENCY_SETTINGS_LIST;
      case ACTION_OK_DL_DRIVER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST;
      case ACTION_OK_DL_CORE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST;
      case ACTION_OK_DL_CORE_INFORMATION_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_INFORMATION_LIST;
#ifdef HAVE_MIST
      case ACTION_OK_DL_CORE_INFORMATION_STEAM_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_INFORMATION_STEAM_LIST;
#endif
      case ACTION_OK_DL_CORE_RESTORE_BACKUP_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_RESTORE_BACKUP_LIST;
      case ACTION_OK_DL_CORE_DELETE_BACKUP_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_DELETE_BACKUP_LIST;
      case ACTION_OK_DL_VIDEO_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST;
      case ACTION_OK_DL_VIDEO_SYNCHRONIZATION_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_SYNCHRONIZATION_SETTINGS_LIST;
      case ACTION_OK_DL_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST;
      case ACTION_OK_DL_VIDEO_WINDOWED_MODE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_WINDOWED_MODE_SETTINGS_LIST;
      case ACTION_OK_DL_VIDEO_SCALING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_SCALING_SETTINGS_LIST;
      case ACTION_OK_DL_VIDEO_HDR_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_HDR_SETTINGS_LIST;
      case ACTION_OK_DL_VIDEO_OUTPUT_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_OUTPUT_SETTINGS_LIST;
      case ACTION_OK_DL_CRT_SWITCHRES_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CRT_SWITCHRES_SETTINGS_LIST;
      case ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CONFIGURATION_SETTINGS_LIST;
      case ACTION_OK_DL_SAVING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_SAVING_SETTINGS_LIST;
      case ACTION_OK_DL_CLOUD_SYNC_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CLOUD_SYNC_SETTINGS_LIST;
      case ACTION_OK_DL_LOGGING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LOGGING_SETTINGS_LIST;
      case ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_FRAME_THROTTLE_SETTINGS_LIST;
      case ACTION_OK_DL_FRAME_TIME_COUNTER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_FRAME_TIME_COUNTER_SETTINGS_LIST;
      case ACTION_OK_DL_REWIND_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_REWIND_SETTINGS_LIST;
      case ACTION_OK_DL_CHEAT_DETAILS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CHEAT_DETAILS_SETTINGS_LIST;
      case ACTION_OK_DL_CHEAT_SEARCH_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CHEAT_SEARCH_SETTINGS_LIST;
      case ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ONSCREEN_DISPLAY_SETTINGS_LIST;
      case ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST;
      case ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST;
      case ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST;
      case ACTION_OK_DL_OSK_OVERLAY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_OSK_OVERLAY_SETTINGS_LIST;
      case ACTION_OK_DL_MENU_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST;
#ifdef _3DS
      case ACTION_OK_DL_MENU_BOTTOM_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_BOTTOM_SETTINGS_LIST;
#endif
      case ACTION_OK_DL_MENU_VIEWS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_VIEWS_SETTINGS_LIST;
      case ACTION_OK_DL_SETTINGS_VIEWS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_SETTINGS_VIEWS_SETTINGS_LIST;
      case ACTION_OK_DL_QUICK_MENU_VIEWS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_VIEWS_SETTINGS_LIST;
      case ACTION_OK_DL_QUICK_MENU_OVERRIDE_OPTIONS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_OVERRIDE_OPTIONS;
      case ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_USER_INTERFACE_SETTINGS_LIST;
      case ACTION_OK_DL_AI_SERVICE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_AI_SERVICE_SETTINGS_LIST;
      case ACTION_OK_DL_ACCESSIBILITY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCESSIBILITY_SETTINGS_LIST;
      case ACTION_OK_DL_POWER_MANAGEMENT_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_POWER_MANAGEMENT_SETTINGS_LIST;
      case ACTION_OK_DL_CPU_PERFPOWER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CPU_PERFPOWER_LIST;
      case ACTION_OK_DL_CPU_POLICY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CPU_POLICY_ENTRY;
      case ACTION_OK_DL_MENU_SOUNDS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_SOUNDS_LIST;
      case ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MENU_FILE_BROWSER_SETTINGS_LIST;
      case ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_RETRO_ACHIEVEMENTS_SETTINGS_LIST;
      case ACTION_OK_DL_CHEEVOS_APPEARANCE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CHEEVOS_APPEARANCE_SETTINGS_LIST;
      case ACTION_OK_DL_CHEEVOS_VISIBILITY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CHEEVOS_VISIBILITY_SETTINGS_LIST;
      case ACTION_OK_DL_UPDATER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_UPDATER_SETTINGS_LIST;
      case ACTION_OK_DL_NETWORK_HOSTING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETWORK_HOSTING_SETTINGS_LIST;
      case ACTION_OK_DL_NETPLAY_KICK_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETPLAY_KICK_LIST;
      case ACTION_OK_DL_NETPLAY_BAN_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETPLAY_BAN_LIST;
      case ACTION_OK_DL_NETPLAY_LOBBY_FILTERS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETPLAY_LOBBY_FILTERS_LIST;
      case ACTION_OK_DL_SUBSYSTEM_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_SUBSYSTEM_SETTINGS_LIST;
      case ACTION_OK_DL_NETWORK_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETWORK_SETTINGS_LIST;
      case ACTION_OK_DL_BLUETOOTH_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_BLUETOOTH_SETTINGS_LIST;
      case ACTION_OK_DL_WIFI_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_WIFI_SETTINGS_LIST;
      case ACTION_OK_DL_WIFI_NETWORKS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_WIFI_NETWORKS_LIST;
      case ACTION_OK_DL_NETPLAY:
         return MENU_ENUM_LABEL_DEFERRED_NETPLAY;
      case ACTION_OK_DL_NETPLAY_LAN_SCAN_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_NETPLAY_LAN_SCAN_SETTINGS_LIST;
      case ACTION_OK_DL_LAKKA_SERVICES_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LAKKA_SERVICES_LIST;
#ifdef HAVE_LAKKA_SWITCH
      case ACTION_OK_DL_LAKKA_SWITCH_OPTIONS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LAKKA_SWITCH_OPTIONS_LIST;
#endif
      case ACTION_OK_DL_USER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_USER_SETTINGS_LIST;
      case ACTION_OK_DL_DIRECTORY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST;
      case ACTION_OK_DL_PRIVACY_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_PRIVACY_SETTINGS_LIST;
      case ACTION_OK_DL_MIDI_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MIDI_SETTINGS_LIST;
      case ACTION_OK_DL_AUDIO_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST;
      case ACTION_OK_DL_AUDIO_OUTPUT_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_AUDIO_OUTPUT_SETTINGS_LIST;
#ifdef HAVE_MICROPHONE
      case ACTION_OK_DL_MICROPHONE_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MICROPHONE_SETTINGS_LIST;
#endif
      case ACTION_OK_DL_AUDIO_SYNCHRONIZATION_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_AUDIO_SYNCHRONIZATION_SETTINGS_LIST;
      case ACTION_OK_DL_AUDIO_MIXER_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_AUDIO_MIXER_SETTINGS_LIST;
      case ACTION_OK_DL_INPUT_RETROPAD_BINDS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_RETROPAD_BINDS_LIST;
      case ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST;
      case ACTION_OK_DL_RECORDING_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_RECORDING_SETTINGS_LIST;
      case ACTION_OK_DL_PLAYLIST_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST;
      case ACTION_OK_DL_PLAYLIST_MANAGER_LIST:
         return MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_LIST;
      case ACTION_OK_DL_PLAYLIST_MANAGER_SETTINGS:
         return MENU_ENUM_LABEL_DEFERRED_PLAYLIST_MANAGER_SETTINGS;
      case ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST;
      case ACTION_OK_DL_ACCOUNTS_TWITCH_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_TWITCH_LIST;
      case ACTION_OK_DL_ACCOUNTS_FACEBOOK_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_FACEBOOK_LIST;
      case ACTION_OK_DL_DUMP_DISC_LIST:
         return MENU_ENUM_LABEL_DEFERRED_DUMP_DISC_LIST;
#ifdef HAVE_LAKKA
      case ACTION_OK_DL_EJECT_DISC:
         return MENU_ENUM_LABEL_DEFERRED_EJECT_DISC;
#endif
      case ACTION_OK_DL_LOAD_DISC_LIST:
         return MENU_ENUM_LABEL_DEFERRED_LOAD_DISC_LIST;
      case ACTION_OK_DL_ACCOUNTS_YOUTUBE_LIST:
         return MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_YOUTUBE_LIST;
      case ACTION_OK_DL_PLAYLIST_COLLECTION:
         return MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST;
      case ACTION_OK_DL_FAVORITES_LIST:
         return MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST;
      case ACTION_OK_DL_BROWSE_URL_LIST:
         return MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_LIST;
      case ACTION_OK_DL_MUSIC_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MUSIC_LIST;
      case ACTION_OK_DL_IMAGES_LIST:
         return MENU_ENUM_LABEL_DEFERRED_IMAGES_LIST;
      case ACTION_OK_DL_CDROM_INFO_DETAIL_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CDROM_INFO_LIST;
      case ACTION_OK_DL_SHADER_PRESET_SAVE:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_SAVE_LIST;
      case ACTION_OK_DL_SHADER_PRESET_REMOVE:
         return MENU_ENUM_LABEL_DEFERRED_VIDEO_SHADER_PRESET_REMOVE_LIST;
      case ACTION_OK_DL_MANUAL_CONTENT_SCAN_LIST:
         return MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST;
      case ACTION_OK_DL_CORE_MANAGER_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_LIST;
#ifdef HAVE_MIST
      case ACTION_OK_DL_STEAM_SETTINGS_LIST:
         return MENU_ENUM_LABEL_DEFERRED_STEAM_SETTINGS_LIST;
      case ACTION_OK_DL_CORE_MANAGER_STEAM_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_MANAGER_STEAM_LIST;
#endif
      case ACTION_OK_DL_CORE_OPTION_OVERRIDE_LIST:
         return MENU_ENUM_LABEL_DEFERRED_CORE_OPTION_OVERRIDE_LIST;
      case ACTION_OK_DL_REMAP_FILE_MANAGER_LIST:
         return MENU_ENUM_LABEL_DEFERRED_REMAP_FILE_MANAGER_LIST;
      default:
         break;
   }

   return MSG_UNKNOWN;
}

static void action_ok_get_file_browser_start_path(
      const char *current_path, const char *default_path,
      char *start_path, size_t start_path_len,
      bool set_pending)
{
   const char *pending_selection = NULL;
   bool current_path_valid       = false;

   if (!start_path || (start_path_len < 1))
      return;

   /* Parse current path */
   if (!string_is_empty(current_path))
   {
      /* Start path is the parent directory of
       * the current path */
      fill_pathname_parent_dir(start_path, current_path,
            start_path_len);

      /* 'Pending selection' is the basename of
       * the current path - either a file name
       * or a directory name */
      pending_selection = path_basename(current_path);

      /* Check if current path is valid */
      if (    path_is_directory(start_path)
          && !string_is_empty(pending_selection))
         current_path_valid = true;
   }

   /* If current path is invalid, use default path */
   if (!current_path_valid)
   {
      if (string_is_empty(default_path) ||
          !path_is_directory(default_path))
      {
         start_path[0] = '\0';
         return;
      }

      strlcpy(start_path, default_path, start_path_len);
      return;
   }
   /* Current path is valid - set pending selection,
    * if required */
   else if (set_pending)
      menu_driver_set_pending_selection(pending_selection);
}

static const char *menu_driver_get_last_start_file_name(void)
{
   struct menu_state *menu_st  = menu_state_get_ptr();
   menu_handle_t *menu         = menu_st->driver_data;
   settings_t *settings        = config_get_ptr();
   bool use_last               = settings->bools.use_last_start_directory;
   /* Return NULL if there is no last 'file name' */
   if (   !menu
       || !use_last
       || string_is_empty(menu->last_start_content.file_name))
      return NULL;
   return menu->last_start_content.file_name;
}

static const char *menu_driver_get_last_start_directory(void)
{
   struct menu_state *menu_st    = menu_state_get_ptr();
   menu_handle_t *menu           = menu_st->driver_data;
   settings_t *settings          = config_get_ptr();
   bool use_last                 = settings->bools.use_last_start_directory;
   const char *default_directory = settings->paths.directory_menu_content;

   /* Return default directory if there is no
    * last directory or it's invalid */
   if (   !menu
       || !use_last
       || string_is_empty(menu->last_start_content.directory)
       || !path_is_directory(menu->last_start_content.directory))
      return default_directory;

   return menu->last_start_content.directory;
}


int generic_action_ok_displaylist_push(
      const char *path, const char *new_path,
      const char *label, unsigned type,
      size_t idx, size_t entry_idx,
      unsigned action_type)
{
   menu_displaylist_info_t info;
   char tmp[PATH_MAX_LENGTH];
   char parent_dir[PATH_MAX_LENGTH];
   enum menu_displaylist_ctl_state dl_type = DISPLAYLIST_NONE;
   const char           *menu_label        = NULL;
   const char            *menu_path        = NULL;
   const char          *content_path       = NULL;
   const char          *info_label         = NULL;
   const char          *info_path          = NULL;
   struct menu_state *menu_st              = menu_state_get_ptr();
   menu_dialog_t        *p_dialog          = &menu_st->dialog_st;
   menu_handle_t *menu                     = menu_st->driver_data;
   menu_list_t *menu_list                  = menu_st->entries.list;
   settings_t            *settings         = config_get_ptr();
   const char *menu_ident                  = (menu_st->driver_ctx && menu_st->driver_ctx->ident) ? menu_st->driver_ctx->ident : NULL;
   file_list_t           *menu_stack       = MENU_LIST_GET(menu_list, 0);
#ifdef HAVE_AUDIOMIXER
   bool audio_enable_menu                  = settings->bools.audio_enable_menu;
   bool audio_enable_menu_ok               = settings->bools.audio_enable_menu_ok;
#endif
   const char *dir_menu_content            = settings->paths.directory_menu_content;
   const char *dir_libretro                = settings->paths.directory_libretro;
   recording_state_t *recording_st         = recording_state_get_ptr();

   if (!menu || string_is_equal(menu_ident, "null"))
   {
      menu_displaylist_info_free(&info);
      return -1;
   }

#ifdef HAVE_AUDIOMIXER
   if (audio_enable_menu && audio_enable_menu_ok)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
#endif

   menu_displaylist_info_init(&info);

   info.list                               = menu_stack;

   tmp[0] = parent_dir[0] = '\0';

   menu_entries_get_last_stack(&menu_path, &menu_label, NULL, NULL, NULL);

   switch (action_type)
   {
      case ACTION_OK_DL_BROWSE_URL_START:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = NULL;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_START);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_BROWSE_URL_START;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_VIDEO_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_VIDEO_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_EXPLORE_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_EXPLORE_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_EXPLORE_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CONTENTLESS_CORES_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_REMAPPINGS_PORT_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info.path          = strdup(label);
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_REMAPPINGS_PORT_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_SPECIAL:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_SPECIAL;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PARAMETER:
         info.type          = (unsigned)(MENU_SETTINGS_SHADER_PARAMETER_0 + idx);
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PARAMETER);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PARAMETER;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PRESET_PARAMETER:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PRESET_PARAMETER);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_PRESET_PARAMETER;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_RESOLUTION:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_RESOLUTION;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_DISK_INDEX:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_DISK_INDEX;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DEVICE_TYPE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
#ifdef ANDROID
       case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_SELECT_PHYSICAL_KEYBOARD:
           info.type          = type;
           info.directory_ptr = idx;
           info_path          = path;
           info_label         = msg_hash_to_str(
                   MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_SELECT_PHYSICAL_KEYBOARD);
           info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_SELECT_PHYSICAL_KEYBOARD;
           dl_type            = DISPLAYLIST_GENERIC;
           break;
#endif
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_AUDIO_DEVICE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_AUDIO_DEVICE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_AUDIO_DEVICE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
#ifdef HAVE_MICROPHONE
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_MICROPHONE_DEVICE:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MICROPHONE_DEVICE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_MICROPHONE_DEVICE;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
#endif
#ifdef HAVE_NETWORKING
      case ACTION_OK_DL_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DROPDOWN_BOX_LIST_NETPLAY_MITM_SERVER;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
#endif
      case ACTION_OK_DL_USER_BINDS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_MUSIC:
         if (!string_is_empty(path))
            strlcpy(menu->scratch_buf, path, sizeof(menu->scratch_buf));
         if (!string_is_empty(menu_path))
            strlcpy(menu->scratch2_buf, menu_path, sizeof(menu->scratch2_buf));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_MUSIC);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_MUSIC;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE:
         if (menu)
         {
            menu_path    = menu->scratch2_buf;
            content_path = menu->scratch_buf;
         }
         if (content_path)
            fill_pathname_join_special(menu->detect_content_path,
                  menu_path, content_path,
                  sizeof(menu->detect_content_path));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_OPEN_ARCHIVE:
         if (menu)
         {
            menu_path    = menu->scratch2_buf;
            content_path = menu->scratch_buf;
         }
         if (content_path)
            fill_pathname_join_special(menu->detect_content_path,
                  menu_path, content_path,
                  sizeof(menu->detect_content_path));

         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN;
         info_path          = path;
         info.type          = type;
         info.directory_ptr = idx;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_HELP:
         info_label             = label;
         dl_type                = DISPLAYLIST_HELP;
         p_dialog->current_type = (enum menu_dialog_type)type;
         p_dialog->pending_push = true;
         break;
      case ACTION_OK_DL_RPL_ENTRY:
         strlcpy(menu->deferred_path, label, sizeof(menu->deferred_path));
         info_label = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS);
         info.enum_idx                 = MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS;
         info.directory_ptr            = idx;
         menu->rpl_entry_selection_ptr = (unsigned)entry_idx;
         dl_type                       = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_AUDIO_DSP_PLUGIN:
         filebrowser_clear_type();
         info.directory_ptr = idx;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN);
         info.enum_idx      = MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

         action_ok_get_file_browser_start_path(
               settings->paths.path_audio_dsp_plugin,
               settings->paths.directory_audio_filter,
               parent_dir, sizeof(parent_dir), true);

         info_path          = parent_dir;
         break;
      case ACTION_OK_DL_VIDEO_FILTER:
         filebrowser_clear_type();
         info.directory_ptr = idx;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FILTER);
         info.enum_idx      = MENU_ENUM_LABEL_VIDEO_FILTER;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

         action_ok_get_file_browser_start_path(
               settings->paths.path_softfilter_plugin,
               settings->paths.directory_video_filter,
               parent_dir, sizeof(parent_dir), true);

         info_path          = parent_dir;
         break;
      case ACTION_OK_DL_OVERLAY_PRESET:
         filebrowser_clear_type();
         info.directory_ptr = idx;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_PRESET);
         info.enum_idx      = MENU_ENUM_LABEL_OVERLAY_PRESET;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

         action_ok_get_file_browser_start_path(
               settings->paths.path_overlay,
               settings->paths.directory_overlay,
               parent_dir, sizeof(parent_dir), true);

         info_path          = parent_dir;
         break;
      case ACTION_OK_DL_OSK_OVERLAY_PRESET:
         filebrowser_clear_type();
            info.directory_ptr = idx;
            info_label         = msg_hash_to_str(MENU_ENUM_LABEL_OSK_OVERLAY_PRESET);
            info.enum_idx      = MENU_ENUM_LABEL_OSK_OVERLAY_PRESET;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

            action_ok_get_file_browser_start_path(
                  settings->paths.path_osk_overlay,
                  settings->paths.directory_osk_overlay,
                  parent_dir, sizeof(parent_dir), true);

            info_path          = parent_dir;
            break;
      case ACTION_OK_DL_VIDEO_FONT:
         filebrowser_set_type(FILEBROWSER_SELECT_VIDEO_FONT);
         info.directory_ptr = idx;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FONT_PATH);
         info.enum_idx      = MENU_ENUM_LABEL_VIDEO_FONT_PATH;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

         action_ok_get_file_browser_start_path(
               settings->paths.path_font,
               settings->paths.directory_assets,
               parent_dir, sizeof(parent_dir), true);

         info_path          = parent_dir;
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
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_FILE_BROWSER_SELECT_FILE:
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_FILE_BROWSER_SELECT_DIR:
         if (path)
            strlcpy(menu->deferred_path, path,
                  sizeof(menu->deferred_path));

         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         info_path          = new_path;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_DIR;
         break;
      case ACTION_OK_DL_PUSH_DEFAULT:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = label;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_SHADER_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            const char *shader_file_name = NULL;

            filebrowser_clear_type();
            info.type          = type;
            info.directory_ptr = idx;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

            menu_driver_get_last_shader_pass_path(&info_path, &shader_file_name);
            menu_driver_set_pending_selection(shader_file_name);
         }
#endif
         break;
      case ACTION_OK_DL_SHADER_PRESET:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            const char *shader_file_name = NULL;

            filebrowser_clear_type();
            info.type          = type;
            info.directory_ptr = idx;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

            menu_driver_get_last_shader_preset_path(&info_path, &shader_file_name);
            menu_driver_set_pending_selection(shader_file_name);
         }
#endif
         break;
      case ACTION_OK_DL_SHADER_PRESET_PREPEND:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            const char *shader_file_name = NULL;

            filebrowser_clear_type();
            info.type          = type;
            info.directory_ptr = idx;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

            menu_driver_get_last_shader_preset_path(&info_path, &shader_file_name);
            menu_driver_set_pending_selection(shader_file_name);
         }
#endif
         break;
      case ACTION_OK_DL_SHADER_PRESET_APPEND:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            const char *shader_file_name = NULL;

            filebrowser_clear_type();
            info.type          = type;
            info.directory_ptr = idx;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

            menu_driver_get_last_shader_preset_path(&info_path, &shader_file_name);
            menu_driver_set_pending_selection(shader_file_name);
         }
#endif
         break;
      case ACTION_OK_DL_CONTENT_LIST:
         info.type          = FILE_TYPE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = new_path;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;

         /* If this is the 'Start Directory' content
          * list, use last selected directory/file */
         if (  (type == MENU_SETTING_ACTION_FAVORITES_DIR)
             && settings->bools.use_last_start_directory)
         {
            info_path       = menu_driver_get_last_start_directory();
            menu_driver_set_pending_selection(
                  menu_driver_get_last_start_file_name());
         }
         break;
      case ACTION_OK_DL_SCAN_DIR_LIST:
         filebrowser_set_type(FILEBROWSER_SCAN_DIR);
         info.type          = FILE_TYPE_DIRECTORY;
         info.directory_ptr = idx;
         info_path          = new_path;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SCAN_DIR;
         break;
      case ACTION_OK_DL_MANUAL_SCAN_DIR_LIST:
         filebrowser_set_type(FILEBROWSER_MANUAL_SCAN_DIR);
         info.type          = FILE_TYPE_DIRECTORY;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_DIR;

         action_ok_get_file_browser_start_path(
               manual_content_scan_get_content_dir_ptr(),
               new_path, parent_dir, sizeof(parent_dir), true);

         info_path          = parent_dir;
         break;
      case ACTION_OK_DL_MANUAL_CONTENT_SCAN_DAT_FILE:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

         action_ok_get_file_browser_start_path(
               manual_content_scan_get_dat_file_path_ptr(),
               dir_menu_content, parent_dir, sizeof(parent_dir), true);

         info_path          = parent_dir;
         break;
      case ACTION_OK_DL_REMAP_FILE:
         {
            struct retro_system_info *sysinfo = &runloop_state_get_ptr()->system.info;
            const char *core_name             = sysinfo ? sysinfo->library_name : NULL;

            if (     !string_is_empty(core_name)
                  && !string_is_empty(settings->paths.directory_input_remapping))
            {
               fill_pathname_join_special(tmp,
                     settings->paths.directory_input_remapping, core_name, sizeof(tmp));
               if (!path_is_directory(tmp))
                  tmp[0] = '\0';
            }

            filebrowser_clear_type();
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = !string_is_empty(tmp) ? tmp : settings->paths.directory_input_remapping;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         }
         break;
      case ACTION_OK_DL_OVERRIDE_FILE:
         {
            struct retro_system_info *sysinfo = &runloop_state_get_ptr()->system.info;
            const char *core_name             = sysinfo ? sysinfo->library_name : NULL;

            if (!string_is_empty(core_name))
            {
               fill_pathname_join_special(tmp,
                     settings->paths.directory_menu_config,
                     core_name, sizeof(tmp));
               if (!path_is_directory(tmp))
                  tmp[0] = '\0';
            }

            filebrowser_clear_type();
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = !string_is_empty(tmp) ? tmp : settings->paths.directory_menu_config;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         }
         break;
      case ACTION_OK_DL_STREAM_CONFIGFILE:
         {
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = recording_st->config_dir;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         }
         break;
      case ACTION_OK_DL_RECORD_CONFIGFILE:
         filebrowser_clear_type();
         {
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = recording_st->config_dir;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         }
         break;
      case ACTION_OK_DL_DISK_IMAGE_APPEND_LIST:
         {
            filebrowser_clear_type();
            strlcpy(tmp, path_get(RARCH_PATH_CONTENT), sizeof(tmp));
            path_basedir(tmp);

            info.type          = type;
            info.directory_ptr = idx;
            info_path          = !string_is_empty(tmp) ? tmp : dir_menu_content;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

            /* Focus on current content entry */
            {
               char path_content[PATH_MAX_LENGTH];
               strlcpy(path_content, path_get(RARCH_PATH_CONTENT), sizeof(path_content));
               /* Remove archive browsed file from the path */
               {
                  char *delim = (char*)strchr(path_content, '#');
                  if (delim)
                     *delim = '\0';
               }
               menu_driver_set_pending_selection(path_basename(path_content));
            }
         }
         break;
      case ACTION_OK_DL_SUBSYSTEM_ADD_LIST:
         {
            filebrowser_clear_type();
            if (content_get_subsystem_rom_id() > 0)
               strlcpy(tmp, content_get_subsystem_rom(content_get_subsystem_rom_id() - 1), sizeof(tmp));
            else
               strlcpy(tmp, path_get(RARCH_PATH_CONTENT), sizeof(tmp));
            path_basedir(tmp);

            if (content_get_subsystem() != (int)type - MENU_SETTINGS_SUBSYSTEM_ADD)
               content_clear_subsystem();
            content_set_subsystem(type - MENU_SETTINGS_SUBSYSTEM_ADD);
            filebrowser_set_type(FILEBROWSER_SELECT_FILE_SUBSYSTEM);
            info.type          = type;
            info.directory_ptr = idx;
            info_path          = !string_is_empty(tmp) ? tmp : dir_menu_content;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         }
         break;
      case ACTION_OK_DL_SUBSYSTEM_LOAD:
         {
            content_ctx_info_t content_info = {0};
            filebrowser_clear_type();
            task_push_load_subsystem_with_core(
                  NULL, &content_info,
                  CORE_TYPE_PLAIN, NULL, NULL);
         }
         break;
      case ACTION_OK_DL_CHEAT_FILE:
      case ACTION_OK_DL_CHEAT_FILE_APPEND:
#ifdef HAVE_CHEATS
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.path_cheat_database;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
#endif
         break;
      case ACTION_OK_DL_RGUI_MENU_THEME_PRESET:
         {
            char rgui_assets_dir[PATH_MAX_LENGTH];

            filebrowser_clear_type();
            info.type          = type;
            info.directory_ptr = idx;
            info_label         = label;
            dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;

            fill_pathname_join_special(rgui_assets_dir,
                  settings->paths.directory_assets, "rgui",
                  sizeof(rgui_assets_dir));

            action_ok_get_file_browser_start_path(
                  settings->paths.path_rgui_theme_preset,
                  rgui_assets_dir,
                  parent_dir, sizeof(parent_dir), true);

            info_path          = parent_dir;
         }
         break;
      case ACTION_OK_DL_CORE_LIST:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = dir_libretro;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_CORE;
         break;
      case ACTION_OK_DL_SIDELOAD_CORE_LIST:
         filebrowser_clear_type();
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_core_assets;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_FILE;
         break;
      case ACTION_OK_DL_SAVESTATE_LIST:
      case ACTION_OK_DL_CORE_OPTIONS_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CONTENT_COLLECTION_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = settings->paths.directory_playlist;
         info_label         = label;
         dl_type            = DISPLAYLIST_FILE_BROWSER_SELECT_COLLECTION;
         break;
      case ACTION_OK_DL_RDB_ENTRY:
         filebrowser_clear_type();
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
         if (string_is_empty(settings->paths.directory_menu_config))
            info_path        = label;
         else
            info_path        = settings->paths.directory_menu_config;
         info_label          = label;
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
         parent_dir[0]  = '\0';

         if (path && menu_path)
            fill_pathname_join_special(tmp,
                  menu_path, path, sizeof(tmp));

         fill_pathname_parent_dir(parent_dir,
               tmp, sizeof(parent_dir));
         fill_pathname_parent_dir(parent_dir,
               parent_dir, sizeof(parent_dir));

         info.type          = type;
         info.directory_ptr = idx;
         info_path          = parent_dir;
         info_label         = menu_label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DIRECTORY_PUSH:
         if (!string_is_empty(path))
         {
            if (!string_is_empty(menu_path))
               fill_pathname_join_special(
                     tmp, menu_path, path, sizeof(tmp));
            else
               strlcpy(tmp, path, sizeof(tmp));
         }

         info.type          = type;
         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = menu_label;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DATABASE_MANAGER_LIST:
         {
            char lpl_basename[PATH_MAX_LENGTH];
            struct menu_state *menu_st = menu_state_get_ptr();
            filebrowser_clear_type();
            fill_pathname_join_special(tmp,
                  settings->paths.path_content_database,
                  path, sizeof(tmp));

            fill_pathname_base(lpl_basename, path, sizeof(lpl_basename));
            path_remove_extension(lpl_basename);
            menu_driver_set_thumbnail_system(
                  menu_st->userdata, lpl_basename, sizeof(lpl_basename));

            info.directory_ptr = idx;
            info_path          = tmp;
            info_label         = msg_hash_to_str(
                  MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST);
            info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST;
            dl_type            = DISPLAYLIST_GENERIC;
         }
         break;
         /* Pending clear */
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
      case ACTION_OK_DL_PL_THUMBNAILS_UPDATER_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_PL_THUMBNAILS_UPDATER_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_PL_THUMBNAILS_UPDATER_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
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

      case ACTION_OK_DL_CORE_CONTENT_DIRS_SUBDIR_LIST:
         fill_pathname_join_delim(tmp, path, label, ';',
               sizeof(tmp));
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = tmp;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_DIRS_SUBDIR_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_CORE_SYSTEM_FILES_LIST:
         info.type          = type;
         info.directory_ptr = idx;
         info_path          = path;
         info_label         = msg_hash_to_str(
               MENU_ENUM_LABEL_DEFERRED_CORE_SYSTEM_FILES_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_SYSTEM_FILES_LIST;
         dl_type            = DISPLAYLIST_PENDING_CLEAR;
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST:
         info.directory_ptr = idx;
         info_path          = dir_libretro;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_LIST);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_LIST;
         dl_type            = DISPLAYLIST_GENERIC;
         break;
      case ACTION_OK_DL_DEFERRED_CORE_LIST_SET:
         info.directory_ptr = idx;
         info_path          = dir_libretro;
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET);
         info.enum_idx      = MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET;
         dl_type            = DISPLAYLIST_GENERIC;

         /* Required for writing to correct playlist entry */
         menu->scratchpad.unsigned_var = (unsigned)type;
         break;
      case ACTION_OK_DL_CHEAT_DETAILS_SETTINGS_LIST:
#ifdef HAVE_CHEATS
         {
            rarch_setting_t *setting = NULL;

            cheat_manager_copy_idx_to_working(type-MENU_SETTINGS_CHEAT_BEGIN);
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_IDX);
            if (setting)
               setting->max = cheat_manager_get_size()-1;
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_VALUE);
            if (setting)
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size);
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_RUMBLE_VALUE);
            if (setting)
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.working_cheat.memory_search_size);
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION);
            if (setting)
            {
               int max_bit_position = cheat_manager_state.working_cheat.memory_search_size<3 ? 7 : 0;
               setting->max = max_bit_position;
            }
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_ADDRESS);
            if (setting)
               cheat_manager_state.browse_address = *setting->value.target.unsigned_integer;
            ACTION_OK_DL_LBL(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         }
#endif
         break;
      case ACTION_OK_DL_CHEAT_SEARCH_SETTINGS_LIST:
#ifdef HAVE_CHEATS
         {
            rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT);
            if (setting)
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS);
            if (setting)
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            setting = menu_setting_find_enum(MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS);
            if (setting)
               setting->max = cheat_manager_get_state_search_size(cheat_manager_state.search_bit_size);
            ACTION_OK_DL_LBL(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         }
#endif
         break;
      case ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST:
      {
         unsigned player_no = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN;
         ACTION_OK_DL_LBL(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         info.type          = MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_BEGIN + player_no;
      }
         break;
      case ACTION_OK_DL_ACCOUNTS_LIST:
      case ACTION_OK_DL_ACHIEVEMENTS_HARDCORE_PAUSE_LIST:
      case ACTION_OK_DL_INPUT_SETTINGS_LIST:
      case ACTION_OK_DL_INPUT_MENU_SETTINGS_LIST:
      case ACTION_OK_DL_INPUT_TURBO_FIRE_SETTINGS_LIST:
      case ACTION_OK_DL_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST:
      case ACTION_OK_DL_LATENCY_SETTINGS_LIST:
      case ACTION_OK_DL_DRIVER_SETTINGS_LIST:
      case ACTION_OK_DL_CORE_SETTINGS_LIST:
      case ACTION_OK_DL_CORE_INFORMATION_LIST:
#ifdef HAVE_MIST
      case ACTION_OK_DL_CORE_INFORMATION_STEAM_LIST:
#endif
      case ACTION_OK_DL_VIDEO_SETTINGS_LIST:
      case ACTION_OK_DL_VIDEO_HDR_SETTINGS_LIST:
      case ACTION_OK_DL_VIDEO_SYNCHRONIZATION_SETTINGS_LIST:
      case ACTION_OK_DL_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST:
      case ACTION_OK_DL_VIDEO_WINDOWED_MODE_SETTINGS_LIST:
      case ACTION_OK_DL_VIDEO_SCALING_SETTINGS_LIST:
      case ACTION_OK_DL_VIDEO_OUTPUT_SETTINGS_LIST:
      case ACTION_OK_DL_CRT_SWITCHRES_SETTINGS_LIST:
      case ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST:
      case ACTION_OK_DL_SAVING_SETTINGS_LIST:
      case ACTION_OK_DL_CLOUD_SYNC_SETTINGS_LIST:
      case ACTION_OK_DL_LOGGING_SETTINGS_LIST:
      case ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST:
      case ACTION_OK_DL_FRAME_TIME_COUNTER_SETTINGS_LIST:
      case ACTION_OK_DL_REWIND_SETTINGS_LIST:
      case ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST:
      case ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST:
      case ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST:
      case ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST:
      case ACTION_OK_DL_OSK_OVERLAY_SETTINGS_LIST:
      case ACTION_OK_DL_MENU_SETTINGS_LIST:
#ifdef _3DS
      case ACTION_OK_DL_MENU_BOTTOM_SETTINGS_LIST:
#endif
      case ACTION_OK_DL_MENU_VIEWS_SETTINGS_LIST:
      case ACTION_OK_DL_SETTINGS_VIEWS_SETTINGS_LIST:
      case ACTION_OK_DL_QUICK_MENU_VIEWS_SETTINGS_LIST:
      case ACTION_OK_DL_QUICK_MENU_OVERRIDE_OPTIONS_LIST:
      case ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST:
      case ACTION_OK_DL_AI_SERVICE_SETTINGS_LIST:
      case ACTION_OK_DL_ACCESSIBILITY_SETTINGS_LIST:
      case ACTION_OK_DL_POWER_MANAGEMENT_SETTINGS_LIST:
      case ACTION_OK_DL_CPU_PERFPOWER_SETTINGS_LIST:
      case ACTION_OK_DL_CPU_POLICY_SETTINGS_LIST:
      case ACTION_OK_DL_MENU_SOUNDS_LIST:
      case ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST:
      case ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST:
      case ACTION_OK_DL_CHEEVOS_APPEARANCE_SETTINGS_LIST:
      case ACTION_OK_DL_CHEEVOS_VISIBILITY_SETTINGS_LIST:
      case ACTION_OK_DL_UPDATER_SETTINGS_LIST:
      case ACTION_OK_DL_NETWORK_SETTINGS_LIST:
      case ACTION_OK_DL_NETWORK_HOSTING_SETTINGS_LIST:
      case ACTION_OK_DL_NETPLAY_KICK_LIST:
      case ACTION_OK_DL_NETPLAY_BAN_LIST:
      case ACTION_OK_DL_NETPLAY_LOBBY_FILTERS_LIST:
      case ACTION_OK_DL_SUBSYSTEM_SETTINGS_LIST:
      case ACTION_OK_DL_BLUETOOTH_SETTINGS_LIST:
      case ACTION_OK_DL_WIFI_SETTINGS_LIST:
      case ACTION_OK_DL_WIFI_NETWORKS_LIST:
      case ACTION_OK_DL_NETPLAY:
      case ACTION_OK_DL_NETPLAY_LAN_SCAN_SETTINGS_LIST:
      case ACTION_OK_DL_LAKKA_SERVICES_LIST:
#ifdef HAVE_LAKKA_SWITCH
      case ACTION_OK_DL_LAKKA_SWITCH_OPTIONS_LIST:
#endif
      case ACTION_OK_DL_USER_SETTINGS_LIST:
      case ACTION_OK_DL_DIRECTORY_SETTINGS_LIST:
      case ACTION_OK_DL_PRIVACY_SETTINGS_LIST:
      case ACTION_OK_DL_MIDI_SETTINGS_LIST:
      case ACTION_OK_DL_AUDIO_SETTINGS_LIST:
      case ACTION_OK_DL_AUDIO_SYNCHRONIZATION_SETTINGS_LIST:
      case ACTION_OK_DL_AUDIO_OUTPUT_SETTINGS_LIST:
#ifdef HAVE_MICROPHONE
      case ACTION_OK_DL_MICROPHONE_SETTINGS_LIST:
#endif
      case ACTION_OK_DL_AUDIO_MIXER_SETTINGS_LIST:
      case ACTION_OK_DL_INPUT_RETROPAD_BINDS_LIST:
      case ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST:
      case ACTION_OK_DL_RECORDING_SETTINGS_LIST:
      case ACTION_OK_DL_PLAYLIST_SETTINGS_LIST:
      case ACTION_OK_DL_PLAYLIST_MANAGER_LIST:
      case ACTION_OK_DL_PLAYLIST_MANAGER_SETTINGS:
      case ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST:
      case ACTION_OK_DL_ACCOUNTS_YOUTUBE_LIST:
      case ACTION_OK_DL_ACCOUNTS_TWITCH_LIST:
      case ACTION_OK_DL_ACCOUNTS_FACEBOOK_LIST:
      case ACTION_OK_DL_PLAYLIST_COLLECTION:
      case ACTION_OK_DL_FAVORITES_LIST:
      case ACTION_OK_DL_BROWSE_URL_LIST:
      case ACTION_OK_DL_MUSIC_LIST:
      case ACTION_OK_DL_IMAGES_LIST:
      case ACTION_OK_DL_LOAD_DISC_LIST:
      case ACTION_OK_DL_DUMP_DISC_LIST:
#ifdef HAVE_LAKKA
      case ACTION_OK_DL_EJECT_DISC:
#endif
      case ACTION_OK_DL_SHADER_PRESET_REMOVE:
      case ACTION_OK_DL_SHADER_PRESET_SAVE:
      case ACTION_OK_DL_CDROM_INFO_LIST:
      case ACTION_OK_DL_MANUAL_CONTENT_SCAN_LIST:
      case ACTION_OK_DL_CORE_MANAGER_LIST:
#ifdef HAVE_MIST
      case ACTION_OK_DL_STEAM_SETTINGS_LIST:
      case ACTION_OK_DL_CORE_MANAGER_STEAM_LIST:
#endif
      case ACTION_OK_DL_CORE_OPTION_OVERRIDE_LIST:
      case ACTION_OK_DL_REMAP_FILE_MANAGER_LIST:
         ACTION_OK_DL_LBL(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         break;
      case ACTION_OK_DL_CDROM_INFO_DETAIL_LIST:
      case ACTION_OK_DL_CORE_RESTORE_BACKUP_LIST:
      case ACTION_OK_DL_CORE_DELETE_BACKUP_LIST:
         ACTION_OK_DL_LBL(action_ok_dl_to_enum(action_type), DISPLAYLIST_GENERIC);
         info_path          = label;
         break;
      case ACTION_OK_DL_CONTENT_SETTINGS:
         info.list          = MENU_LIST_GET_SELECTION(menu_list, 0);
         info_path          = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS);
         info_label         = msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS);
         info.enum_idx      = MENU_ENUM_LABEL_CONTENT_SETTINGS;
         menu_entries_append(menu_stack, info_path, info_label,
               MENU_ENUM_LABEL_CONTENT_SETTINGS,
               0, 0, 0, NULL);
         dl_type            = DISPLAYLIST_CONTENT_SETTINGS;
         break;
   }

   if (info_label)
      info.label = strdup(info_label);
   if (info_path)
      info.path  = strdup(info_path);

   if (menu_displaylist_ctl(dl_type, &info, settings))
   {
      if (menu_displaylist_process(&info))
      {
         menu_displaylist_info_free(&info);
         return 0;
      }
   }

   menu_displaylist_info_free(&info);
   return -1;
}

/**
 * menu_content_find_first_core:
 * @core_info            : Core info list handle.
 * @dir                  : Directory. Gets joined with @path.
 * @path                 : Path. Gets joined with @dir.
 * @menu_label           : Label identifier of menu setting.
 * @s                    : Deferred core path. Will be filled in
 *                         by function.
 * @len                  : Size of @s.
 *
 * Gets deferred core.
 *
 * Returns: false if there are multiple deferred cores and a
 * selection needs to be made from a list, otherwise
 * returns true and fills in @s with path to core.
 **/
static bool menu_content_find_first_core(menu_content_ctx_defer_info_t *def_info,
      bool load_content_with_current_core,
      char *new_core_path, size_t len)
{
   const core_info_t *info                 = NULL;
   size_t supported                        = 0;
   core_info_list_t *core_info             = (core_info_list_t*)def_info->data;
   const char *default_info_dir            = def_info->dir;

   if (!string_is_empty(default_info_dir))
   {
      const char *default_info_path = def_info->path;
      size_t default_info_length    = def_info->len;

      if (!string_is_empty(default_info_path))
         fill_pathname_join_special(def_info->s,
               default_info_dir, default_info_path,
               default_info_length);

#ifdef HAVE_COMPRESSION
      if (path_is_compressed_file(default_info_dir))
      {
         size_t _len = strlen(default_info_dir);
         /* In case of a compressed archive, we have to join with a hash */
         /* We are going to write at the position of dir: */
         def_info->s[_len] = '#';
      }
#endif
   }

   if (core_info)
      core_info_list_get_supported_cores(core_info,
            def_info->s, &info,
            &supported);

   /* We started the menu with 'Load Content', we are
    * going to use the current core to load this. */
   if (load_content_with_current_core)
   {
      core_info_get_current_core((core_info_t**)&info);
      if (info)
         supported = 1;
   }

   /* There are multiple deferred cores and a
    * selection needs to be made from a list, return 0. */
   if (supported != 1)
      return false;

    if (info)
      strlcpy(new_core_path, info->path, len);

   return true;
}

#ifdef HAVE_LIBRETRODB
void handle_dbscan_finished(retro_task_t *task,
      void *task_data, void *user_data, const char *err);
#endif

static void menu_driver_set_last_start_content(struct menu_state *menu_st, const char *start_content_path)
{
   char archive_path[PATH_MAX_LENGTH];
   menu_handle_t *menu         = menu_st->driver_data;
   settings_t *settings        = config_get_ptr();
   bool use_last               = settings->bools.use_last_start_directory;
   const char *archive_delim   = NULL;
   const char *file_name       = NULL;

   if (!menu)
      return;

   /* Reset existing cache */
   menu->last_start_content.directory[0] = '\0';
   menu->last_start_content.file_name[0] = '\0';

   /* If 'use_last_start_directory' is disabled or
    * path is empty, do nothing */
   if (!use_last ||
       string_is_empty(start_content_path))
      return;

   /* Cache directory */
   fill_pathname_parent_dir(menu->last_start_content.directory,
         start_content_path, sizeof(menu->last_start_content.directory));

   /* Cache file name */
   if ((archive_delim = path_get_archive_delim(start_content_path)))
   {
      /* If path references a file inside an
       * archive, must extract the string segment
       * before the archive delimiter (i.e. path of
       * 'parent' archive file) */
      size_t len      = (size_t)(1 + archive_delim - start_content_path);
      if (len >= PATH_MAX_LENGTH)
         len          = PATH_MAX_LENGTH;

      strlcpy(archive_path, start_content_path, len * sizeof(char));

      file_name       = path_basename(archive_path);
   }
   else
      file_name       = path_basename_nocompression(start_content_path);

   if (!string_is_empty(file_name))
      strlcpy(menu->last_start_content.file_name, file_name,
            sizeof(menu->last_start_content.file_name));
}


static int file_load_with_detect_core_wrapper(
      enum msg_hash_enums enum_label_idx,
      size_t idx, size_t entry_idx,
      const char *path, const char *label,
      unsigned type, bool is_carchive)
{
   int ret                             = 0;
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;

   if (!menu)
      return -1;

   {
      menu_content_ctx_defer_info_t def_info;
      char menu_path_new[PATH_MAX_LENGTH];
      char new_core_path[PATH_MAX_LENGTH];
      const char *menu_path                  = NULL;
      const char *menu_label                 = NULL;
      core_info_list_t *list                 = NULL;
      new_core_path[0]    = menu_path_new[0] = '\0';

      menu_entries_get_last_stack(&menu_path, &menu_label, NULL, NULL, NULL);

      if (!string_is_empty(menu_path))
         strlcpy(menu_path_new, menu_path, sizeof(menu_path_new));

      if (string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)))
         fill_pathname_join_special(menu_path_new,
               menu->scratch2_buf, menu->scratch_buf, sizeof(menu_path_new));
      else if (string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN)))
         fill_pathname_join_special(menu_path_new,
               menu->scratch2_buf, menu->scratch_buf, sizeof(menu_path_new));

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
         fill_pathname_join_special(menu->detect_content_path,
               menu_path_new, path,
               sizeof(menu->detect_content_path));

      /* Return to idx 0 */
      if (enum_label_idx == MENU_ENUM_LABEL_COLLECTION)
         return generic_action_ok_displaylist_push(
               path, NULL,
               NULL, menu->rpl_entry_selection_ptr,
               0, entry_idx,
               ACTION_OK_DL_DEFERRED_CORE_LIST_SET);

      switch (ret)
      {
         case -1:
            {
               content_ctx_info_t content_info;

               content_info.argc        = 0;
               content_info.argv        = NULL;
               content_info.args        = NULL;
               content_info.environ_get = NULL;

               if (!task_push_load_content_with_new_core_from_menu(
                        new_core_path, def_info.s,
                        &content_info,
                        CORE_TYPE_PLAIN,
                        NULL, NULL))
                  return -1;

               menu_driver_set_last_start_content(menu_st, def_info.s);

               ret = 0;
               break;
            }
         case 0:
            ret = generic_action_ok_displaylist_push(
                  path, NULL,
                  label, type,
                  idx, entry_idx,
                  ACTION_OK_DL_DEFERRED_CORE_LIST);
            break;
         default:
            break;
      }
   }

   return ret;
}

static int action_ok_file_load_with_detect_core_carchive(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu                 = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   fill_pathname_join_delim(menu->detect_content_path,
         menu->detect_content_path, path,
         '#', sizeof(menu->detect_content_path));

   type = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(
         MSG_UNKNOWN, idx, entry_idx,
         path, label, type, true);
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{

   type  = 0;
   label = NULL;

   return file_load_with_detect_core_wrapper(
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
         idx, entry_idx,
         path, label, type, false);
}

static int set_path_generic(const char *label, const char *action_path)
{
   rarch_setting_t *setting = menu_setting_find(label);

   if (setting)
   {
      if (setting->value.target.string)
         strlcpy(setting->value.target.string, action_path, setting->size);
      if (setting->change_handler)
         setting->change_handler(setting);
      return menu_setting_generic(setting, 0, false);
   }

   return 0;
}

int generic_action_ok_command(enum event_command cmd)
{
#ifdef HAVE_AUDIOMIXER
   settings_t *settings      = config_get_ptr();
   bool audio_enable_menu    = settings->bools.audio_enable_menu;
   bool audio_enable_menu_ok = settings->bools.audio_enable_menu_ok;

   if (audio_enable_menu && audio_enable_menu_ok)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
#endif

   if (!command_event(cmd, NULL))
      return -1;
   return 0;
}

static int generic_action_ok(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned id, enum msg_hash_enums flush_id)
{
   char action_path[PATH_MAX_LENGTH];
   unsigned flush_type               = 0;
   int ret                           = 0;
   enum msg_hash_enums enum_idx      = MSG_UNKNOWN;
   const char             *menu_path = NULL;
   const char            *menu_label = NULL;
   const char *flush_char            = NULL;
   struct menu_state *menu_st        = menu_state_get_ptr();
   menu_handle_t *menu               = menu_st->driver_data;
   settings_t *settings              = config_get_ptr();
#ifdef HAVE_AUDIOMIXER
   bool audio_enable_menu            = settings->bools.audio_enable_menu;
   bool audio_enable_menu_ok         = settings->bools.audio_enable_menu_ok;
   if (audio_enable_menu && audio_enable_menu_ok)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
#endif

   if (!menu)
      return -1;

   menu_entries_get_last_stack(&menu_path,
         &menu_label, NULL, &enum_idx, NULL);

   if (!string_is_empty(path))
      fill_pathname_join_special(action_path,
            menu_path, path, sizeof(action_path));
   else
      strlcpy(action_path, menu_path, sizeof(action_path));

   switch (id)
   {
      case ACTION_OK_LOAD_WALLPAPER:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST);
         if (filestream_exists(action_path))
         {
            configuration_set_string(settings,
                  settings->paths.path_menu_wallpaper,
                  action_path);

            task_push_image_load(action_path,
                  video_driver_supports_rgba(), 0,
                  menu_display_handle_wallpaper_upload, NULL);
         }
         break;
      case ACTION_OK_LOAD_CORE:
         {
            content_ctx_info_t content_info;

            content_info.argc        = 0;
            content_info.argv        = NULL;
            content_info.args        = NULL;
            content_info.environ_get = NULL;

            flush_type = MENU_SETTINGS;

            if (!task_push_load_new_core(
                     action_path, NULL,
                     &content_info,
                     CORE_TYPE_PLAIN,
                     NULL, NULL))
            {
#ifndef HAVE_DYNAMIC
               ret = -1;
#endif
            }
         }
         break;
      case ACTION_OK_LOAD_CONFIG_FILE:
#ifdef HAVE_CONFIGFILE
         {
            bool config_save_on_exit        = settings->bools.config_save_on_exit;
            flush_type                      = MENU_SETTINGS;

            disp_get_ptr()->flags          |= GFX_DISP_FLAG_MSG_FORCE;

            if (config_replace(config_save_on_exit, action_path))
            {
               bool pending_push            = false;
               menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
               ret = -1;
            }
         }
#endif
         break;
      case ACTION_OK_LOAD_PRESET:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            struct video_shader      *shader  = menu_shader_get();
            flush_char = msg_hash_to_str(flush_id);

            /* Cache selected shader parent directory/file name */
            menu_driver_set_last_shader_preset_path(action_path);

            /* Check whether this a load or append action */
            if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND)) ||
                string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND)))
               menu_shader_manager_append_preset(shader, action_path,
                                                 string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND)));
            else
               menu_shader_manager_set_preset(shader,
                     menu_driver_get_last_shader_preset_type(),
                     action_path,
                     true);
         }
#endif
         break;
      case ACTION_OK_LOAD_SHADER_PASS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            struct video_shader *shader           = menu_shader_get();
            struct video_shader_pass *shader_pass = shader ? &shader->pass[menu->scratchpad.unsigned_var] : NULL;
            flush_char                            = msg_hash_to_str((enum msg_hash_enums)flush_id);

            if (shader_pass)
            {
               /* Cache selected shader parent directory/file name */
               menu_driver_set_last_shader_pass_path(action_path);

               strlcpy(
                     shader_pass->source.path,
                     action_path,
                     sizeof(shader_pass->source.path));
               video_shader_resolve_parameters(shader);

               shader->flags |= SHDR_FLAG_MODIFIED;
            }
         }
#endif
         break;
      case ACTION_OK_LOAD_STREAM_CONFIGFILE:
         flush_char       = msg_hash_to_str(flush_id);
         if (settings)
         {
            configuration_set_string(settings,
                  settings->paths.path_stream_config, action_path);
         }
         break;
      case ACTION_OK_LOAD_RECORD_CONFIGFILE:
         flush_char           = msg_hash_to_str(flush_id);
         if (settings)
         {
            configuration_set_string(settings,
                  settings->paths.path_record_config, action_path);
         }
         break;
      case ACTION_OK_LOAD_REMAPPING_FILE:
#ifdef HAVE_CONFIGFILE
         {
            char conf_key[64];
            config_file_t     *conf = config_file_new_from_path_to_string(
                  action_path);
            retro_ctx_controller_info_t pad;
            unsigned current_device = 0;
            unsigned port           = 0;
            int conf_val            = 0;
            flush_char              = msg_hash_to_str(flush_id);

            conf_key[0]             = '\0';

            if (conf)
            {
               if (input_remapping_load_file(conf, action_path))
               {
                  size_t _len = strlcpy(conf_key, "input_libretro_device_p", sizeof(conf_key));
                  for (port = 0; port < MAX_USERS; port++)
                  {
                     snprintf(conf_key + _len, sizeof(conf_key) - _len, "%u", port + 1);
                     if (!config_get_int(conf, conf_key, &conf_val))
                        continue;

                     current_device = input_config_get_device(port);
                     input_config_set_device(port, current_device);
                     pad.port       = port;
                     pad.device     = current_device;
                     core_set_controller_port_device(&pad);
                  }
               }
               config_file_free(conf);
               conf = NULL;
            }
         }
#endif
         break;
      case ACTION_OK_LOAD_OVERRIDE_FILE:
#ifdef HAVE_CONFIGFILE
         flush_char = msg_hash_to_str(flush_id);
         config_unload_override();

         if (!config_load_override_file(action_path))
            return -1;
#endif
         break;
      case ACTION_OK_LOAD_CHEAT_FILE:
#ifdef HAVE_CHEATS
         flush_char = msg_hash_to_str(flush_id);
         cheat_manager_state_free();

         if (!cheat_manager_load(action_path,false))
            return -1;
#endif
         break;
      case ACTION_OK_LOAD_CHEAT_FILE_APPEND:
#ifdef HAVE_CHEATS
         flush_char = msg_hash_to_str(flush_id);

         if (!cheat_manager_load(action_path,true))
            return -1;
#endif
         break;
      case ACTION_OK_LOAD_RGUI_MENU_THEME_PRESET:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MENU_SETTINGS_LIST);
         if (settings)
         {
            configuration_set_string(settings,
                  settings->paths.path_rgui_theme_preset, action_path);
         }
         break;
      case ACTION_OK_SUBSYSTEM_ADD:
         flush_type = MENU_SETTINGS;
         content_add_subsystem(action_path);
         break;
      case ACTION_OK_SET_DIRECTORY:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DIRECTORY_SETTINGS_LIST);
#ifdef HAVE_COCOATOUCH
         /* For iOS, set the path using realpath because the
          * path name can start with /private and this ensures
          * the path starts with it.
          *
          * This will allow the path to be properly substituted
          * when fill_pathname_expand_special
          * is called.
          */
         {
            char real_action_path[PATH_MAX_LENGTH];
            real_action_path[0] = '\0';
            realpath(action_path, real_action_path);
            strlcpy(action_path, real_action_path, sizeof(action_path));
         }
#endif
         ret        = set_path_generic(menu->filebrowser_label, action_path);
         break;
      case ACTION_OK_SET_PATH_VIDEO_FILTER:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH_AUDIO_FILTER:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH_OVERLAY:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ONSCREEN_OVERLAY_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH_OSK_OVERLAY:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_OSK_OVERLAY_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH_VIDEO_FONT:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_MANUAL_CONTENT_SCAN_DAT_FILE:
         flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST);
         ret        = set_path_generic(menu_label, action_path);
         break;
      case ACTION_OK_SET_PATH:
         flush_type = MENU_SETTINGS;
         ret        = set_path_generic(menu_label, action_path);
         break;
      default:
         flush_char = msg_hash_to_str(flush_id);
         break;
   }

   menu_entries_flush_stack(flush_char, flush_type);

   return ret;
}

static int default_action_ok_load_content_with_core_from_menu(const char *_path, unsigned _type)
{
   content_ctx_info_t content_info;
   struct menu_state *menu_st          = menu_state_get_ptr();
   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;
   if (!task_push_load_content_with_core(
            _path, &content_info,
            (enum rarch_core_type)_type, NULL, NULL))
      return -1;
   menu_driver_set_last_start_content(menu_st, _path);
   return 0;
}

static int default_action_ok_load_content_from_playlist_from_menu(const char *_path,
      const char *path, const char *entry_label)
{
   content_ctx_info_t content_info;
   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;
   if (!task_push_load_content_from_playlist_from_menu(
            _path, path, entry_label,
            &content_info,
            NULL, NULL))
      return -1;
   return 0;
}

DEFAULT_ACTION_OK_SET(action_ok_set_path_audiofilter, ACTION_OK_SET_PATH_AUDIO_FILTER, MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_set_path_videofilter, ACTION_OK_SET_PATH_VIDEO_FILTER, MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_set_path_overlay,     ACTION_OK_SET_PATH_OVERLAY,      MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_set_path_osk_overlay, ACTION_OK_SET_PATH_OSK_OVERLAY,  MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_set_path_video_font,  ACTION_OK_SET_PATH_VIDEO_FONT,   MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_set_path,             ACTION_OK_SET_PATH,              MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_load_core,            ACTION_OK_LOAD_CORE,             MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_config_load,          ACTION_OK_LOAD_CONFIG_FILE,      MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_subsystem_add,        ACTION_OK_SUBSYSTEM_ADD,         MSG_UNKNOWN)
DEFAULT_ACTION_OK_SET(action_ok_cheat_file_load,      ACTION_OK_LOAD_CHEAT_FILE,       MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)
DEFAULT_ACTION_OK_SET(action_ok_cheat_file_load_append,      ACTION_OK_LOAD_CHEAT_FILE_APPEND,       MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS)
DEFAULT_ACTION_OK_SET(action_ok_record_configfile_load,      ACTION_OK_LOAD_RECORD_CONFIGFILE,       MENU_ENUM_LABEL_RECORDING_SETTINGS)
DEFAULT_ACTION_OK_SET(action_ok_stream_configfile_load,      ACTION_OK_LOAD_STREAM_CONFIGFILE,       MENU_ENUM_LABEL_RECORDING_SETTINGS)
DEFAULT_ACTION_OK_SET(action_ok_remap_file_load,      ACTION_OK_LOAD_REMAPPING_FILE,   MENU_ENUM_LABEL_DEFERRED_REMAP_FILE_MANAGER_LIST)
DEFAULT_ACTION_OK_SET(action_ok_override_file_load,   ACTION_OK_LOAD_OVERRIDE_FILE,    MENU_ENUM_LABEL_DEFERRED_QUICK_MENU_OVERRIDE_OPTIONS)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
DEFAULT_ACTION_OK_SET(action_ok_shader_preset_load,   ACTION_OK_LOAD_PRESET,           MENU_ENUM_LABEL_SHADER_OPTIONS)
DEFAULT_ACTION_OK_SET(action_ok_shader_pass_load,     ACTION_OK_LOAD_SHADER_PASS,      MENU_ENUM_LABEL_SHADER_OPTIONS)
#endif
DEFAULT_ACTION_OK_SET(action_ok_rgui_menu_theme_preset_load,  ACTION_OK_LOAD_RGUI_MENU_THEME_PRESET,  MENU_ENUM_LABEL_MENU_SETTINGS)
DEFAULT_ACTION_OK_SET(action_ok_set_manual_content_scan_dat_file, ACTION_OK_SET_MANUAL_CONTENT_SCAN_DAT_FILE, MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST)

static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char menu_path_new[PATH_MAX_LENGTH];
   char full_path_new[PATH_MAX_LENGTH];
   const char *menu_label              = NULL;
   const char *menu_path               = NULL;
   rarch_setting_t *setting            = NULL;
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;
   menu_list_t *menu_list              = menu_st->entries.list;
   file_list_t *menu_stack             = MENU_LIST_GET(menu_list, 0);

   if (filebrowser_get_type() == FILEBROWSER_SELECT_FILE_SUBSYSTEM)
   {
      /* TODO/FIXME - this path is triggered when we try to load a
       * file from an archive while inside the load subsystem
       * action */
      if (!menu)
         return -1;

      fill_pathname_join_special(menu_path_new,
            menu->scratch2_buf, menu->scratch_buf,
            sizeof(menu_path_new));
      switch (type)
      {
         case FILE_TYPE_IN_CARCHIVE:
            fill_pathname_join_delim(full_path_new, menu_path_new, path,
                  '#',sizeof(full_path_new));
            break;
         default:
            fill_pathname_join_special(full_path_new, menu_path_new, path,
                  sizeof(full_path_new));
            break;
      }

      content_add_subsystem(full_path_new);
      menu_entries_flush_stack(NULL, MENU_SETTINGS);
      return 0;
   }

   if (menu_stack && menu_stack->size)
   {
      menu_path  = menu_stack->list[menu_stack->size - 1].path;
      menu_label = menu_stack->list[menu_stack->size - 1].label;
   }

   if (!string_is_empty(menu_label))
      setting = menu_setting_find(menu_label);

   if (setting && setting->type == ST_PATH)
      return action_ok_set_path(path, label, type, idx, entry_idx);

   if (!string_is_empty(menu_path))
      strlcpy(menu_path_new, menu_path, sizeof(menu_path_new));

   if (!string_is_empty(menu_label))
   {
      if (
            string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE)) ||
            string_is_equal(menu_label,
               msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN))
         )
      {
         if (!menu)
            return -1;
         fill_pathname_join_special(menu_path_new,
               menu->scratch2_buf, menu->scratch_buf,
               sizeof(menu_path_new));
      }
   }

   switch (type)
   {
      case FILE_TYPE_IN_CARCHIVE:
         fill_pathname_join_delim(full_path_new, menu_path_new, path,
               '#',sizeof(full_path_new));
         break;
      default:
         fill_pathname_join_special(full_path_new, menu_path_new, path,
               sizeof(full_path_new));
         break;
   }

   return default_action_ok_load_content_with_core_from_menu(full_path_new,
         CORE_TYPE_PLAIN);
}

static bool playlist_entry_path_is_valid(const char *entry_path)
{
   char *archive_delim = NULL;
   char *file_path     = NULL;

   if (string_is_empty(entry_path))
      return false;

   file_path = strdup(entry_path);

   /* We need to check whether the file referenced by the
    * entry path actually exists. If it is a normal file,
    * we can do this directly. If the path contains an
    * archive delimiter, then we have to trim everything
    * after the archive extension
    * > Note: Have to do a nasty cast here, since
    *   path_get_archive_delim() returns a const char *
    *   (this cast is safe, though, and is done in many
    *   places throughout the codebase...) */
   archive_delim = (char *)path_get_archive_delim(file_path);

   if (archive_delim)
   {
      *archive_delim = '\0';
      if (string_is_empty(file_path))
         goto error;
   }

   /* Path is 'sanitised' - can now check if it exists */
   if (!path_is_valid(file_path))
      goto error;

   /* File is valid */
   free(file_path);
   file_path = NULL;

   return true;

error:
   free(file_path);
   file_path = NULL;

   return false;
}

static int action_ok_playlist_entry_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_config_t playlist_config;
   char content_path[PATH_MAX_LENGTH];
   char content_label[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   size_t selection_ptr                   = entry_idx;
   bool playlist_initialized              = false;
   playlist_t *playlist                   = NULL;
   playlist_t *tmp_playlist               = NULL;
   const struct playlist_entry *entry     = NULL;
   core_info_t* core_info                 = NULL;
   bool core_is_builtin                   = false;
   menu_handle_t *menu                    = menu_state_get_ptr()->driver_data;
   settings_t *settings                   = config_get_ptr();
   runloop_state_t *runloop_st            = runloop_state_get_ptr();
   bool playlist_sort_alphabetical        = settings->bools.playlist_sort_alphabetical;
   const char *path_content_history       = settings->paths.path_content_history;
   const char *path_content_image_history = settings->paths.path_content_image_history;
   const char *path_content_music_history = settings->paths.path_content_music_history;
   const char *path_content_video_history = settings->paths.path_content_video_history;

   playlist_config.capacity               = COLLECTION_SIZE;
   playlist_config.old_format             = settings->bools.playlist_use_old_format;
   playlist_config.compress               = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match    = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config, settings->bools.playlist_portable_paths ? settings->paths.directory_menu_content : NULL);

   content_path[0]  = '\0';
   content_label[0] = '\0';
   core_path[0]     = '\0';

   if (!menu)
      goto error;

   /* Get playlist */
   if (!(tmp_playlist = playlist_get_cached()))
   {
      /* If playlist is not cached, have to load
       * it here
       * > Since the menu will always sort playlists
       *   based on current user config, have to do
       *   the same here - otherwise entry_idx may
       *   go out of sync... */
      bool is_content_history = string_is_equal(menu->db_playlist_file, path_content_history) ||
                                string_is_equal(menu->db_playlist_file, path_content_image_history) ||
                                string_is_equal(menu->db_playlist_file, path_content_music_history) ||
                                string_is_equal(menu->db_playlist_file, path_content_video_history);

      enum playlist_sort_mode current_sort_mode;

      playlist_config_set_path(&playlist_config, menu->db_playlist_file);
      tmp_playlist = playlist_init(&playlist_config);

      if (!tmp_playlist)
         goto error;

      current_sort_mode = playlist_get_sort_mode(tmp_playlist);

      if (!is_content_history &&
          ((playlist_sort_alphabetical && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT)) ||
           (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL)))
         playlist_qsort(tmp_playlist);

      playlist_initialized = true;
   }

   playlist = tmp_playlist;

   /* Get playlist entry */
   playlist_get_index(playlist, selection_ptr, &entry);
   if (!entry)
      goto error;

   /* Cache entry path */
   if (!string_is_empty(entry->path))
   {
      strlcpy(content_path, entry->path, sizeof(content_path));
      playlist_resolve_path(PLAYLIST_LOAD, false, content_path, sizeof(content_path));
   }

   runloop_st->entry_state_slot = entry->entry_slot;

   /* Cache entry label */
   if (!string_is_empty(entry->label))
      strlcpy(content_label, entry->label, sizeof(content_label));

   /* Get core path */
   if (!playlist_entry_has_core(entry))
   {
      struct playlist_entry update_entry = {0};

      /* Entry core is not set - attempt to use
       * playlist default */
      core_info = playlist_get_default_core_info(playlist);

      /* If default core is not set, prompt user
       * to select one */
      if (!core_info)
      {
         /* TODO: figure out if this should refer to the inner or outer content_path */
         int ret = action_ok_file_load_with_detect_core_collection(content_path,
               label, type, selection_ptr, entry_idx);

         if (playlist_initialized && tmp_playlist)
         {
            playlist_free(tmp_playlist);
            tmp_playlist = NULL;
            playlist     = NULL;
         }

         return ret;
      }

      /* Update playlist entry */
      strlcpy(core_path, core_info->path, sizeof(core_path));
      playlist_resolve_path(PLAYLIST_SAVE, true, core_path, sizeof(core_path));
      update_entry.core_path = core_path;
      update_entry.core_name = core_info->display_name;

      command_playlist_update_write(
            playlist,
            selection_ptr,
            &update_entry);

      /* Cache core path */
      strlcpy(core_path, core_info->path, sizeof(core_path));
   }
   else
   {
      /* Entry does have a core assignment
       * > If core is 'built-in' (imageviewer, etc.),
       *   then copy the path without modification
       * > If this is a standard core, ensure
       *   it has a corresponding core info entry */
      if (string_ends_with_size(entry->core_path, "builtin",
               strlen(entry->core_path),
               STRLEN_CONST("builtin")))
      {
         strlcpy(core_path, entry->core_path, sizeof(core_path));
         core_is_builtin = true;
      }
      else
      {
#ifndef IOS
         core_info = playlist_entry_get_core_info(entry);

         if (core_info && !string_is_empty(core_info->path))
            strlcpy(core_path, core_info->path, sizeof(core_path));
         else
            /* Core path is invalid - just copy what we have
             * and hope for the best... */
#endif
         {
            strlcpy(core_path, entry->core_path, sizeof(core_path));
            playlist_resolve_path(PLAYLIST_LOAD, true, core_path, sizeof(core_path));
         }
      }
   }

   /* Ensure core path is valid */
   if (    string_is_empty(core_path)
       || (!core_is_builtin && !path_is_valid(core_path)))
      goto error;

   /* Subsystem codepath */
   if (!string_is_empty(entry->subsystem_ident))
   {
      content_ctx_info_t content_info = {0};
      size_t i;

      task_push_load_new_core(core_path, NULL,
            &content_info, CORE_TYPE_PLAIN, NULL, NULL);

      content_clear_subsystem();

      if (!content_set_subsystem_by_name(entry->subsystem_ident))
      {
         RARCH_LOG("[playlist] subsystem not found in implementation\n");
         goto error;
      }

      for (i = 0; i < entry->subsystem_roms->size; i++)
         content_add_subsystem(entry->subsystem_roms->elems[i].data);

      task_push_load_subsystem_with_core(
            NULL, &content_info,
            CORE_TYPE_PLAIN, NULL, NULL);

      /* TODO: update playlist entry? move to first position I guess? */
      if (playlist_initialized && tmp_playlist)
      {
         playlist_free(tmp_playlist);
         tmp_playlist = NULL;
         playlist     = NULL;
      }
      return 1;
   }

   /* Ensure entry path is valid */
   if (!playlist_entry_path_is_valid(content_path))
      goto error;

   /* Free temporary playlist, if required */
   if (playlist_initialized && tmp_playlist)
   {
      playlist_free(tmp_playlist);
      tmp_playlist = NULL;
      playlist     = NULL;
   }

   /* Note: Have to use cached entry label, since entry
    * may be free()'d by above playlist_free() - but need
    * to pass NULL explicitly if label is empty */
   return default_action_ok_load_content_from_playlist_from_menu(
         core_path, content_path, string_is_empty(content_label) ? NULL : content_label);

error:
   runloop_msg_queue_push(
         "File could not be loaded from playlist.\n",
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   if (playlist_initialized && tmp_playlist)
   {
      playlist_free(tmp_playlist);
      tmp_playlist = NULL;
      playlist     = NULL;
   }

   return -1;
}

#ifdef HAVE_AUDIOMIXER
static int action_ok_mixer_stream_action_play(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id           = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);
   if (state == AUDIO_STREAM_STATE_STOPPED)
      audio_driver_mixer_play_stream(stream_id);
   return 0;
}

static int action_ok_mixer_stream_action_play_looped(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id           = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);
   if (state == AUDIO_STREAM_STATE_STOPPED)
      audio_driver_mixer_play_stream_looped(stream_id);
   return 0;
}

static int action_ok_mixer_stream_action_play_sequential(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);
   if (state == AUDIO_STREAM_STATE_STOPPED)
      audio_driver_mixer_play_stream_sequential(stream_id);
   return 0;
}

static int action_ok_mixer_stream_action_remove(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);
   switch (state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_STOPPED:
         audio_driver_mixer_remove_stream(stream_id);
         break;
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   return 0;
}

static int action_ok_mixer_stream_action_stop(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned stream_id           = type - MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN;
   enum audio_mixer_state state = audio_driver_mixer_get_stream_state(stream_id);

   switch (state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         audio_driver_mixer_stop_stream(stream_id);
         break;
      case AUDIO_STREAM_STATE_STOPPED:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   return 0;
}
#endif

static int action_ok_load_cdrom(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_CDROM
   struct retro_system_info *sysinfo;

   if (!cdrom_drive_has_media(label[0]))
   {
      RARCH_LOG("[CDROM]: No media is inserted or drive is not ready.\n");

      runloop_msg_queue_push(
            msg_hash_to_str(MSG_NO_DISC_INSERTED),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      return -1;
   }

   sysinfo = &runloop_state_get_ptr()->system.info;

   if (sysinfo && !string_is_empty(sysinfo->library_name))
   {
      char cdrom_path[256] = {0};

      cdrom_device_fillpath(cdrom_path, sizeof(cdrom_path), label[0], 0, true);

      RARCH_LOG("[CDROM]: Loading disc from path: %s\n", cdrom_path);

      path_clear(RARCH_PATH_CONTENT);
      path_set(RARCH_PATH_CONTENT, cdrom_path);

#if defined(HAVE_DYNAMIC)
      {
         content_ctx_info_t content_info;

         content_info.argc        = 0;
         content_info.argv        = NULL;
         content_info.args        = NULL;
         content_info.environ_get = NULL;

         task_push_load_content_with_core(cdrom_path, &content_info, CORE_TYPE_PLAIN, NULL, NULL);
      }
#else
      frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS);
#endif
   }
   else
   {
      RARCH_LOG("[CDROM]: Cannot load disc without a core.\n");

      runloop_msg_queue_push(
         msg_hash_to_str(MSG_LOAD_CORE_FIRST),
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      return -1;
   }
#endif
   return 0;
}

static int action_ok_dump_cdrom(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (string_is_empty(label))
      return -1;
#ifdef HAVE_CDROM
   if (!cdrom_drive_has_media(label[0]))
   {
      RARCH_LOG("[CDROM]: No media is inserted or drive is not ready.\n");

      runloop_msg_queue_push(
            msg_hash_to_str(MSG_NO_DISC_INSERTED),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      return -1;
   }

   task_push_cdrom_dump(label);
#endif
   return 0;
}

#ifdef HAVE_LAKKA
static int action_ok_eject_disc(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_CDROM
   system("eject & disown");
#endif /* HAVE_CDROM */
   return 0;
}
#endif /* HAVE_LAKKA */

static int action_ok_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return menu_setting_set(type, MENU_ACTION_OK, false);
}

#ifdef HAVE_AUDIOMIXER
static int action_ok_audio_add_to_mixer(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *tmp_playlist            = playlist_get_cached();
   const struct playlist_entry *entry  = NULL;

   if (!tmp_playlist)
      return -1;

   playlist_get_index(tmp_playlist, entry_idx, &entry);

   if (filestream_exists(entry->path))
      task_push_audio_mixer_load(entry->path,
            NULL, NULL, false,
            AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC,
            0
            );

   return 0;
}

static int action_ok_audio_add_to_mixer_and_play(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *tmp_playlist            = playlist_get_cached();
   const struct playlist_entry *entry  = NULL;

   if (!tmp_playlist)
      return -1;

   playlist_get_index(tmp_playlist, entry_idx, &entry);

   if (filestream_exists(entry->path))
      task_push_audio_mixer_load_and_play(entry->path,
            NULL, NULL, false,
            AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC,
            0);

   return 0;
}

static int action_ok_audio_add_to_mixer_and_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char combined_path[PATH_MAX_LENGTH];
   struct playlist_entry entry = {0};
   menu_handle_t *menu         = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   fill_pathname_join_special(combined_path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(combined_path));

   /* The push function reads our entry as const,
      so these casts are safe */
   entry.path      = combined_path;
   entry.core_path = (char*)"builtin";
   entry.core_name = (char*)"musicplayer";

   command_playlist_push_write(g_defaults.music_history, &entry);

   if (filestream_exists(combined_path))
      task_push_audio_mixer_load(combined_path,
            NULL, NULL, false,
            AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC,
            0);

   return 0;
}

static int action_ok_audio_add_to_mixer_and_collection_and_play(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char combined_path[PATH_MAX_LENGTH];
   struct playlist_entry entry = {0};
   menu_handle_t *menu         = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   fill_pathname_join_special(combined_path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(combined_path));

   /* the push function reads our entry as const, so these casts are safe */
   entry.path      = combined_path;
   entry.core_path = (char*)"builtin";
   entry.core_name = (char*)"musicplayer";

   command_playlist_push_write(g_defaults.music_history, &entry);

   if (filestream_exists(combined_path))
      task_push_audio_mixer_load_and_play(combined_path,
            NULL, NULL, false,
            AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC,
            0);

   return 0;
}
#endif

static int action_ok_menu_wallpaper(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filebrowser_set_type(FILEBROWSER_SELECT_IMAGE);
   return action_ok_lookup_setting(path, label, type, idx, entry_idx);
}

static int action_ok_menu_wallpaper_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings            = config_get_ptr();

   filebrowser_clear_type();

   settings->uints.menu_xmb_shader_pipeline = XMB_SHADER_PIPELINE_WALLPAPER;
   return generic_action_ok(path, label, type, idx, entry_idx,
         ACTION_OK_LOAD_WALLPAPER, MSG_UNKNOWN);
}

int  generic_action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums id, enum menu_dialog_type id2)
{
   const char               *lbl  = msg_hash_to_str(id);

   return generic_action_ok_displaylist_push(
         path, NULL,
         lbl, id2,
         idx, entry_idx,
         ACTION_OK_DL_HELP);
}

#ifdef HAVE_BLUETOOTH
static int action_ok_bluetooth(const char *path, const char *label,
         unsigned type, size_t idx, size_t entry_idx)
{
   driver_bluetooth_connect_device((unsigned)idx);

   return 0;
}
#endif

#ifdef HAVE_NETWORKING
#ifdef HAVE_WIFI
static void menu_input_wifi_cb(void *userdata, const char *passphrase)
{
   struct menu_state *menu_st   = menu_state_get_ptr();
   unsigned idx                 = menu_st->input_dialog_kb_idx;
   wifi_network_scan_t *scan    = driver_wifi_get_ssids();
   wifi_network_info_t *netinfo = &scan->net_list[idx];

   if (     (idx < RBUF_LEN(scan->net_list))
         && passphrase)
   {
      /* Need to fill in the passphrase that we got from the user! */
      strlcpy(netinfo->passphrase, passphrase, sizeof(netinfo->passphrase));
      task_push_wifi_connect(NULL, netinfo);
   }

   menu_input_dialog_end();
}

static int action_ok_wifi(const char *path, const char *label_setting,
      unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_ctx_line_t line;
   wifi_network_scan_t* scan = driver_wifi_get_ssids();
   if (idx >= RBUF_LEN(scan->net_list))
      return -1;

   /* No need to ask for a password, should be stored */
   if (scan->net_list[idx].saved_password)
      task_push_wifi_connect(NULL, &scan->net_list[idx]);
   else
   {
      /* Show password input dialog */
      line.label         = "Passphrase";
      line.label_setting = label_setting;
      line.type          = type;
      line.idx           = (unsigned)idx;
      line.cb            = menu_input_wifi_cb;
      if (!menu_input_dialog_start(&line))
         return -1;
   }

   return 0;
}
#endif
#endif

static void menu_input_st_string_cb_rename_entry(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      const char        *label    = menu_input_dialog_get_buffer();

      if (!string_is_empty(label))
      {
         struct playlist_entry entry = {0};
         struct menu_state *menu_st   = menu_state_get_ptr();
         unsigned idx                 = menu_st->input_dialog_kb_idx;

         /* the update function reads our entry as const,
          * so these casts are safe */
         entry.label                  = (char*)label;
         command_playlist_update_write(NULL, idx, &entry);
      }
   }

   menu_input_dialog_end();
}

static void menu_input_st_string_cb_disable_kiosk_mode(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      const char                    *label = menu_input_dialog_get_buffer();
      settings_t                 *settings = config_get_ptr();
      const char *path_kiosk_mode_password =
         settings->paths.kiosk_mode_password;

      if (string_is_equal(label, path_kiosk_mode_password))
      {
         settings->bools.kiosk_mode_enable = false;

         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_KIOSK_MODE_PASSWORD_OK),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
      else
         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_KIOSK_MODE_PASSWORD_NOK),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   menu_input_dialog_end();
}

static void menu_input_st_string_cb_enable_settings(void *userdata,
      const char *str)
{
   if (str && *str)
   {
      const char                                 *label    =
         menu_input_dialog_get_buffer();
      settings_t                                 *settings = config_get_ptr();
      const char *menu_content_show_settings_password      = settings->paths.menu_content_show_settings_password;

      if (string_is_equal(label, menu_content_show_settings_password))
      {
         settings->bools.menu_content_show_settings = true;

         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
      else
         runloop_msg_queue_push(
            msg_hash_to_str(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   menu_input_dialog_end();
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
static int action_ok_shader_pass(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu       = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   menu->scratchpad.unsigned_var = type - MENU_SETTINGS_SHADER_PASS_0;
   return generic_action_ok_displaylist_push(
         path, NULL,
         label, type,
         idx, entry_idx,
         ACTION_OK_DL_SHADER_PASS);
}

static void menu_input_st_string_cb_save_preset(void *userdata,
      const char *str)
{
   if (!string_is_empty(str))
   {
      rarch_setting_t *setting     = NULL;
      bool                 ret     = false;
      struct menu_state *menu_st   = menu_state_get_ptr();
      const char *label            = menu_st->input_dialog_kb_label;
      settings_t *settings         = config_get_ptr();
      const char *dir_video_shader = settings->paths.directory_video_shader;
      const char *dir_menu_config  = settings->paths.directory_menu_config;

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         if (setting->value.target.string)
            strlcpy(setting->value.target.string, str, setting->size);
         if (setting->change_handler)
            setting->change_handler(setting);
         menu_setting_generic(setting, 0, false);
      }
      else if (!string_is_empty(label))
         ret = menu_shader_manager_save_preset(
               menu_shader_get(),
               str,
               dir_video_shader,
               dir_menu_config,
               true);

      if (ret)
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY),
               1, 100, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_SAVING_SHADER_PRESET),
               1, 100, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   menu_input_dialog_end();
}

DEFAULT_ACTION_DIALOG_START(action_ok_shader_preset_save_as,
   msg_hash_to_str(MSG_INPUT_PRESET_FILENAME),
   (unsigned)idx,
   menu_input_st_string_cb_save_preset)

enum
{
   ACTION_OK_SHADER_PRESET_SAVE_GLOBAL = 0,
   ACTION_OK_SHADER_PRESET_SAVE_CORE,
   ACTION_OK_SHADER_PRESET_SAVE_PARENT,
   ACTION_OK_SHADER_PRESET_SAVE_GAME
};

enum
{
   ACTION_OK_SHADER_PRESET_REMOVE_GLOBAL = 0,
   ACTION_OK_SHADER_PRESET_REMOVE_CORE,
   ACTION_OK_SHADER_PRESET_REMOVE_PARENT,
   ACTION_OK_SHADER_PRESET_REMOVE_GAME
};

static int generic_action_ok_shader_preset_remove(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   enum auto_shader_type preset_type;
   settings_t         *settings = config_get_ptr();
   const char *dir_video_shader = settings->paths.directory_video_shader;
   const char *dir_menu_config  = settings->paths.directory_menu_config;

   switch (action_type)
   {
      case ACTION_OK_SHADER_PRESET_REMOVE_GLOBAL:
         preset_type = SHADER_PRESET_GLOBAL;
         break;
      case ACTION_OK_SHADER_PRESET_REMOVE_CORE:
         preset_type = SHADER_PRESET_CORE;
         break;
      case ACTION_OK_SHADER_PRESET_REMOVE_PARENT:
         preset_type = SHADER_PRESET_PARENT;
         break;
      case ACTION_OK_SHADER_PRESET_REMOVE_GAME:
         preset_type = SHADER_PRESET_GAME;
         break;
      default:
         return 0;
   }

   if (menu_shader_manager_remove_auto_preset(preset_type,
         dir_video_shader, dir_menu_config))
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      menu_st->flags |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
   }
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_REMOVING_SHADER_PRESET),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int generic_action_ok_shader_preset_save(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
   enum auto_shader_type preset_type;
   settings_t      *settings     = config_get_ptr();
   const char *dir_video_shader  = settings->paths.directory_video_shader;
   const char *dir_menu_config   = settings->paths.directory_menu_config;

   switch (action_type)
   {
      case ACTION_OK_SHADER_PRESET_SAVE_GLOBAL:
         preset_type = SHADER_PRESET_GLOBAL;
         break;
      case ACTION_OK_SHADER_PRESET_SAVE_CORE:
         preset_type = SHADER_PRESET_CORE;
         break;
      case ACTION_OK_SHADER_PRESET_SAVE_PARENT:
         preset_type = SHADER_PRESET_PARENT;
         break;
      case ACTION_OK_SHADER_PRESET_SAVE_GAME:
         preset_type = SHADER_PRESET_GAME;
         break;
      default:
         return 0;
   }

   /* Save Auto Preset and have it immediately reapply the preset
    * TODO: This seems necessary so that the loaded shader gains a link to the file saved
    * But this is slow and seems like a redundant way to do this
    * It seems like it would be better to just set the path and shader_preset_loaded
    * on the current shader */
   if (menu_shader_manager_save_auto_preset(menu_shader_get(), preset_type,
            dir_video_shader, dir_menu_config,
            true))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_ERROR_SAVING_SHADER_PRESET),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_shader_preset_save_global(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_save(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_SAVE_GLOBAL);
}

static int action_ok_shader_preset_save_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_save(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_SAVE_CORE);
}

static int action_ok_shader_preset_save_parent(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_save(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_SAVE_PARENT);
}

static int action_ok_shader_preset_save_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_save(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_SAVE_GAME);
}

static int action_ok_shader_preset_remove_global(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_remove(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_REMOVE_GLOBAL);
}

static int action_ok_shader_preset_remove_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_remove(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_REMOVE_CORE);
}

static int action_ok_shader_preset_remove_parent(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_remove(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_REMOVE_PARENT);
}

static int action_ok_shader_preset_remove_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_shader_preset_remove(path, label, type,
         idx, entry_idx, ACTION_OK_SHADER_PRESET_REMOVE_GAME);
}
#endif


static int action_ok_video_filter_remove(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   settings_t *settings       = config_get_ptr();
   if (!settings)
      return -1;
   if (!string_is_empty(settings->paths.path_softfilter_plugin))
   {
      /* Unload video filter */
      settings->paths.path_softfilter_plugin[0] = '\0';
      command_event(CMD_EVENT_REINIT, NULL);
      /* Refresh menu */
      menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                      |  MENU_ST_FLAG_PREVENT_POPULATE;
   }
   return 0;
}

static int action_ok_audio_dsp_plugin_remove(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   settings_t *settings       = config_get_ptr();

   if (!settings)
      return -1;

   if (!string_is_empty(settings->paths.path_audio_dsp_plugin))
   {
      /* Unload DSP plugin filter */
      settings->paths.path_audio_dsp_plugin[0] = '\0';
      command_event(CMD_EVENT_DSP_FILTER_INIT, NULL);

      /* Refresh menu */
      menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                      |  MENU_ST_FLAG_PREVENT_POPULATE;
   }

   return 0;
}

#ifdef HAVE_CHEATS
static void menu_input_st_string_cb_cheat_file_save_as(
      void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t *setting        = NULL;
      struct menu_state *menu_st      = menu_state_get_ptr();
      const char *label               = menu_st->input_dialog_kb_label;
      settings_t *settings            = config_get_ptr();
      const char *path_cheat_database = settings->paths.path_cheat_database;

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         if (setting->value.target.string)
            strlcpy(setting->value.target.string, str, setting->size);
         if (setting->change_handler)
            setting->change_handler(setting);
         menu_setting_generic(setting, 0, false);
      }
      else if (!string_is_empty(label))
         cheat_manager_save(str, path_cheat_database,
               false);
   }

   menu_input_dialog_end();
}
#endif

DEFAULT_ACTION_DIALOG_START(action_ok_enable_settings,
   msg_hash_to_str(MSG_INPUT_ENABLE_SETTINGS_PASSWORD),
   (unsigned)entry_idx,
   menu_input_st_string_cb_enable_settings)
#ifdef HAVE_CHEATS
DEFAULT_ACTION_DIALOG_START(action_ok_cheat_file_save_as,
   msg_hash_to_str(MSG_INPUT_CHEAT_FILENAME),
   (unsigned)idx,
   menu_input_st_string_cb_cheat_file_save_as)
#endif
DEFAULT_ACTION_DIALOG_START(action_ok_disable_kiosk_mode,
   msg_hash_to_str(MSG_INPUT_KIOSK_MODE_PASSWORD),
   (unsigned)entry_idx,
   menu_input_st_string_cb_disable_kiosk_mode)
DEFAULT_ACTION_DIALOG_START(action_ok_rename_entry,
   msg_hash_to_str(MSG_INPUT_RENAME_ENTRY),
   (unsigned)entry_idx,
   menu_input_st_string_cb_rename_entry)


static int generic_action_ok_remap_file_operation(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      unsigned action_type)
{
#ifdef HAVE_CONFIGFILE
   char content_dir_name[PATH_MAX_LENGTH];
   char remap_file_path[PATH_MAX_LENGTH];
   struct menu_state *menu_st            = menu_state_get_ptr();
   rarch_system_info_t *sys_info         = &runloop_state_get_ptr()->system;
   const char *core_name                 = sys_info ? sys_info->info.library_name : NULL;
   const char *rarch_path_basename       = path_get(RARCH_PATH_BASENAME);
   bool has_content                      = !string_is_empty(rarch_path_basename);
   settings_t *settings                  = config_get_ptr();
   const char *directory_input_remapping = settings->paths.directory_input_remapping;

   content_dir_name[0] = '\0';
   remap_file_path[0]  = '\0';

   /* Cannot perform remap file operation if we
    * have no core */
   if (string_is_empty(core_name))
      return -1;

   switch (action_type)
   {
      case ACTION_OK_REMAP_FILE_SAVE_AS:
         fill_pathname_join_special_ext(remap_file_path,
               directory_input_remapping, core_name,
               path,
               FILE_PATH_REMAP_EXTENSION,
               sizeof(remap_file_path));
         break;
      case ACTION_OK_REMAP_FILE_SAVE_CORE:
      case ACTION_OK_REMAP_FILE_REMOVE_CORE:
         fill_pathname_join_special_ext(remap_file_path,
               directory_input_remapping, core_name,
               core_name,
               FILE_PATH_REMAP_EXTENSION,
               sizeof(remap_file_path));
         break;
      case ACTION_OK_REMAP_FILE_SAVE_GAME:
      case ACTION_OK_REMAP_FILE_REMOVE_GAME:
         if (has_content)
            fill_pathname_join_special_ext(remap_file_path,
                  directory_input_remapping, core_name,
                  path_basename(rarch_path_basename),
                  FILE_PATH_REMAP_EXTENSION,
                  sizeof(remap_file_path));
         break;
      case ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR:
      case ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR:
         if (has_content)
         {
            fill_pathname_parent_dir_name(content_dir_name,
                  rarch_path_basename, sizeof(content_dir_name));

            fill_pathname_join_special_ext(remap_file_path,
                  directory_input_remapping, core_name,
                  content_dir_name,
                  FILE_PATH_REMAP_EXTENSION,
                  sizeof(remap_file_path));
         }
         break;
   }

   if (action_type < ACTION_OK_REMAP_FILE_REMOVE_CORE)
   {
      if (  !string_is_empty(remap_file_path)
          && input_remapping_save_file(remap_file_path))
      {
         switch (action_type)
         {
            case ACTION_OK_REMAP_FILE_SAVE_CORE:
               retroarch_ctl(RARCH_CTL_SET_REMAPS_CORE_ACTIVE, NULL);
               break;
            case ACTION_OK_REMAP_FILE_SAVE_GAME:
               retroarch_ctl(RARCH_CTL_SET_REMAPS_GAME_ACTIVE, NULL);
               break;
            case ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR:
               retroarch_ctl(RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE, NULL);
               break;
         }

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_REMAP_FILE_SAVED_SUCCESSFULLY),
               1, 100, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_SAVING_REMAP_FILE),
               1, 100, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   else
   {
      if (   !string_is_empty(remap_file_path)
          && (filestream_delete(remap_file_path) == 0))
      {
         uint32_t flags = runloop_get_flags();
         switch (action_type)
         {
            case ACTION_OK_REMAP_FILE_REMOVE_CORE:
               if (flags & RUNLOOP_FLAG_REMAPS_CORE_ACTIVE)
                  input_remapping_deinit(false);
               break;
            case ACTION_OK_REMAP_FILE_REMOVE_GAME:
               if (flags & RUNLOOP_FLAG_REMAPS_GAME_ACTIVE)
                  input_remapping_deinit(false);
               break;
            case ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR:
               if (flags & RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE)
                  input_remapping_deinit(false);
               break;
         }

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_REMAP_FILE_REMOVED_SUCCESSFULLY),
               1, 100, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         /* After removing a remap file, attempt to
          * load any remaining remap file with the
          * next highest priority */
         config_load_remap(directory_input_remapping, sys_info);
      }
      else
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_ERROR_REMOVING_REMAP_FILE),
               1, 100, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   /* Refresh menu */
   menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                   |  MENU_ST_FLAG_PREVENT_POPULATE;
#endif
   return 0;
}

static void menu_input_st_string_cb_remap_file_save_as(
      void *userdata, const char *str)
{
#ifdef HAVE_CONFIGFILE
   if (str && *str)
   {
      rarch_setting_t *setting        = NULL;
      struct menu_state *menu_st      = menu_state_get_ptr();
      const char *label               = menu_st->input_dialog_kb_label;

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         if (setting->value.target.string)
            strlcpy(setting->value.target.string, str, setting->size);
         if (setting->change_handler)
            setting->change_handler(setting);
         menu_setting_generic(setting, 0, false);
      }
      else if (!string_is_empty(label))
         generic_action_ok_remap_file_operation(str, label, 0,
               0, 0, ACTION_OK_REMAP_FILE_SAVE_AS);
   }

   menu_input_dialog_end();
#endif
}

DEFAULT_ACTION_DIALOG_START(action_ok_remap_file_save_as,
   msg_hash_to_str(MSG_INPUT_REMAP_FILENAME),
   (unsigned)idx,
   menu_input_st_string_cb_remap_file_save_as)

static int action_ok_remap_file_save_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_CORE);
}

static int action_ok_remap_file_save_content_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_CONTENT_DIR);
}

static int action_ok_remap_file_save_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_SAVE_GAME);
}

static int action_ok_remap_file_remove_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_REMOVE_CORE);
}

static int action_ok_remap_file_remove_content_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_REMOVE_CONTENT_DIR);
}

static int action_ok_remap_file_remove_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_remap_file_operation(path, label, type,
         idx, entry_idx, ACTION_OK_REMAP_FILE_REMOVE_GAME);
}

static int action_ok_remap_file_reset(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   input_remapping_set_defaults(false);
   runloop_msg_queue_push(
         msg_hash_to_str(MSG_REMAP_FILE_RESET),
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   return 0;
}

static int action_ok_remap_file_flush(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *path_remapfile  = runloop_st->name.remapfile;
   const char *remapfile       = NULL;
   bool success                = false;
   char msg[256];

   msg[0] = '\0';

   /* Check if a remap file is active */
   if (!string_is_empty(path_remapfile))
   {
      /* Update existing remap file */
      success   = input_remapping_save_file(path_remapfile);
      /* Get remap file name for display purposes */
      remapfile = path_basename_nocompression(path_remapfile);
   }

   if (string_is_empty(remapfile))
      remapfile = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN);

   /* Log result */
   if (success)
   {
      /* TODO/FIXME - localize */
      RARCH_LOG(
            "[Remaps]: Saved input remapping options to \"%s\".\n",
            path_remapfile ? path_remapfile : "UNKNOWN");
      snprintf(msg, sizeof(msg), "%s \"%s\"",
            msg_hash_to_str(MSG_REMAP_FILE_FLUSHED),
            remapfile);
   }
   else
   {
      /* TODO/FIXME - localize */
      RARCH_LOG(
            "[Remaps]: Failed to save input remapping options to \"%s\".\n",
            path_remapfile ? path_remapfile : "UNKNOWN");
      snprintf(msg, sizeof(msg), "%s \"%s\"",
            msg_hash_to_str(MSG_REMAP_FILE_FLUSH_FAILED),
            remapfile);
   }

   runloop_msg_queue_push(
         msg, 1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static void menu_input_st_string_cb_override_file_save_as(
      void *userdata, const char *str)
{
#ifdef HAVE_CONFIGFILE
   if (str && *str)
   {
      rarch_setting_t *setting        = NULL;
      struct menu_state *menu_st      = menu_state_get_ptr();
      const char *label               = menu_st->input_dialog_kb_label;
      const char *msg_str             = NULL;
      int ret                         = false;

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         if (setting->value.target.string)
            strlcpy(setting->value.target.string, str, setting->size);
         if (setting->change_handler)
            setting->change_handler(setting);
         menu_setting_generic(setting, 0, false);
      }
      else if (!string_is_empty(label))
      {
         runloop_state_t *runloop_st  = runloop_state_get_ptr();
         ret = config_save_overrides(OVERRIDE_AS, &runloop_st->system, false, str);
      }

      switch (ret)
      {
         case 1:
            msg_str = msg_hash_to_str(MSG_OVERRIDES_SAVED_SUCCESSFULLY);
            break;
         case -1:
            msg_str = msg_hash_to_str(MSG_OVERRIDES_NOT_SAVED);
            break;
         default:
         case 0:
            msg_str = msg_hash_to_str(MSG_OVERRIDES_ERROR_SAVING);
            break;
      }

      runloop_msg_queue_push(
            msg_str,
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   menu_input_dialog_end();
#endif
}

DEFAULT_ACTION_DIALOG_START(action_ok_override_file_save_as,
   msg_hash_to_str(MSG_INPUT_OVERRIDE_FILENAME),
   (unsigned)idx,
   menu_input_st_string_cb_override_file_save_as)

static int action_ok_override_unload(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_CONFIGFILE
   if (config_unload_override())
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_OVERRIDES_UNLOADED_SUCCESSFULLY),
            1, 100, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#endif
   return 0;
}

int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filebrowser_clear_type();
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

static int action_ok_path_manual_scan_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char content_dir[PATH_MAX_LENGTH];
   const char *flush_char = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_MANUAL_CONTENT_SCAN_LIST);
   unsigned flush_type    = 0;
   const char *menu_path  = NULL;

   content_dir[0]         = '\0';

   /* 'Reset' file browser */
   filebrowser_clear_type();

   /* Get user-selected scan directory */
   menu_entries_get_last_stack(&menu_path,
         NULL, NULL, NULL, NULL);

   if (!string_is_empty(menu_path))
      strlcpy(content_dir, menu_path, sizeof(content_dir));

#ifdef HAVE_COCOATOUCH
   {
      /* For iOS, set the path using realpath because the path name
       * can start with /private and this ensures the path starts with it.
       * This will allow the path to be properly substituted when
       * fill_pathname_expand_special() is called. */
      char real_content_dir[PATH_MAX_LENGTH];
      real_content_dir[0] = '\0';
      realpath(content_dir, real_content_dir);
      strlcpy(content_dir, real_content_dir, sizeof(content_dir));
   }
#endif

   /* Update manual content scan settings */
   manual_content_scan_set_menu_content_dir(content_dir);

   /* Return to 'manual content scan' menu */
   menu_entries_flush_stack(flush_char, flush_type);

   return 0;
}

static int action_ok_core_deferred_set(const char *new_core_path,
      const char *content_label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t _len;
   char msg[128];
   char resolved_core_path[PATH_MAX_LENGTH];
   struct menu_state *menu_st    = menu_state_get_ptr();
   struct playlist_entry entry   = {0};
   size_t selection              = menu_st->selection_ptr;
   menu_handle_t *menu           = menu_st->driver_data;
   core_info_t *core_info        = NULL;
   const char *core_display_name = NULL;

   if (  !menu
       || string_is_empty(new_core_path))
      return -1;

   /* Get core display name */
   if (core_info_find(new_core_path, &core_info))
      core_display_name = core_info->display_name;

   if (string_is_empty(core_display_name))
      core_display_name = path_basename_nocompression(new_core_path);

   /* Get 'real' core path */
   strlcpy(resolved_core_path, new_core_path, sizeof(resolved_core_path));
   playlist_resolve_path(PLAYLIST_SAVE, true, resolved_core_path, sizeof(resolved_core_path));

   /* the update function reads our entry
    * as const, so these casts are safe */
   entry.core_path = (char*)resolved_core_path;
   entry.core_name = (char*)core_display_name;

   command_playlist_update_write(
         NULL,
         menu->scratchpad.unsigned_var,
         &entry);

   /* Provide visual feedback */
   _len = strlcpy(msg, msg_hash_to_str(MSG_SET_CORE_ASSOCIATION), sizeof(msg));
   strlcpy(msg + _len, core_display_name, sizeof(msg) - _len);
   runloop_msg_queue_push(msg, 1, 100, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   menu_entries_pop_stack(&selection, 0, 1);
   menu_st->selection_ptr = selection;

   return 0;
}

static int action_ok_deferred_list_stub(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}

#if defined(HAVE_LIBNX)
static int action_ok_set_switch_cpu_profile(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char command[PATH_MAX_LENGTH] = {0};
   unsigned profile_clock          = SWITCH_CPU_SPEEDS_VALUES[entry_idx];
   settings_t *settings            = config_get_ptr();

   settings->uints.libnx_overclock = entry_idx;

   if (hosversionBefore(8, 0, 0))
      pcvSetClockRate(PcvModule_CpuBus, (u32)profile_clock);
   else
   {
      ClkrstSession session = {0};
      clkrstOpenSession(&session, PcvModuleId_CpuBus, 3);
      clkrstSetClockRate(&session, profile_clock);
      clkrstCloseSession(&session);
   }
   /* TODO/FIXME - localize */
   snprintf(command, sizeof(command),
         "Current Clock set to %i", profile_clock);

   runloop_msg_queue_push(command, 1, 90, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return -1;
}
#endif

static int action_ok_load_core_deferred(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   content_ctx_info_t content_info;
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   if (!menu)
      return -1;

   if (!task_push_load_content_with_new_core_from_menu(
            path, menu->deferred_path,
            &content_info,
            CORE_TYPE_PLAIN,
            NULL, NULL))
      return -1;
   menu_driver_set_last_start_content(menu_st, path);

   return 0;
}

DEFAULT_ACTION_OK_START_BUILTIN_CORE(action_ok_start_net_retropad_core, CORE_TYPE_NETRETROPAD)
DEFAULT_ACTION_OK_START_BUILTIN_CORE(action_ok_start_video_processor_core, CORE_TYPE_VIDEO_PROCESSOR)

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
static int action_ok_file_load_ffmpeg(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char new_path[PATH_MAX_LENGTH];
   const char *menu_path           = NULL;
   struct menu_state *menu_st      = menu_state_get_ptr();
   menu_handle_t *menu             = menu_st->driver_data;
   menu_list_t *menu_list          = menu_st->entries.list;
   file_list_t *menu_stack         = MENU_LIST_GET(menu_list, 0);

   if (menu_stack && menu_stack->size)
      menu_path = menu_stack->list[menu_stack->size - 1].path;

   if (string_is_empty(menu_path))
      return -1;
   fill_pathname_join_special(new_path, menu_path, path,
         sizeof(new_path));

   /* TODO/FIXME - should become runtime optional */
#ifdef HAVE_MPV
   return default_action_ok_load_content_with_core_from_menu(
         new_path, CORE_TYPE_MPV);
#else
   return default_action_ok_load_content_with_core_from_menu(
         new_path, CORE_TYPE_FFMPEG);
#endif
}
#endif

static int action_ok_audio_run(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char combined_path[PATH_MAX_LENGTH];
   menu_handle_t *menu                 = menu_state_get_ptr()->driver_data;
   if (!menu)
      return -1;

   fill_pathname_join_special(combined_path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(combined_path));

   /* TODO/FIXME - should become runtime optional */
#ifdef HAVE_MPV
   return default_action_ok_load_content_with_core_from_menu(
         combined_path, CORE_TYPE_MPV);
#else
   return default_action_ok_load_content_with_core_from_menu(
         combined_path, CORE_TYPE_FFMPEG);
#endif
}

int action_ok_core_option_dropdown_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char option_path_str[256];
   char option_lbl_str[256];
   core_option_manager_t *coreopts = NULL;
   struct core_option *option      = NULL;
   const char *value_label_0       = NULL;
   const char *value_label_1       = NULL;
   size_t option_index             = type - MENU_SETTINGS_CORE_OPTION_START;

   option_path_str[0]              = '\0';
   option_lbl_str[0]               = '\0';

   /* Boolean options are toggled directly,
    * without the use of a drop-down list */

   /* > Get current option index */
   if (type < MENU_SETTINGS_CORE_OPTION_START)
      goto push_dropdown_list;

   /* > Get core options struct */
   if (!retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts) ||
       (option_index >= coreopts->size))
      goto push_dropdown_list;

   /* > Get current option, and check whether
    *   it has exactly 2 values (i.e. on/off) */
   option = (struct core_option*)&coreopts->opts[option_index];

   if (   (!option)
       ||  (option->vals->size != 2)
       || ((option->index != 0)
       &&  (option->index != 1))
      )
      goto push_dropdown_list;

   /* > Check whether option values correspond
    *   to a boolean toggle */
   value_label_0 = option->val_labels->elems[0].data;
   value_label_1 = option->val_labels->elems[1].data;

   if (   string_is_empty(value_label_0)
       || string_is_empty(value_label_1)
       || !( (  string_is_equal(value_label_0, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON))
             && string_is_equal(value_label_1, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF)))
          || (  string_is_equal(value_label_0, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF))
             && string_is_equal(value_label_1, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON)))))
      goto push_dropdown_list;

   /* > Update value and return */
   core_option_manager_set_val(coreopts, option_index,
         (option->index == 0) ? 1 : 0, true);

   return 0;

push_dropdown_list:
   /* If this option is not a boolean toggle,
    * push drop-down list */
   snprintf(option_path_str, sizeof(option_path_str),
         "core_option_%d", (int)option_index);
   snprintf(option_lbl_str, sizeof(option_lbl_str),
         "%d", type);

   /* TODO/FIXME: This should be refactored to make
    * use of a core-option-specific drop-down list,
    * rather than hijacking the generic one... */
   generic_action_ok_displaylist_push(
         option_path_str, NULL,
         option_lbl_str, 0,
         idx, 0,
         ACTION_OK_DL_DROPDOWN_BOX_LIST);

   return 0;
}

#ifdef HAVE_CHEATS
static int action_ok_cheat_reload_cheats(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st      = menu_state_get_ptr();
   settings_t           *settings  = config_get_ptr();
   const char *path_cheat_database = settings->paths.path_cheat_database;

   cheat_manager_realloc(0, CHEAT_HANDLER_TYPE_EMU);

   cheat_manager_load_game_specific_cheats(
         path_cheat_database);

   menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                   |  MENU_ST_FLAG_PREVENT_POPULATE;
   return 0;
}
#endif

static int action_ok_start_recording(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   streaming_set_state(false);
   command_event(CMD_EVENT_RECORD_INIT, NULL);
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_start_streaming(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   streaming_set_state(true);
   command_event(CMD_EVENT_RECORD_INIT, NULL);
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_stop_recording(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_stop_streaming(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

#ifdef HAVE_CHEATS
static int action_ok_cheat_add_top(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   char msg[256];
   struct item_cheat tmp;
   struct menu_state *menu_st      = menu_state_get_ptr();
   unsigned int new_size           = cheat_manager_get_size() + 1;

   menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                   |  MENU_ST_FLAG_PREVENT_POPULATE;
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_EMU);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.size-1],
         sizeof(struct item_cheat));
   tmp.idx = 0;

   for (i = cheat_manager_state.size-2; i >=0; i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1],
            &cheat_manager_state.cheats[i], sizeof(struct item_cheat));
      cheat_manager_state.cheats[i+1].idx++;
   }

   memcpy(&cheat_manager_state.cheats[0], &tmp, sizeof(struct item_cheat));

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_ADD_TOP_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_cheat_add_bottom(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char msg[256];
   struct menu_state *menu_st      = menu_state_get_ptr();
   unsigned int new_size           = cheat_manager_get_size() + 1;

   menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                   |  MENU_ST_FLAG_PREVENT_POPULATE;
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_EMU);

   strlcpy(msg,
         msg_hash_to_str(MSG_CHEAT_ADD_BOTTOM_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_cheat_delete_all(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char msg[256];
   struct menu_state *menu_st       = menu_state_get_ptr();

   cheat_manager_state.delete_state = 0;
   cheat_manager_realloc(0, CHEAT_HANDLER_TYPE_EMU);
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_DELETE_ALL_SUCCESS),
          sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_cheat_add_new_after(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   char msg[256];
   struct item_cheat tmp;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned int new_size      = cheat_manager_get_size() + 1;

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_EMU);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.size-1],
         sizeof(struct item_cheat));
   tmp.idx = cheat_manager_state.working_cheat.idx+1;

   for (i = cheat_manager_state.size-2; i >= (int)(cheat_manager_state.working_cheat.idx+1); i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat));
      cheat_manager_state.cheats[i+1].idx++;
   }

   memcpy(&cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx+1], &tmp, sizeof(struct item_cheat));

   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_ADD_AFTER_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_cheat_add_new_before(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   char msg[256];
   struct item_cheat tmp;
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned int new_size      = cheat_manager_get_size() + 1;

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_EMU);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.size-1], sizeof(struct item_cheat));
   tmp.idx = cheat_manager_state.working_cheat.idx;

   for (i = cheat_manager_state.size-2; i >=(int)tmp.idx; i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat));
      cheat_manager_state.cheats[i+1].idx++;
   }

   memcpy(&cheat_manager_state.cheats[tmp.idx],
         &tmp, sizeof(struct item_cheat));
   memcpy(&cheat_manager_state.working_cheat,
         &tmp, sizeof(struct item_cheat));

   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_ADD_BEFORE_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_cheat_copy_before(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   struct item_cheat tmp;
   char msg[256];
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned int new_size      = cheat_manager_get_size() + 1;
   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx], sizeof(struct item_cheat));
   tmp.idx = cheat_manager_state.working_cheat.idx;
   if (tmp.code)
      tmp.code = strdup(tmp.code);
   if (tmp.desc)
      tmp.desc = strdup(tmp.desc);

   for (i = cheat_manager_state.size-2; i >=(int)tmp.idx; i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat));
      cheat_manager_state.cheats[i+1].idx++;
   }

   memcpy(&cheat_manager_state.cheats[tmp.idx], &tmp, sizeof(struct item_cheat));
   memcpy(&cheat_manager_state.working_cheat, &tmp, sizeof(struct item_cheat));

   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_COPY_BEFORE_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_cheat_copy_after(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int i;
   struct item_cheat tmp;
   char msg[256];
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned int new_size      = cheat_manager_get_size() + 1;

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   memcpy(&tmp, &cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx], sizeof(struct item_cheat));
   tmp.idx = cheat_manager_state.working_cheat.idx+1;
   if (tmp.code)
      tmp.code = strdup(tmp.code);
   if (tmp.desc)
      tmp.desc = strdup(tmp.desc);

   for (i = cheat_manager_state.size-2; i >= (int)(cheat_manager_state.working_cheat.idx+1); i--)
   {
      memcpy(&cheat_manager_state.cheats[i+1], &cheat_manager_state.cheats[i], sizeof(struct item_cheat));
      cheat_manager_state.cheats[i+1].idx++;
   }

   memcpy(&cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx+1], &tmp, sizeof(struct item_cheat ));

   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_COPY_AFTER_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return 0;
}

static int action_ok_cheat_delete(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char msg[256];
   size_t new_selection_ptr   = 0;
   struct menu_state *menu_st = menu_state_get_ptr();
   size_t selection           = menu_st->selection_ptr;
   unsigned int new_size      = cheat_manager_get_size() - 1;

   if (new_size >0)
   {
      unsigned i;

      if (cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].code)
         free(cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].code);
      if (cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].desc)
         free(cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].desc);

      for (i = cheat_manager_state.working_cheat.idx; i <cheat_manager_state.size-1; i++)
      {
         memcpy(&cheat_manager_state.cheats[i], &cheat_manager_state.cheats[i+1], sizeof(struct item_cheat ));
         cheat_manager_state.cheats[i].idx--;
      }

      cheat_manager_state.cheats[cheat_manager_state.size-1].code            = NULL;
      cheat_manager_state.cheats[cheat_manager_state.size-1].desc            = NULL;
      cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].desc = NULL;
      cheat_manager_state.cheats[cheat_manager_state.working_cheat.idx].code = NULL;

   }

   cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_RETRO);

   strlcpy(msg, msg_hash_to_str(MSG_CHEAT_DELETE_SUCCESS), sizeof(msg));
   msg[sizeof(msg) - 1] = 0;

   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   new_selection_ptr      = menu_st->selection_ptr;
   menu_entries_pop_stack(&new_selection_ptr, 0, 1);
   menu_st->selection_ptr = new_selection_ptr;

   if (menu_st->driver_ctx)
   {
      if (menu_st->driver_ctx->update_savestate_thumbnail_path)
         menu_st->driver_ctx->update_savestate_thumbnail_path(
               menu_st->userdata, (unsigned)selection);
      if (menu_st->driver_ctx->update_savestate_thumbnail_image)
         menu_st->driver_ctx->update_savestate_thumbnail_image(menu_st->userdata);
   }

   return 0;
}
#endif

static int action_ok_file_load_imageviewer(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char fullpath[PATH_MAX_LENGTH];
   const char *menu_path           = NULL;
   struct menu_state *menu_st      = menu_state_get_ptr();
   menu_list_t *menu_list          = menu_st->entries.list;
   file_list_t *menu_stack         = MENU_LIST_GET(menu_list, 0);

   if (menu_stack && menu_stack->size)
      menu_path = menu_stack->list[menu_stack->size - 1].path;

   if (!string_is_empty(menu_path))
      fill_pathname_join_special(fullpath, menu_path, path,
            sizeof(fullpath));
   else
      fullpath[0] = '\0';

   return default_action_ok_load_content_with_core_from_menu(fullpath, CORE_TYPE_IMAGEVIEWER);
}

static int action_ok_file_load_current_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu                 = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   return default_action_ok_load_content_with_core_from_menu(
         menu->detect_content_path, CORE_TYPE_PLAIN);
}

static int action_ok_file_load_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   content_ctx_info_t content_info;
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;

   if (!menu)
      return -1;

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   if (!task_push_load_content_with_new_core_from_menu(
            path, menu->detect_content_path,
            &content_info,
            CORE_TYPE_PLAIN,
            NULL, NULL))
      return -1;
   menu_driver_set_last_start_content(menu_st, menu->detect_content_path);

   return 0;
}

static int action_ok_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   bool resume          = settings->bools.menu_savestate_resume;

   if (generic_action_ok_command(CMD_EVENT_LOAD_STATE) == -1)
      return -1;

   if (resume)
      return generic_action_ok_command(CMD_EVENT_RESUME);

   return 0;
}

static int action_ok_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   bool resume          = settings->bools.menu_savestate_resume;

   if (generic_action_ok_command(CMD_EVENT_SAVE_STATE) == -1)
      return -1;

   if (resume)
      return generic_action_ok_command(CMD_EVENT_RESUME);

   return 0;
}

static int action_ok_play_replay(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   bool resume          = settings->bools.menu_savestate_resume;

   if (generic_action_ok_command(CMD_EVENT_PLAY_REPLAY) == -1)
      return -1;

   if (resume)
      return generic_action_ok_command(CMD_EVENT_RESUME);

   return 0;
}

static int action_ok_record_replay(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   bool resume          = settings->bools.menu_savestate_resume;

   if (generic_action_ok_command(CMD_EVENT_RECORD_REPLAY) == -1)
      return -1;

   if (resume)
      return generic_action_ok_command(CMD_EVENT_RESUME);

   return 0;
}

static int action_ok_halt_replay(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();
   bool resume          = settings->bools.menu_savestate_resume;

   if (generic_action_ok_command(CMD_EVENT_HALT_REPLAY) == -1)
      return -1;

   if (resume)
      return generic_action_ok_command(CMD_EVENT_RESUME);

   return 0;
}

static int action_ok_close_submenu(const char* path,
   const char* label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_cancel_pop_default(path, label, type, idx);
}

static int action_ok_cheevos_toggle_hardcore_mode(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   generic_action_ok_command(CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE);
   action_cancel_pop_default(path, label, type, idx);
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_undo_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_UNDO_LOAD_STATE) == -1)
      return -1;
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

static int action_ok_undo_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(CMD_EVENT_UNDO_SAVE_STATE) == -1)
      return -1;
   return generic_action_ok_command(CMD_EVENT_RESUME);
}

#ifdef HAVE_NETWORKING

#ifdef HAVE_ZLIB
static void cb_decompressed(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;

   if (dec && !err)
   {
      unsigned enum_idx = (unsigned)(uintptr_t)user_data;

      switch (enum_idx)
      {
         case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
            generic_action_ok_command(CMD_EVENT_REINIT);
            break;
         default:
            break;
      }
   }

   if (err)
      RARCH_ERR("%s", err);

   if (dec)
   {
      if (filestream_exists(dec->source_file))
         filestream_delete(dec->source_file);

      free(dec->source_file);
      free(dec);
   }
}
#endif

static int action_ok_core_updater_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   /* Get cached core updater list, initialising
    * it if required */
   core_updater_list_t *core_list = core_updater_list_get_cached();

   if (!core_list)
   {
      core_updater_list_init_cached();
      if (!(core_list = core_updater_list_get_cached()))
         return -1;
   }

#if defined(ANDROID)
   if (play_feature_delivery_enabled())
   {
      settings_t *settings                = config_get_ptr();
      const char *path_dir_libretro       = settings->paths.directory_libretro;
      const char *path_libretro_info      = settings->paths.path_libretro_info;
      /* Core downloads are handled via play
       * feature delivery
       * > Core list can be populated directly
       *   using the play feature delivery
       *   interface */
      struct string_list *available_cores =
         play_feature_delivery_available_cores();
      bool success                        = false;

      if (!available_cores)
         return -1;

      core_updater_list_reset(core_list);

      success = core_updater_list_parse_pfd_data(
            core_list,
            path_dir_libretro,
            path_libretro_info,
            available_cores);

      string_list_free(available_cores);

      if (!success)
         return -1;

      /* Ensure network is initialised */
      generic_action_ok_command(CMD_EVENT_NETWORK_INIT);
   }
   else
#endif
   {
      /* Initial setup... */
      struct menu_state *menu_st = menu_state_get_ptr();
      menu_st->flags            |=  MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH;
      generic_action_ok_command(CMD_EVENT_NETWORK_INIT);

      /* Push core list update task */
      task_push_get_core_updater_list(core_list, false, true);
   }

   return generic_action_ok_displaylist_push(
         path, NULL,
         label, type,
         idx, entry_idx,
         ACTION_OK_DL_CORE_UPDATER_LIST);
}

static void cb_net_generic_subdir(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   if (user_data)
      free(user_data);
}

static void cb_net_generic(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   http_transfer_data_t *data     = (http_transfer_data_t*)task_data;
   file_transfer_t *state         = (file_transfer_t*)user_data;
   struct menu_state *menu_st     = menu_state_get_ptr();
   menu_handle_t *menu            = menu_st->driver_data;

   if (!menu)
      goto finish;

   if (menu->core_buf)
      free(menu->core_buf);

   menu->core_buf = NULL;
   menu->core_len = 0;

   if (!data || err || !data->data)
      goto finish;

   menu->core_buf = (char*)malloc((data->len+1) * sizeof(char));

   if (!menu->core_buf)
      goto finish;

   if (!string_is_empty(data->data))
      memcpy(menu->core_buf, data->data, data->len * sizeof(char));
   menu->core_buf[data->len] = '\0';
   menu->core_len            = data->len;

finish:
   menu_st->flags &= ~MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH;

   if (     !err
         && !string_ends_with_size(state->path,
            FILE_PATH_INDEX_DIRS_URL,
            strlen(state->path),
            STRLEN_CONST(FILE_PATH_INDEX_DIRS_URL)
            ))
   {
      char parent_dir[PATH_MAX_LENGTH];
      char parent_dir_encoded[PATH_MAX_LENGTH];
      file_transfer_t *transf     = NULL;

      parent_dir_encoded[0]       = '\0';

      fill_pathname_parent_dir(parent_dir,
            state->path, sizeof(parent_dir));
      strlcat(parent_dir, FILE_PATH_INDEX_DIRS_URL,
            sizeof(parent_dir));

      transf           = (file_transfer_t*)malloc(sizeof(*transf));

      transf->enum_idx = MSG_UNKNOWN;
      strlcpy(transf->path, parent_dir, sizeof(transf->path));

      net_http_urlencode_full(parent_dir_encoded, parent_dir,
            sizeof(parent_dir_encoded));
      task_push_http_transfer_file(parent_dir_encoded, true,
            "index_dirs", cb_net_generic_subdir, transf);
   }

   if (state)
      free(state);
}

static int generic_action_ok_network(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums enum_idx)
{
   char url_path[PATH_MAX_LENGTH];
   char url_path_encoded[PATH_MAX_LENGTH];
   unsigned type_id2                       = 0;
   file_transfer_t *transf                 = NULL;
   const char *url_label                   = NULL;
   retro_task_callback_t callback          = NULL;
   bool suppress_msg                       = false;
   settings_t *settings                    = config_get_ptr();
   struct menu_state *menu_st              = menu_state_get_ptr();
   const char *network_buildbot_assets_url =
      settings->paths.network_buildbot_assets_url;

   url_path[0]         = '\0';
   url_path_encoded[0] = '\0';

   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DIRS_LIST:

         if (string_is_empty(network_buildbot_assets_url))
            return -1;

         fill_pathname_join_special(url_path,
               network_buildbot_assets_url,
               "cores/" FILE_PATH_INDEX_DIRS_URL,
               sizeof(url_path));
         url_label    = msg_hash_to_str(enum_idx);
         type_id2     = ACTION_OK_DL_CORE_CONTENT_DIRS_LIST;
         callback     = cb_net_generic;
         suppress_msg = true;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST:
         fill_pathname_join_special(url_path, path,
               FILE_PATH_INDEX_URL, sizeof(url_path));
         url_label    = msg_hash_to_str(enum_idx);
         type_id2     = ACTION_OK_DL_CORE_CONTENT_LIST;
         callback     = cb_net_generic;
         suppress_msg = true;
         break;
      case MENU_ENUM_LABEL_CB_CORE_SYSTEM_FILES_LIST:
         if (string_is_empty(network_buildbot_assets_url))
            return -1;
         fill_pathname_join_special(url_path,
               network_buildbot_assets_url,
               "system/" FILE_PATH_INDEX_URL,
               sizeof(url_path));
         url_label    = msg_hash_to_str(enum_idx);
         type_id2     = ACTION_OK_DL_CORE_SYSTEM_FILES_LIST;
         callback     = cb_net_generic;
         suppress_msg = true;
         break;
      case MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST:
         fill_pathname_join_special(url_path,
               FILE_PATH_CORE_THUMBNAILPACKS_URL,
               FILE_PATH_INDEX_URL, sizeof(url_path));
         url_label    = msg_hash_to_str(enum_idx);
         type_id2     = ACTION_OK_DL_THUMBNAILS_UPDATER_LIST;
         callback     = cb_net_generic;
         break;
#ifdef HAVE_LAKKA
      case MENU_ENUM_LABEL_CB_LAKKA_LIST:
         /* TODO unhardcode this path */
         fill_pathname_join_special(url_path,
               FILE_PATH_LAKKA_URL,
               lakka_get_project(), sizeof(url_path));
         fill_pathname_join_special(url_path, url_path,
               FILE_PATH_INDEX_URL,
               sizeof(url_path));
         url_label    = msg_hash_to_str(enum_idx);
         type_id2     = ACTION_OK_DL_LAKKA_LIST;
         callback     = cb_net_generic;
         break;
#endif
      default:
         break;
   }

   menu_st->flags |= MENU_ST_FLAG_ENTRIES_NONBLOCKING_REFRESH;

   generic_action_ok_command(CMD_EVENT_NETWORK_INIT);

   transf           = (file_transfer_t*)calloc(1, sizeof(*transf));
   strlcpy(transf->path, url_path, sizeof(transf->path));

   net_http_urlencode_full(url_path_encoded, url_path, sizeof(url_path_encoded));
   task_push_http_transfer_file(url_path_encoded, suppress_msg, url_label, callback, transf);

   return generic_action_ok_displaylist_push(
         path, NULL,
         label, type,
         idx, entry_idx,
         type_id2);
}

DEFAULT_ACTION_OK_LIST(action_ok_core_content_list, MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST)
DEFAULT_ACTION_OK_LIST(action_ok_core_content_dirs_list, MENU_ENUM_LABEL_CB_CORE_CONTENT_DIRS_LIST)
DEFAULT_ACTION_OK_LIST(action_ok_core_system_files_list, MENU_ENUM_LABEL_CB_CORE_SYSTEM_FILES_LIST)
DEFAULT_ACTION_OK_LIST(action_ok_thumbnails_updater_list, MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST)
DEFAULT_ACTION_OK_LIST(action_ok_lakka_list, MENU_ENUM_LABEL_CB_LAKKA_LIST)

static void cb_generic_dir_download(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   file_transfer_t *transf = (file_transfer_t*)user_data;
   if (transf)
   {
      generic_action_ok_network(transf->path, transf->path, 0, 0, 0,
            MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST);

      free(transf);
   }
}

/* expects http_transfer_t*, file_transfer_t* */
void cb_generic_download(retro_task_t *task,
      void *task_data,
      void *user_data, const char *err)
{
   char output_path[PATH_MAX_LENGTH];
   char buf[PATH_MAX_LENGTH];
#if defined(HAVE_COMPRESSION) && defined(HAVE_ZLIB)
   bool extract                          = true;
#endif
   const char             *dir_path      = NULL;
   file_transfer_t     *transf           = (file_transfer_t*)user_data;
   settings_t              *settings     = config_get_ptr();
   http_transfer_data_t        *data     = (http_transfer_data_t*)task_data;

   if (!data || !data->data || !transf)
      goto finish;

   output_path[0] = '\0';

   /* we have to determine dir_path at the time of writting or else
    * we'd run into races when the user changes the setting during an
    * http transfer. */
   switch (transf->enum_idx)
   {
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         dir_path = settings->paths.directory_thumbnails;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         dir_path = settings->paths.directory_core_assets;
#if defined(HAVE_COMPRESSION) && defined(HAVE_ZLIB)
         extract  = settings->bools.network_buildbot_auto_extract_archive;
#endif
         break;
      case MENU_ENUM_LABEL_CB_CORE_SYSTEM_FILES_DOWNLOAD:
         dir_path = settings->paths.directory_system;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         dir_path = settings->paths.path_libretro_info;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         dir_path = settings->paths.directory_assets;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         dir_path = settings->paths.directory_autoconfig;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         dir_path = settings->paths.path_content_database;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         dir_path = settings->paths.directory_overlay;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         dir_path = settings->paths.path_cheat_database;
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            static char shaderdir[PATH_MAX_LENGTH] = {0};
            const char *dirname                    = NULL;
            const char *dir_video_shader           = settings->paths.directory_video_shader;

            switch (transf->enum_idx)
            {
               case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
                  dirname                                   = "shaders_cg";
                  break;
               case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
                  dirname                                   = "shaders_glsl";
                  break;
               case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG:
                  dirname                                   = "shaders_slang";
                  break;
               default:
                  break;
            }

            fill_pathname_join_special(shaderdir, dir_video_shader,
                  dirname, sizeof(shaderdir));

            if (     !path_is_directory(shaderdir)
                  && !path_mkdir(shaderdir))
               goto finish;

            dir_path = shaderdir;
         }
#endif
         break;
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
         dir_path = LAKKA_UPDATE_DIR;
         break;
      case MENU_ENUM_LABEL_CB_DISCORD_AVATAR:
         fill_pathname_application_special(buf, sizeof(buf),
               APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
         dir_path = buf;
         break;
      default:
         RARCH_WARN("Unknown transfer type '%s' bailing out.\n",
               msg_hash_to_str(transf->enum_idx));
         break;
   }

   if (!string_is_empty(dir_path))
      fill_pathname_join_special(output_path, dir_path,
            transf->path, sizeof(output_path));

   /* Make sure the directory exists
    * This function is horrible. It mutates the original path
    * so after operating we'll have to set the path to the intended
    * location again...
    */
   path_basedir_wrapper(output_path);

   if (!path_mkdir(output_path))
   {
      err = msg_hash_to_str(MSG_FAILED_TO_CREATE_THE_DIRECTORY);
      goto finish;
   }

   if (!string_is_empty(dir_path))
      fill_pathname_join_special(output_path, dir_path,
            transf->path, sizeof(output_path));

#ifdef HAVE_COMPRESSION
   if (path_is_compressed_file(output_path))
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

#if defined(HAVE_COMPRESSION) && defined(HAVE_ZLIB)
   if (!extract)
      goto finish;

   if (path_is_compressed_file(output_path))
   {
      retro_task_t *decompress_task = NULL;
      void *frontend_userdata       = task->frontend_userdata;
      task->frontend_userdata       = NULL;

      decompress_task = (retro_task_t*)task_push_decompress(
            output_path, dir_path,
            NULL, NULL, NULL,
            cb_decompressed,
            (void*)(uintptr_t)transf->enum_idx,
            frontend_userdata, false);

      if (!decompress_task)
      {
         err = msg_hash_to_str(MSG_DECOMPRESSION_FAILED);
         goto finish;
      }
   }
#endif

finish:
   if (err)
   {
      RARCH_ERR("[Updater]: Download of \"%s\" failed: %s\n",
            (transf ? transf->path : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN)), err);
   }
   else
   {
      RARCH_LOG("[Updater]: Download \"%s\".\n",
            (transf ? transf->path : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN)));

#ifdef HAVE_DISCORD
      if (transf && transf->enum_idx == MENU_ENUM_LABEL_CB_DISCORD_AVATAR)
         discord_avatar_set_ready(true);
#endif
   }

   if (transf)
      free(transf);
}

static int action_ok_download_generic(const char *path,
      const char *label, const char *menu_label,
      unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums enum_idx)
{
   char s[PATH_MAX_LENGTH];
   char s2[PATH_MAX_LENGTH];
   char s3[PATH_MAX_LENGTH];
   file_transfer_t *transf      = NULL;
   bool suppress_msg            = false;
   retro_task_callback_t cb     = cb_generic_download;
   settings_t *settings         = config_get_ptr();
   const char *network_buildbot_assets_url =
      settings->paths.network_buildbot_assets_url;
   const char *network_buildbot_url = settings->paths.network_buildbot_url;

   s3[0] = '\0';

   fill_pathname_join_special(s,
         network_buildbot_assets_url,
         "frontend", sizeof(s));

   switch (enum_idx)
   {
      case MENU_ENUM_LABEL_CB_DOWNLOAD_URL:
         suppress_msg = true;
         fill_pathname_join_special(s, label,
               path, sizeof(s));
         path = s;
         cb   = cb_generic_dir_download;
         break;
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         {
            struct string_list str_list  = {0};

            string_list_initialize(&str_list);
            if (string_split_noalloc(&str_list, menu_label, ";"))
               strlcpy(s, str_list.elems[0].data, sizeof(s));
            string_list_deinitialize(&str_list);
         }
         break;
      case MENU_ENUM_LABEL_CB_CORE_SYSTEM_FILES_DOWNLOAD:
         fill_pathname_join_special(s,
               network_buildbot_assets_url,
               "system", sizeof(s));
         break;
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
#ifdef HAVE_LAKKA
         /* TODO unhardcode this path*/
         fill_pathname_join_special(s, FILE_PATH_LAKKA_URL,
               lakka_get_project(), sizeof(s));
#endif
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         path = "assets.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         path = "autoconfig.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         path = "info.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         path = "cheats.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         path = "overlays.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         path = "database-rdb.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
         path = "shaders_glsl.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG:
         path = "shaders_slang.zip";
         break;
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
         path = "shaders_cg.zip";
         break;
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         strlcpy(s, "http://thumbnailpacks.libretro.com", sizeof(s));
         break;
      default:
         strlcpy(s, network_buildbot_url, sizeof(s));
         break;
   }

   fill_pathname_join_special(s2, s, path, sizeof(s2));

   transf           = (file_transfer_t*)calloc(1, sizeof(*transf));
   transf->enum_idx = enum_idx;
   strlcpy(transf->path, path, sizeof(transf->path));

   if (string_is_equal(path, s))
      net_http_urlencode_full(s3, s, sizeof(s3));
   else
      net_http_urlencode_full(s3, s2, sizeof(s3));

   task_push_http_transfer_file(s3, suppress_msg,
         msg_hash_to_str(enum_idx), cb, transf);
   return 0;
}

static int action_ok_core_content_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path   = NULL;
   const char *menu_label  = NULL;
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;

   menu_entries_get_last_stack(&menu_path, &menu_label,
         NULL, &enum_idx, NULL);

   return action_ok_download_generic(path, label,
         menu_path, type, idx, entry_idx,
         MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD);
}

static int action_ok_core_updater_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   core_updater_list_t *core_list    = core_updater_list_get_cached();
   settings_t          *settings     = config_get_ptr();
   bool auto_backup                  = settings->bools.core_updater_auto_backup;
   unsigned auto_backup_history_size = settings->uints.core_updater_auto_backup_history_size;
   const char *path_dir_libretro     = settings->paths.directory_libretro;
   const char *path_dir_core_assets  = settings->paths.directory_core_assets;

   if (!core_list)
      return -1;

#if defined(ANDROID)
   /* Play Store builds install cores via
    * the play feature delivery interface */
   if (play_feature_delivery_enabled())
      task_push_play_feature_delivery_core_install(
            core_list, path, false);
   else
#endif
      task_push_core_updater_download(
            core_list, path, 0, false,
            auto_backup, (size_t)auto_backup_history_size,
            path_dir_libretro, path_dir_core_assets);

   return 0;
}

static int action_ok_update_installed_cores(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t          *settings     = config_get_ptr();
   bool auto_backup                  = settings->bools.core_updater_auto_backup;
   unsigned auto_backup_history_size = settings->uints.core_updater_auto_backup_history_size;
   const char *path_dir_libretro     = settings->paths.directory_libretro;
   const char *path_dir_core_assets  = settings->paths.directory_core_assets;

   /* Ensure networking is initialised */
   generic_action_ok_command(CMD_EVENT_NETWORK_INIT);

   /* Push update task */
   task_push_update_installed_cores(
         auto_backup, auto_backup_history_size,
         path_dir_libretro, path_dir_core_assets);

   return 0;
}

#if defined(ANDROID)
static int action_ok_switch_installed_cores_pfd(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t          *settings     = config_get_ptr();
   const char *path_dir_libretro     = settings->paths.directory_libretro;
   const char *path_libretro_info    = settings->paths.path_libretro_info;

   /* Ensure networking is initialised */
   generic_action_ok_command(CMD_EVENT_NETWORK_INIT);

   /* Push core switch/update task */
   task_push_play_feature_delivery_switch_installed_cores(
         path_dir_libretro, path_libretro_info);

   return 0;
}
#endif
#endif

static int action_ok_sideload_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char backup_path[PATH_MAX_LENGTH];
   const char *menu_path    = NULL;
   const char *core_file    = path;
   bool core_loaded         = false;
   menu_handle_t *menu      = menu_state_get_ptr()->driver_data;
   settings_t *settings     = config_get_ptr();
   const char *dir_libretro = settings->paths.directory_libretro;

   if (string_is_empty(core_file) || !menu)
      return -1;

   /* Get path of source (core 'backup') file */
   menu_entries_get_last_stack(
         &menu_path, NULL, NULL, NULL, NULL);

   if (!string_is_empty(menu_path))
      fill_pathname_join_special(
            backup_path, menu_path, core_file, sizeof(backup_path));
   else
      strlcpy(backup_path, core_file, sizeof(backup_path));

   /* Push core 'restore' task */
   task_push_core_restore(backup_path, dir_libretro, &core_loaded);

   /* Flush stack
    * > Since the 'sideload core' option is present
    *   in several locations, can't flush to a predefined
    *   level - just go to the top */
   menu_entries_flush_stack(NULL, 0);

   return 0;
}

#ifdef HAVE_NETWORKING
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_core_system_files_download, MENU_ENUM_LABEL_CB_CORE_SYSTEM_FILES_DOWNLOAD)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_core_content_thumbnails, MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_thumbnails_updater_download, MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_DOWNLOAD)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_download_url, MENU_ENUM_LABEL_CB_DOWNLOAD_URL)
#ifdef HAVE_LAKKA
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_lakka_download, MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD)
#endif
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_assets, MENU_ENUM_LABEL_CB_UPDATE_ASSETS)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_core_info_files, MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_overlays, MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_shaders_cg, MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_shaders_glsl, MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL)
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_shaders_slang, MENU_ENUM_LABEL_CB_UPDATE_SHADERS_SLANG)
#endif
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_databases, MENU_ENUM_LABEL_CB_UPDATE_DATABASES)
#ifdef HAVE_CHEATS
#ifdef HAVE_NETWORKING
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_cheats, MENU_ENUM_LABEL_CB_UPDATE_CHEATS)
#endif
#endif
DEFAULT_ACTION_OK_DOWNLOAD(action_ok_update_autoconfig_profiles, MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES)
#endif

static int action_ok_game_specific_core_options_create(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st       = menu_state_get_ptr();
   core_options_create_override(true);
   /* Refresh menu */
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
   return 0;
}

static int action_ok_folder_specific_core_options_create(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st       = menu_state_get_ptr();
   core_options_create_override(false);
   /* Refresh menu */
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
   return 0;
}

static int action_ok_game_specific_core_options_remove(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st       = menu_state_get_ptr();
   core_options_remove_override(true);
   /* Refresh menu */
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
   return 0;
}

static int action_ok_folder_specific_core_options_remove(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st       = menu_state_get_ptr();
   core_options_remove_override(false);
   /* Refresh menu */
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
   return 0;
}

static int action_ok_core_options_reset(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   core_options_reset();
   return 0;
}

static int action_ok_core_options_flush(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   core_options_flush();
   return 0;
}

int action_ok_close_content(const char *path, const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int ret;
   struct menu_state   *menu_st = menu_state_get_ptr();
   /* Reset navigation pointer
    * > If we are returning to the quick menu, want
    *   the active entry to be 'Run' (first item in
    *   menu list) */
   menu_st->selection_ptr       = 0;

   /* Unload core */
   ret = generic_action_ok_command(CMD_EVENT_UNLOAD_CORE);

   /* If close content was selected via any means other than
    * 'Playlist > Quick Menu', have to flush the menu stack
    * (otherwise users will be presented with an empty
    * 'No items' quick menu, requiring needless backwards
    * navigation) */
   if (type == MENU_SETTING_ACTION_CLOSE)
   {
      const char *parent_label   = NULL;
      const char *flush_target   = msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU);
      file_list_t *list          = NULL;
      if (menu_st->entries.list)
         list                    = MENU_LIST_GET(menu_st->entries.list, 0);
      if (list && (list->size > 1))
      {
         parent_label = list->list[list->size - 2].label;

         if (   string_is_equal(parent_label, msg_hash_to_str(MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB))
             || string_is_equal(parent_label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CONTENTLESS_CORES_LIST)))
            flush_target = parent_label;
      }

      menu_entries_flush_stack(flush_target, 0);
      /* An annoyance - some menu drivers (Ozone...) set
       * MENU_ST_FLAG_PREVENT_POPULATE in awkward places,
       * which can cause breakage here when flushing
       * the menu stack. We therefore have to unset
       * MENU_ST_FLAG_PREVENT_POPULATE */
      menu_st->flags &= ~MENU_ST_FLAG_PREVENT_POPULATE;
   }

   return ret;
}

STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_cheat_apply_changes,      CMD_EVENT_CHEATS_APPLY)
STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_quit,                     CMD_EVENT_QUIT)
STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_save_new_config,          CMD_EVENT_MENU_SAVE_CONFIG)
STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_resume_content,           CMD_EVENT_RESUME)
STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_restart_content,          CMD_EVENT_RESET)
STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_screenshot,               CMD_EVENT_TAKE_SCREENSHOT)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_shader_apply_changes,     CMD_EVENT_SHADERS_APPLY_CHANGES)
#endif
STATIC_DEFAULT_ACTION_OK_CMD_FUNC(action_ok_show_wimp,                CMD_EVENT_UI_COMPANION_TOGGLE)

static int action_ok_set_core_association(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu                 = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   /* TODO/FIXME - menu->rpl_entry_selection_ptr - find
    * a way so that we can remove this temporary state
    * required for playlist entry write */
   return generic_action_ok_displaylist_push(
         path, NULL,
         NULL, menu->rpl_entry_selection_ptr,
         idx, entry_idx,
         ACTION_OK_DL_DEFERRED_CORE_LIST_SET);
}

static int action_ok_reset_core_association(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t playlist_index;
   menu_handle_t *menu                 = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   playlist_index = (size_t)menu->rpl_entry_selection_ptr;

   if (!command_event(CMD_EVENT_RESET_CORE_ASSOCIATION,
            (void *)&playlist_index))
      return -1;
   return 0;
}

/* This function is called when selecting 'add to favorites'
 * while viewing the quick menu (i.e. content is running) */
static int action_ok_add_to_favorites(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *content_path = path_get(RARCH_PATH_CONTENT);
   int ret = 0;

   /* Error checking
    * > If content path is empty, cannot do anything... */
   if (!string_is_empty(content_path))
   {
      runloop_state_t *runloop_st       = runloop_state_get_ptr();
      struct retro_system_info *sysinfo = &runloop_st->system.info;
      struct string_list *str_list      = NULL;
      const char *crc32                 = NULL;
      const char *db_name               = NULL;

      union string_list_elem_attr attr;
      char content_label[PATH_MAX_LENGTH];
      char core_path[PATH_MAX_LENGTH];
      char core_name[PATH_MAX_LENGTH];

      core_path[0]     = '\0';
      core_name[0]     = '\0';

      /* Create string list container for playlist parameters */
      attr.i           = 0;
      if (!(str_list = string_list_new()))
         return 0;

      /* Determine playlist parameters */

      /* > content_label */
      if (!string_is_empty(runloop_st->name.label))
         strlcpy(content_label, runloop_st->name.label,
               sizeof(content_label));
      else
         content_label[0] = '\0';

      /* Label is empty - use file name instead */
      if (string_is_empty(content_label))
         fill_pathname(content_label,
               path_basename(content_path), "",
               sizeof(content_label));

      /* > core_path + core_name */
      if (sysinfo)
      {
         if (!string_is_empty(path_get(RARCH_PATH_CORE)))
         {
            core_info_t *core_info = NULL;

            /* >> core_path */
            strlcpy(core_path, path_get(RARCH_PATH_CORE),
                  sizeof(core_path));
            /* >> core_name
             * (always use display name, if available) */
            if (core_info_find(core_path, &core_info))
               if (!string_is_empty(core_info->display_name))
                  strlcpy(core_name, core_info->display_name,
                        sizeof(core_name));
         }

         /* >> core_name (continued) */
         if (      string_is_empty(core_name)
               && !string_is_empty(sysinfo->library_name))
            strlcpy(core_name, sysinfo->library_name, sizeof(core_name));
      }

      if (string_is_empty(core_path) || string_is_empty(core_name))
      {
         strlcpy(core_path, FILE_PATH_DETECT, sizeof(core_path));
         strlcpy(core_name, FILE_PATH_DETECT, sizeof(core_name));
      }

      /* > crc32 + db_name */
      {
         menu_handle_t *menu = menu_state_get_ptr()->driver_data;
         if (menu)
         {
            playlist_t *playlist_curr = playlist_get_cached();

            if (playlist_index_is_valid(playlist_curr,
                     menu->rpl_entry_selection_ptr,
                     content_path, core_path))
            {
               playlist_get_crc32(playlist_curr,
                     menu->rpl_entry_selection_ptr, &crc32);
               playlist_get_db_name(playlist_curr,
                     menu->rpl_entry_selection_ptr, &db_name);
            }
         }
      }

      /* Copy playlist parameters into string list
       *   [0]: content_path
       *   [1]: content_label
       *   [2]: core_path
       *   [3]: core_name
       *   [4]: crc32
       *   [5]: db_name */
      string_list_append(str_list, content_path, attr);
      string_list_append(str_list, content_label, attr);
      string_list_append(str_list, core_path, attr);
      string_list_append(str_list, core_name, attr);
      string_list_append(str_list, !string_is_empty(crc32)
            ? crc32 : "", attr);
      string_list_append(str_list, !string_is_empty(db_name)
            ? db_name : "", attr);

      /* Trigger 'ADD_TO_FAVORITES' event */
      if (!command_event(CMD_EVENT_ADD_TO_FAVORITES, (void*)str_list))
         ret = -1;

      /* Clean up */
      string_list_free(str_list);
      str_list = NULL;
   }

   return ret;
}

/* This function is called when selecting 'add to favorites'
 * while viewing a playlist entry */
static int action_ok_add_to_favorites_playlist(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist_curr           = playlist_get_cached();
   const struct playlist_entry *entry  = NULL;
   menu_handle_t *menu                 = menu_state_get_ptr()->driver_data;
   int ret                             = 0;

   if (!playlist_curr)
      return 0;
   if (!menu)
      return -1;

   /* Read current playlist parameters */
   playlist_get_index(playlist_curr, menu->rpl_entry_selection_ptr, &entry);

   /* Error checking
    * > If content path is empty, cannot do anything... */
   if (!string_is_empty(entry->path))
   {
      union string_list_elem_attr attr;
      char core_display_name[PATH_MAX_LENGTH];
      char core_path[PATH_MAX_LENGTH];
      char core_name[PATH_MAX_LENGTH];
      struct string_list
         *str_list         = NULL;

      core_display_name[0] = '\0';
      core_path[0]         = '\0';
      core_name[0]         = '\0';

      /* Create string list container for playlist parameters */
      attr.i               = 0;
      if (!(str_list = string_list_new()))
         return 0;

      /* Copy playlist parameters into string list
       *   [0]: content_path
       *   [1]: content_label
       *   [2]: core_path
       *   [3]: core_name
       *   [4]: crc32
       *   [5]: db_name */

      /* > content_path */
      string_list_append(str_list, entry->path, attr);

      /* > content_label */
      if (!string_is_empty(entry->label))
         string_list_append(str_list, entry->label, attr);
      else
      {
         /* Label is empty - use file name instead */
         char fallback_content_label[PATH_MAX_LENGTH];
         fallback_content_label[0] = '\0';
         fill_pathname(fallback_content_label,
               path_basename(entry->path), "",
               sizeof(fallback_content_label));
         string_list_append(str_list, fallback_content_label, attr);
      }

      /* Replace "DETECT" with default_core_path + name if available */
      if (     !string_is_empty(entry->core_path)
            && !string_is_empty(entry->core_name))
      {
         if (     string_is_equal(entry->core_path, FILE_PATH_DETECT)
               && string_is_equal(entry->core_name, FILE_PATH_DETECT))
         {
            const char *default_core_path = playlist_get_default_core_path(playlist_curr);
            const char *default_core_name = playlist_get_default_core_name(playlist_curr);

            if (     !string_is_empty(default_core_path)
                  && !string_is_empty(default_core_name))
            {
               strlcpy(core_path, default_core_path, sizeof(core_path));
               strlcpy(core_name, default_core_name, sizeof(core_name));
            }
         }
         else
         {
            strlcpy(core_path, entry->core_path, sizeof(core_path));
            strlcpy(core_name, entry->core_name, sizeof(core_name));
         }
      }

      /* > core_path + core_name */
      if (     !string_is_empty(core_path)
            && !string_is_empty(core_name))
      {
         core_info_t *core_info = NULL;

         /* >> core_path */
         string_list_append(str_list, core_path, attr);

         /* >> core_name
          * (always use display name, if available) */
         if (core_info_find(core_path, &core_info))
            if (!string_is_empty(core_info->display_name))
               strlcpy(core_display_name, core_info->display_name, sizeof(core_display_name));

         if (!string_is_empty(core_display_name))
            string_list_append(str_list, core_display_name, attr);
         else
            string_list_append(str_list, core_name, attr);
      }
      else
      {
         string_list_append(str_list, FILE_PATH_DETECT, attr);
         string_list_append(str_list, FILE_PATH_DETECT, attr);
      }

      /* crc32 */
      string_list_append(str_list, !string_is_empty(entry->crc32) ? entry->crc32 : "", attr);

      /* db_name */
      string_list_append(str_list, !string_is_empty(entry->db_name) ? entry->db_name : "", attr);

      /* Trigger 'ADD_TO_FAVORITES' event */
      if (!command_event(CMD_EVENT_ADD_TO_FAVORITES, (void*)str_list))
         ret = -1;

      /* Clean up */
      string_list_free(str_list);
      str_list = NULL;
   }

   return ret;
}

static int action_ok_delete_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   size_t new_selection_ptr;
   char *conf_path           = NULL;
   char *def_conf_path       = NULL;
   char *def_conf_music_path = NULL;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   char *def_conf_video_path = NULL;
#endif
#ifdef HAVE_IMAGEVIEWER
   char *def_conf_img_path   = NULL;
#endif
   char *def_conf_fav_path   = NULL;
   playlist_t *playlist      = playlist_get_cached();
   struct menu_state *menu_st= menu_state_get_ptr();
   menu_handle_t *menu       = menu_st->driver_data;

   if (!menu)
      return -1;

   conf_path                 = playlist_get_conf_path(playlist);
   def_conf_path             = playlist_get_conf_path(g_defaults.content_history);
   def_conf_music_path       = playlist_get_conf_path(g_defaults.music_history);
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   def_conf_video_path       = playlist_get_conf_path(g_defaults.video_history);
#endif
#ifdef HAVE_IMAGEVIEWER
   def_conf_img_path         = playlist_get_conf_path(g_defaults.image_history);
#endif
   def_conf_fav_path         = playlist_get_conf_path(g_defaults.content_favorites);

   if (string_is_equal(conf_path, def_conf_path))
      playlist               = g_defaults.content_history;
   else if (string_is_equal(conf_path, def_conf_music_path))
      playlist               = g_defaults.music_history;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
   else if (string_is_equal(conf_path, def_conf_video_path))
      playlist               = g_defaults.video_history;
#endif
#ifdef HAVE_IMAGEVIEWER
   else if (string_is_equal(conf_path, def_conf_img_path))
      playlist               = g_defaults.image_history;
#endif
   else if (string_is_equal(conf_path, def_conf_fav_path))
      playlist               = g_defaults.content_favorites;

   if (playlist)
   {
      playlist_delete_index(playlist, menu->rpl_entry_selection_ptr);
      playlist_write_file(playlist);
   }

   new_selection_ptr      = menu_st->selection_ptr;
   menu_entries_pop_stack(&new_selection_ptr, 0, 1);
   menu_st->selection_ptr = new_selection_ptr;

   /* Thumbnail must be refreshed */
   if (menu_st->driver_ctx && menu_st->driver_ctx->refresh_thumbnail_image)
      menu_st->driver_ctx->refresh_thumbnail_image(
            menu_st->userdata, (unsigned)new_selection_ptr);

   return 0;
}

static int action_ok_rdb_entry_submenu(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   union string_list_elem_attr attr;
   char new_label[PATH_MAX_LENGTH];
   int ret                         = -1;
   char *rdb                       = NULL;
   int len                         = 0;
   struct string_list str_list     = {0};
   struct string_list str_list2    = {0};

   if (!label)
      return -1;

   new_label[0]                    =  '\0';

   string_list_initialize(&str_list);
   if (!string_split_noalloc(&str_list, label, "|"))
      goto end;

   string_list_initialize(&str_list2);

   /* element 0 : label
    * element 1 : value
    * element 2 : database path
    */

   attr.i = 0;

   len += strlen(str_list.elems[1].data) + 1;
   string_list_append(&str_list2, str_list.elems[1].data, attr);

   len += strlen(str_list.elems[2].data) + 1;
   string_list_append(&str_list2, str_list.elems[2].data, attr);

   if (!(rdb = (char*)calloc(len, sizeof(char))))
      goto end;

   string_list_join_concat(rdb, len, &str_list2, "|");

   fill_pathname_join_delim(new_label,
         msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST),
         str_list.elems[0].data, '_',
         sizeof(new_label));

   ret = generic_action_ok_displaylist_push(
         rdb, NULL,
         new_label, type,
         idx, entry_idx,
         ACTION_OK_DL_RDB_ENTRY_SUBMENU);

end:
   if (rdb)
      free(rdb);
   string_list_deinitialize(&str_list);
   string_list_deinitialize(&str_list2);

   return ret;
}

STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_browse_url_start, ACTION_OK_DL_BROWSE_URL_START)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_goto_favorites, ACTION_OK_DL_FAVORITES_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_goto_images, ACTION_OK_DL_IMAGES_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cdrom_info_list, ACTION_OK_DL_CDROM_INFO_DETAIL_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_goto_video, ACTION_OK_DL_VIDEO_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_goto_music, ACTION_OK_DL_MUSIC_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_goto_explore, ACTION_OK_DL_EXPLORE_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_goto_contentless_cores, ACTION_OK_DL_CONTENTLESS_CORES_LIST)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_shader_preset_save, ACTION_OK_DL_SHADER_PRESET_SAVE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_shader_preset_remove, ACTION_OK_DL_SHADER_PRESET_REMOVE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_shader_parameters, ACTION_OK_DL_SHADER_PARAMETERS)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_parent_directory_push, ACTION_OK_DL_PARENT_DIRECTORY_PUSH)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_directory_push, ACTION_OK_DL_DIRECTORY_PUSH)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_configurations_list, ACTION_OK_DL_CONFIGURATIONS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_saving_list, ACTION_OK_DL_SAVING_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cloud_sync_list, ACTION_OK_DL_CLOUD_SYNC_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_network_list, ACTION_OK_DL_NETWORK_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_network_hosting_list, ACTION_OK_DL_NETWORK_HOSTING_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_netplay_kick_list, ACTION_OK_DL_NETPLAY_KICK_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_netplay_ban_list, ACTION_OK_DL_NETPLAY_BAN_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_netplay_lobby_filters_list, ACTION_OK_DL_NETPLAY_LOBBY_FILTERS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_subsystem_list, ACTION_OK_DL_SUBSYSTEM_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_database_manager_list, ACTION_OK_DL_DATABASE_MANAGER_LIST)
#ifdef HAVE_BLUETOOTH
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_bluetooth_list, ACTION_OK_DL_BLUETOOTH_SETTINGS_LIST)
#endif
#ifdef HAVE_NETWORKING
#ifdef HAVE_WIFI
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_wifi_list, ACTION_OK_DL_WIFI_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_wifi_networks_list, ACTION_OK_DL_WIFI_NETWORKS_LIST)
#endif
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_compressed_archive_push, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_compressed_archive_push_detect_core, ACTION_OK_DL_COMPRESSED_ARCHIVE_PUSH_DETECT_CORE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_logging_list, ACTION_OK_DL_LOGGING_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_frame_throttle_list, ACTION_OK_DL_FRAME_THROTTLE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_frame_time_counter_list, ACTION_OK_DL_FRAME_TIME_COUNTER_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_rewind_list, ACTION_OK_DL_REWIND_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cheat, ACTION_OK_DL_CHEAT_DETAILS_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cheat_start_or_cont, ACTION_OK_DL_CHEAT_SEARCH_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_onscreen_display_list, ACTION_OK_DL_ONSCREEN_DISPLAY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_onscreen_notifications_list, ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_onscreen_notifications_views_list, ACTION_OK_DL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_onscreen_overlay_list, ACTION_OK_DL_ONSCREEN_OVERLAY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_osk_overlay_list, ACTION_OK_DL_OSK_OVERLAY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_menu_list, ACTION_OK_DL_MENU_SETTINGS_LIST)
#ifdef _3DS
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_menu_bottom_list, ACTION_OK_DL_MENU_BOTTOM_SETTINGS_LIST)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_quick_menu_override_options, ACTION_OK_DL_QUICK_MENU_OVERRIDE_OPTIONS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_menu_views_list, ACTION_OK_DL_MENU_VIEWS_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_settings_views_list, ACTION_OK_DL_SETTINGS_VIEWS_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_quick_menu_views_list, ACTION_OK_DL_QUICK_MENU_VIEWS_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_power_management_list, ACTION_OK_DL_POWER_MANAGEMENT_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cpu_perfpower_list, ACTION_OK_DL_CPU_PERFPOWER_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cpu_policy_entry, ACTION_OK_DL_CPU_POLICY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_menu_sounds_list, ACTION_OK_DL_MENU_SOUNDS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_user_interface_list, ACTION_OK_DL_USER_INTERFACE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_menu_file_browser_list, ACTION_OK_DL_MENU_FILE_BROWSER_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_retro_achievements_list, ACTION_OK_DL_RETRO_ACHIEVEMENTS_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cheevos_appearance_list, ACTION_OK_DL_CHEEVOS_APPEARANCE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cheevos_visibility_list, ACTION_OK_DL_CHEEVOS_VISIBILITY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_updater_list, ACTION_OK_DL_UPDATER_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_lakka_services, ACTION_OK_DL_LAKKA_SERVICES_LIST)
#ifdef HAVE_LAKKA_SWITCH
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_lakka_switch_options, ACTION_OK_DL_LAKKA_SWITCH_OPTIONS_LIST)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_user_list, ACTION_OK_DL_USER_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_netplay_sublist, ACTION_OK_DL_NETPLAY)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_directory_list, ACTION_OK_DL_DIRECTORY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_privacy_list, ACTION_OK_DL_PRIVACY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_midi_list, ACTION_OK_DL_MIDI_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_rdb_entry, ACTION_OK_DL_RDB_ENTRY)
#ifdef HAVE_AUDIOMIXER
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_mixer_stream_actions, ACTION_OK_DL_MIXER_STREAM_SETTINGS_LIST)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_browse_url_list, ACTION_OK_DL_BROWSE_URL_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_core_list, ACTION_OK_DL_CORE_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_sideload_core_list, ACTION_OK_DL_SIDELOAD_CORE_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cheat_file, ACTION_OK_DL_CHEAT_FILE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_cheat_file_append, ACTION_OK_DL_CHEAT_FILE_APPEND)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_playlist_collection, ACTION_OK_DL_PLAYLIST_COLLECTION)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_disk_image_append_list, ACTION_OK_DL_DISK_IMAGE_APPEND_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_subsystem_add_list, ACTION_OK_DL_SUBSYSTEM_ADD_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_subsystem_add_load, ACTION_OK_DL_SUBSYSTEM_LOAD)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_record_configfile, ACTION_OK_DL_RECORD_CONFIGFILE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_stream_configfile, ACTION_OK_DL_STREAM_CONFIGFILE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_remap_file, ACTION_OK_DL_REMAP_FILE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_override_file, ACTION_OK_DL_OVERRIDE_FILE)
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_shader_preset, ACTION_OK_DL_SHADER_PRESET)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_shader_preset_prepend, ACTION_OK_DL_SHADER_PRESET_PREPEND)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_shader_preset_append, ACTION_OK_DL_SHADER_PRESET_APPEND)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_generic_list, ACTION_OK_DL_GENERIC)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_audio_dsp_plugin, ACTION_OK_DL_AUDIO_DSP_PLUGIN)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_video_filter, ACTION_OK_DL_VIDEO_FILTER)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_overlay_preset, ACTION_OK_DL_OVERLAY_PRESET)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_osk_overlay_preset, ACTION_OK_DL_OSK_OVERLAY_PRESET)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_video_font, ACTION_OK_DL_VIDEO_FONT)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_rpl_entry, ACTION_OK_DL_RPL_ENTRY)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_open_archive_detect_core, ACTION_OK_DL_OPEN_ARCHIVE_DETECT_CORE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_file_load_music, ACTION_OK_DL_MUSIC)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_accounts_list, ACTION_OK_DL_ACCOUNTS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_driver_settings_list, ACTION_OK_DL_DRIVER_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_crt_switchres_settings_list, ACTION_OK_DL_CRT_SWITCHRES_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_video_settings_list, ACTION_OK_DL_VIDEO_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_video_fullscreen_mode_settings_list, ACTION_OK_DL_VIDEO_FULLSCREEN_MODE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_video_synchronization_settings_list, ACTION_OK_DL_VIDEO_SYNCHRONIZATION_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_video_windowed_mode_settings_list, ACTION_OK_DL_VIDEO_WINDOWED_MODE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_video_scaling_settings_list, ACTION_OK_DL_VIDEO_SCALING_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_video_hdr_settings_list, ACTION_OK_DL_VIDEO_HDR_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_video_output_settings_list, ACTION_OK_DL_VIDEO_OUTPUT_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_configuration_settings_list, ACTION_OK_DL_CONFIGURATION_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_core_settings_list, ACTION_OK_DL_CORE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_core_restore_backup_list, ACTION_OK_DL_CORE_RESTORE_BACKUP_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_core_delete_backup_list, ACTION_OK_DL_CORE_DELETE_BACKUP_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_audio_settings_list, ACTION_OK_DL_AUDIO_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_audio_output_settings_list, ACTION_OK_DL_AUDIO_OUTPUT_SETTINGS_LIST)
#ifdef HAVE_MICROPHONE
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_microphone_settings_list, ACTION_OK_DL_MICROPHONE_SETTINGS_LIST)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_audio_synchronization_settings_list, ACTION_OK_DL_AUDIO_SYNCHRONIZATION_SETTINGS_LIST)
#ifdef HAVE_AUDIOMIXER
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_audio_mixer_settings_list, ACTION_OK_DL_AUDIO_MIXER_SETTINGS_LIST)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_ai_service_settings_list, ACTION_OK_DL_AI_SERVICE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_accessibility_settings_list, ACTION_OK_DL_ACCESSIBILITY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_input_settings_list, ACTION_OK_DL_INPUT_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_input_menu_settings_list, ACTION_OK_DL_INPUT_MENU_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_input_turbo_fire_settings_list, ACTION_OK_DL_INPUT_TURBO_FIRE_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_input_haptic_feedback_settings_list, ACTION_OK_DL_INPUT_HAPTIC_FEEDBACK_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_latency_settings_list, ACTION_OK_DL_LATENCY_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_recording_settings_list, ACTION_OK_DL_RECORDING_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_playlist_settings_list, ACTION_OK_DL_PLAYLIST_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_playlist_manager_list, ACTION_OK_DL_PLAYLIST_MANAGER_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_input_retropad_binds_list, ACTION_OK_DL_INPUT_RETROPAD_BINDS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_input_hotkey_binds_list, ACTION_OK_DL_INPUT_HOTKEY_BINDS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_user_binds_list, ACTION_OK_DL_USER_BINDS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_accounts_cheevos_list, ACTION_OK_DL_ACCOUNTS_CHEEVOS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_accounts_youtube_list, ACTION_OK_DL_ACCOUNTS_YOUTUBE_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_accounts_twitch_list, ACTION_OK_DL_ACCOUNTS_TWITCH_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_accounts_facebook_list, ACTION_OK_DL_ACCOUNTS_FACEBOOK_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_dump_disc_list, ACTION_OK_DL_DUMP_DISC_LIST)
#ifdef HAVE_LAKKA
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_eject_disc, ACTION_OK_DL_EJECT_DISC)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_load_disc_list, ACTION_OK_DL_LOAD_DISC_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_open_archive, ACTION_OK_DL_OPEN_ARCHIVE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_rgui_menu_theme_preset, ACTION_OK_DL_RGUI_MENU_THEME_PRESET)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_pl_thumbnails_updater_list, ACTION_OK_DL_PL_THUMBNAILS_UPDATER_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_manual_content_scan_list, ACTION_OK_DL_MANUAL_CONTENT_SCAN_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_manual_content_scan_dat_file, ACTION_OK_DL_MANUAL_CONTENT_SCAN_DAT_FILE)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_core_manager_list, ACTION_OK_DL_CORE_MANAGER_LIST)
#ifdef HAVE_MIST
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_steam_settings_list, ACTION_OK_DL_STEAM_SETTINGS_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_core_manager_steam_list, ACTION_OK_DL_CORE_MANAGER_STEAM_LIST)
#endif
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_core_option_override_list, ACTION_OK_DL_CORE_OPTION_OVERRIDE_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_remap_file_manager_list, ACTION_OK_DL_REMAP_FILE_MANAGER_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_savestate_list, ACTION_OK_DL_SAVESTATE_LIST)
STATIC_DEFAULT_ACTION_OK_FUNC(action_ok_push_core_options_list, ACTION_OK_DL_CORE_OPTIONS_LIST)
DEFAULT_ACTION_OK_FUNC(action_ok_push_playlist_manager_settings, ACTION_OK_DL_PLAYLIST_MANAGER_SETTINGS)
#ifdef HAVE_CHEEVOS
DEFAULT_ACTION_OK_FUNC(action_ok_push_achievements_hardcore_pause_list, ACTION_OK_DL_ACHIEVEMENTS_HARDCORE_PAUSE_LIST)
#endif
DEFAULT_ACTION_OK_FUNC(action_ok_push_core_information_list, ACTION_OK_DL_CORE_INFORMATION_LIST)
#ifdef HAVE_MIST
DEFAULT_ACTION_OK_FUNC(action_ok_push_core_information_steam_list, ACTION_OK_DL_CORE_INFORMATION_STEAM_LIST)
#endif

static int action_ok_open_uwp_permission_settings(const char *path,
   const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef __WINRT__
   uwp_open_broadfilesystemaccess_settings();
#endif
   return 0;
}

static int action_ok_open_picker(const char *path,
   const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char *new_path = NULL;
   int ret        = generic_action_ok_displaylist_push(
         path, new_path,
         msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES), MENU_SETTING_ACTION_FAVORITES_DIR,
         idx, entry_idx,
         ACTION_OK_DL_CONTENT_LIST);

   free(new_path);
   return ret;
}

#ifdef HAVE_NETWORKING
#ifdef HAVE_WIFI
static void wifi_menu_refresh_callback(retro_task_t *task,
      void *task_data,
      void *user_data, const char *error)
{
   struct menu_state *menu_st       = menu_state_get_ptr();

   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
}

static int action_ok_wifi_disconnect(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   task_push_wifi_disconnect(wifi_menu_refresh_callback);
   return true;
}
#endif

static int action_ok_netplay_connect_room(const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   char hostname[512];
   struct netplay_room *room;
   net_driver_state_t *net_st = networking_state_get_ptr();
   unsigned room_index        = type - MENU_SETTINGS_NETPLAY_ROOMS_START;

   if (room_index >= (unsigned)net_st->room_count)
      return -1;

   room = &net_st->room_list[room_index];

   if (room->host_method == NETPLAY_HOST_METHOD_MITM)
      snprintf(hostname, sizeof(hostname), "%s|%d|%s",
         room->mitm_address, room->mitm_port, room->mitm_session);
   else
      snprintf(hostname, sizeof(hostname), "%s|%d", room->address, room->port);

   task_push_netplay_crc_scan(room->gamecrc, room->gamename,
      room->subsystem_name, room->corename, hostname);

   return 0;
}

static void netplay_refresh_rooms_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   char *room_data              = NULL;
   const char *path             = NULL;
   const char *label            = NULL;
   unsigned menu_type           = 0;
   enum msg_hash_enums enum_idx = MSG_UNKNOWN;
   http_transfer_data_t *data   = (http_transfer_data_t*)task_data;
   net_driver_state_t *net_st   = networking_state_get_ptr();
   struct menu_state *menu_st   = menu_state_get_ptr();

   free(net_st->room_list);
   net_st->room_list  = NULL;
   net_st->room_count = 0;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   /* Don't push the results if we left the netplay menu */
   if (     !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB))
         && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY)))
      return;

   if (error)
   {
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), error);
      goto done;
   }
   if (!data || !data->data || !data->len || data->status != 200)
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED));
      goto done;
   }

   room_data = (char*)malloc(data->len + 1);
   if (!room_data)
      goto done;
   memcpy(room_data, data->data, data->len);
   room_data[data->len] = '\0';

   if (!string_is_empty(room_data))
   {
      int room_count;

      netplay_rooms_parse(room_data, strlen(room_data));

      if ((room_count = netplay_rooms_get_count()) > 0)
      {
         net_st->room_list = (struct netplay_room*)calloc(room_count,
            sizeof(*net_st->room_list));
         if (net_st->room_list)
         {
            int i;

            net_st->room_count = room_count;
            for (i = 0; i < room_count; i++)
               memcpy(&net_st->room_list[i], netplay_room_get(i),
                  sizeof(*net_st->room_list));
         }
      }

      netplay_rooms_free();
   }

   free(room_data);

done:
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
}

static int action_ok_push_netplay_refresh_rooms(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   task_push_http_transfer(FILE_PATH_LOBBY_LIBRETRO_URL "list", true, NULL,
      netplay_refresh_rooms_cb, NULL);

   return 0;
}

#ifdef HAVE_NETPLAYDISCOVERY
static void netplay_refresh_lan_cb(const void *data)
{
   int i;
   const char *path                      = NULL;
   const char *label                     = NULL;
   unsigned menu_type                    = 0;
   enum msg_hash_enums enum_idx          = MSG_UNKNOWN;
   const struct netplay_host_list *hosts =
      (const struct netplay_host_list*)data;
   net_driver_state_t *net_st            = networking_state_get_ptr();
   struct menu_state *menu_st            = menu_state_get_ptr();

   free(net_st->room_list);
   net_st->room_list  = NULL;
   net_st->room_count = 0;

   menu_entries_get_last_stack(&path, &label, &menu_type, &enum_idx, NULL);

   /* Don't push the results if we left the netplay menu */
   if (     !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB))
         && !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY)))
      return;

   if (!hosts || !hosts->size)
      goto done;

   net_st->room_list  =
      (struct netplay_room*)calloc(hosts->size, sizeof(*net_st->room_list));
   if (!net_st->room_list)
      goto done;
   net_st->room_count = (int)hosts->size;

   for (i = 0; i < net_st->room_count; i++)
   {
      struct netplay_host *host = &hosts->hosts[i];
      struct netplay_room *room = &net_st->room_list[i];

      room->gamecrc = host->content_crc;
      room->port    = host->port;

      strlcpy(room->nickname, host->nick, sizeof(room->nickname));
      strlcpy(room->frontend, host->frontend, sizeof(room->frontend));
      strlcpy(room->corename, host->core, sizeof(room->corename));
      strlcpy(room->coreversion, host->core_version,
         sizeof(room->coreversion));
      strlcpy(room->retroarch_version, host->retroarch_version,
         sizeof(room->retroarch_version));
      strlcpy(room->gamename, host->content, sizeof(room->gamename));
      strlcpy(room->subsystem_name, host->subsystem_name,
         sizeof(room->subsystem_name));
      strlcpy(room->address, host->address, sizeof(room->address));

      room->has_password          = host->has_password;
      room->has_spectate_password = host->has_spectate_password;

      room->connectable  = true;
      room->is_retroarch = true;
      room->lan          = true;
   }

done:
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
}

static int action_ok_push_netplay_refresh_lan(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   task_push_netplay_lan_scan(netplay_refresh_lan_cb, 800);

   return 0;
}
#endif

static int action_ok_push_netplay_kick(const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   char msg[256];
   netplay_client_info_t client;

   client.id = (int)strtol(label, NULL, 10);
   strlcpy(client.name, path, sizeof(client.name));

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_KICK_CLIENT, &client))
      snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_NETPLAY_KICKED_CLIENT_S), client.name);
   else
      snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S), client.name);

   runloop_msg_queue_push(msg, 1, 180, true, NULL,
      MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_netplay_ban(const char *path, const char *label,
      unsigned type, size_t idx, size_t entry_idx)
{
   char msg[256];
   netplay_client_info_t client;

   client.id = (int)strtol(label, NULL, 10);
   strlcpy(client.name, path, sizeof(client.name));

   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_BAN_CLIENT, &client))
      snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_NETPLAY_BANNED_CLIENT_S), client.name);
   else
      snprintf(msg, sizeof(msg),
         msg_hash_to_str(MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S), client.name);

   runloop_msg_queue_push(msg, 1, 180, true, NULL,
      MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}
#endif

DEFAULT_ACTION_OK_DL_PUSH(action_ok_content_collection_list, FILEBROWSER_SELECT_COLLECTION, ACTION_OK_DL_CONTENT_COLLECTION_LIST, NULL)
DEFAULT_ACTION_OK_DL_PUSH(action_ok_push_content_list, FILEBROWSER_SELECT_FILE, ACTION_OK_DL_CONTENT_LIST, settings->paths.directory_menu_content)
DEFAULT_ACTION_OK_DL_PUSH(action_ok_push_scan_file, FILEBROWSER_SCAN_FILE, ACTION_OK_DL_CONTENT_LIST, settings->paths.directory_menu_content)

static int action_ok_scan_directory_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();
   const char *dir_menu_content      = settings->paths.directory_menu_content;

   filebrowser_clear_type();
   return generic_action_ok_displaylist_push(
         path, dir_menu_content,
         label, type,
         idx, entry_idx,
         ACTION_OK_DL_SCAN_DIR_LIST);
}

static int action_ok_push_random_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(
         path, path,
         msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES), type,
         idx, entry_idx,
         ACTION_OK_DL_CONTENT_LIST);
}

static int action_ok_push_downloads_dir(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();
   const char *dir_core_assets       = settings->paths.directory_core_assets;

   filebrowser_set_type(FILEBROWSER_SELECT_FILE);
   return generic_action_ok_displaylist_push(
         path, dir_core_assets,
         msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES), type,
         idx, entry_idx,
         ACTION_OK_DL_CONTENT_LIST);
}

int action_ok_push_filebrowser_list_dir_select(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char current_value[PATH_MAX_LENGTH];
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_handle_t *menu        = menu_st->driver_data;

   current_value[0] = '\0';

   if (!menu)
      return -1;

   /* Start browsing from current directory */
   get_current_menu_value(menu_st, current_value, sizeof(current_value));
   if (!path_is_directory(current_value))
      current_value[0] = '\0';

   filebrowser_set_type(FILEBROWSER_SELECT_DIR);
   strlcpy(menu->filebrowser_label, label, sizeof(menu->filebrowser_label));
   return generic_action_ok_displaylist_push(
         path, current_value,
         label, type,
         idx, entry_idx,
         ACTION_OK_DL_FILE_BROWSER_SELECT_DIR);
}

int action_ok_push_filebrowser_list_file_select(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu       = menu_state_get_ptr()->driver_data;

   if (!menu)
      return -1;

   filebrowser_set_type(FILEBROWSER_SELECT_FILE);
   strlcpy(menu->filebrowser_label, label, sizeof(menu->filebrowser_label));
   return generic_action_ok_displaylist_push(
         path, NULL,
         label, type,
         idx, entry_idx,
         ACTION_OK_DL_FILE_BROWSER_SELECT_DIR);
}

int action_ok_push_manual_content_scan_dir_select(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t            *settings   = config_get_ptr();
   const char *dir_menu_content      = settings->paths.directory_menu_content;

   filebrowser_clear_type();
   return generic_action_ok_displaylist_push(
         path, dir_menu_content,
         label, type,
         idx, entry_idx,
         ACTION_OK_DL_MANUAL_SCAN_DIR_LIST);
}

/* TODO/FIXME */
static int action_ok_push_dropdown_setting_core_options_item_special(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   core_option_manager_t *coreopts = NULL;
   int core_option_idx             = (int)atoi(label);

   retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

   if (!coreopts)
      return -1;

   core_option_manager_set_val(coreopts,
         core_option_idx, idx, false);
   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_dropdown_setting_core_options_item(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   core_option_manager_t *coreopts = NULL;
   int core_option_idx             = (int)atoi(label);

   retroarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &coreopts);

   if (!coreopts)
      return -1;

   core_option_manager_set_val(coreopts,
         core_option_idx, idx, false);
   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

/* TODO/FIXME */

static int action_ok_push_dropdown_setting_uint_item_special(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned value;
   enum msg_hash_enums enum_idx = (enum msg_hash_enums)atoi(label);
   rarch_setting_t     *setting = menu_setting_find_enum(enum_idx);

   if (!setting)
      return -1;

   value = (unsigned)(idx + setting->offset_by);

   if (!string_is_empty(path))
   {
      unsigned path_value = atoi(path);
      if (path_value != value)
         value = path_value;
   }

   *setting->value.target.unsigned_integer = value;

   if (setting->change_handler)
      setting->change_handler(setting);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}


static int generic_action_ok_dropdown_setting(const char *path, const char *label,
      unsigned type, size_t idx)
{
   enum msg_hash_enums enum_idx = (enum msg_hash_enums)atoi(label);
   rarch_setting_t     *setting = menu_setting_find_enum(enum_idx);

   if (!setting)
      return -1;

   switch (setting->type)
   {
      case ST_INT:
         *setting->value.target.integer = (int32_t)(idx + setting->offset_by);
         break;
      case ST_UINT:
         {
            unsigned value = (unsigned)(idx + setting->offset_by);
            *setting->value.target.unsigned_integer = value;
         }
         break;
      case ST_FLOAT:
         {
            float val                    = (float)atof(path);
            *setting->value.target.fraction = (float)val;
         }
         break;
      case ST_STRING_OPTIONS:
         if (setting->get_string_representation)
         {
            struct string_list tmp_str_list = { 0 };
            string_list_initialize(&tmp_str_list);
            string_split_noalloc(&tmp_str_list,
               setting->values, "|");

            if (idx < tmp_str_list.size)
            {
               strlcpy(setting->value.target.string,
                  tmp_str_list.elems[idx].data, setting->size);
            }

            string_list_deinitialize(&tmp_str_list);
            break;
         }
         /* fallthrough */
      case ST_STRING:
      case ST_PATH:
      case ST_DIR:
         strlcpy(setting->value.target.string, path,
               setting->size);
         break;
      default:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_dropdown_setting(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_dropdown_setting(path, label, type, idx);
}

static int action_ok_push_dropdown_item(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}

int action_cb_push_dropdown_item_resolution(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char str[100];
   char *pch            = NULL;
   unsigned width       = 0;
   unsigned height      = 0;
   unsigned refreshrate = 0;

   strlcpy(str, path, sizeof(str));
   pch            = strtok(str, "x");
   if (pch)
      width       = (unsigned)strtoul(pch, NULL, 0);
   pch            = strtok(NULL, " ");
   if (pch)
      height      = (unsigned)strtoul(pch, NULL, 0);
   pch            = strtok(NULL, "(");
   if (pch)
      refreshrate = (unsigned)strtoul(pch, NULL, 0);

   if (video_display_server_set_resolution(width, height,
         refreshrate, (float)refreshrate, 0, 0, 0, 0))
   {
      settings_t *settings = config_get_ptr();
#ifdef _MSC_VER
      float num            = refreshrate / 60.0f;
      unsigned refresh_mod = num > 0 ? (unsigned)(floorf(num + 0.5f)) : (unsigned)(ceilf(num - 0.5f));
#else
      unsigned refresh_mod = (unsigned)lroundf((float)(refreshrate / 60.0f));
#endif
      float refresh_exact  = refreshrate;

      /* 59 Hz is an inaccurate representation of the real value (59.94).
       * And since we at this point only have the integer to work with,
       * the exact float needs to be calculated for 'video_refresh_rate' */
      if (refreshrate == (60.0f * refresh_mod) - 1)
         refresh_exact = 59.94f * refresh_mod;

      video_monitor_set_refresh_rate(refresh_exact);

      settings->uints.video_fullscreen_x = width;
      settings->uints.video_fullscreen_y = height;

      action_cancel_pop_default(NULL, NULL, 0, 0);
   }

   return 0;
}

static int action_ok_push_dropdown_item_video_shader_num_pass(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return -1;

   shader->passes  = (unsigned)idx;

   video_shader_resolve_parameters(shader);

   shader->flags  |= SHDR_FLAG_MODIFIED;

   return action_cancel_pop_default(NULL, NULL, 0, 0);
#else
   return 0;
#endif
}

static int action_ok_push_dropdown_item_video_shader_param_generic(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      size_t setting_offset)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   video_shader_ctx_t shader_info;
   unsigned offset                           = (unsigned)setting_offset;
   float val                                 = atof(path);
   struct video_shader *shader               = menu_shader_get();
   struct video_shader_parameter *param_menu = NULL;
   struct video_shader_parameter *param_prev = NULL;

   video_shader_driver_get_current_shader(&shader_info);

   param_prev    = &shader_info.data->parameters[entry_idx - offset];
   if (shader)
      param_menu = &shader->parameters [entry_idx - offset];

   if (!param_prev || !param_menu)
      return -1;

   param_prev->current  = val;
   param_menu->current  = param_prev->current;

   shader->flags       |= SHDR_FLAG_MODIFIED;

   return action_cancel_pop_default(NULL, NULL, 0, 0);
#else
   return 0;
#endif
}

static int action_ok_push_dropdown_item_video_shader_param(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_push_dropdown_item_video_shader_param_generic(
         path, label, type,
         idx, entry_idx, MENU_SETTINGS_SHADER_PARAMETER_0);
}

static int action_ok_push_dropdown_item_video_shader_preset_param(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return action_ok_push_dropdown_item_video_shader_param_generic(
         path, label, type,
         idx, entry_idx, MENU_SETTINGS_SHADER_PRESET_PARAMETER_0);
}

static int action_ok_push_dropdown_item_resolution(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   /* TODO/FIXME - menu drivers like XMB don't rescale
    * automatically */
   if (action_cb_push_dropdown_item_resolution(path,
            label, type, idx, entry_idx) == 1)
      return -1;
   return 0;
}

static int action_ok_push_dropdown_item_playlist_default_core(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   core_info_list_t *core_info_list = NULL;
   playlist_t *playlist             = playlist_get_cached();
   const char* core_name            = path;

   /* Get core list */
   core_info_get_list(&core_info_list);

   if (!core_info_list || !playlist)
      return -1;

   /* Handle N/A or empty path input */
   if (string_is_empty(core_name) ||
       string_is_equal(core_name, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)))
   {
      playlist_set_default_core_path(playlist, FILE_PATH_DETECT);
      playlist_set_default_core_name(playlist, FILE_PATH_DETECT);
   }
   else
   {
      size_t i;

      /* Loop through cores until we find a match */
      for (i = 0; i < core_info_list->count; i++)
      {
         core_info_t *core_info = core_info_get(core_info_list, i);

         if (core_info)
         {
            const char *core_info_display_name   = core_info->display_name;
            const char *core_info_path           = core_info->path;
            if (string_is_equal(core_name, core_info_display_name))
            {
               /* Update playlist */
               playlist_set_default_core_path(playlist, core_info_path);
               playlist_set_default_core_name(playlist, core_info_display_name);
               goto end;
            }
         }
      }

      /* if we couldn't find a match, add this fallback... */
      playlist_set_default_core_path(playlist, FILE_PATH_DETECT);
      playlist_set_default_core_name(playlist, FILE_PATH_DETECT);
   }

end:
   /* In all cases, update file on disk */
   playlist_write_file(playlist);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}


static int action_ok_push_dropdown_item_playlist_label_display_mode(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist = playlist_get_cached();

   playlist_set_label_display_mode(playlist,
         (enum playlist_label_display_mode)idx);

   /* In all cases, update file on disk */
   playlist_write_file(playlist);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int generic_set_thumbnail_mode(
      enum playlist_thumbnail_id thumbnail_id, size_t idx)
{
   playlist_t *playlist = playlist_get_cached();

   if (!playlist)
      return -1;

   playlist_set_thumbnail_mode(playlist, thumbnail_id,
         (enum playlist_thumbnail_mode)idx);
   playlist_write_file(playlist);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_dropdown_item_playlist_right_thumbnail_mode(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_set_thumbnail_mode(PLAYLIST_THUMBNAIL_RIGHT, idx);
}

static int action_ok_push_dropdown_item_playlist_left_thumbnail_mode(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_set_thumbnail_mode(PLAYLIST_THUMBNAIL_LEFT, idx);
}

static int action_ok_push_dropdown_item_playlist_sort_mode(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist = playlist_get_cached();

   playlist_set_sort_mode(playlist, (enum playlist_sort_mode)idx);
   playlist_write_file(playlist);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_dropdown_item_manual_content_scan_system_name(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char* system_name                                    = path;
   enum manual_content_scan_system_name_type system_name_type =
         MANUAL_CONTENT_SCAN_SYSTEM_NAME_DATABASE;

   /* Get system name type (i.e. check if setting is
    * 'use content directory' or 'use custom') */
   switch (idx)
   {
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CONTENT_DIR:
      case MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM:
         system_name_type = (enum manual_content_scan_system_name_type)idx;
         break;
      default:
         break;
   }

   /* Set system name */
   manual_content_scan_set_menu_system_name(
      system_name_type, system_name);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_dropdown_item_manual_content_scan_core_name(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char* core_name                        = path;
   enum manual_content_scan_core_type core_type =
         MANUAL_CONTENT_SCAN_CORE_SET;

   /* Get core type (i.e. check if setting is
    * DETECT/Unspecified) */
   if (idx == (size_t)MANUAL_CONTENT_SCAN_CORE_DETECT)
      core_type = MANUAL_CONTENT_SCAN_CORE_DETECT;

   /* Set core name */
   manual_content_scan_set_menu_core_name(
      core_type, core_name);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_dropdown_item_disk_index(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   unsigned disk_index        = (unsigned)idx;

   command_event(CMD_EVENT_DISK_INDEX, &disk_index);

   /* When choosing a disk, menu selection should
    * automatically be reset to the 'insert disk'
    * option */
   menu_entries_pop_stack(NULL, 0, 1);
   menu_st->selection_ptr = 0;

   return 0;
}

static int action_ok_push_dropdown_item_audio_device(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path        = NULL;
   enum msg_hash_enums enum_idx;
   rarch_setting_t     *setting;
   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);
   enum_idx = (enum msg_hash_enums)atoi(menu_path);
   setting  = menu_setting_find_enum(enum_idx);

   if (!setting)
      return -1;

   strlcpy(setting->value.target.string, label, setting->size);

   command_event(CMD_EVENT_AUDIO_REINIT, NULL);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

#ifdef HAVE_MICROPHONE
static int action_ok_push_dropdown_item_microphone_device(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path        = NULL;
   enum msg_hash_enums enum_idx;
   rarch_setting_t     *setting;
   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);
   enum_idx = (enum msg_hash_enums)atoi(menu_path);
   setting  = menu_setting_find_enum(enum_idx);

   if (!setting)
      return -1;

   strlcpy(setting->value.target.string, label, setting->size);

   command_event(CMD_EVENT_MICROPHONE_REINIT, NULL);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}
#endif

static int action_ok_push_dropdown_item_input_device_type(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   rarch_setting_t *setting;
   enum msg_hash_enums enum_idx;
   retro_ctx_controller_info_t pad;
   unsigned port                = 0;
   unsigned device              = 0;

   const char *menu_path        = NULL;
   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);
   enum_idx = (enum msg_hash_enums)atoi(menu_path);
   setting  = menu_setting_find_enum(enum_idx);

   if (!setting)
      return -1;

   port   = setting->index_offset;
   device = atoi(label);

   input_config_set_device(port, device);

   pad.port   = port;
   pad.device = device;

   core_set_controller_port_device(&pad);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

#ifdef ANDROID
static int action_ok_push_dropdown_item_input_select_physical_keyboard(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
    char* keyboard;
    const char *no_keyboard;
    const char *keyboard_name;
    enum msg_hash_enums enum_idx;
    rarch_setting_t *setting     = NULL;
    settings_t *settings         = config_get_ptr();
    const char *menu_path        = NULL;
    struct menu_state *menu_st   = menu_state_get_ptr();
    menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);
    enum_idx = (enum msg_hash_enums)atoi(menu_path);
    setting  = menu_setting_find_enum(enum_idx);

    if (!setting)
        return -1;

    keyboard_name = path;
    no_keyboard   = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
    if (string_is_equal(keyboard_name, no_keyboard))
        settings->arrays.input_android_physical_keyboard[0] = '\0';
    else
    {
       int i;
       for (i = 0; i < MAX_INPUT_DEVICES; i++)
       {
          const char* device_name = input_config_get_device_name(i);
          if (string_is_equal(device_name, keyboard_name))
          {
             uint16_t vendor_id = input_config_get_device_vid(i);
             uint16_t product_id = input_config_get_device_pid(i);
             snprintf(settings->arrays.input_android_physical_keyboard,
                   sizeof(settings->arrays.input_android_physical_keyboard),
                   "%04x:%04x %s",
                   vendor_id, product_id, keyboard_name);
             break;
          }
       }
       /*
        * if we did not find the selected device, do nothing, the user has chosen to keep
        * the previous configuration, which is to use a device that is either not plugged right
        * now or already working as the physical keyboard.
        */
    }
    settings->modified = true;

    command_event(CMD_EVENT_REINIT, NULL);

    /* Refresh menu */
    menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                     | MENU_ST_FLAG_PREVENT_POPULATE;

    return action_cancel_pop_default(NULL, NULL, 0, 0);
}

#endif

static int action_ok_push_dropdown_item_input_description(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned remap_idx   = (unsigned)entry_idx;
   unsigned entry_type  = string_to_unsigned(label);
   settings_t *settings = config_get_ptr();
   unsigned user_idx;
   unsigned btn_idx;

   if (     !settings
       ||  (entry_type < MENU_SETTINGS_INPUT_DESC_BEGIN)
       || ((remap_idx >= RARCH_CUSTOM_BIND_LIST_END)
       &&  (remap_idx != RARCH_UNMAPPED)))
      return -1;

   /* Determine user/button indices */
   user_idx = (entry_type - MENU_SETTINGS_INPUT_DESC_BEGIN)
      / (RARCH_FIRST_CUSTOM_BIND + 8);
   btn_idx  = (entry_type - MENU_SETTINGS_INPUT_DESC_BEGIN)
      - (RARCH_FIRST_CUSTOM_BIND + 8) * user_idx;

   if ((user_idx >= MAX_USERS) || (btn_idx >= RARCH_CUSTOM_BIND_LIST_END))
      return -1;

   /* Assign new mapping */
   settings->uints.input_remap_ids[user_idx][btn_idx] = remap_idx;

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_push_dropdown_item_input_description_kbd(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned key_id      = (unsigned)entry_idx;
   unsigned entry_type  = string_to_unsigned(label);
   settings_t *settings = config_get_ptr();
   unsigned user_idx;
   unsigned btn_idx;

   if (   (!settings)
       || (entry_type < MENU_SETTINGS_INPUT_DESC_KBD_BEGIN)
       || (key_id >= (RARCH_MAX_KEYS + MENU_SETTINGS_INPUT_DESC_KBD_BEGIN)))
      return -1;

   /* Determine user/button indices */
   user_idx = (entry_type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN)
      / RARCH_ANALOG_BIND_LIST_END;
   btn_idx  = (entry_type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN)
      - RARCH_ANALOG_BIND_LIST_END * user_idx;

   if ((user_idx >= MAX_USERS) || (btn_idx >= RARCH_CUSTOM_BIND_LIST_END))
      return -1;

   /* Assign new mapping */
   settings->uints.input_keymapper_ids[user_idx][btn_idx] = key_id;

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

#ifdef HAVE_NETWORKING
static int action_ok_push_dropdown_item_netplay_mitm_server(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path        = NULL;
   enum msg_hash_enums enum_idx;
   rarch_setting_t     *setting;

   menu_entries_get_last_stack(&menu_path, NULL, NULL, NULL, NULL);
   enum_idx = (enum msg_hash_enums)atoi(menu_path);
   setting  = menu_setting_find_enum(enum_idx);

   if (!setting)
      return -1;

   strlcpy(setting->value.target.string,
           label, setting->size);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}
#endif

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   filebrowser_clear_type();
   return generic_action_ok_displaylist_push(
         path, NULL,
         label, type,
         idx, entry_idx,
         ACTION_OK_DL_PUSH_DEFAULT);
}

static int action_ok_start_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   content_ctx_info_t content_info;
   struct menu_state *menu_st          = menu_state_get_ptr();

   content_info.argc                   = 0;
   content_info.argv                   = NULL;
   content_info.args                   = NULL;
   content_info.environ_get            = NULL;

   /* We are going to push a new menu; ensure
    * that the current one is cached for animation
    * purposes */
   if (menu_st->driver_ctx && menu_st->driver_ctx->list_cache)
      menu_st->driver_ctx->list_cache(menu_st->userdata,
            MENU_LIST_PLAIN, MENU_ACTION_NOOP);

   path_clear(RARCH_PATH_BASENAME);
   if (!task_push_start_current_core(&content_info))
      return -1;

   return 0;
}

static int action_ok_contentless_core_run(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *core_path      = path;
   struct menu_state *menu_st = menu_state_get_ptr();
   /* TODO/FIXME: If this function succeeds, the
    * quick menu will be pushed on the subsequent
    * frame via the RARCH_MENU_CTL_SET_PENDING_QUICK_MENU
    * command. The way this is implemented 'breaks' the
    * menu stack record, so when leaving the quick
    * menu via a 'cancel' operation, the last selected
    * menu index is lost. We therefore have to cache
    * the current selection here, and reapply it manually
    * when building the contentless cores list... */
   size_t selection           = menu_st->selection_ptr;
   uint32_t flags             = runloop_get_flags();

   if (string_is_empty(core_path))
      return -1;

   /* If core is already running, open quick menu */
   if (   retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path)
       && (flags & RUNLOOP_FLAG_CORE_RUNNING))
   {
      bool flush_menu               = false;
      menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, &flush_menu);
      menu_st->contentless_core_ptr = selection;
      menu_st->selection_ptr        = 0;
      return 0;
   }

   /* Cache current menu selection *before* attempting
    * to start the core, to ensure consistent menu
    * navigation (i.e. running a core will in general
    * cause a redraw of the menu, so must record current
    * position even if the operation fails) */
   menu_st->contentless_core_ptr    = selection;

   /* Load and start core */
   path_clear(RARCH_PATH_BASENAME);
   if (!task_push_load_contentless_core_from_menu(core_path))
   {
      if (retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
         generic_action_ok_command(CMD_EVENT_UNLOAD_CORE);
      return -1;
   }

   return 0;
}

static int action_ok_load_archive(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path           = NULL;
   const char *content_path        = NULL;
   struct menu_state *menu_st      = menu_state_get_ptr();
   menu_handle_t *menu             = menu_st->driver_data;

   if (!menu)
      return -1;

   menu_path    = menu->scratch2_buf;
   content_path = menu->scratch_buf;

   fill_pathname_join_special(menu->detect_content_path,
         menu_path, content_path,
         sizeof(menu->detect_content_path));

   generic_action_ok_command(CMD_EVENT_LOAD_CORE);

   return default_action_ok_load_content_with_core_from_menu(
         menu->detect_content_path,
         CORE_TYPE_PLAIN);
}

static int action_ok_load_archive_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char new_core_path[PATH_MAX_LENGTH];
   menu_content_ctx_defer_info_t def_info;
   int ret                             = 0;
   core_info_list_t *list              = NULL;
   const char *menu_path               = NULL;
   const char *content_path            = NULL;
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;

   if (!menu)
      return -1;

   menu_path           = menu->scratch2_buf;
   content_path        = menu->scratch_buf;

   core_info_get_list(&list);

   def_info.data       = list;
   def_info.dir        = menu_path;
   def_info.path       = content_path;
   def_info.menu_label = label;
   def_info.s          = menu->deferred_path;
   def_info.len        = sizeof(menu->deferred_path);

   new_core_path[0]    = '\0';

   if (menu_content_find_first_core(&def_info, false,
            new_core_path, sizeof(new_core_path)))
      ret = -1;

   fill_pathname_join_special(
         menu->detect_content_path, menu_path, content_path,
         sizeof(menu->detect_content_path));

   switch (ret)
   {
      case -1:
         {
            content_ctx_info_t content_info;

            content_info.argc                   = 0;
            content_info.argv                   = NULL;
            content_info.args                   = NULL;
            content_info.environ_get            = NULL;

            ret                                 = 0;

            if (!task_push_load_content_with_new_core_from_menu(
                     new_core_path, def_info.s,
                     &content_info,
                     CORE_TYPE_PLAIN,
                     NULL, NULL))
               ret = -1;
            else
               menu_driver_set_last_start_content(menu_st, def_info.s);
         }
         break;
      case 0:
         idx = menu_st->selection_ptr;
         ret = generic_action_ok_displaylist_push(
               path, NULL,
               label, type,
               idx, entry_idx,
               ACTION_OK_DL_DEFERRED_CORE_LIST);
         break;
      default:
         break;
   }

   return ret;
}

DEFAULT_ACTION_OK_HELP(action_ok_help_audio_video_troubleshooting, MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING, MENU_DIALOG_HELP_AUDIO_VIDEO_TROUBLESHOOTING)
DEFAULT_ACTION_OK_HELP(action_ok_help, MENU_ENUM_LABEL_HELP, MENU_DIALOG_WELCOME)
DEFAULT_ACTION_OK_HELP(action_ok_help_controls, MENU_ENUM_LABEL_HELP_CONTROLS, MENU_DIALOG_HELP_CONTROLS)
DEFAULT_ACTION_OK_HELP(action_ok_help_what_is_a_core, MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE, MENU_DIALOG_HELP_WHAT_IS_A_CORE)
DEFAULT_ACTION_OK_HELP(action_ok_help_scanning_content, MENU_ENUM_LABEL_HELP_SCANNING_CONTENT, MENU_DIALOG_HELP_SCANNING_CONTENT)
DEFAULT_ACTION_OK_HELP(action_ok_help_change_virtual_gamepad, MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD, MENU_DIALOG_HELP_CHANGE_VIRTUAL_GAMEPAD)
DEFAULT_ACTION_OK_HELP(action_ok_help_load_content, MENU_ENUM_LABEL_HELP_LOADING_CONTENT, MENU_DIALOG_HELP_LOADING_CONTENT)

static int generic_dropdown_box_list(size_t idx, unsigned lbl)
{
   generic_action_ok_displaylist_push(
         NULL, NULL,
         NULL, 0,
         idx, 0,
         lbl);
   return 0;
}

static int action_ok_video_resolution(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#if defined(GEKKO) || defined(PS2) || defined(__PS3__)
   unsigned width   = 0;
   unsigned  height = 0;
   char desc[64]    = {0};

   if (video_driver_get_video_output_size(&width, &height, desc, sizeof(desc)))
   {
      char msg[128];
      msg[0] = '\0';

#if defined(_WIN32) || defined(__PS3__)
      generic_action_ok_command(CMD_EVENT_REINIT);
#endif
      video_driver_set_video_mode(width, height, true);
#ifdef GEKKO
      if (width == 0 || height == 0)
         snprintf(msg, sizeof(msg),
               msg_hash_to_str(MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT));
      else
#endif
      {
         if (!string_is_empty(desc))
            snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_SCREEN_RESOLUTION_APPLYING_DESC),
                  width, height, desc);
         else
            snprintf(msg, sizeof(msg),
                  msg_hash_to_str(MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC),
                  width, height);
      }
      runloop_msg_queue_push(msg, 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   return 0;
#else
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_RESOLUTION);
#endif
}

static int action_ok_playlist_default_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_DEFAULT_CORE);
}

static int action_ok_playlist_label_display_mode(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LABEL_DISPLAY_MODE);
}

static int action_ok_playlist_right_thumbnail_mode(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_RIGHT_THUMBNAIL_MODE);
}

static int action_ok_playlist_left_thumbnail_mode(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_LEFT_THUMBNAIL_MODE);
}

static int action_ok_playlist_sort_mode(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_PLAYLIST_SORT_MODE);
}

static int action_ok_remappings_port_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   generic_action_ok_displaylist_push(
         path, NULL,
         label, 0,
         idx, 0,
         ACTION_OK_DL_REMAPPINGS_PORT_LIST);
   return 0;
}

static int action_ok_shader_parameter_dropdown_box_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PARAMETER);
}

static int action_ok_shader_preset_parameter_dropdown_box_list(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_SHADER_PRESET_PARAMETER);
}

static int action_ok_manual_content_scan_system_name(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_SYSTEM_NAME);
}

static int action_ok_manual_content_scan_core_name(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_MANUAL_CONTENT_SCAN_CORE_NAME);
}

static int action_ok_video_shader_num_passes_dropdown_box_list(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_VIDEO_SHADER_NUM_PASSES);
}

static int action_ok_disk_index_dropdown_box_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_dropdown_box_list(idx,
         ACTION_OK_DL_DROPDOWN_BOX_LIST_DISK_INDEX);
}

static int action_ok_input_description_dropdown_box_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(
      path, NULL,
      label, type,
      idx, entry_idx,
      ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION);
}

static int action_ok_input_description_kbd_dropdown_box_list(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_displaylist_push(
      path, NULL,
      label, type,
      idx, entry_idx,
      ACTION_OK_DL_DROPDOWN_BOX_LIST_INPUT_DESCRIPTION_KBD);
}

static int action_ok_disk_cycle_tray_status(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   bool disk_ejected              = false;
   bool print_log                 = false;
   struct menu_state *menu_st     = menu_state_get_ptr();
   rarch_system_info_t *sys_info  = &runloop_state_get_ptr()->system;
   settings_t *settings           = config_get_ptr();
#ifdef HAVE_AUDIOMIXER
   bool audio_enable_menu         = settings->bools.audio_enable_menu;
   bool audio_enable_menu_ok      = settings->bools.audio_enable_menu_ok;
#endif
   bool menu_insert_disk_resume   = settings->bools.menu_insert_disk_resume;

   if (!settings)
      return -1;

#ifdef HAVE_AUDIOMIXER
   if (audio_enable_menu && audio_enable_menu_ok)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
#endif

   /* Get disk eject state *before* toggling drive status */
   if (sys_info)
      disk_ejected = disk_control_get_eject_state(&sys_info->disk_control);

   /* Only want to display a notification if we are
    * going to resume content immediately after
    * inserting a disk (i.e. if quick menu remains
    * open, there is sufficient visual feedback
    * without a notification) */
   print_log = menu_insert_disk_resume && disk_ejected;

   if (!command_event(CMD_EVENT_DISK_EJECT_TOGGLE, &print_log))
      return -1;

   /* If we reach this point, then tray toggle
    * was successful */
   disk_ejected = !disk_ejected;

   /* If disk is now ejected, menu selection should
    * automatically increment to the 'current disk
    * index' option */
   if (disk_ejected)
      menu_st->selection_ptr = 1;

   /* If disk is now inserted and user has enabled
    * 'menu_insert_disk_resume', resume running content */
   if (!disk_ejected && menu_insert_disk_resume)
      generic_action_ok_command(CMD_EVENT_RESUME);

   return 0;
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char image_path[PATH_MAX_LENGTH];
   rarch_system_info_t *sys_info = &runloop_state_get_ptr()->system;
   struct menu_state *menu_st    = menu_state_get_ptr();
   menu_handle_t *menu           = menu_st->driver_data;
   const char *menu_path         = NULL;
   settings_t *settings          = config_get_ptr();
#ifdef HAVE_AUDIOMIXER
   bool audio_enable_menu        = settings->bools.audio_enable_menu;
   bool audio_enable_menu_ok     = settings->bools.audio_enable_menu_ok;
#endif
   bool menu_insert_disk_resume  = settings->bools.menu_insert_disk_resume;

   image_path[0]                 = '\0';

   if (!menu)
      return -1;

#ifdef HAVE_AUDIOMIXER
   if (audio_enable_menu && audio_enable_menu_ok)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_OK);
#endif

   /* Get file path of new disk image */
   menu_entries_get_last_stack(&menu_path,
         NULL, NULL, NULL, NULL);

   if (!string_is_empty(menu_path))
   {
      if (!string_is_empty(path))
         fill_pathname_join_special(image_path,
               menu_path, path, sizeof(image_path));
      else
         strlcpy(image_path, menu_path, sizeof(image_path));
   }

   /* Append image */
   command_event(CMD_EVENT_DISK_APPEND_IMAGE, image_path);

   /* In all cases, return to the disk options menu */
   menu_entries_flush_stack(msg_hash_to_str(MENU_ENUM_LABEL_DISK_OPTIONS), 0);

   /* > If disk tray is open, reset menu selection to
    *   the 'insert disk' option
    * > If disk try is closed and user has enabled
    *   'menu_insert_disk_resume', resume running content */
   if (sys_info && disk_control_get_eject_state(&sys_info->disk_control))
      menu_st->selection_ptr = 0;
   else if (menu_insert_disk_resume)
      generic_action_ok_command(CMD_EVENT_RESUME);

   return 0;
}

static int action_ok_manual_content_scan_start(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_config_t playlist_config;
   settings_t *settings                = config_get_ptr();
   const char *directory_playlist      = settings->paths.directory_playlist;

   /* Note: playlist_config.path will set by the
    * task itself */
   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings->bools.playlist_use_old_format;
   playlist_config.compress            = settings->bools.playlist_compression;
   playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(&playlist_config,
         settings->bools.playlist_portable_paths ?
               settings->paths.directory_menu_content : NULL);

   task_push_manual_content_scan(&playlist_config, directory_playlist);
   return 0;
}

#ifdef HAVE_NETWORKING
static int action_ok_netplay_enable_host(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (command_event(CMD_EVENT_NETPLAY_ENABLE_HOST, NULL))
      return generic_action_ok_command(CMD_EVENT_RESUME);

   return -1;
}

static void action_ok_netplay_enable_client_hostname_cb(void *userdata,
      const char *line)
{
   if (!string_is_empty(line))
   {
      if (!task_push_netplay_content_reload(line))
      {
#ifdef HAVE_DYNAMIC
         command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
         netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
         command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, (void*)line);

         runloop_msg_queue_push(
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED),
            1, 480, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#else
         runloop_msg_queue_push(
            msg_hash_to_str(MSG_NETPLAY_NEED_CONTENT_LOADED),
            1, 480, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#endif

         menu_input_dialog_end();
      }
      else
      {
         menu_input_dialog_end();
         retroarch_menu_running_finished(false);
      }
   }
   else
      menu_input_dialog_end();
}

static int action_ok_netplay_enable_client(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings       = config_get_ptr();
   const char *netplay_server = settings->paths.netplay_server;

   if (!string_is_empty(netplay_server))
   {
      action_ok_netplay_enable_client_hostname_cb(NULL, netplay_server);

      return 0;
   }
   else
   {
      menu_input_ctx_line_t hostname = {0};

      hostname.label         =
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS);
      hostname.label_setting = "no_setting";
      hostname.cb            = action_ok_netplay_enable_client_hostname_cb;

      if (menu_input_dialog_start(&hostname))
         return 0;
   }

   return -1;
}

static int action_ok_netplay_disconnect(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);

   return generic_action_ok_command(CMD_EVENT_RESUME);
}
#endif

static int action_ok_core_create_backup(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *core_path             = label;
   settings_t *settings              = config_get_ptr();
   unsigned auto_backup_history_size = settings->uints.core_updater_auto_backup_history_size;
   const char *dir_core_assets       = settings->paths.directory_core_assets;

   if (string_is_empty(core_path))
      return -1;

   task_push_core_backup(core_path, NULL, 0, CORE_BACKUP_MODE_MANUAL,
         (size_t)auto_backup_history_size, dir_core_assets, false);

   return 0;
}

static int action_ok_core_restore_backup(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *backup_path  = label;
   bool core_loaded         = false;
   settings_t *settings     = config_get_ptr();
   const char *dir_libretro = settings->paths.directory_libretro;

   if (string_is_empty(backup_path))
      return -1;

   /* If core to be restored is currently loaded, the task
    * will unload it
    * > In this case, must flush the menu stack
    *   (otherwise user will be faced with 'no information
    *   available' when popping the stack - this would be
    *   confusing/ugly) */
   if (   task_push_core_restore(backup_path, dir_libretro, &core_loaded)
       && core_loaded)
      menu_entries_flush_stack(NULL, 0);

   return 0;
}

static int action_ok_core_delete_backup(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st       = menu_state_get_ptr();
   const char *backup_path          = label;
   if (string_is_empty(backup_path))
      return -1;
   /* Delete backup file (if it exists) */
   if (path_is_valid(backup_path))
      filestream_delete(backup_path);
   /* Refresh menu */
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;
   return 0;
}

/* Do not declare this static - it is also used
 * in menu_cbs_left.c and menu_cbs_right.c */
int action_ok_core_lock(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   const char *core_path      = path;
   bool lock                  = false;
   int ret                    = 0;

   if (string_is_empty(core_path))
      return -1;

   /* Simply toggle current lock status */
   lock = !core_info_get_core_lock(core_path, true);

   if (!core_info_set_core_lock(core_path, lock))
   {
      size_t _len;
      const char *core_name = NULL;
      core_info_t *core_info = NULL;
      char msg[128];

      /* Need to fetch core name for error message */

      /* If core is found, use display name */
      if (
             core_info_find(core_path, &core_info)
          && core_info->display_name)
         core_name = core_info->display_name;
      /* If not, use core file name */
      else
         core_name = path_basename_nocompression(core_path);

      /* Build error message */
      _len = strlcpy(msg,
            msg_hash_to_str(lock
                  ? MSG_CORE_LOCK_FAILED : MSG_CORE_UNLOCK_FAILED),
            sizeof(msg));

      if (!string_is_empty(core_name))
         strlcpy(msg + _len, core_name, sizeof(msg) - _len);

      /* Generate log + notification */
      RARCH_ERR("%s\n", msg);

      runloop_msg_queue_push(msg,
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      ret = -1;
   }

   /* Whenever lock status is changed, menu must be
    * refreshed - do this even in the event of an error,
    * since we don't want to leave the menu in an
    * undefined state */
   menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                    | MENU_ST_FLAG_PREVENT_POPULATE;

   return ret;
}

/* Do not declare this static - it is also used
 * in menu_cbs_left.c and menu_cbs_right.c */
int action_ok_core_set_standalone_exempt(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *core_path = path;
   bool exempt           = false;
   int ret               = 0;

   if (string_is_empty(core_path))
      return -1;

   /* Simply toggle current 'exempt' status */
   exempt = !core_info_get_core_standalone_exempt(core_path);

   if (!core_info_set_core_standalone_exempt(core_path, exempt))
   {
      size_t _len;
      const char *core_name  = NULL;
      core_info_t *core_info = NULL;
      char msg[128];

      /* Need to fetch core name for error message */

      /* If core is found, use display name */
      if (
             core_info_find(core_path, &core_info)
          && core_info->display_name)
         core_name = core_info->display_name;
      /* If not, use core file name */
      else
         core_name = path_basename_nocompression(core_path);

      /* Build error message */
      _len = strlcpy(msg,
            msg_hash_to_str(exempt ?
                  MSG_CORE_SET_STANDALONE_EXEMPT_FAILED :
                  MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED),
            sizeof(msg));

      if (!string_is_empty(core_name))
         strlcpy(msg + _len, core_name, sizeof(msg) - _len);

      /* Generate log + notification */
      RARCH_ERR("%s\n", msg);

      runloop_msg_queue_push(
         msg,
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      ret = -1;
   }

   return ret;
}

static int action_ok_core_delete(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *core_path = label;

   if (string_is_empty(core_path))
      return -1;

   /* Check whether core is locked */
   if (core_info_get_core_lock(core_path, true))
   {
      size_t _len;
      const char *core_name  = NULL;
      core_info_t *core_info = NULL;
      char msg[128];

      /* Need to fetch core name for notification */

      /* If core is found, use display name */
      if (
               core_info_find(core_path, &core_info)
            && core_info->display_name)
         core_name = core_info->display_name;
      /* If not, use core file name */
      else
         core_name = path_basename_nocompression(core_path);

      /* Build notification message */
      _len = strlcpy(msg, msg_hash_to_str(MSG_CORE_DELETE_DISABLED), sizeof(msg));

      if (!string_is_empty(core_name))
         strlcpy(msg + _len, core_name, sizeof(msg) - _len);

      runloop_msg_queue_push(
         msg,
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      /* We do not consider this an 'error' - we are
       * merely telling the user that this operation
       * is not currently supported */
      return 0;
   }

   /* Check if core to be deleted is currently
    * loaded - if so, unload it */
   if (retroarch_ctl(RARCH_CTL_IS_CORE_LOADED, (void*)core_path))
      generic_action_ok_command(CMD_EVENT_UNLOAD_CORE);

   /* Delete core file */
#if defined(ANDROID)
   /* If this is a Play Store build and the
    * core is currently installed via
    * play feature delivery, must delete
    * the core via the play feature delivery
    * interface */
   if (play_feature_delivery_enabled())
   {
      size_t _len;
      const char *core_filename = path_basename_nocompression(core_path);
      char backup_core_path[PATH_MAX_LENGTH];

      if (play_feature_delivery_core_installed(core_filename))
         play_feature_delivery_delete(core_filename);
      else
         filestream_delete(core_path);

      /* When installing cores via play feature
       * delivery, there is a low probability of
       * backup core files being left behind if
       * something interrupts the install process
       * (i.e. a crash or user exit while the
       * install task is running). To prevent the
       * accumulation of mess, additionally check
       * for and remove any such backups when deleting
       * a core */
      _len = strlcpy(backup_core_path, core_path,
            sizeof(backup_core_path));
      strlcpy(backup_core_path + _len, FILE_PATH_BACKUP_EXTENSION,
            sizeof(backup_core_path) - _len);

      if (  !string_is_empty(backup_core_path)
          && path_is_valid(backup_core_path))
         filestream_delete(backup_core_path);
   }
   else
#endif
      filestream_delete(core_path);

   /* Reload core info files */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);

   /* Force reload of contentless cores icons */
   menu_contentless_cores_free();

   /* Return to higher level menu */
   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

static int action_ok_delete_playlist(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t       *playlist = playlist_get_cached();
   struct menu_state *menu_st = menu_state_get_ptr();

   if (!playlist)
      return -1;

   path = playlist_get_conf_path(playlist);

   filestream_delete(path);

   if (menu_st->driver_ctx->environ_cb)
      menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
               NULL, menu_st->userdata);

   return action_cancel_pop_default(NULL, NULL, 0, 0);
}

#ifdef HAVE_NETWORKING
static int action_ok_pl_content_thumbnails(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings = config_get_ptr();

   if (settings)
   {
      playlist_config_t playlist_config;
      const char *path_dir_playlist       = settings->paths.directory_playlist;
      const char *path_dir_thumbnails     = settings->paths.directory_thumbnails;

      playlist_config.capacity            = COLLECTION_SIZE;
      playlist_config.old_format          = settings->bools.playlist_use_old_format;
      playlist_config.compress            = settings->bools.playlist_compression;
      playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;

      if (!string_is_empty(path_dir_playlist))
      {
         char playlist_path[PATH_MAX_LENGTH];
         fill_pathname_join_special(
               playlist_path, path_dir_playlist, label,
               sizeof(playlist_path));
         playlist_config_set_path(&playlist_config, playlist_path);

         task_push_pl_thumbnail_download(path, &playlist_config,
               path_dir_thumbnails);

         return 0;
      }
   }
   return -1;
}

static int action_ok_pl_entry_content_thumbnails(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char system[PATH_MAX_LENGTH];
   struct menu_state *menu_st  = menu_state_get_ptr();
   playlist_t *playlist        = playlist_get_cached();
   menu_handle_t *menu         = menu_st->driver_data;

   system[0] = '\0';

   if (!playlist || !menu)
      return -1;

   menu_driver_get_thumbnail_system(
         menu_st->userdata, system, sizeof(system));

   task_push_pl_entry_thumbnail_download(system,
         playlist, menu->rpl_entry_selection_ptr,
         true, false);

   return 0;
}
#endif

static int action_ok_playlist_reset_cores(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist               = playlist_get_cached();
   playlist_config_t *playlist_config = NULL;

   if (!playlist)
      return -1;

   playlist_config = playlist_get_config(playlist);

   if (!playlist_config || string_is_empty(playlist_config->path))
      return -1;

   task_push_pl_manager_reset_cores(playlist_config);

   return 0;
}

static int action_ok_playlist_clean(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_t *playlist               = playlist_get_cached();
   playlist_config_t *playlist_config = NULL;

   if (!playlist)
      return -1;

   playlist_config = playlist_get_config(playlist);

   if (!playlist_config || string_is_empty(playlist_config->path))
      return -1;

   task_push_pl_manager_clean_playlist(playlist_config);

   return 0;
}

static int action_ok_playlist_refresh(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   playlist_config_t *playlist_config = NULL;
   playlist_t *playlist               = playlist_get_cached();
   settings_t *settings               = config_get_ptr();
   bool scan_record_valid             = false;
   const char *msg_prefix             = NULL;
   const char *msg_subject            = NULL;
   const char *log_text               = NULL;
   char system_name[256];

   system_name[0] = '\0';

   if (!playlist || !settings)
      return -1;

   playlist_config = playlist_get_config(playlist);

   if (!playlist_config || string_is_empty(playlist_config->path))
      return -1;

   /* Configure manual scan using playlist record */
   switch (manual_content_scan_set_menu_from_playlist(playlist,
         settings->paths.path_content_database,
         settings->bools.show_hidden_files))
   {
      case MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_OK:
         scan_record_valid = true;
         break;
      case MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CONTENT_DIR:
         msg_prefix  = msg_hash_to_str(MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR);
         msg_subject = playlist_get_scan_content_dir(playlist);
         log_text    = "[Playlist Refresh]: Invalid content directory: %s\n";
         break;
      case MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_SYSTEM_NAME:
         {
            const char *playlist_file = NULL;

            if ((playlist_file = path_basename(playlist_config->path)))
            {
               strlcpy(system_name, playlist_file, sizeof(system_name));
               path_remove_extension(system_name);
            }

            msg_prefix  = msg_hash_to_str(MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME);
            msg_subject = system_name;
            log_text    = "[Playlist Refresh]: Invalid system name: %s\n";
         }
         break;
      case MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_CORE:
         msg_prefix  = msg_hash_to_str(MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE);
         msg_subject = playlist_get_default_core_name(playlist);
         log_text    = "[Playlist Refresh]: Invalid core name: %s\n";
         break;
      case MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_INVALID_DAT_FILE:
         msg_prefix  = msg_hash_to_str(MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE);
         msg_subject = playlist_get_scan_dat_file_path(playlist);
         log_text    = "[Playlist Refresh]: Invalid arcade dat file: %s\n";
         break;
      case MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_DAT_FILE_TOO_LARGE:
         msg_prefix  = msg_hash_to_str(MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE);
         msg_subject = playlist_get_scan_dat_file_path(playlist);
         log_text    = "[Playlist Refresh]: Arcade dat file too large: %s\n";
         break;
      case MANUAL_CONTENT_SCAN_PLAYLIST_REFRESH_MISSING_CONFIG:
      default:
         msg_prefix  = msg_hash_to_str(MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG);
         msg_subject = path_basename(playlist_config->path);
         log_text    = "[Playlist Refresh]: No scan record found: %s\n";
         break;
   }

   /* Log errors in the event of an invalid
    * scan record */
   if (!scan_record_valid)
   {
      char msg[128];
      if (string_is_empty(msg_subject))
         msg_subject = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
      fill_pathname_join_special(msg, msg_prefix, msg_subject, sizeof(msg));
      RARCH_ERR(log_text, msg_subject);
      runloop_msg_queue_push(msg, 1, 150, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      /* Even though this is a failure condition, we
       * return 0 here to suppress any refreshing of
       * the menu (this can appear ugly, depending
       * on the active menu driver...) */
      return 0;
   }

   /* Perform manual scan
    * > Since we are refreshing the playlist,
    *   additionally ensure that all pertinent
    *   'playlist_config' parameters are synchronised
    *   with the current settings struct */
   playlist_config->capacity            = COLLECTION_SIZE;
   playlist_config->old_format          = settings->bools.playlist_use_old_format;
   playlist_config->compress            = settings->bools.playlist_compression;
   playlist_config->fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   playlist_config_set_base_content_directory(playlist_config,
         settings->bools.playlist_portable_paths ?
               settings->paths.directory_menu_content : NULL);

   task_push_manual_content_scan(playlist_config,
         settings->paths.directory_playlist);
   return 0;
}

#ifdef HAVE_MIST
static int action_ok_core_steam_install(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   steam_core_dlc_list_t *core_dlc_list = NULL;
   steam_core_dlc_t *core_dlc = NULL;

   if (MIST_IS_ERROR(steam_get_core_dlcs(&core_dlc_list, true))) return 0;

   core_dlc = steam_get_core_dlc_by_name(core_dlc_list, label);
   if (core_dlc == NULL) return 0;

   steam_install_core_dlc(core_dlc);

   return 0;
}

static int action_ok_core_steam_uninstall(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   steam_core_dlc_list_t *core_dlc_list = NULL;
   steam_core_dlc_t *core_dlc = NULL;

   if (MIST_IS_ERROR(steam_get_core_dlcs(&core_dlc_list, true))) return 0;

   core_dlc = steam_get_core_dlc_by_name(core_dlc_list, label);
   if (core_dlc == NULL) return 0;

   steam_uninstall_core_dlc(core_dlc);

   return 0;
}
#endif

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
      const char *label)
{
   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      const char     *str = msg_hash_to_str(cbs->enum_idx);

      if (str)
      {
         if (is_rdb_entry(cbs->enum_idx) == 0)
         {
            BIND_ACTION_OK(cbs, action_ok_rdb_entry_submenu);
            return 0;
         }

         if (string_ends_with_size(str, "input_binds_list",
                  strlen(str),
                  STRLEN_CONST("input_binds_list")))
         {
            unsigned i;
            unsigned first_char = atoi(&str[0]);

            for (i = 0; i < MAX_USERS; i++)
            {
               if (first_char != ((i+1)))
                  continue;

               BIND_ACTION_OK(cbs, action_ok_push_user_binds_list);
               return 0;
            }
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
      unsigned i;
      typedef struct temp_ok_list
      {
         enum msg_hash_enums type;
         int (*cb)(const char *path, const char *label, unsigned type,
               size_t idx, size_t entry_idx);
      } temp_ok_list_t;

      temp_ok_list_t ok_list[] = {
         {MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING,          action_ok_start_recording},
         {MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING,          action_ok_start_streaming},
         {MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING,           action_ok_stop_recording},
         {MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING,           action_ok_stop_streaming},
#ifdef HAVE_CHEATS
         {MENU_ENUM_LABEL_CHEAT_START_OR_CONT,                 action_ok_cheat_start_or_cont},
         {MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP,                   action_ok_cheat_add_top},
         {MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS,                 action_ok_cheat_reload_cheats},
         {MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM,                action_ok_cheat_add_bottom},
         {MENU_ENUM_LABEL_CHEAT_DELETE_ALL,                    action_ok_cheat_delete_all},
         {MENU_ENUM_LABEL_CHEAT_ADD_NEW_AFTER,                 action_ok_cheat_add_new_after},
         {MENU_ENUM_LABEL_CHEAT_ADD_NEW_BEFORE,                action_ok_cheat_add_new_before},
         {MENU_ENUM_LABEL_CHEAT_COPY_AFTER,                    action_ok_cheat_copy_after},
         {MENU_ENUM_LABEL_CHEAT_COPY_BEFORE,                   action_ok_cheat_copy_before},
         {MENU_ENUM_LABEL_CHEAT_DELETE,                        action_ok_cheat_delete},
#endif
         {MENU_ENUM_LABEL_RUN_MUSIC,                           action_ok_audio_run},
#ifdef HAVE_AUDIOMIXER
         {MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION,         action_ok_audio_add_to_mixer_and_collection},
         {MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,action_ok_audio_add_to_mixer_and_collection_and_play},
         {MENU_ENUM_LABEL_ADD_TO_MIXER,                        action_ok_audio_add_to_mixer},
         {MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY,               action_ok_audio_add_to_mixer_and_play},
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {MENU_ENUM_LABEL_VIDEO_SHADER_PASS,                   action_ok_shader_pass},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET,                 action_ok_shader_preset},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND,         action_ok_shader_preset_prepend},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND,          action_ok_shader_preset_append},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS,             action_ok_shader_parameters},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS,      action_ok_shader_parameters},
         {MENU_ENUM_LABEL_SHADER_APPLY_CHANGES,                action_ok_shader_apply_changes},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE,          action_ok_shader_preset_remove},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE,            action_ok_shader_preset_save},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS,         action_ok_shader_preset_save_as},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,     action_ok_shader_preset_save_global},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE,       action_ok_shader_preset_save_core},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,     action_ok_shader_preset_save_parent},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME,       action_ok_shader_preset_save_game},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,   action_ok_shader_preset_remove_global},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,     action_ok_shader_preset_remove_core},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,   action_ok_shader_preset_remove_parent},
         {MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,     action_ok_shader_preset_remove_game},
#ifdef HAVE_NETWORKING
         {MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS,                 action_ok_update_shaders_glsl},
         {MENU_ENUM_LABEL_UPDATE_CG_SHADERS,                   action_ok_update_shaders_cg},
         {MENU_ENUM_LABEL_UPDATE_SLANG_SHADERS,                action_ok_update_shaders_slang},
#endif
#endif
#ifdef HAVE_AUDIOMIXER
         {MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS,                action_ok_push_audio_mixer_settings_list},
#endif
#ifdef HAVE_NETWORKING
         {MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT,               action_ok_core_content_list},
         {MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT_DIRS,          action_ok_core_content_dirs_list},
         {MENU_ENUM_LABEL_DOWNLOAD_CORE_SYSTEM_FILES,          action_ok_core_system_files_list},
         {MENU_ENUM_LABEL_CORE_UPDATER_LIST,                   action_ok_core_updater_list},
         {MENU_ENUM_LABEL_UPDATE_INSTALLED_CORES,              action_ok_update_installed_cores},
#if defined(ANDROID)
         {MENU_ENUM_LABEL_SWITCH_INSTALLED_CORES_PFD,          action_ok_switch_installed_cores_pfd},
#endif
         {MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST,             action_ok_thumbnails_updater_list},
         {MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST,          action_ok_pl_thumbnails_updater_list},
         {MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,        action_ok_pl_entry_content_thumbnails},
         {MENU_ENUM_LABEL_UPDATE_LAKKA,                        action_ok_lakka_list},
         {MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS,               action_ok_push_netplay_refresh_rooms},
#ifdef HAVE_NETPLAYDISCOVERY
         {MENU_ENUM_LABEL_NETPLAY_REFRESH_LAN,                 action_ok_push_netplay_refresh_lan},
#endif
#endif
#if defined(HAVE_LIBNX)
         {MENU_ENUM_LABEL_SWITCH_CPU_PROFILE,                  action_ok_push_default},
#endif
         {MENU_ENUM_LABEL_MENU_WALLPAPER,                      action_ok_menu_wallpaper},
         {MENU_ENUM_LABEL_VIDEO_FONT_PATH,                     action_ok_video_font},
         {MENU_ENUM_LABEL_GOTO_FAVORITES,                      action_ok_goto_favorites},
         {MENU_ENUM_LABEL_GOTO_MUSIC,                          action_ok_goto_music},
         {MENU_ENUM_LABEL_GOTO_IMAGES,                         action_ok_goto_images},
         {MENU_ENUM_LABEL_GOTO_VIDEO,                          action_ok_goto_video},
         {MENU_ENUM_LABEL_GOTO_EXPLORE,                        action_ok_goto_explore},
         {MENU_ENUM_LABEL_GOTO_CONTENTLESS_CORES,              action_ok_goto_contentless_cores},
         {MENU_ENUM_LABEL_BROWSE_START,                        action_ok_browse_url_start},
         {MENU_ENUM_LABEL_FILE_BROWSER_CORE,                   action_ok_load_core},
         {MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,action_ok_core_deferred_set},
         {MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION_CURRENT_CORE,action_ok_core_deferred_set},
         {MENU_ENUM_LABEL_START_CORE,                          action_ok_start_core},
         {MENU_ENUM_LABEL_START_NET_RETROPAD,                  action_ok_start_net_retropad_core},
         {MENU_ENUM_LABEL_START_VIDEO_PROCESSOR,               action_ok_start_video_processor_core},
         {MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE,            action_ok_open_archive_detect_core},
         {MENU_ENUM_LABEL_OPEN_ARCHIVE,                        action_ok_open_archive},
         {MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE,            action_ok_load_archive_detect_core},
         {MENU_ENUM_LABEL_LOAD_ARCHIVE,                        action_ok_load_archive},
         {MENU_ENUM_LABEL_CUSTOM_BIND_ALL,                     action_ok_lookup_setting},
         {MENU_ENUM_LABEL_SAVE_STATE,                          action_ok_save_state},
         {MENU_ENUM_LABEL_LOAD_STATE,                          action_ok_load_state},
         {MENU_ENUM_LABEL_UNDO_LOAD_STATE,                     action_ok_undo_load_state},
         {MENU_ENUM_LABEL_UNDO_SAVE_STATE,                     action_ok_undo_save_state},
         {MENU_ENUM_LABEL_RECORD_REPLAY,                       action_ok_record_replay},
         {MENU_ENUM_LABEL_PLAY_REPLAY,                         action_ok_play_replay},
         {MENU_ENUM_LABEL_HALT_REPLAY,                         action_ok_halt_replay},
         {MENU_ENUM_LABEL_RESUME_CONTENT,                      action_ok_resume_content},
         {MENU_ENUM_LABEL_ADD_TO_FAVORITES_PLAYLIST,           action_ok_add_to_favorites_playlist},
         {MENU_ENUM_LABEL_SET_CORE_ASSOCIATION,                action_ok_set_core_association},
         {MENU_ENUM_LABEL_RESET_CORE_ASSOCIATION,              action_ok_reset_core_association},
         {MENU_ENUM_LABEL_ADD_TO_FAVORITES,                    action_ok_add_to_favorites},
         {MENU_ENUM_LABEL_RESTART_CONTENT,                     action_ok_restart_content},
         {MENU_ENUM_LABEL_TAKE_SCREENSHOT,                     action_ok_screenshot},
         {MENU_ENUM_LABEL_RENAME_ENTRY,                        action_ok_rename_entry},
         {MENU_ENUM_LABEL_DELETE_ENTRY,                        action_ok_delete_entry},
         {MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE,             action_ok_disable_kiosk_mode},
         {MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,       action_ok_enable_settings},
         {MENU_ENUM_LABEL_SHOW_WIMP,                           action_ok_show_wimp},
         {MENU_ENUM_LABEL_QUIT_RETROARCH,                      action_ok_quit},
         {MENU_ENUM_LABEL_CLOSE_CONTENT,                       action_ok_close_content},
         {MENU_ENUM_LABEL_SAVE_NEW_CONFIG,                     action_ok_save_new_config},
         {MENU_ENUM_LABEL_HELP,                                action_ok_help},
         {MENU_ENUM_LABEL_HELP_CONTROLS,                       action_ok_help_controls},
         {MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE,                 action_ok_help_what_is_a_core},
         {MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD,         action_ok_help_change_virtual_gamepad},
         {MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING,    action_ok_help_audio_video_troubleshooting},
         {MENU_ENUM_LABEL_HELP_SCANNING_CONTENT,               action_ok_help_scanning_content},
         {MENU_ENUM_LABEL_HELP_LOADING_CONTENT,                action_ok_help_load_content},
#ifdef HAVE_CHEATS
         {MENU_ENUM_LABEL_CHEAT_FILE_LOAD,                     action_ok_cheat_file},
         {MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND,              action_ok_cheat_file_append},
#ifdef HAVE_NETWORKING
         {MENU_ENUM_LABEL_UPDATE_CHEATS,                       action_ok_update_cheats},
#endif
         {MENU_ENUM_LABEL_CHEAT_ADD_MATCHES,                   cheat_manager_add_matches},
#endif
         {MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN,                    action_ok_audio_dsp_plugin},
         {MENU_ENUM_LABEL_VIDEO_FILTER,                        action_ok_video_filter},
         {MENU_ENUM_LABEL_OVERLAY_PRESET,                      action_ok_overlay_preset},
         {MENU_ENUM_LABEL_OSK_OVERLAY_PRESET,                  action_ok_osk_overlay_preset},
         {MENU_ENUM_LABEL_RECORD_CONFIG,                       action_ok_record_configfile},
         {MENU_ENUM_LABEL_STREAM_CONFIG,                       action_ok_stream_configfile},
#ifdef HAVE_RGUI
         {MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET,              action_ok_rgui_menu_theme_preset},
#endif
         {MENU_ENUM_LABEL_ACCOUNTS_LIST,                       action_ok_push_accounts_list},
         {MENU_ENUM_LABEL_ACCESSIBILITY_SETTINGS,              action_ok_push_accessibility_settings_list},
         {MENU_ENUM_LABEL_AI_SERVICE_SETTINGS,                 action_ok_push_ai_service_settings_list},
         {MENU_ENUM_LABEL_INPUT_SETTINGS,                      action_ok_push_input_settings_list},
         {MENU_ENUM_LABEL_INPUT_MENU_SETTINGS,                 action_ok_push_input_menu_settings_list},
         {MENU_ENUM_LABEL_INPUT_TURBO_FIRE_SETTINGS,           action_ok_push_input_turbo_fire_settings_list},
         {MENU_ENUM_LABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,      action_ok_push_input_haptic_feedback_settings_list},
         {MENU_ENUM_LABEL_DRIVER_SETTINGS,                     action_ok_push_driver_settings_list},
         {MENU_ENUM_LABEL_VIDEO_SETTINGS,                      action_ok_push_video_settings_list},
         {MENU_ENUM_LABEL_VIDEO_SYNCHRONIZATION_SETTINGS,      action_ok_push_video_synchronization_settings_list},
         {MENU_ENUM_LABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,      action_ok_push_video_fullscreen_mode_settings_list},
         {MENU_ENUM_LABEL_VIDEO_WINDOWED_MODE_SETTINGS,        action_ok_push_video_windowed_mode_settings_list},
         {MENU_ENUM_LABEL_VIDEO_SCALING_SETTINGS,              action_ok_push_video_scaling_settings_list},
         {MENU_ENUM_LABEL_VIDEO_HDR_SETTINGS,                  action_ok_push_video_hdr_settings_list},
         {MENU_ENUM_LABEL_VIDEO_OUTPUT_SETTINGS,               action_ok_push_video_output_settings_list},
         {MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS,              action_ok_push_crt_switchres_settings_list},
         {MENU_ENUM_LABEL_AUDIO_SETTINGS,                      action_ok_push_audio_settings_list},
         {MENU_ENUM_LABEL_AUDIO_SYNCHRONIZATION_SETTINGS,      action_ok_push_audio_synchronization_settings_list},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DIR,             action_ok_push_manual_content_scan_dir_select},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,     action_ok_manual_content_scan_system_name},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME,       action_ok_manual_content_scan_core_name},
         {MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES,             action_ok_video_shader_num_passes_dropdown_box_list},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE,        action_ok_manual_content_scan_dat_file},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_START,           action_ok_manual_content_scan_start},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE, action_ok_playlist_label_display_mode},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE,action_ok_playlist_right_thumbnail_mode},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE,action_ok_playlist_left_thumbnail_mode},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE,          action_ok_playlist_sort_mode},
#ifdef HAVE_NETWORKING
         {MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES,              action_ok_update_core_info_files},
         {MENU_ENUM_LABEL_UPDATE_OVERLAYS,                     action_ok_update_overlays},
         {MENU_ENUM_LABEL_UPDATE_DATABASES,                    action_ok_update_databases},
         {MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES,          action_ok_update_autoconfig_profiles},
         {MENU_ENUM_LABEL_UPDATE_ASSETS,                       action_ok_update_assets},
         {MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST,                 action_ok_netplay_enable_host},
         {MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT,               action_ok_netplay_enable_client},
         {MENU_ENUM_LABEL_NETPLAY_DISCONNECT,                  action_ok_netplay_disconnect},
#endif
         {MENU_ENUM_LABEL_CORE_DELETE,                         action_ok_core_delete},
         {MENU_ENUM_LABEL_CORE_CREATE_BACKUP,                  action_ok_core_create_backup},
         {MENU_ENUM_LABEL_DELETE_PLAYLIST,                     action_ok_delete_playlist},
         {MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_MENU,              action_ok_push_default},
         {MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE,                   action_ok_cheevos_toggle_hardcore_mode},
         {MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE_CANCEL,            action_ok_close_submenu},
         {MENU_ENUM_LABEL_ACHIEVEMENT_RESUME,                  action_ok_cheevos_toggle_hardcore_mode},
         {MENU_ENUM_LABEL_ACHIEVEMENT_RESUME_CANCEL,           action_ok_close_submenu },
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_LIST,            action_ok_push_manual_content_scan_list},
         {MENU_ENUM_LABEL_AUDIO_OUTPUT_SETTINGS,               action_ok_push_audio_output_settings_list},
#ifdef HAVE_MICROPHONE
         {MENU_ENUM_LABEL_MICROPHONE_SETTINGS,                 action_ok_push_microphone_settings_list},
#endif
         {MENU_ENUM_LABEL_LATENCY_SETTINGS,                    action_ok_push_latency_settings_list},
         {MENU_ENUM_LABEL_CORE_SETTINGS,                       action_ok_push_core_settings_list},
         {MENU_ENUM_LABEL_CORE_INFORMATION,                    action_ok_push_core_information_list},
         {MENU_ENUM_LABEL_CORE_MANAGER_ENTRY,                  action_ok_push_core_information_list},
#ifdef HAVE_MIST
         {MENU_ENUM_LABEL_CORE_MANAGER_STEAM_ENTRY,            action_ok_push_core_information_steam_list},
#endif
         {MENU_ENUM_LABEL_CORE_RESTORE_BACKUP_LIST,            action_ok_push_core_restore_backup_list},
         {MENU_ENUM_LABEL_CORE_DELETE_BACKUP_LIST,             action_ok_push_core_delete_backup_list},
         {MENU_ENUM_LABEL_CONFIGURATION_SETTINGS,              action_ok_push_configuration_settings_list},
         {MENU_ENUM_LABEL_PLAYLIST_SETTINGS,                   action_ok_push_playlist_settings_list},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST,               action_ok_push_playlist_manager_list},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_SETTINGS,           action_ok_push_playlist_manager_settings},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES,        action_ok_playlist_reset_cores},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,     action_ok_playlist_clean},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,   action_ok_playlist_refresh},
         {MENU_ENUM_LABEL_RECORDING_SETTINGS,                  action_ok_push_recording_settings_list},
         {MENU_ENUM_LABEL_INPUT_RETROPAD_BINDS,                action_ok_push_input_retropad_binds_list},
         {MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS,                  action_ok_push_input_hotkey_binds_list},
         {MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,         action_ok_push_accounts_cheevos_list},
         {MENU_ENUM_LABEL_ACCOUNTS_YOUTUBE,                    action_ok_push_accounts_youtube_list},
         {MENU_ENUM_LABEL_ACCOUNTS_TWITCH,                     action_ok_push_accounts_twitch_list},
         {MENU_ENUM_LABEL_ACCOUNTS_FACEBOOK,                   action_ok_push_accounts_facebook_list},
         {MENU_ENUM_LABEL_DUMP_DISC,                           action_ok_push_dump_disc_list},
#ifdef HAVE_LAKKA
         {MENU_ENUM_LABEL_EJECT_DISC,                          action_ok_push_eject_disc},
#endif
         {MENU_ENUM_LABEL_LOAD_DISC,                           action_ok_push_load_disc_list},
         {MENU_ENUM_LABEL_SHADER_OPTIONS,                      action_ok_push_default},
         {MENU_ENUM_LABEL_SAVESTATE_LIST,                      action_ok_push_savestate_list},
         {MENU_ENUM_LABEL_CORE_OPTIONS,                        action_ok_push_core_options_list},
         {MENU_ENUM_LABEL_CORE_OPTION_OVERRIDE_LIST,           action_ok_push_core_option_override_list},
         {MENU_ENUM_LABEL_REMAP_FILE_MANAGER_LIST,             action_ok_push_remap_file_manager_list},
         {MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS,                  action_ok_push_default},
         {MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS,        action_ok_push_default},
         {MENU_ENUM_LABEL_DISC_INFORMATION,                    action_ok_push_default},
         {MENU_ENUM_LABEL_SYSTEM_INFORMATION,                  action_ok_push_default},
         {MENU_ENUM_LABEL_NETWORK_INFORMATION,                 action_ok_push_default},
         {MENU_ENUM_LABEL_ACHIEVEMENT_LIST,                    action_ok_push_default},
         {MENU_ENUM_LABEL_ACHIEVEMENT_LIST_HARDCORE,           action_ok_push_default},
         {MENU_ENUM_LABEL_DISK_OPTIONS,                        action_ok_push_default},
         {MENU_ENUM_LABEL_SETTINGS,                            action_ok_push_default},
         {MENU_ENUM_LABEL_FRONTEND_COUNTERS,                   action_ok_push_default},
         {MENU_ENUM_LABEL_CORE_COUNTERS,                       action_ok_push_default},
         {MENU_ENUM_LABEL_MANAGEMENT,                          action_ok_push_default},
         {MENU_ENUM_LABEL_ONLINE_UPDATER,                      action_ok_push_default},
         {MENU_ENUM_LABEL_NETPLAY,                             action_ok_push_default},
         {MENU_ENUM_LABEL_LOAD_CONTENT_LIST,                   action_ok_push_default},
         {MENU_ENUM_LABEL_ADD_CONTENT_LIST,                    action_ok_push_default},
         {MENU_ENUM_LABEL_CONFIGURATIONS_LIST,                 action_ok_push_default},
         {MENU_ENUM_LABEL_HELP_LIST,                           action_ok_push_default},
         {MENU_ENUM_LABEL_INFORMATION_LIST,                    action_ok_push_default},
         {MENU_ENUM_LABEL_INFORMATION,                         action_ok_push_default},
         {MENU_ENUM_LABEL_CONTENT_SETTINGS,                    action_ok_push_default},
         {MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL,                action_ok_push_filebrowser_list_file_select},
         {MENU_ENUM_LABEL_SCAN_DIRECTORY,                      action_ok_scan_directory_list},
         {MENU_ENUM_LABEL_SCAN_FILE,                           action_ok_push_scan_file},
         {MENU_ENUM_LABEL_FAVORITES,                           action_ok_push_content_list},
         {MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR,      action_ok_push_random_dir},
         {MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,    action_ok_push_downloads_dir},
         {MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,                 action_ok_file_load_detect_core},
         {MENU_ENUM_LABEL_DETECT_CORE_LIST_OK_CURRENT_CORE,    action_ok_file_load_current_core},
         {MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY,                action_ok_push_generic_list},
         {MENU_ENUM_LABEL_DATABASE_MANAGER_LIST,               action_ok_push_generic_list},
#ifdef HAVE_CHEATS
         {MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES,                 action_ok_cheat_apply_changes},
         {MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS,                  action_ok_cheat_file_save_as},
#endif
         {MENU_ENUM_LABEL_REMAP_FILE_LOAD,                     action_ok_remap_file},
         {MENU_ENUM_LABEL_REMAP_FILE_SAVE_AS,                  action_ok_remap_file_save_as},
         {MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE,                action_ok_remap_file_save_core},
         {MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR,         action_ok_remap_file_save_content_dir},
         {MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME,                action_ok_remap_file_save_game},
         {MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CORE,              action_ok_remap_file_remove_core},
         {MENU_ENUM_LABEL_REMAP_FILE_REMOVE_CONTENT_DIR,       action_ok_remap_file_remove_content_dir},
         {MENU_ENUM_LABEL_REMAP_FILE_REMOVE_GAME,              action_ok_remap_file_remove_game},
         {MENU_ENUM_LABEL_REMAP_FILE_RESET,                    action_ok_remap_file_reset},
         {MENU_ENUM_LABEL_REMAP_FILE_FLUSH,                    action_ok_remap_file_flush},
         {MENU_ENUM_LABEL_OVERRIDE_FILE_LOAD,                  action_ok_override_file},
         {MENU_ENUM_LABEL_OVERRIDE_FILE_SAVE_AS,               action_ok_override_file_save_as},
         {MENU_ENUM_LABEL_OVERRIDE_UNLOAD,                     action_ok_override_unload},
         {MENU_ENUM_LABEL_PLAYLISTS_TAB,                       action_ok_content_collection_list},
         {MENU_ENUM_LABEL_BROWSE_URL_LIST,                     action_ok_browse_url_list},
         {MENU_ENUM_LABEL_CORE_LIST,                           action_ok_core_list},
         {MENU_ENUM_LABEL_SIDELOAD_CORE_LIST,                  action_ok_sideload_core_list},
         {MENU_ENUM_LABEL_DISK_IMAGE_APPEND,                   action_ok_disk_image_append_list},
         {MENU_ENUM_LABEL_SUBSYSTEM_ADD,                       action_ok_subsystem_add_list},
         {MENU_ENUM_LABEL_SUBSYSTEM_LOAD,                      action_ok_subsystem_add_load},
         {MENU_ENUM_LABEL_CONFIGURATIONS,                      action_ok_configurations_list},
         {MENU_ENUM_LABEL_SAVING_SETTINGS,                     action_ok_saving_list},
         {MENU_ENUM_LABEL_CLOUD_SYNC_SETTINGS,                 action_ok_cloud_sync_list},
         {MENU_ENUM_LABEL_LOGGING_SETTINGS,                    action_ok_logging_list},
         {MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS,             action_ok_frame_throttle_list},
         {MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS,         action_ok_frame_time_counter_list},
         {MENU_ENUM_LABEL_REWIND_SETTINGS,                     action_ok_rewind_list},
         {MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS,           action_ok_onscreen_display_list},
         {MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,     action_ok_onscreen_notifications_list},
         {MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,action_ok_onscreen_notifications_views_list},
         {MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS,           action_ok_onscreen_overlay_list},
         {MENU_ENUM_LABEL_OSK_OVERLAY_SETTINGS,                action_ok_osk_overlay_list},
         {MENU_ENUM_LABEL_MENU_SETTINGS,                       action_ok_menu_list},
#ifdef _3DS
         {MENU_ENUM_LABEL_MENU_BOTTOM_SETTINGS,                action_ok_menu_bottom_list},
#endif
         {MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS,                 action_ok_menu_views_list},
         {MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS,         action_ok_quick_menu_override_options},
         {MENU_ENUM_LABEL_SETTINGS_VIEWS_SETTINGS,             action_ok_settings_views_list},
         {MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS,           action_ok_quick_menu_views_list},
         {MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS,             action_ok_user_interface_list},
         {MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS,           action_ok_power_management_list},
         {MENU_ENUM_LABEL_CPU_PERFPOWER,                       action_ok_cpu_perfpower_list},
         {MENU_ENUM_LABEL_CPU_POLICY_ENTRY,                    action_ok_cpu_policy_entry},
         {MENU_ENUM_LABEL_MENU_SOUNDS,                         action_ok_menu_sounds_list},
         {MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS,          action_ok_menu_file_browser_list},
         {MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,   action_ok_open_uwp_permission_settings},
         {MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER,            action_ok_open_picker},
         {MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS,         action_ok_retro_achievements_list},
         {MENU_ENUM_LABEL_CHEEVOS_APPEARANCE_SETTINGS,         action_ok_cheevos_appearance_list},
         {MENU_ENUM_LABEL_CHEEVOS_VISIBILITY_SETTINGS,         action_ok_cheevos_visibility_list},
         {MENU_ENUM_LABEL_UPDATER_SETTINGS,                    action_ok_updater_list},
#ifdef HAVE_BLUETOOTH
         {MENU_ENUM_LABEL_BLUETOOTH_SETTINGS,                  action_ok_bluetooth_list},
#endif
#ifdef HAVE_NETWORKING
#ifdef HAVE_WIFI
         {MENU_ENUM_LABEL_WIFI_SETTINGS,                       action_ok_wifi_list},
         {MENU_ENUM_LABEL_WIFI_NETWORK_SCAN,                   action_ok_wifi_networks_list},
         {MENU_ENUM_LABEL_WIFI_DISCONNECT,                     action_ok_wifi_disconnect},
#endif
         {MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM,                action_ok_netplay_connect_room},
#endif
         {MENU_ENUM_LABEL_NETWORK_HOSTING_SETTINGS,            action_ok_network_hosting_list},
         {MENU_ENUM_LABEL_NETPLAY_KICK,                        action_ok_netplay_kick_list},
         {MENU_ENUM_LABEL_NETPLAY_BAN,                         action_ok_netplay_ban_list},
         {MENU_ENUM_LABEL_NETPLAY_LOBBY_FILTERS,               action_ok_netplay_lobby_filters_list},
         {MENU_ENUM_LABEL_SUBSYSTEM_SETTINGS,                  action_ok_subsystem_list},
         {MENU_ENUM_LABEL_NETWORK_SETTINGS,                    action_ok_network_list},
         {MENU_ENUM_LABEL_LAKKA_SERVICES,                      action_ok_lakka_services},
#ifdef HAVE_LAKKA_SWITCH
         {MENU_ENUM_LABEL_LAKKA_SWITCH_OPTIONS,                action_ok_lakka_switch_options},
#endif
         {MENU_ENUM_LABEL_NETPLAY_SETTINGS,                    action_ok_netplay_sublist},
         {MENU_ENUM_LABEL_USER_SETTINGS,                       action_ok_user_list},
         {MENU_ENUM_LABEL_DIRECTORY_SETTINGS,                  action_ok_directory_list},
         {MENU_ENUM_LABEL_PRIVACY_SETTINGS,                    action_ok_privacy_list},
         {MENU_ENUM_LABEL_MIDI_SETTINGS,                       action_ok_midi_list},
         {MENU_ENUM_LABEL_SCREEN_RESOLUTION,                   action_ok_video_resolution},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE,       action_ok_playlist_default_core},
         {MENU_ENUM_LABEL_CORE_MANAGER_LIST,                   action_ok_push_core_manager_list},
#ifdef HAVE_MIST
         {MENU_ENUM_LABEL_STEAM_SETTINGS,                      action_ok_steam_settings_list},
         {MENU_ENUM_LABEL_CORE_MANAGER_STEAM_LIST,             action_ok_push_core_manager_steam_list},
         {MENU_ENUM_LABEL_CORE_STEAM_INSTALL,                  action_ok_core_steam_install},
         {MENU_ENUM_LABEL_CORE_STEAM_UNINSTALL,                action_ok_core_steam_uninstall},
#endif
         {MENU_ENUM_LABEL_EXPLORE_TAB,                         action_ok_push_default},
         {MENU_ENUM_LABEL_CONTENTLESS_CORES_TAB,               action_ok_push_default},
      };

      for (i = 0; i < ARRAY_SIZE(ok_list); i++)
      {
         if (cbs->enum_idx == ok_list[i].type)
         {
            BIND_ACTION_OK(cbs, ok_list[i].cb);
            return 0;
         }
      }
   }
   else
   {
      unsigned i;
      typedef struct temp_ok_list
      {
         enum msg_hash_enums type;
         int (*cb)(const char *path, const char *label, unsigned type,
               size_t idx, size_t entry_idx);
      } temp_ok_list_t;

      temp_ok_list_t ok_list[] = {
         {MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE,            action_ok_open_archive_detect_core},
         {MENU_ENUM_LABEL_OPEN_ARCHIVE,                        action_ok_open_archive},
         {MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE,            action_ok_load_archive_detect_core},
         {MENU_ENUM_LABEL_LOAD_ARCHIVE,                        action_ok_load_archive},
         {MENU_ENUM_LABEL_CHEAT_FILE_LOAD,                     action_ok_cheat_file},
         {MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND,              action_ok_cheat_file_append},
         {MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN,                    action_ok_audio_dsp_plugin},
         {MENU_ENUM_LABEL_VIDEO_FILTER,                        action_ok_video_filter},
         {MENU_ENUM_LABEL_OVERLAY_PRESET,                      action_ok_overlay_preset},
         {MENU_ENUM_LABEL_OSK_OVERLAY_PRESET,                  action_ok_osk_overlay_preset},
         {MENU_ENUM_LABEL_REMAP_FILE_LOAD,                     action_ok_remap_file},
         {MENU_ENUM_LABEL_OVERRIDE_FILE_LOAD,                  action_ok_override_file},
         {MENU_ENUM_LABEL_RECORD_CONFIG,                       action_ok_record_configfile},
         {MENU_ENUM_LABEL_STREAM_CONFIG,                       action_ok_stream_configfile},
         {MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET,              action_ok_rgui_menu_theme_preset},
         {MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,         action_ok_push_accounts_cheevos_list},
         {MENU_ENUM_LABEL_FAVORITES,                           action_ok_push_content_list},
         {MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST,    action_ok_push_downloads_dir},
         {MENU_ENUM_LABEL_DETECT_CORE_LIST_OK,                 action_ok_file_load_detect_core},
#ifdef HAVE_CHEATS
         {MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES,                 action_ok_cheat_apply_changes},
         {MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS,                  action_ok_cheat_file_save_as},
#endif
         {MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE,                action_ok_remap_file_save_core},
         {MENU_ENUM_LABEL_REMAP_FILE_SAVE_CONTENT_DIR,         action_ok_remap_file_save_content_dir},
         {MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME,                action_ok_remap_file_save_game},
         {MENU_ENUM_LABEL_DISK_IMAGE_APPEND,                   action_ok_disk_image_append_list},
         {MENU_ENUM_LABEL_SUBSYSTEM_ADD,                       action_ok_subsystem_add_list},
         {MENU_ENUM_LABEL_SCREEN_RESOLUTION,                   action_ok_video_resolution},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE,       action_ok_playlist_default_core},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE, action_ok_playlist_label_display_mode},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE,action_ok_playlist_right_thumbnail_mode},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE,action_ok_playlist_left_thumbnail_mode},
         {MENU_ENUM_LABEL_PLAYLIST_MANAGER_SORT_MODE,          action_ok_playlist_sort_mode},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,     action_ok_manual_content_scan_system_name},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_CORE_NAME,       action_ok_manual_content_scan_core_name},
         {MENU_ENUM_LABEL_MANUAL_CONTENT_SCAN_DAT_FILE,        action_ok_manual_content_scan_dat_file},
      };

      for (i = 0; i < ARRAY_SIZE(ok_list); i++)
      {
         if (string_is_equal(label, msg_hash_to_str(ok_list[i].type)))
         {
            BIND_ACTION_OK(cbs, ok_list[i].cb);
            return 0;
         }
      }
   }

   return -1;
}

static int menu_cbs_init_bind_ok_compare_type(menu_file_list_cbs_t *cbs,
      const char *label, const char *menu_label, unsigned type)
{
   if (type == MENU_SET_CDROM_LIST)
   {
      BIND_ACTION_OK(cbs, action_ok_dump_cdrom);
   }
#ifdef HAVE_LAKKA
   else if (type == MENU_SET_EJECT_DISC)
   {
      BIND_ACTION_OK(cbs, action_ok_eject_disc);
   }
#endif
   else if (type == MENU_SET_CDROM_INFO)
   {
      BIND_ACTION_OK(cbs, action_ok_cdrom_info_list);
   }
   else if (type == MENU_SET_LOAD_CDROM_LIST)
   {
      BIND_ACTION_OK(cbs, action_ok_load_cdrom);
   }
   else if (type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD ||
         type == MENU_SETTINGS_CUSTOM_BIND)
   {
      BIND_ACTION_OK(cbs, action_ok_lookup_setting);
   }
#ifdef HAVE_AUDIOMIXER
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_play);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_play_looped);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_play_sequential);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_remove);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_action_stop);
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_END)
   {
      BIND_ACTION_OK(cbs, action_ok_mixer_stream_actions);
   }
#endif
   else if (type >= MENU_SETTINGS_REMAPPING_PORT_BEGIN
         && type <= MENU_SETTINGS_REMAPPING_PORT_END)
   {
      BIND_ACTION_OK(cbs, action_ok_remappings_port_list);
   }
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_OK(cbs, action_ok_shader_parameter_dropdown_box_list);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_OK(cbs, action_ok_shader_preset_parameter_dropdown_box_list);
   }
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_OK(cbs, action_ok_cheat);
   }
   else if (   (type >= MENU_SETTINGS_CORE_OPTION_START)
            && (type < MENU_SETTINGS_CHEEVOS_START))
   {
      BIND_ACTION_OK(cbs, action_ok_core_option_dropdown_list);
   }
   else if (   (type >= MENU_SETTINGS_INPUT_DESC_BEGIN)
            && (type <= MENU_SETTINGS_INPUT_DESC_END))
   {
      BIND_ACTION_OK(cbs, action_ok_input_description_dropdown_box_list);
   }
   else if (   (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN)
            && (type <= MENU_SETTINGS_INPUT_DESC_KBD_END))
   {
      BIND_ACTION_OK(cbs, action_ok_input_description_kbd_dropdown_box_list);
   }
   else
   {
      switch (type)
      {
         case MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_setting_core_options_item);
            break;
         case MENU_SETTING_DROPDOWN_SETTING_CORE_OPTIONS_ITEM_SPECIAL:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_setting_core_options_item_special);
            break;
         case MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_INT_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM:
         case MENU_SETTING_DROPDOWN_SETTING_STRING_OPTIONS_ITEM_SPECIAL:
         case MENU_SETTING_DROPDOWN_SETTING_INT_ITEM_SPECIAL:
         case MENU_SETTING_DROPDOWN_SETTING_FLOAT_ITEM_SPECIAL:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_setting);
            break;
         case MENU_SETTING_DROPDOWN_SETTING_UINT_ITEM_SPECIAL:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_setting_uint_item_special);
            break;
         case MENU_SETTING_DROPDOWN_ITEM:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PARAM:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_video_shader_param);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_PRESET_PARAM:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_video_shader_preset_param);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_RESOLUTION:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_resolution);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_VIDEO_SHADER_NUM_PASS:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_video_shader_num_pass);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_DEFAULT_CORE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_playlist_default_core);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LABEL_DISPLAY_MODE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_playlist_label_display_mode);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_RIGHT_THUMBNAIL_MODE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_playlist_right_thumbnail_mode);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_LEFT_THUMBNAIL_MODE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_playlist_left_thumbnail_mode);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_PLAYLIST_SORT_MODE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_playlist_sort_mode);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_SYSTEM_NAME:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_manual_content_scan_system_name);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_MANUAL_CONTENT_SCAN_CORE_NAME:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_manual_content_scan_core_name);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_DISK_INDEX:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_disk_index);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_INPUT_DEVICE_TYPE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_input_device_type);
            break;
#ifdef ANDROID
          case MENU_SETTING_DROPDOWN_ITEM_INPUT_SELECT_PHYSICAL_KEYBOARD:
              BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_input_select_physical_keyboard);
              break;
#endif
         case MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_input_description);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_INPUT_DESCRIPTION_KBD:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_input_description_kbd);
            break;
         case MENU_SETTING_DROPDOWN_ITEM_AUDIO_DEVICE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_audio_device);
            break;
#ifdef HAVE_MICROPHONE
         case MENU_SETTING_DROPDOWN_ITEM_MICROPHONE_DEVICE:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_microphone_device);
            break;
#endif
#ifdef HAVE_NETWORKING
         case MENU_SETTING_DROPDOWN_ITEM_NETPLAY_MITM_SERVER:
            BIND_ACTION_OK(cbs, action_ok_push_dropdown_item_netplay_mitm_server);
            break;
#endif
         case MENU_SETTING_ACTION_CORE_DISK_OPTIONS:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case FILE_TYPE_PLAYLIST_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_playlist_entry_collection);
            break;
#if defined(HAVE_LIBNX)
         case MENU_SET_SWITCH_CPU_PROFILE:
            BIND_ACTION_OK(cbs, action_ok_set_switch_cpu_profile);
            break;
#endif
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
            if (string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND)))
            {
               BIND_ACTION_OK(cbs, action_ok_cheat_file_load_append);
            }
            else
            {
               BIND_ACTION_OK(cbs, action_ok_cheat_file_load);
            }
            break;
         case FILE_TYPE_RECORD_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_record_configfile_load);
            break;
         case FILE_TYPE_STREAM_CONFIG:
            BIND_ACTION_OK(cbs, action_ok_stream_configfile_load);
            break;
         case FILE_TYPE_RGUI_THEME_PRESET:
            BIND_ACTION_OK(cbs, action_ok_rgui_menu_theme_preset_load);
            break;
         case FILE_TYPE_REMAP:
            BIND_ACTION_OK(cbs, action_ok_remap_file_load);
            break;
         case FILE_TYPE_SHADER_PRESET:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_shader_preset_load);
#endif
            break;
         case FILE_TYPE_SHADER:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_shader_pass_load);
#endif
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
         case FILE_TYPE_MANUAL_SCAN_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_path_manual_scan_directory);
            break;
         case FILE_TYPE_MANUAL_SCAN_DAT:
            BIND_ACTION_OK(cbs, action_ok_set_manual_content_scan_dat_file);
            break;
         case FILE_TYPE_CONFIG:
            if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_OVERRIDE_FILE_LOAD)))
            {
               BIND_ACTION_OK(cbs, action_ok_override_file_load);
               break;
            }
            BIND_ACTION_OK(cbs, action_ok_config_load);
            break;
         case FILE_TYPE_PARENT_DIRECTORY:
            BIND_ACTION_OK(cbs, action_ok_parent_directory_push);
            break;
         case FILE_TYPE_DIRECTORY:
            if (cbs->enum_idx != MSG_UNKNOWN
                  || string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_DISK_IMAGE_APPEND))
                  || string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_SUBSYSTEM_ADD))
                  || string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FONT_PATH))
                  || string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_XMB_FONT))
                  || string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN))
                  || string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FILTER)))
               BIND_ACTION_OK(cbs, action_ok_directory_push);
            else
               BIND_ACTION_OK(cbs, action_ok_push_random_dir);
            break;
         case FILE_TYPE_CARCHIVE:
            if (filebrowser_get_type() == FILEBROWSER_SCAN_FILE)
            {
#ifdef HAVE_LIBRETRODB
               BIND_ACTION_OK(cbs, action_ok_scan_file);
#endif
            }
            else
            {
               if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES)))
               {
                  BIND_ACTION_OK(cbs, action_ok_compressed_archive_push_detect_core);
               }
               else
               {
                  BIND_ACTION_OK(cbs, action_ok_compressed_archive_push);
               }
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
               if (string_is_equal(menu_label,
                        msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_LIST)))
               {
                  BIND_ACTION_OK(cbs, action_ok_load_core_deferred);
               }
               else if (string_is_equal(menu_label,
                        msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET)))
               {
                  BIND_ACTION_OK(cbs, action_ok_core_deferred_set);
               }
               else if (string_is_equal(menu_label,
                        msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST)))
               {
                  BIND_ACTION_OK(cbs, action_ok_load_core);
               }
            }
            break;
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_core_content_download);
#endif
            break;
         case FILE_TYPE_DOWNLOAD_CORE_SYSTEM_FILES:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_core_system_files_download);
#endif
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_core_content_thumbnails);
#endif
            break;
         case FILE_TYPE_DOWNLOAD_PL_THUMBNAIL_CONTENT:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_pl_content_thumbnails);
#endif
            break;
         case FILE_TYPE_DOWNLOAD_CORE:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_core_updater_download);
#endif
            break;
         case FILE_TYPE_SIDELOAD_CORE:
            BIND_ACTION_OK(cbs, action_ok_sideload_core);
            break;
         case FILE_TYPE_DOWNLOAD_URL:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_download_url);
#endif
            break;
         case FILE_TYPE_DOWNLOAD_THUMBNAIL:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_thumbnails_updater_download);
#endif
            break;
         case FILE_TYPE_DOWNLOAD_LAKKA:
#if defined(HAVE_NETWORKING) && defined(HAVE_LAKKA)
            BIND_ACTION_OK(cbs, action_ok_lakka_download);
#endif
            break;
         case FILE_TYPE_DOWNLOAD_CORE_INFO:
            break;
         case FILE_TYPE_RDB:
            if (string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST)))
            {
               BIND_ACTION_OK(cbs, action_ok_deferred_list_stub);
            }
            else if (string_is_equal(menu_label,
                     msg_hash_to_str(MENU_ENUM_LABEL_DATABASE_MANAGER_LIST)))
            {
               BIND_ACTION_OK(cbs, action_ok_database_manager_list);
            }
            /* TODO/FIXME - refactor this */
            else if (string_is_equal(menu_label, "Horizontal Menu"))
            {
               BIND_ACTION_OK(cbs, action_ok_database_manager_list);
            }
            break;
         case FILE_TYPE_RDB_ENTRY:
            BIND_ACTION_OK(cbs, action_ok_rdb_entry);
            break;
         case MENU_BLUETOOTH:
#ifdef HAVE_BLUETOOTH
            BIND_ACTION_OK(cbs, action_ok_bluetooth);
#endif
            break;
         case MENU_WIFI:
#ifdef HAVE_NETWORKING
#ifdef HAVE_WIFI
            BIND_ACTION_OK(cbs, action_ok_wifi);
#endif
#endif
            break;
         case MENU_NETPLAY_KICK:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_push_netplay_kick);
#endif
            break;
         case MENU_NETPLAY_BAN:
#ifdef HAVE_NETWORKING
            BIND_ACTION_OK(cbs, action_ok_push_netplay_ban);
#endif
            break;
         case FILE_TYPE_VIDEOFILTER:
            BIND_ACTION_OK(cbs, action_ok_set_path_videofilter);
            break;
         case FILE_TYPE_FONT:
            BIND_ACTION_OK(cbs, action_ok_set_path);
            break;
         case FILE_TYPE_VIDEO_FONT:
            BIND_ACTION_OK(cbs, action_ok_set_path_video_font);
            break;
         case FILE_TYPE_OVERLAY:
            BIND_ACTION_OK(cbs, action_ok_set_path_overlay);
            break;
         case FILE_TYPE_OSK_OVERLAY:
            BIND_ACTION_OK(cbs, action_ok_set_path_osk_overlay);
            break;
         case FILE_TYPE_AUDIOFILTER:
            BIND_ACTION_OK(cbs, action_ok_set_path_audiofilter);
            break;
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_PLAIN:
            if (filebrowser_get_type() == FILEBROWSER_SCAN_FILE)
            {
#ifdef HAVE_LIBRETRODB
               BIND_ACTION_OK(cbs, action_ok_scan_file);
#endif
            }
            else if (cbs->enum_idx != MSG_UNKNOWN)
            {
               switch (cbs->enum_idx)
               {
                  case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
                  case MENU_ENUM_LABEL_FAVORITES:
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
                  case MENU_ENUM_LABEL_SUBSYSTEM_ADD:
                     BIND_ACTION_OK(cbs, action_ok_subsystem_add);
                     break;
                  default:
                     BIND_ACTION_OK(cbs, action_ok_file_load);
                     break;
               }
            }
            else
            {
               if (
                     string_is_equal(menu_label, "deferred_archive_open_detect_core") ||
                     string_is_equal(menu_label, "downloaded_file_detect_core_list") ||
                     string_is_equal(menu_label, "favorites")
                  )
               {
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
               }
               else if (string_is_equal(menu_label, "disk_image_append"))
               {
                  BIND_ACTION_OK(cbs, action_ok_disk_image_append);
               }
               else if (string_is_equal(menu_label, "subsystem_add"))
               {
                  BIND_ACTION_OK(cbs, action_ok_subsystem_add);
               }
               else
               {
                  BIND_ACTION_OK(cbs, action_ok_file_load);
               }
            }
            break;
         case FILE_TYPE_MOVIE:
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_file_load_ffmpeg);
#endif
            break;
         case FILE_TYPE_MUSIC:
            BIND_ACTION_OK(cbs, action_ok_file_load_music);
            break;
         case FILE_TYPE_IMAGEVIEWER:
            /* TODO/FIXME - handle scan case */
            BIND_ACTION_OK(cbs, action_ok_file_load_imageviewer);
            break;
         case FILE_TYPE_DIRECT_LOAD:
            BIND_ACTION_OK(cbs, action_ok_file_load);
            break;
         case MENU_SETTINGS:
         case MENU_SETTING_GROUP:
         case MENU_SETTING_SUBGROUP:
            BIND_ACTION_OK(cbs, action_ok_push_default);
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
            BIND_ACTION_OK(cbs, action_ok_disk_cycle_tray_status);
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            BIND_ACTION_OK(cbs, action_ok_disk_index_dropdown_box_list);
            break;
         case MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_CREATE:
            BIND_ACTION_OK(cbs, action_ok_game_specific_core_options_create);
            break;
         case MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE:
            BIND_ACTION_OK(cbs, action_ok_folder_specific_core_options_create);
            break;
         case MENU_SETTING_ACTION_GAME_SPECIFIC_CORE_OPTIONS_REMOVE:
            BIND_ACTION_OK(cbs, action_ok_game_specific_core_options_remove);
            break;
         case MENU_SETTING_ACTION_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE:
            BIND_ACTION_OK(cbs, action_ok_folder_specific_core_options_remove);
            break;
         case MENU_SETTING_ACTION_CORE_OPTIONS_RESET:
            BIND_ACTION_OK(cbs, action_ok_core_options_reset);
            break;
         case MENU_SETTING_ACTION_CORE_OPTIONS_FLUSH:
            BIND_ACTION_OK(cbs, action_ok_core_options_flush);
            break;
         case MENU_SETTING_ITEM_CORE_RESTORE_BACKUP:
            BIND_ACTION_OK(cbs, action_ok_core_restore_backup);
            break;
         case MENU_SETTING_ITEM_CORE_DELETE_BACKUP:
            BIND_ACTION_OK(cbs, action_ok_core_delete_backup);
            break;
         case MENU_SETTING_ACTION_CORE_LOCK:
            BIND_ACTION_OK(cbs, action_ok_core_lock);
            break;
         case MENU_SETTING_ACTION_CORE_SET_STANDALONE_EXEMPT:
            BIND_ACTION_OK(cbs, action_ok_core_set_standalone_exempt);
            break;
         case MENU_SETTING_ACTION_VIDEO_FILTER_REMOVE:
            BIND_ACTION_OK(cbs, action_ok_video_filter_remove);
            break;
         case MENU_SETTING_ACTION_AUDIO_DSP_PLUGIN_REMOVE:
            BIND_ACTION_OK(cbs, action_ok_audio_dsp_plugin_remove);
            break;
         case MENU_SETTING_ACTION_CONTENTLESS_CORE_RUN:
            BIND_ACTION_OK(cbs, action_ok_contentless_core_run);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, size_t lbl_len,
      unsigned type, size_t idx,
      const char *menu_label, size_t menu_lbl_len)
{
   if (!cbs)
      return -1;

   BIND_ACTION_OK(cbs, action_ok_lookup_setting);

   if (menu_cbs_init_bind_ok_compare_label(cbs, label) == 0)
      return 0;

   if (menu_cbs_init_bind_ok_compare_type(cbs, label,
            menu_label, type) == 0)
      return 0;

   return -1;
}
