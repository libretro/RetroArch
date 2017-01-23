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

#include <compat/strl.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"

#ifdef HAVE_CHEEVOS
#include "../../cheevos.h"
#endif
#include "../../verbosity.h"

#ifndef BIND_ACTION_SUBLABEL
#define BIND_ACTION_SUBLABEL(cbs, name) \
   cbs->action_sublabel = name; \
   cbs->action_sublabel_ident = #name;
#endif

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay.h"
#include "../../network/netplay/netplay_discovery.h"
#endif

#define default_sublabel_macro(func_name, lbl) \
  static int (func_name)(file_list_t *list, unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len) \
{ \
   strlcpy(s, msg_hash_to_str(lbl), len); \
   return 0; \
}

default_sublabel_macro(action_bind_sublabel_driver_settings_list,             MENU_ENUM_SUBLABEL_DRIVER_SETTINGS)
default_sublabel_macro(action_bind_sublabel_retro_achievements_settings_list, MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS)
default_sublabel_macro(action_bind_sublabel_saving_settings_list,          MENU_ENUM_SUBLABEL_SAVING_SETTINGS)
default_sublabel_macro(action_bind_sublabel_logging_settings_list,         MENU_ENUM_SUBLABEL_LOGGING_SETTINGS)
default_sublabel_macro(action_bind_sublabel_user_interface_settings_list,  MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_privacy_settings_list,         MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS)
default_sublabel_macro(action_bind_sublabel_directory_settings_list,       MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS)
default_sublabel_macro(action_bind_sublabel_playlist_settings_list,        MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS)
default_sublabel_macro(action_bind_sublabel_network_settings_list,         MENU_ENUM_SUBLABEL_NETWORK_SETTINGS)
default_sublabel_macro(action_bind_sublabel_user_settings_list,            MENU_ENUM_SUBLABEL_USER_SETTINGS)
default_sublabel_macro(action_bind_sublabel_recording_settings_list,       MENU_ENUM_SUBLABEL_RECORDING_SETTINGS)
default_sublabel_macro(action_bind_sublabel_frame_throttle_settings_list,  MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_onscreen_display_settings_list,MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS)
default_sublabel_macro(action_bind_sublabel_core_settings_list,            MENU_ENUM_SUBLABEL_CORE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_information_list_list,         MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST)
default_sublabel_macro(action_bind_sublabel_cheevos_enable,                MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE)
default_sublabel_macro(action_bind_sublabel_cheevos_test_unofficial,       MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL)
default_sublabel_macro(action_bind_sublabel_cheevos_hardcore_mode_enable,  MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_settings_list,            MENU_ENUM_SUBLABEL_MENU_SETTINGS)
default_sublabel_macro(action_bind_sublabel_video_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_SETTINGS)
default_sublabel_macro(action_bind_sublabel_suspend_screensaver_enable,    MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE)
default_sublabel_macro(action_bind_sublabel_video_window_scale,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE)
default_sublabel_macro(action_bind_sublabel_audio_settings_list,           MENU_ENUM_SUBLABEL_AUDIO_SETTINGS)
default_sublabel_macro(action_bind_sublabel_input_settings_list,           MENU_ENUM_SUBLABEL_INPUT_SETTINGS)
default_sublabel_macro(action_bind_sublabel_wifi_settings_list,            MENU_ENUM_SUBLABEL_WIFI_SETTINGS)
default_sublabel_macro(action_bind_sublabel_netplay_lan_scan_settings_list,MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS)
default_sublabel_macro(action_bind_sublabel_help_list,                     MENU_ENUM_SUBLABEL_HELP_LIST)
default_sublabel_macro(action_bind_sublabel_services_settings_list,        MENU_ENUM_SUBLABEL_SERVICES_SETTINGS)
default_sublabel_macro(action_bind_sublabel_ssh_enable,                    MENU_ENUM_SUBLABEL_SSH_ENABLE)
default_sublabel_macro(action_bind_sublabel_samba_enable,                  MENU_ENUM_SUBLABEL_SAMBA_ENABLE )
default_sublabel_macro(action_bind_sublabel_bluetooth_enable,              MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE )
default_sublabel_macro(action_bind_sublabel_user_language,                 MENU_ENUM_SUBLABEL_USER_LANGUAGE)
default_sublabel_macro(action_bind_sublabel_max_swapchain_images,          MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES )
default_sublabel_macro(action_bind_sublabel_online_updater,                MENU_ENUM_SUBLABEL_ONLINE_UPDATER)
default_sublabel_macro(action_bind_sublabel_fps_show,                      MENU_ENUM_SUBLABEL_FPS_SHOW)
default_sublabel_macro(action_bind_sublabel_netplay_settings,              MENU_ENUM_SUBLABEL_NETPLAY)
default_sublabel_macro(action_bind_sublabel_user_bind_settings,            MENU_ENUM_SUBLABEL_INPUT_USER_BINDS)
default_sublabel_macro(action_bind_sublabel_input_hotkey_settings,         MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS)
default_sublabel_macro(action_bind_sublabel_add_content_list,              MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST)
default_sublabel_macro(action_bind_sublabel_video_frame_delay,             MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY)
default_sublabel_macro(action_bind_sublabel_video_black_frame_insertion,   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION)
default_sublabel_macro(action_bind_sublabel_systeminfo_cpu_cores,          MENU_ENUM_SUBLABEL_CPU_CORES)
default_sublabel_macro(action_bind_sublabel_toggle_gamepad_combo,          MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO)
default_sublabel_macro(action_bind_sublabel_show_hidden_files,             MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES)
default_sublabel_macro(action_bind_sublabel_log_verbosity,                 MENU_ENUM_SUBLABEL_LOG_VERBOSITY)
default_sublabel_macro(action_bind_sublabel_video_monitor_index,           MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX)
default_sublabel_macro(action_bind_sublabel_video_refresh_rate_auto,       MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO)
default_sublabel_macro(action_bind_sublabel_video_hard_sync,               MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC)
default_sublabel_macro(action_bind_sublabel_video_hard_sync_frames,        MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES)
default_sublabel_macro(action_bind_sublabel_video_threaded,                MENU_ENUM_SUBLABEL_VIDEO_THREADED)
default_sublabel_macro(action_bind_sublabel_config_save_on_exit,           MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT)
default_sublabel_macro(action_bind_sublabel_configuration_settings_list,   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS)
default_sublabel_macro(action_bind_sublabel_configurations_list_list,      MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST)
default_sublabel_macro(action_bind_sublabel_video_shared_context,          MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT)
default_sublabel_macro(action_bind_sublabel_audio_latency,                 MENU_ENUM_SUBLABEL_AUDIO_LATENCY)
default_sublabel_macro(action_bind_sublabel_audio_rate_control_delta,      MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA)
default_sublabel_macro(action_bind_sublabel_audio_mute,                    MENU_ENUM_SUBLABEL_AUDIO_MUTE)
default_sublabel_macro(action_bind_sublabel_camera_allow,                  MENU_ENUM_SUBLABEL_CAMERA_ALLOW)
default_sublabel_macro(action_bind_sublabel_location_allow,                MENU_ENUM_SUBLABEL_LOCATION_ALLOW)
default_sublabel_macro(action_bind_sublabel_input_max_users,               MENU_ENUM_SUBLABEL_INPUT_MAX_USERS)
default_sublabel_macro(action_bind_sublabel_input_poll_type_behavior,      MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR)
default_sublabel_macro(action_bind_sublabel_input_all_users_control_menu,  MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU)
default_sublabel_macro(action_bind_sublabel_input_bind_timeout,            MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT)
default_sublabel_macro(action_bind_sublabel_audio_volume,                  MENU_ENUM_SUBLABEL_AUDIO_VOLUME)
default_sublabel_macro(action_bind_sublabel_audio_sync,                    MENU_ENUM_SUBLABEL_AUDIO_SYNC)
default_sublabel_macro(action_bind_sublabel_axis_threshold,                MENU_ENUM_SUBLABEL_INPUT_AXIS_THRESHOLD)
default_sublabel_macro(action_bind_sublabel_input_turbo_period,            MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD)
default_sublabel_macro(action_bind_sublabel_input_duty_cycle,              MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE)
default_sublabel_macro(action_bind_sublabel_video_vertical_sync,           MENU_ENUM_SUBLABEL_VIDEO_VSYNC)
default_sublabel_macro(action_bind_sublabel_core_allow_rotate,             MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE)
default_sublabel_macro(action_bind_sublabel_dummy_on_core_shutdown,        MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN)
default_sublabel_macro(action_bind_sublabel_dummy_check_missing_firmware,  MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE)
default_sublabel_macro(action_bind_sublabel_video_refresh_rate,            MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE)
default_sublabel_macro(action_bind_sublabel_audio_enable,                  MENU_ENUM_SUBLABEL_AUDIO_ENABLE)
default_sublabel_macro(action_bind_sublabel_audio_max_timing_skew,         MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW)
default_sublabel_macro(action_bind_sublabel_pause_nonactive,               MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE)
default_sublabel_macro(action_bind_sublabel_video_disable_composition,     MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION)
default_sublabel_macro(action_bind_sublabel_history_list_enable,           MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE)
default_sublabel_macro(action_bind_sublabel_content_history_size,          MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE)
default_sublabel_macro(action_bind_sublabel_menu_input_unified_controls,   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS)
default_sublabel_macro(action_bind_sublabel_onscreen_notifications_enable, MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE)

/* MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM*/

static int action_bind_sublabel_cheevos_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
#ifdef HAVE_CHEEVOS
   cheevos_ctx_desc_t desc_info;
   unsigned new_id = type - MENU_SETTINGS_CHEEVOS_START;
   desc_info.idx   = new_id;
   desc_info.s     = s;
   desc_info.len   = len;
   cheevos_get_description(&desc_info);

   strlcpy(s, desc_info.s, len);
#endif
   return 0;
}

#ifdef HAVE_NETWORKING
static int action_bind_sublabel_netplay_room(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   if (i < 1)
      return 0;

   snprintf(s,len, "%s (%s)\n%s (%08x)", 
      netplay_room_list[i - 1].corename, netplay_room_list[i - 1].coreversion, 
      netplay_room_list[i - 1].gamename, netplay_room_list[i - 1].gamecrc);
#if 0
   strlcpy(s, netplay_room_list[i - 1].corename, len);
#endif
   return 0;
}
#endif

static int action_bind_sublabel_generic(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   return 0;
}

int menu_cbs_init_bind_sublabel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_generic);

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_onscreen_notifications_enable);
            break;
         case MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_input_unified_controls);
            break;
         case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_max_timing_skew);
            break;
         case MENU_ENUM_LABEL_AUDIO_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_enable);
            break;
         case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_refresh_rate);
            break;
         case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_dummy_on_core_shutdown);
            break;
         case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_dummy_check_missing_firmware);
            break;
         case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_allow_rotate);
            break;
         case MENU_ENUM_LABEL_VIDEO_VSYNC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_vertical_sync);
            break;
         case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_duty_cycle);
            break;
         case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_turbo_period);
            break;
         case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_bind_timeout);
            break;
         case MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_axis_threshold);
            break;
         case MENU_ENUM_LABEL_AUDIO_SYNC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_sync);
            break;
         case MENU_ENUM_LABEL_AUDIO_VOLUME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_volume);
            break;
         case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_all_users_control_menu);
            break;
         case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_poll_type_behavior);
            break;
         case MENU_ENUM_LABEL_INPUT_MAX_USERS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_max_users);
            break;
         case MENU_ENUM_LABEL_LOCATION_ALLOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_location_allow);
            break;
         case MENU_ENUM_LABEL_CAMERA_ALLOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_camera_allow);
            break;
         case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_rate_control_delta);
            break;
         case MENU_ENUM_LABEL_AUDIO_MUTE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mute);
            break;
         case MENU_ENUM_LABEL_AUDIO_LATENCY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_latency);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_shared_context);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY:
         case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_entry);
            break;
#ifdef HAVE_NETWORKING
         case MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_room);
            break;
#endif
         case MENU_ENUM_LABEL_CHEEVOS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_test_unofficial);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_hardcore_mode_enable);
            break;
         case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_config_save_on_exit);
            break;
         case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_configuration_settings_list);
            break;
         case MENU_ENUM_LABEL_CONFIGURATIONS_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_configurations_list_list);
            break;
         case MENU_ENUM_LABEL_VIDEO_THREADED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_threaded);
            break;
         case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_hard_sync);
            break;
         case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_hard_sync_frames);
            break;
         case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_refresh_rate_auto);
            break;
         case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_monitor_index);
            break;
         case MENU_ENUM_LABEL_LOG_VERBOSITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_log_verbosity);
            break;
         case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_show_hidden_files);
            break;
         case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_toggle_gamepad_combo);
            break;
         case MENU_ENUM_LABEL_CPU_CORES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_systeminfo_cpu_cores);
            break;
         case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_black_frame_insertion);
            break;
         case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_frame_delay);
            break;
         case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_add_content_list);
            break;
         case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_hotkey_settings);
            break;
         case MENU_ENUM_LABEL_INPUT_USER_1_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_2_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_3_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_4_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_5_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_6_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_7_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_8_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_9_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_10_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_11_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_12_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_13_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_14_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_15_BINDS:
         case MENU_ENUM_LABEL_INPUT_USER_16_BINDS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_user_bind_settings);
            break;
         case MENU_ENUM_LABEL_INFORMATION_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_information_list_list);
            break;
         case MENU_ENUM_LABEL_NETPLAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_settings);
            break;
         case MENU_ENUM_LABEL_ONLINE_UPDATER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_online_updater);
            break;
         case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_max_swapchain_images);
            break;
         case MENU_ENUM_LABEL_FPS_SHOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_fps_show);
            break;
         case MENU_ENUM_LABEL_MENU_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_settings_list);
            break;
         case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_settings_list);
            break;
         case MENU_ENUM_LABEL_RECORDING_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_recording_settings_list);
            break;
         case MENU_ENUM_LABEL_CORE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_settings_list);
            break;
         case MENU_ENUM_LABEL_DRIVER_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_driver_settings_list);
            break;
         case MENU_ENUM_LABEL_SAVING_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_saving_settings_list);
            break;
         case MENU_ENUM_LABEL_LOGGING_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_logging_settings_list);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_settings_list);
            break;
         case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_user_interface_settings_list);
            break;
         case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_privacy_settings_list);
            break;
         case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_directory_settings_list);
            break;
         case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_frame_throttle_settings_list);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_onscreen_display_settings_list);
            break;
         case MENU_ENUM_LABEL_NETWORK_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_network_settings_list);
            break;
         case MENU_ENUM_LABEL_USER_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_user_settings_list);
            break;
         case MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_retro_achievements_settings_list);
            break;
         case MENU_ENUM_LABEL_INPUT_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_settings_list);
            break;
         case MENU_ENUM_LABEL_WIFI_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_wifi_settings_list);
            break;
         case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_lan_scan_settings_list);
            break;
         case MENU_ENUM_LABEL_HELP_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_help_list);
            break;
         case MENU_ENUM_LABEL_LAKKA_SERVICES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_services_settings_list);
            break;
         case MENU_ENUM_LABEL_SSH_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ssh_enable);
            break;
         case MENU_ENUM_LABEL_SAMBA_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_samba_enable);
            break;
         case MENU_ENUM_LABEL_BLUETOOTH_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_bluetooth_enable);
            break;
         case MENU_ENUM_LABEL_USER_LANGUAGE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_user_language);
            break;
         case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_suspend_screensaver_enable);
            break;
         case MENU_ENUM_LABEL_VIDEO_SCALE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_window_scale);
            break;
         case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_pause_nonactive);
            break;
         case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_disable_composition);
            break;
         case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_history_list_enable);
            break;
         case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_history_size);
            break;
         default:
         case MSG_UNKNOWN:
            return -1;
      }
   }

   return 0;
}
