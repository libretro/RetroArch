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

#include <string.h>
#include <string/stdstring.h>
#include <file/file_path.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"

#include "../../retroarch.h"
#include "../../managers/core_option_manager.h"

#ifdef HAVE_CHEEVOS
#include "../../cheevos-new/cheevos.h"
#endif
#include "../../core_info.h"
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

#include "../../retroarch.h"
#include "../../content.h"
#include "../../dynamic.h"
#include "../../configuration.h"
#include "../../managers/cheat_manager.h"
#include "../../tasks/tasks_internal.h"

#include "../../playlist.h"
#include "../../runtime_file.h"

#define default_sublabel_macro(func_name, lbl) \
  static int (func_name)(file_list_t *list, unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len) \
{ \
   strlcpy(s, msg_hash_to_str(lbl), len); \
   return 1; \
}

static int menu_action_sublabel_file_browser_core(file_list_t *list, unsigned type, unsigned i, const char *label, const char *path, char *s, size_t len)
{
   core_info_list_t *core_list = NULL;

   core_info_get_list(&core_list);

   strlcpy(s, "License: ", len);

   if (core_list)
   {
      unsigned j;
      for (j = 0; j < core_list->count; j++)
      {
         if (string_is_equal(path_basename(core_list->list[j].path), 
                  path))
         {
            if (core_list->list[j].licenses_list)
            {
               char tmp[PATH_MAX_LENGTH];
               tmp[0]  = '\0';

               string_list_join_concat(tmp, sizeof(tmp),
                     core_list->list[j].licenses_list, ", ");
               strlcat(s, tmp, len);
               return 1;
            }
         }
      }
   }

   strlcat(s, "N/A", len);
   return 1;
}

#ifdef HAVE_AUDIOMIXER
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_add_to_mixer_and_play,
      MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY)
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_add_to_mixer,
      MENU_ENUM_SUBLABEL_ADD_TO_MIXER)
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_stream_play,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY)
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_stream_play_looped,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED)
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_stream_play_sequential,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL)
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_stream_stop,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP)
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_stream_remove,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE)
default_sublabel_macro(menu_action_sublabel_setting_audio_mixer_stream_volume,
      MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME)
#endif
default_sublabel_macro(action_bind_sublabel_reset_to_default_config,             MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG)
default_sublabel_macro(action_bind_sublabel_quick_menu_override_options,             MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS)
default_sublabel_macro(action_bind_sublabel_quick_menu_start_streaming,             MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING)
default_sublabel_macro(action_bind_sublabel_quick_menu_start_recording,             MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING)
default_sublabel_macro(action_bind_sublabel_quick_menu_stop_streaming,             MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING)
default_sublabel_macro(action_bind_sublabel_quick_menu_stop_recording,             MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING)
default_sublabel_macro(action_bind_sublabel_crt_switchres,             MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION)
default_sublabel_macro(action_bind_sublabel_crt_switchres_super,       MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER)
default_sublabel_macro(action_bind_sublabel_crt_switchres_x_axis_centering,       MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING)
default_sublabel_macro(action_bind_sublabel_crt_switchres_use_custom_refresh_rate,       MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE)
default_sublabel_macro(action_bind_sublabel_automatically_add_content_to_playlist,             MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST)
default_sublabel_macro(action_bind_sublabel_driver_settings_list,             MENU_ENUM_SUBLABEL_DRIVER_SETTINGS)
default_sublabel_macro(action_bind_sublabel_retro_achievements_settings_list, MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS)
default_sublabel_macro(action_bind_sublabel_saving_settings_list,          MENU_ENUM_SUBLABEL_SAVING_SETTINGS)
default_sublabel_macro(action_bind_sublabel_logging_settings_list,         MENU_ENUM_SUBLABEL_LOGGING_SETTINGS)
default_sublabel_macro(action_bind_sublabel_user_interface_settings_list,  MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_ai_service_settings_list,  MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_ai_service_mode,  MENU_ENUM_SUBLABEL_AI_SERVICE_MODE)
default_sublabel_macro(action_bind_sublabel_ai_service_target_lang,  MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG)
default_sublabel_macro(action_bind_sublabel_ai_service_source_lang,  MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG)
default_sublabel_macro(action_bind_sublabel_ai_service_url,  MENU_ENUM_SUBLABEL_AI_SERVICE_URL)
default_sublabel_macro(action_bind_sublabel_ai_service_enable,  MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE)
default_sublabel_macro(action_bind_sublabel_power_management_settings_list,  MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS)
default_sublabel_macro(action_bind_sublabel_privacy_settings_list,         MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS)
default_sublabel_macro(action_bind_sublabel_midi_settings_list,            MENU_ENUM_SUBLABEL_MIDI_SETTINGS)
default_sublabel_macro(action_bind_sublabel_directory_settings_list,       MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS)
default_sublabel_macro(action_bind_sublabel_playlist_settings_list,        MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS)
default_sublabel_macro(action_bind_sublabel_playlist_manager_list,         MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST)
default_sublabel_macro(action_bind_sublabel_playlist_manager_default_core, MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE)
default_sublabel_macro(action_bind_sublabel_playlist_manager_reset_cores,  MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES)
default_sublabel_macro(action_bind_sublabel_playlist_manager_label_display_mode, MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE)
default_sublabel_macro(action_bind_sublabel_network_settings_list,         MENU_ENUM_SUBLABEL_NETWORK_SETTINGS)
default_sublabel_macro(action_bind_sublabel_network_on_demand_thumbnails,  MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_user_settings_list,            MENU_ENUM_SUBLABEL_USER_SETTINGS)
default_sublabel_macro(action_bind_sublabel_recording_settings_list,       MENU_ENUM_SUBLABEL_RECORDING_SETTINGS)
default_sublabel_macro(action_bind_sublabel_frame_throttle_settings_list,  MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_frame_time_counter_settings_list,  MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS)
default_sublabel_macro(action_bind_sublabel_frame_time_counter_reset_after_fastforwarding,  MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING)
default_sublabel_macro(action_bind_sublabel_frame_time_counter_reset_after_load_state,  MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE)
default_sublabel_macro(action_bind_sublabel_frame_time_counter_reset_after_save_state,  MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE)
default_sublabel_macro(action_bind_sublabel_onscreen_display_settings_list,MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS)
default_sublabel_macro(action_bind_sublabel_core_settings_list,            MENU_ENUM_SUBLABEL_CORE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_information_list_list,         MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST)
default_sublabel_macro(action_bind_sublabel_cheevos_enable,                MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE)
default_sublabel_macro(action_bind_sublabel_cheevos_test_unofficial,       MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL)
default_sublabel_macro(action_bind_sublabel_cheevos_hardcore_mode_enable,  MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE)
default_sublabel_macro(action_bind_sublabel_cheevos_leaderboards_enable,   MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE)
default_sublabel_macro(action_bind_sublabel_cheevos_badges_enable,         MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE)
default_sublabel_macro(action_bind_sublabel_cheevos_verbose_enable,        MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE)
default_sublabel_macro(action_bind_sublabel_cheevos_auto_screenshot,       MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT)
default_sublabel_macro(action_bind_sublabel_menu_views_settings_list,      MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS)
default_sublabel_macro(action_bind_sublabel_quick_menu_views_settings_list, MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS)
default_sublabel_macro(action_bind_sublabel_settings_views_settings_list, MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS)
default_sublabel_macro(action_bind_sublabel_menu_settings_list,            MENU_ENUM_SUBLABEL_MENU_SETTINGS)
default_sublabel_macro(action_bind_sublabel_video_settings_list,           MENU_ENUM_SUBLABEL_VIDEO_SETTINGS)
default_sublabel_macro(action_bind_sublabel_crt_switchres_settings_list,           MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS)
default_sublabel_macro(action_bind_sublabel_suspend_screensaver_enable,    MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE)
default_sublabel_macro(action_bind_sublabel_video_window_scale,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE)
default_sublabel_macro(action_bind_sublabel_audio_settings_list,           MENU_ENUM_SUBLABEL_AUDIO_SETTINGS)
#ifdef HAVE_AUDIOMIXER
default_sublabel_macro(action_bind_sublabel_mixer_settings_list,           MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS)
#endif
default_sublabel_macro(action_bind_sublabel_input_settings_list,           MENU_ENUM_SUBLABEL_INPUT_SETTINGS)
default_sublabel_macro(action_bind_sublabel_latency_settings_list,         MENU_ENUM_SUBLABEL_LATENCY_SETTINGS)
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
default_sublabel_macro(action_bind_sublabel_fps_update_interval,                      MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL)
default_sublabel_macro(action_bind_sublabel_framecount_show,               MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW)
default_sublabel_macro(action_bind_sublabel_memory_show,                   MENU_ENUM_SUBLABEL_MEMORY_SHOW)
default_sublabel_macro(action_bind_sublabel_statistics_show,               MENU_ENUM_SUBLABEL_STATISTICS_SHOW)
default_sublabel_macro(action_bind_sublabel_netplay_settings,              MENU_ENUM_SUBLABEL_NETPLAY)
default_sublabel_macro(action_bind_sublabel_user_bind_settings,            MENU_ENUM_SUBLABEL_INPUT_USER_BINDS)
default_sublabel_macro(action_bind_sublabel_input_hotkey_settings,         MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS)
default_sublabel_macro(action_bind_sublabel_materialui_icons_enable,       MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE)
default_sublabel_macro(action_bind_sublabel_add_content_list,              MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST)
default_sublabel_macro(action_bind_sublabel_video_frame_delay,             MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY)
default_sublabel_macro(action_bind_sublabel_video_shader_delay,            MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY)
default_sublabel_macro(action_bind_sublabel_video_black_frame_insertion,   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION)
default_sublabel_macro(action_bind_sublabel_systeminfo_cpu_cores,          MENU_ENUM_SUBLABEL_CPU_CORES)
default_sublabel_macro(action_bind_sublabel_toggle_gamepad_combo,          MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO)
default_sublabel_macro(action_bind_sublabel_show_hidden_files,             MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES)
default_sublabel_macro(action_bind_sublabel_log_verbosity,                 MENU_ENUM_SUBLABEL_LOG_VERBOSITY)
default_sublabel_macro(action_bind_sublabel_log_to_file,                   MENU_ENUM_SUBLABEL_LOG_TO_FILE)
default_sublabel_macro(action_bind_sublabel_log_to_file_timestamp,         MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP)
default_sublabel_macro(action_bind_sublabel_log_dir,                       MENU_ENUM_SUBLABEL_LOG_DIR)
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
#ifdef HAVE_AUDIOMIXER
default_sublabel_macro(action_bind_sublabel_audio_mixer_mute,              MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE)
#endif
default_sublabel_macro(action_bind_sublabel_camera_allow,                  MENU_ENUM_SUBLABEL_CAMERA_ALLOW)
default_sublabel_macro(action_bind_sublabel_location_allow,                MENU_ENUM_SUBLABEL_LOCATION_ALLOW)
default_sublabel_macro(action_bind_sublabel_input_max_users,               MENU_ENUM_SUBLABEL_INPUT_MAX_USERS)
default_sublabel_macro(action_bind_sublabel_input_poll_type_behavior,      MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR)
default_sublabel_macro(action_bind_sublabel_input_all_users_control_menu,  MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU)
default_sublabel_macro(action_bind_sublabel_input_bind_timeout,            MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT)
default_sublabel_macro(action_bind_sublabel_input_bind_hold,               MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD)
default_sublabel_macro(action_bind_sublabel_audio_volume,                  MENU_ENUM_SUBLABEL_AUDIO_VOLUME)
#ifdef HAVE_AUDIOMIXER
default_sublabel_macro(action_bind_sublabel_audio_mixer_volume,            MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME)
#endif
default_sublabel_macro(action_bind_sublabel_audio_sync,                    MENU_ENUM_SUBLABEL_AUDIO_SYNC)
#if defined(GEKKO)
default_sublabel_macro(action_bind_sublabel_input_mouse_scale, MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE)
#endif
default_sublabel_macro(action_bind_sublabel_axis_threshold,                MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD)
default_sublabel_macro(action_bind_sublabel_input_turbo_period,            MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD)
default_sublabel_macro(action_bind_sublabel_input_duty_cycle,              MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE)
default_sublabel_macro(action_bind_sublabel_video_vertical_sync,           MENU_ENUM_SUBLABEL_VIDEO_VSYNC)
default_sublabel_macro(action_bind_sublabel_video_adaptive_vsync,          MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC)
default_sublabel_macro(action_bind_sublabel_core_allow_rotate,             MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE)
default_sublabel_macro(action_bind_sublabel_dummy_on_core_shutdown,        MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN)
default_sublabel_macro(action_bind_sublabel_dummy_check_missing_firmware,  MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE)
default_sublabel_macro(action_bind_sublabel_video_refresh_rate,            MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE)
default_sublabel_macro(action_bind_sublabel_video_refresh_rate_polled,     MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED)
default_sublabel_macro(action_bind_sublabel_audio_enable,                  MENU_ENUM_SUBLABEL_AUDIO_ENABLE)
default_sublabel_macro(action_bind_sublabel_audio_enable_menu,             MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU)
default_sublabel_macro(action_bind_sublabel_audio_max_timing_skew,         MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW)
default_sublabel_macro(action_bind_sublabel_pause_nonactive,               MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE)
default_sublabel_macro(action_bind_sublabel_video_disable_composition,     MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION)
default_sublabel_macro(action_bind_sublabel_video_smooth,                  MENU_ENUM_SUBLABEL_VIDEO_SMOOTH)
default_sublabel_macro(action_bind_sublabel_history_list_enable,           MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE)
default_sublabel_macro(action_bind_sublabel_content_history_size,          MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE)
default_sublabel_macro(action_bind_sublabel_content_favorites_size,        MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE)
default_sublabel_macro(action_bind_sublabel_menu_input_unified_controls,   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS)
default_sublabel_macro(action_bind_sublabel_quit_press_twice,              MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE)
default_sublabel_macro(action_bind_sublabel_onscreen_notifications_enable, MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE)
default_sublabel_macro(action_bind_sublabel_video_crop_overscan,           MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN)
default_sublabel_macro(action_bind_sublabel_video_filter,                  MENU_ENUM_SUBLABEL_VIDEO_FILTER)
default_sublabel_macro(action_bind_sublabel_netplay_nickname,              MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME)
default_sublabel_macro(action_bind_sublabel_cheevos_username,              MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME)
default_sublabel_macro(action_bind_sublabel_cheevos_password,              MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD)
default_sublabel_macro(action_bind_sublabel_video_post_filter_record,      MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD)
default_sublabel_macro(action_bind_sublabel_start_core,                    MENU_ENUM_SUBLABEL_START_CORE)
default_sublabel_macro(action_bind_sublabel_core_list,                     MENU_ENUM_SUBLABEL_CORE_LIST)
default_sublabel_macro(action_bind_sublabel_download_core,                 MENU_ENUM_SUBLABEL_DOWNLOAD_CORE)
default_sublabel_macro(action_bind_sublabel_sideload_core_list,            MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST)
default_sublabel_macro(action_bind_sublabel_load_disc,                  MENU_ENUM_SUBLABEL_LOAD_DISC)
default_sublabel_macro(action_bind_sublabel_dump_disc,                  MENU_ENUM_SUBLABEL_DUMP_DISC)
default_sublabel_macro(action_bind_sublabel_content_list,                  MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST)
default_sublabel_macro(action_bind_sublabel_content_special,               MENU_ENUM_SUBLABEL_LOAD_CONTENT_SPECIAL)
default_sublabel_macro(action_bind_sublabel_network_information,           MENU_ENUM_SUBLABEL_NETWORK_INFORMATION)
default_sublabel_macro(action_bind_sublabel_system_information,            MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION)
default_sublabel_macro(action_bind_sublabel_quit_retroarch,                MENU_ENUM_SUBLABEL_QUIT_RETROARCH)
default_sublabel_macro(action_bind_sublabel_restart_retroarch,             MENU_ENUM_SUBLABEL_RESTART_RETROARCH)
default_sublabel_macro(action_bind_sublabel_menu_widgets,             MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE)
default_sublabel_macro(action_bind_sublabel_video_window_width,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH)
default_sublabel_macro(action_bind_sublabel_video_window_height,           MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT)
default_sublabel_macro(action_bind_sublabel_video_fullscreen_x,            MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X)
default_sublabel_macro(action_bind_sublabel_video_fullscreen_y,            MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y)
default_sublabel_macro(action_bind_sublabel_video_save_window_position,    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION)
default_sublabel_macro(action_bind_sublabel_video_message_pos_x,           MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X)
default_sublabel_macro(action_bind_sublabel_video_message_pos_y,           MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y)
default_sublabel_macro(action_bind_sublabel_video_font_size,               MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE)
default_sublabel_macro(action_bind_sublabel_input_overlay_hide_in_menu,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU)
default_sublabel_macro(action_bind_sublabel_input_overlay_show_mouse_cursor,       MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR)
default_sublabel_macro(action_bind_sublabel_content_collection_list,       MENU_ENUM_SUBLABEL_PLAYLISTS_TAB)
default_sublabel_macro(action_bind_sublabel_video_scale_integer,           MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER)
default_sublabel_macro(action_bind_sublabel_video_gpu_screenshot,          MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT)
default_sublabel_macro(action_bind_sublabel_video_rotation,                MENU_ENUM_SUBLABEL_VIDEO_ROTATION)
default_sublabel_macro(action_bind_sublabel_screen_orientation,            MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION)
default_sublabel_macro(action_bind_sublabel_video_force_srgb_enable,       MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE)
default_sublabel_macro(action_bind_sublabel_video_fullscreen,              MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN)
default_sublabel_macro(action_bind_sublabel_video_windowed_fullscreen,     MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN)
default_sublabel_macro(action_bind_sublabel_video_gpu_record,              MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD)
default_sublabel_macro(action_bind_sublabel_savestate_auto_index,          MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX)
default_sublabel_macro(action_bind_sublabel_block_sram_overwrite,          MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE)
default_sublabel_macro(action_bind_sublabel_fastforward_ratio,             MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO)
default_sublabel_macro(action_bind_sublabel_vrr_runloop_enable,            MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE)
default_sublabel_macro(action_bind_sublabel_slowmotion_ratio,              MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO)
default_sublabel_macro(action_bind_sublabel_run_ahead_enabled,             MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED)
default_sublabel_macro(action_bind_sublabel_run_ahead_secondary_instance,  MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE)
default_sublabel_macro(action_bind_sublabel_run_ahead_hide_warnings,       MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS)
default_sublabel_macro(action_bind_sublabel_run_ahead_frames,              MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES)
default_sublabel_macro(action_bind_sublabel_input_block_timeout,           MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT)
default_sublabel_macro(action_bind_sublabel_rewind,                        MENU_ENUM_SUBLABEL_REWIND_ENABLE)
default_sublabel_macro(action_bind_sublabel_cheat_apply_after_toggle,      MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE)
default_sublabel_macro(action_bind_sublabel_cheat_apply_after_load,        MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD)
default_sublabel_macro(action_bind_sublabel_rewind_granularity,            MENU_ENUM_SUBLABEL_REWIND_GRANULARITY)
default_sublabel_macro(action_bind_sublabel_rewind_buffer_size,            MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE)
default_sublabel_macro(action_bind_sublabel_rewind_buffer_size_step,       MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP)
default_sublabel_macro(action_bind_sublabel_cheat_idx,                     MENU_ENUM_SUBLABEL_CHEAT_IDX)
default_sublabel_macro(action_bind_sublabel_cheat_match_idx,               MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX)
default_sublabel_macro(action_bind_sublabel_cheat_big_endian,              MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN)
default_sublabel_macro(action_bind_sublabel_cheat_start_or_cont,           MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT)
default_sublabel_macro(action_bind_sublabel_cheat_start_or_restart,        MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART)
default_sublabel_macro(action_bind_sublabel_cheat_search_exact,            MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT)
default_sublabel_macro(action_bind_sublabel_cheat_search_lt,               MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LT)
default_sublabel_macro(action_bind_sublabel_cheat_search_gt,               MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GT)
default_sublabel_macro(action_bind_sublabel_cheat_search_eq,               MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQ)
default_sublabel_macro(action_bind_sublabel_cheat_search_neq,              MENU_ENUM_SUBLABEL_CHEAT_SEARCH_NEQ)
default_sublabel_macro(action_bind_sublabel_cheat_search_eqplus,           MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS)
default_sublabel_macro(action_bind_sublabel_cheat_search_eqminus,          MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS)
default_sublabel_macro(action_bind_sublabel_cheat_repeat_count,            MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT)
default_sublabel_macro(action_bind_sublabel_cheat_repeat_add_to_address,   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS)
default_sublabel_macro(action_bind_sublabel_cheat_repeat_add_to_value,     MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE)
default_sublabel_macro(action_bind_sublabel_cheat_add_matches,             MENU_ENUM_SUBLABEL_CHEAT_ADD_MATCHES)
default_sublabel_macro(action_bind_sublabel_cheat_view_matches,            MENU_ENUM_SUBLABEL_CHEAT_VIEW_MATCHES)
default_sublabel_macro(action_bind_sublabel_cheat_create_option,           MENU_ENUM_SUBLABEL_CHEAT_CREATE_OPTION)
default_sublabel_macro(action_bind_sublabel_cheat_delete_option,           MENU_ENUM_SUBLABEL_CHEAT_DELETE_OPTION)
default_sublabel_macro(action_bind_sublabel_cheat_add_new_top,             MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP)
default_sublabel_macro(action_bind_sublabel_cheat_add_new_bottom,          MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM)
default_sublabel_macro(action_bind_sublabel_cheat_reload_cheats,           MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS)
default_sublabel_macro(action_bind_sublabel_cheat_address_bit_position,    MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION)
default_sublabel_macro(action_bind_sublabel_cheat_delete_all,              MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL)
default_sublabel_macro(action_bind_sublabel_libretro_log_level,            MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL)
default_sublabel_macro(action_bind_sublabel_frontend_log_level,            MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL)
default_sublabel_macro(action_bind_sublabel_perfcnt_enable,                MENU_ENUM_SUBLABEL_PERFCNT_ENABLE)
default_sublabel_macro(action_bind_sublabel_savestate_auto_save,           MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE)
default_sublabel_macro(action_bind_sublabel_savestate_auto_load,           MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD)
default_sublabel_macro(action_bind_sublabel_savestate_thumbnail_enable,    MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE)
default_sublabel_macro(action_bind_sublabel_autosave_interval,             MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL)
default_sublabel_macro(action_bind_sublabel_input_remap_binds_enable,      MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE)
default_sublabel_macro(action_bind_sublabel_input_autodetect_enable,       MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE)
default_sublabel_macro(action_bind_sublabel_input_swap_ok_cancel,          MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL)
default_sublabel_macro(action_bind_sublabel_pause_libretro,                MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO)
default_sublabel_macro(action_bind_sublabel_menu_savestate_resume,         MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME)
default_sublabel_macro(action_bind_sublabel_video_driver,                  MENU_ENUM_SUBLABEL_VIDEO_DRIVER)
default_sublabel_macro(action_bind_sublabel_audio_driver,                  MENU_ENUM_SUBLABEL_AUDIO_DRIVER)
default_sublabel_macro(action_bind_sublabel_input_driver,                  MENU_ENUM_SUBLABEL_INPUT_DRIVER)
default_sublabel_macro(action_bind_sublabel_joypad_driver,                 MENU_ENUM_SUBLABEL_JOYPAD_DRIVER)
default_sublabel_macro(action_bind_sublabel_audio_resampler_driver,        MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER)
default_sublabel_macro(action_bind_sublabel_camera_driver,                 MENU_ENUM_SUBLABEL_CAMERA_DRIVER)
default_sublabel_macro(action_bind_sublabel_location_driver,               MENU_ENUM_SUBLABEL_LOCATION_DRIVER)
default_sublabel_macro(action_bind_sublabel_menu_driver,                   MENU_ENUM_SUBLABEL_MENU_DRIVER)
default_sublabel_macro(action_bind_sublabel_record_driver,                 MENU_ENUM_SUBLABEL_RECORD_DRIVER)
default_sublabel_macro(action_bind_sublabel_midi_driver,                   MENU_ENUM_SUBLABEL_MIDI_DRIVER)
default_sublabel_macro(action_bind_sublabel_wifi_driver,                   MENU_ENUM_SUBLABEL_WIFI_DRIVER)
default_sublabel_macro(action_bind_sublabel_filter_supported_extensions,   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE)
default_sublabel_macro(action_bind_sublabel_wallpaper,                     MENU_ENUM_SUBLABEL_MENU_WALLPAPER)
default_sublabel_macro(action_bind_sublabel_dynamic_wallpaper,             MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER)
default_sublabel_macro(action_bind_sublabel_audio_device,                  MENU_ENUM_SUBLABEL_AUDIO_DEVICE)
default_sublabel_macro(action_bind_sublabel_audio_output_rate,             MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE)
default_sublabel_macro(action_bind_sublabel_audio_dsp_plugin,              MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN)
default_sublabel_macro(action_bind_sublabel_audio_wasapi_exclusive_mode,   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE)
default_sublabel_macro(action_bind_sublabel_audio_wasapi_float_format,     MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT)
default_sublabel_macro(action_bind_sublabel_audio_wasapi_sh_buffer_length, MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH)
default_sublabel_macro(action_bind_sublabel_overlay_opacity,               MENU_ENUM_SUBLABEL_OVERLAY_OPACITY)
default_sublabel_macro(action_bind_sublabel_overlay_scale,                 MENU_ENUM_SUBLABEL_OVERLAY_SCALE)
default_sublabel_macro(action_bind_sublabel_overlay_enable,                MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE)
default_sublabel_macro(action_bind_sublabel_overlay_preset,                MENU_ENUM_SUBLABEL_OVERLAY_PRESET)
#ifdef HAVE_VIDEO_LAYOUT
default_sublabel_macro(action_bind_sublabel_video_layout_enable,           MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE)
default_sublabel_macro(action_bind_sublabel_video_layout_path,             MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH)
#endif
default_sublabel_macro(action_bind_sublabel_netplay_public_announce,       MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE)
default_sublabel_macro(action_bind_sublabel_netplay_ip_address,            MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS)
default_sublabel_macro(action_bind_sublabel_netplay_tcp_udp_port,          MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT)
default_sublabel_macro(action_bind_sublabel_netplay_password,              MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD)
default_sublabel_macro(action_bind_sublabel_netplay_spectate_password,     MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD)
default_sublabel_macro(action_bind_sublabel_netplay_start_as_spectator,    MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR)
default_sublabel_macro(action_bind_sublabel_netplay_allow_slaves,          MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES)
default_sublabel_macro(action_bind_sublabel_netplay_require_slaves,        MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES)
default_sublabel_macro(action_bind_sublabel_netplay_stateless_mode,        MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE)
default_sublabel_macro(action_bind_sublabel_netplay_check_frames,          MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES)
default_sublabel_macro(action_bind_sublabel_netplay_nat_traversal,         MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL)
default_sublabel_macro(action_bind_sublabel_stdin_cmd_enable,              MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE)
default_sublabel_macro(action_bind_sublabel_mouse_enable,                  MENU_ENUM_SUBLABEL_MOUSE_ENABLE)
default_sublabel_macro(action_bind_sublabel_pointer_enable,                MENU_ENUM_SUBLABEL_POINTER_ENABLE)
default_sublabel_macro(action_bind_sublabel_thumbnails,                    MENU_ENUM_SUBLABEL_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_thumbnails_rgui,               MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI)
default_sublabel_macro(action_bind_sublabel_left_thumbnails,               MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_left_thumbnails_rgui,          MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI)
default_sublabel_macro(action_bind_sublabel_left_thumbnails_ozone,         MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE)
default_sublabel_macro(action_bind_sublabel_menu_thumbnail_upscale_threshold, MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD)
default_sublabel_macro(action_bind_sublabel_timedate_enable,               MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE)
default_sublabel_macro(action_bind_sublabel_timedate_style,                MENU_ENUM_SUBLABEL_TIMEDATE_STYLE)
default_sublabel_macro(action_bind_sublabel_battery_level_enable,          MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_show_sublabels,           MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS)
default_sublabel_macro(action_bind_sublabel_navigation_wraparound,         MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND)
default_sublabel_macro(action_bind_sublabel_audio_resampler_quality,       MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY)
default_sublabel_macro(action_bind_sublabel_netplay_enable_host,           MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST)
default_sublabel_macro(action_bind_sublabel_netplay_enable_client,         MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT)
default_sublabel_macro(action_bind_sublabel_netplay_disconnect,            MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT)
default_sublabel_macro(action_bind_sublabel_scan_file,                     MENU_ENUM_SUBLABEL_SCAN_FILE)
default_sublabel_macro(action_bind_sublabel_scan_directory,                MENU_ENUM_SUBLABEL_SCAN_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_video_swap_interval,           MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL)
default_sublabel_macro(action_bind_sublabel_sort_savefiles_enable,         MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE)
default_sublabel_macro(action_bind_sublabel_sort_savestates_enable,        MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE)
default_sublabel_macro(action_bind_sublabel_core_updater_buildbot_url,     MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL)
default_sublabel_macro(action_bind_sublabel_input_overlay_show_physical_inputs,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS)
default_sublabel_macro(action_bind_sublabel_input_overlay_show_physical_inputs_port,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT)
default_sublabel_macro(action_bind_sublabel_core_updater_buildbot_assets_url,      MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL)
default_sublabel_macro(action_bind_sublabel_core_updater_auto_extract_archive,     MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE)
default_sublabel_macro(action_bind_sublabel_netplay_refresh_rooms,                 MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS)
default_sublabel_macro(action_bind_sublabel_rename_entry,                          MENU_ENUM_SUBLABEL_RENAME_ENTRY)
default_sublabel_macro(action_bind_sublabel_delete_entry,                          MENU_ENUM_SUBLABEL_DELETE_ENTRY)
default_sublabel_macro(action_bind_sublabel_information,                           MENU_ENUM_SUBLABEL_INFORMATION)
default_sublabel_macro(action_bind_sublabel_run,                                   MENU_ENUM_SUBLABEL_RUN)
default_sublabel_macro(action_bind_sublabel_add_to_favorites,                      MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES)
default_sublabel_macro(action_bind_sublabel_download_pl_entry_thumbnails,          MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_goto_favorites,                        MENU_ENUM_SUBLABEL_GOTO_FAVORITES)
default_sublabel_macro(action_bind_sublabel_goto_images,                           MENU_ENUM_SUBLABEL_GOTO_IMAGES)
default_sublabel_macro(action_bind_sublabel_goto_music,                            MENU_ENUM_SUBLABEL_GOTO_MUSIC)
default_sublabel_macro(action_bind_sublabel_goto_video,                            MENU_ENUM_SUBLABEL_GOTO_VIDEO)
default_sublabel_macro(action_bind_sublabel_menu_filebrowser_settings,             MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS)
default_sublabel_macro(action_bind_sublabel_menu_filebrowser_open_uwp_permissions, MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS)
default_sublabel_macro(action_bind_sublabel_menu_filebrowser_open_picker,          MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER)
default_sublabel_macro(action_bind_sublabel_auto_remaps_enable,                    MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE)
default_sublabel_macro(action_bind_sublabel_auto_overrides_enable,                 MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE)
default_sublabel_macro(action_bind_sublabel_game_specific_options,                 MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS)
default_sublabel_macro(action_bind_sublabel_global_core_options,                   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS)
default_sublabel_macro(action_bind_sublabel_core_enable,                           MENU_ENUM_SUBLABEL_CORE_ENABLE)
default_sublabel_macro(action_bind_sublabel_database_manager,                      MENU_ENUM_SUBLABEL_DATABASE_MANAGER)
default_sublabel_macro(action_bind_sublabel_cursor_manager,                        MENU_ENUM_SUBLABEL_CURSOR_MANAGER)
default_sublabel_macro(action_bind_sublabel_take_screenshot,                       MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT)
default_sublabel_macro(action_bind_sublabel_close_content,                         MENU_ENUM_SUBLABEL_CLOSE_CONTENT)
default_sublabel_macro(action_bind_sublabel_load_state,                            MENU_ENUM_SUBLABEL_LOAD_STATE)
default_sublabel_macro(action_bind_sublabel_save_state,                            MENU_ENUM_SUBLABEL_SAVE_STATE)
default_sublabel_macro(action_bind_sublabel_resume_content,                        MENU_ENUM_SUBLABEL_RESUME_CONTENT)
default_sublabel_macro(action_bind_sublabel_state_slot,                            MENU_ENUM_SUBLABEL_STATE_SLOT)
default_sublabel_macro(action_bind_sublabel_undo_load_state,                       MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE)
default_sublabel_macro(action_bind_sublabel_undo_save_state,                       MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE)
default_sublabel_macro(action_bind_sublabel_accounts_retro_achievements,           MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS)
default_sublabel_macro(action_bind_sublabel_accounts_list,                         MENU_ENUM_SUBLABEL_ACCOUNTS_LIST)
default_sublabel_macro(action_bind_sublabel_input_meta_rewind,                     MENU_ENUM_SUBLABEL_INPUT_META_REWIND)
default_sublabel_macro(action_bind_sublabel_input_meta_cheat_details,              MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS)
default_sublabel_macro(action_bind_sublabel_input_meta_cheat_search,               MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH)
default_sublabel_macro(action_bind_sublabel_restart_content,                       MENU_ENUM_SUBLABEL_RESTART_CONTENT)
default_sublabel_macro(action_bind_sublabel_save_current_config_override_core,     MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE)
default_sublabel_macro(action_bind_sublabel_save_current_config_override_content_dir,
                                                                                   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR)
default_sublabel_macro(action_bind_sublabel_save_current_config_override_game,     MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME)
default_sublabel_macro(action_bind_sublabel_core_cheat_options,                    MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS)
default_sublabel_macro(action_bind_sublabel_shader_options,                        MENU_ENUM_SUBLABEL_SHADER_OPTIONS)
default_sublabel_macro(action_bind_sublabel_core_input_remapping_options,          MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS)
default_sublabel_macro(action_bind_sublabel_core_options,                          MENU_ENUM_SUBLABEL_CORE_OPTIONS)
default_sublabel_macro(action_bind_sublabel_show_advanced_settings,                MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS)
default_sublabel_macro(action_bind_sublabel_threaded_data_runloop_enable,          MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE)
default_sublabel_macro(action_bind_sublabel_playlist_entry_rename,                 MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME)
default_sublabel_macro(action_bind_sublabel_playlist_entry_remove,                 MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE)
default_sublabel_macro(action_bind_sublabel_system_directory,                      MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_rgui_browser_directory,                MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_content_dir,                           MENU_ENUM_SUBLABEL_CONTENT_DIR)
default_sublabel_macro(action_bind_dynamic_wallpapers_directory,                   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY)
default_sublabel_macro(action_bind_thumbnails_directory,                           MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY)
default_sublabel_macro(action_bind_rgui_config_directory,                          MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_input_latency_frames,                  MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN)
default_sublabel_macro(action_bind_sublabel_input_latency_frames_range,            MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE)
default_sublabel_macro(action_bind_sublabel_disk_cycle_tray_status,                MENU_ENUM_SUBLABEL_DISK_CYCLE_TRAY_STATUS)
default_sublabel_macro(action_bind_sublabel_disk_image_append,                     MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND)
default_sublabel_macro(action_bind_sublabel_disk_index,                            MENU_ENUM_SUBLABEL_DISK_INDEX)
default_sublabel_macro(action_bind_sublabel_disk_options,                          MENU_ENUM_SUBLABEL_DISK_OPTIONS)
default_sublabel_macro(action_bind_sublabel_menu_throttle_framerate,               MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE)
default_sublabel_macro(action_bind_sublabel_xmb_layout,                            MENU_ENUM_SUBLABEL_XMB_LAYOUT)
default_sublabel_macro(action_bind_sublabel_xmb_icon_theme,                        MENU_ENUM_SUBLABEL_XMB_THEME)
default_sublabel_macro(action_bind_sublabel_xmb_shadows_enable,                    MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE)
default_sublabel_macro(action_bind_sublabel_xmb_vertical_thumbnails,               MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_menu_xmb_thumbnail_scale_factor,       MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR)
default_sublabel_macro(action_bind_sublabel_menu_color_theme,                      MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME)
default_sublabel_macro(action_bind_sublabel_ozone_menu_color_theme,                MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME)
default_sublabel_macro(action_bind_sublabel_ozone_collapse_sidebar,                MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR)
default_sublabel_macro(action_bind_sublabel_ozone_truncate_playlist_name,          MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME)
default_sublabel_macro(action_bind_sublabel_ozone_scroll_content_metadata,         MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA)
default_sublabel_macro(action_bind_sublabel_menu_use_preferred_system_color_theme, MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME)
default_sublabel_macro(action_bind_sublabel_menu_wallpaper_opacity,                MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY)
default_sublabel_macro(action_bind_sublabel_menu_framebuffer_opacity,              MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY)
default_sublabel_macro(action_bind_sublabel_menu_horizontal_animation,             MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION)
default_sublabel_macro(action_bind_sublabel_menu_xmb_animation_horizontal_higlight,             MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT)
default_sublabel_macro(action_bind_sublabel_menu_xmb_animation_move_up_down,             MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN)
default_sublabel_macro(action_bind_sublabel_menu_xmb_animation_opening_main_menu,             MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU)
default_sublabel_macro(action_bind_sublabel_menu_ribbon_enable,                    MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_font,                             MENU_ENUM_SUBLABEL_XMB_FONT)
default_sublabel_macro(action_bind_sublabel_settings_show_drivers,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS)
default_sublabel_macro(action_bind_sublabel_settings_show_video,                   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO)
default_sublabel_macro(action_bind_sublabel_settings_show_audio,                   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO)
default_sublabel_macro(action_bind_sublabel_settings_show_input,                   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT)
default_sublabel_macro(action_bind_sublabel_settings_show_latency,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY)
default_sublabel_macro(action_bind_sublabel_settings_show_core,                    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE)
default_sublabel_macro(action_bind_sublabel_settings_show_configuration,           MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION)
default_sublabel_macro(action_bind_sublabel_settings_show_saving,                  MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING)
default_sublabel_macro(action_bind_sublabel_settings_show_logging,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING)
default_sublabel_macro(action_bind_sublabel_settings_show_frame_throttle,          MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE)
default_sublabel_macro(action_bind_sublabel_settings_show_recording,               MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING)
default_sublabel_macro(action_bind_sublabel_settings_show_onscreen_display,        MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY)
default_sublabel_macro(action_bind_sublabel_settings_show_user_interface,          MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE)
default_sublabel_macro(action_bind_sublabel_settings_show_ai_service,              MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE)
default_sublabel_macro(action_bind_sublabel_settings_show_power_management,        MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT)
default_sublabel_macro(action_bind_sublabel_settings_show_achievements,            MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS)
default_sublabel_macro(action_bind_sublabel_settings_show_network,                 MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK)
default_sublabel_macro(action_bind_sublabel_settings_show_playlists,               MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS)
default_sublabel_macro(action_bind_sublabel_settings_show_user,                    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER)
default_sublabel_macro(action_bind_sublabel_settings_show_directory,               MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_take_screenshot,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_resume_content,        MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_restart_content,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_close_content,         MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_save_load_state,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_undo_save_load_state,  MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_add_to_favorites,      MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_start_recording,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_start_streaming,       MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_set_core_association,  MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_reset_core_association, MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_options,               MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_controls,              MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_cheats,                MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_shaders,               MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS)
default_sublabel_macro(action_bind_sublabel_content_show_overlays,                 MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS)
#ifdef HAVE_VIDEO_LAYOUT
default_sublabel_macro(action_bind_sublabel_content_show_video_layout,             MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT)
#endif
default_sublabel_macro(action_bind_sublabel_content_show_rewind,                   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND)
default_sublabel_macro(action_bind_sublabel_content_show_latency,                  MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_save_core_overrides,   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_save_game_overrides,   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_information,           MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION)
default_sublabel_macro(action_bind_sublabel_quick_menu_show_download_thumbnails,   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_menu_enable_kiosk_mode,                MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE)
default_sublabel_macro(action_bind_sublabel_menu_disable_kiosk_mode,               MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE)
default_sublabel_macro(action_bind_sublabel_menu_kiosk_mode_password,              MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD)
default_sublabel_macro(action_bind_sublabel_menu_favorites_tab,                    MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES)
default_sublabel_macro(action_bind_sublabel_menu_images_tab,                       MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES)
default_sublabel_macro(action_bind_sublabel_menu_show_load_core,                   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE)
default_sublabel_macro(action_bind_sublabel_menu_show_load_content,                MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT)
default_sublabel_macro(action_bind_sublabel_menu_show_load_disc,                   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC)
default_sublabel_macro(action_bind_sublabel_menu_show_dump_disc,                   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC)
default_sublabel_macro(action_bind_sublabel_menu_show_information,                 MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION)
default_sublabel_macro(action_bind_sublabel_menu_show_configurations,              MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS)
default_sublabel_macro(action_bind_sublabel_menu_show_help,                        MENU_ENUM_SUBLABEL_MENU_SHOW_HELP)
default_sublabel_macro(action_bind_sublabel_menu_show_quit_retroarch,              MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH)
#ifndef HAVE_LAKKA
default_sublabel_macro(action_bind_sublabel_menu_show_restart_retroarch,           MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH)
#endif
default_sublabel_macro(action_bind_sublabel_menu_show_reboot,                      MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT)
default_sublabel_macro(action_bind_sublabel_menu_show_shutdown,                    MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN)
default_sublabel_macro(action_bind_sublabel_menu_show_online_updater,              MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER)
default_sublabel_macro(action_bind_sublabel_menu_show_core_updater,                MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER)
default_sublabel_macro(action_bind_sublabel_menu_show_legacy_thumbnail_updater,    MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER)
default_sublabel_macro(action_bind_sublabel_menu_music_tab,                        MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC)
default_sublabel_macro(action_bind_sublabel_menu_video_tab,                        MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO)
default_sublabel_macro(action_bind_sublabel_menu_netplay_tab,                      MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY)
default_sublabel_macro(action_bind_sublabel_menu_settings_tab,                     MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS)
default_sublabel_macro(action_bind_sublabel_menu_settings_tab_enable_password,     MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD)
default_sublabel_macro(action_bind_sublabel_menu_history_tab,                      MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY)
default_sublabel_macro(action_bind_sublabel_menu_import_content_tab,               MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD)
default_sublabel_macro(action_bind_sublabel_menu_playlist_tabs,                    MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS)
default_sublabel_macro(action_bind_sublabel_main_menu_enable_settings,             MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS)
default_sublabel_macro(action_bind_sublabel_rgui_show_start_screen,                MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN)
default_sublabel_macro(action_bind_sublabel_menu_header_opacity,                   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY)
default_sublabel_macro(action_bind_sublabel_menu_footer_opacity,                   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY)
default_sublabel_macro(action_bind_sublabel_dpi_override_enable,                   MENU_ENUM_SUBLABEL_DPI_OVERRIDE_ENABLE)
default_sublabel_macro(action_bind_sublabel_dpi_override_value,                    MENU_ENUM_SUBLABEL_DPI_OVERRIDE_VALUE)
default_sublabel_macro(action_bind_sublabel_core_assets_directory,                 MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_input_remapping_directory,             MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_core_directory,                        MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH)
default_sublabel_macro(action_bind_sublabel_core_info_directory,                   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH)
default_sublabel_macro(action_bind_sublabel_joypad_autoconfig_directory,           MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR)
default_sublabel_macro(action_bind_sublabel_playlists_directory,                   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_runtime_log_directory,                 MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_cache_directory,                       MENU_ENUM_SUBLABEL_CACHE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_database_directory,                    MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_cursor_directory,                      MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_assets_directory,                      MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_savefile_directory,                    MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_savestate_directory,                   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_screenshot_directory,                  MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_overlay_directory,                     MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY)
#ifdef HAVE_VIDEO_LAYOUT
default_sublabel_macro(action_bind_sublabel_video_layout_directory,                MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY)
#endif
default_sublabel_macro(action_bind_sublabel_cheatfile_directory,                   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH)
default_sublabel_macro(action_bind_sublabel_audio_filter_directory,                MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR)
default_sublabel_macro(action_bind_sublabel_video_filter_directory,                MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR)
default_sublabel_macro(action_bind_sublabel_video_shader_directory,                MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR)
default_sublabel_macro(action_bind_sublabel_recording_output_directory,            MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_recording_config_directory,            MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_video_font_path,                       MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH)
default_sublabel_macro(action_bind_sublabel_shader_apply_changes,                  MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES)
default_sublabel_macro(action_bind_sublabel_shader_watch_for_changes,              MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES)
default_sublabel_macro(action_bind_sublabel_shader_num_passes,                     MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES)
default_sublabel_macro(action_bind_sublabel_shader_preset,                         MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET)
default_sublabel_macro(action_bind_sublabel_shader_preset_save,                    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE)
default_sublabel_macro(action_bind_sublabel_shader_preset_remove,                    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_as,                 MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_global,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_core,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_parent,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_game,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME)
default_sublabel_macro(action_bind_sublabel_shader_preset_remove_global,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL)
default_sublabel_macro(action_bind_sublabel_shader_preset_remove_core,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE)
default_sublabel_macro(action_bind_sublabel_shader_preset_remove_parent,             MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT)
default_sublabel_macro(action_bind_sublabel_shader_preset_remove_game,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME)
default_sublabel_macro(action_bind_sublabel_shader_parameters,                     MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS)
default_sublabel_macro(action_bind_sublabel_shader_preset_parameters,              MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS)
default_sublabel_macro(action_bind_sublabel_cheat_apply_changes,                   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES)
default_sublabel_macro(action_bind_sublabel_cheat_num_passes,                      MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES)
default_sublabel_macro(action_bind_sublabel_cheat_file_load,                       MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD)
default_sublabel_macro(action_bind_sublabel_cheat_file_load_append,                MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND)
default_sublabel_macro(action_bind_sublabel_cheat_file_save_as,                    MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS)
default_sublabel_macro(action_bind_sublabel_quick_menu,                            MENU_ENUM_SUBLABEL_CONTENT_SETTINGS)
default_sublabel_macro(action_bind_sublabel_core_information,                      MENU_ENUM_SUBLABEL_CORE_INFORMATION)
default_sublabel_macro(action_bind_sublabel_disc_information,                      MENU_ENUM_SUBLABEL_DISC_INFORMATION)
default_sublabel_macro(action_bind_sublabel_video_aspect_ratio,                    MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_height,          MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_width,           MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_x,               MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_y,               MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y)
default_sublabel_macro(action_bind_sublabel_netplay_use_mitm_server,               MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER)
default_sublabel_macro(action_bind_sublabel_netplay_mitm_server,                   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER)
default_sublabel_macro(action_bind_sublabel_core_delete,                           MENU_ENUM_SUBLABEL_CORE_DELETE)
default_sublabel_macro(action_bind_sublabel_pause_hardcode_mode,                   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE)
default_sublabel_macro(action_bind_sublabel_resume_hardcode_mode,                  MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME)
default_sublabel_macro(action_bind_sublabel_midi_input,                            MENU_ENUM_SUBLABEL_MIDI_INPUT)
default_sublabel_macro(action_bind_sublabel_midi_output,                           MENU_ENUM_SUBLABEL_MIDI_OUTPUT)
default_sublabel_macro(action_bind_sublabel_midi_volume,                           MENU_ENUM_SUBLABEL_MIDI_VOLUME)
default_sublabel_macro(action_bind_sublabel_onscreen_overlay_settings_list,        MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS)
#ifdef HAVE_VIDEO_LAYOUT
default_sublabel_macro(action_bind_sublabel_onscreen_video_layout_settings_list,   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS)
#endif
default_sublabel_macro(action_bind_sublabel_onscreen_notifications_settings_list,  MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS)
#ifdef HAVE_QT
default_sublabel_macro(action_bind_sublabel_show_wimp,                             MENU_ENUM_SUBLABEL_SHOW_WIMP)
#endif
default_sublabel_macro(action_bind_sublabel_discord_allow,                         MENU_ENUM_SUBLABEL_DISCORD_ALLOW)

#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
default_sublabel_macro(action_bind_sublabel_switch_cpu_profile,             MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE)
#endif

#ifdef HAVE_LAKKA_SWITCH
default_sublabel_macro(action_bind_sublabel_switch_gpu_profile,             MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE)
default_sublabel_macro(action_bind_sublabel_switch_backlight_control,       MENU_ENUM_SUBLABEL_SWITCH_BACKLIGHT_CONTROL)
#endif

#if defined(_3DS)
default_sublabel_macro(action_bind_sublabel_video_3ds_lcd_bottom,           MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM)
default_sublabel_macro(action_bind_sublabel_video_3ds_display_mode,         MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE)
#endif

#if defined(GEKKO)
default_sublabel_macro(action_bind_sublabel_video_overscan_correction_top,    MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP)
default_sublabel_macro(action_bind_sublabel_video_overscan_correction_bottom, MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM)
#endif

default_sublabel_macro(action_bind_sublabel_playlist_show_sublabels,                       MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS)
default_sublabel_macro(action_bind_sublabel_menu_rgui_border_filler_enable,                MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_rgui_border_filler_thickness_enable,      MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_rgui_background_filler_thickness_enable,  MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_linear_filter,                            MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER)
default_sublabel_macro(action_bind_sublabel_menu_rgui_aspect_ratio_lock,                   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK)
default_sublabel_macro(action_bind_sublabel_rgui_menu_color_theme,                         MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME)
default_sublabel_macro(action_bind_sublabel_rgui_menu_theme_preset,                        MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET)
default_sublabel_macro(action_bind_sublabel_menu_rgui_shadows,                             MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS)
default_sublabel_macro(action_bind_sublabel_menu_rgui_particle_effect,                     MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT)
default_sublabel_macro(action_bind_sublabel_menu_rgui_particle_effect_speed,               MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED)
default_sublabel_macro(action_bind_sublabel_menu_rgui_inline_thumbnails,                   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_menu_rgui_swap_thumbnails,                     MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS)
default_sublabel_macro(action_bind_sublabel_menu_rgui_thumbnail_downscaler,                MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER)
default_sublabel_macro(action_bind_sublabel_menu_rgui_thumbnail_delay,                     MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY)
default_sublabel_macro(action_bind_sublabel_content_runtime_log,                           MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG)
default_sublabel_macro(action_bind_sublabel_content_runtime_log_aggregate,                 MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE)
default_sublabel_macro(action_bind_sublabel_scan_without_core_match,                 MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH)
default_sublabel_macro(action_bind_sublabel_playlist_sublabel_runtime_type,                MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE)
default_sublabel_macro(action_bind_sublabel_playlist_sublabel_last_played_style,           MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE)
default_sublabel_macro(action_bind_sublabel_menu_rgui_internal_upscale_level,              MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL)
default_sublabel_macro(action_bind_sublabel_menu_rgui_aspect_ratio,                        MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO)
default_sublabel_macro(action_bind_sublabel_menu_ticker_type,                              MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE)
default_sublabel_macro(action_bind_sublabel_menu_ticker_speed,                             MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED)
default_sublabel_macro(action_bind_sublabel_menu_ticker_smooth,                            MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH)
default_sublabel_macro(action_bind_sublabel_playlist_show_inline_core_name,                MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME)
default_sublabel_macro(action_bind_sublabel_playlist_sort_alphabetical,                    MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL)
default_sublabel_macro(action_bind_sublabel_playlist_fuzzy_archive_match,                  MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH)
default_sublabel_macro(action_bind_sublabel_menu_rgui_full_width_layout,                   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT)
default_sublabel_macro(action_bind_sublabel_menu_rgui_extended_ascii,                      MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII)
default_sublabel_macro(action_bind_sublabel_thumbnails_updater_list,                       MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST)
default_sublabel_macro(action_bind_sublabel_pl_thumbnails_updater_list,                    MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST)
default_sublabel_macro(action_bind_sublabel_help_send_debug_info,                          MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO)
default_sublabel_macro(action_bind_sublabel_rdb_entry_detail,                              MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL)

static int action_bind_sublabel_systeminfo_controller_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   char tmp[4096];
   unsigned controller;

   for(controller = 0; controller < MAX_USERS; controller++)
   {
      if (input_is_autoconfigured(controller))
      {
            snprintf(tmp, sizeof(tmp), "Port #%d device name: %s (#%d)",
               controller,
               input_config_get_device_name(controller),
               input_autoconfigure_get_device_name_index(controller));

            if (string_is_equal(path, tmp))
               break;
      }
   }
   snprintf(tmp, sizeof(tmp), "Device display name: %s\nDevice config name: %s\nDevice identifiers: %d/%d",
      input_config_get_device_display_name(controller) ? input_config_get_device_display_name(controller) : "N/A",
      input_config_get_device_display_name(controller) ? input_config_get_device_config_name(controller) : "N/A",
      input_config_get_vid(controller), input_config_get_pid(controller));
   strlcpy(s, tmp, len);

   return 0;
}

static int action_bind_sublabel_cheevos_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
#ifdef HAVE_CHEEVOS
   rcheevos_ctx_desc_t desc_info;
   unsigned new_id;
   char fetched_sublabel[MENU_SUBLABEL_MAX_LENGTH];

   fetched_sublabel[0] = '\0';

   new_id              = type - MENU_SETTINGS_CHEEVOS_START;
   desc_info.idx       = new_id;
   desc_info.s         = fetched_sublabel;
   desc_info.len       = len;

   rcheevos_get_description((rcheevos_ctx_desc_t*) &desc_info);

   strlcpy(s, desc_info.s, len);
#endif
   return 0;
}

static int action_bind_sublabel_subsystem_add(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   rarch_system_info_t *system                  = runloop_get_system_info();
   const struct retro_subsystem_info *subsystem;

   /* Core fully loaded, use the subsystem data */
   if (system->subsystem.data)
      subsystem = system->subsystem.data + (type - MENU_SETTINGS_SUBSYSTEM_ADD);
   /* Core not loaded completely, use the data we peeked on load core */
   else
      subsystem = subsystem_data + (type - MENU_SETTINGS_SUBSYSTEM_ADD);

   if (subsystem && subsystem_current_count > 0)
   {
      if (content_get_subsystem_rom_id() < subsystem->num_roms)
         snprintf(s, len, " Current Content: %s",
            content_get_subsystem() == type - MENU_SETTINGS_SUBSYSTEM_ADD
            ? subsystem->roms[content_get_subsystem_rom_id()].desc
            : subsystem->roms[0].desc);
   }

   return 0;
}

static int action_bind_sublabel_subsystem_load(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   unsigned j = 0;
   char buf[4096];

   buf[0] = '\0';

   for (j = 0; j < content_get_subsystem_rom_id(); j++)
   {
      strlcat(buf, "   ", sizeof(buf));
      strlcat(buf, path_basename(content_get_subsystem_rom(j)), sizeof(buf));
      if (j != content_get_subsystem_rom_id() - 1)
         strlcat(buf, "\n", sizeof(buf));
   }

   if (!string_is_empty(buf))
      strlcpy(s, buf, len);

   return 0;
}

static int action_bind_sublabel_remap_kbd_sublabel(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   unsigned user_idx = (type - MENU_SETTINGS_INPUT_DESC_KBD_BEGIN) / RARCH_FIRST_CUSTOM_BIND;

   snprintf(s, len, "%s #%d: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER),
         user_idx + 1,
         input_config_get_device_display_name(user_idx) ?
         input_config_get_device_display_name(user_idx) :
         (input_config_get_device_name(user_idx) ?
            input_config_get_device_name(user_idx) :
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)));
   return 0;
}

#ifdef HAVE_AUDIOMIXER
static int action_bind_sublabel_audio_mixer_stream(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   char msg[64];
   unsigned              offset = (type - MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN);
   audio_mixer_stream_t *stream = audio_driver_mixer_get_stream(offset);

   if (!stream)
      return -1;

   switch (stream->state)
   {
      case AUDIO_STREAM_STATE_NONE:
         strlcpy(msg,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_STOPPED:
         strlcpy(msg, "Stopped", sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_PLAYING:
         strlcpy(msg, "Playing", sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
         strlcpy(msg, "Playing (Looped)", sizeof(msg));
         break;
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         strlcpy(msg, "Playing (Sequential)", sizeof(msg));
         break;
   }

   snprintf(s, len, "State : %s | %s: %.2f dB", msg,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME),
         stream->volume);
   return 0;
}
#endif

static int action_bind_sublabel_remap_sublabel(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   unsigned offset = (type - MENU_SETTINGS_INPUT_DESC_BEGIN)
      / (RARCH_FIRST_CUSTOM_BIND + 8);

   snprintf(s, len, "%s #%d: %s",
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER),
         offset + 1,
         input_config_get_device_display_name(offset) ?
         input_config_get_device_display_name(offset) :
         (input_config_get_device_name(offset) ?
          input_config_get_device_name(offset) :
          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE)));
   return 0;
}

static int action_bind_sublabel_cheat_desc(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   unsigned offset = (type - MENU_SETTINGS_CHEAT_BEGIN);

   if (cheat_manager_state.cheats)
   {
      if (cheat_manager_state.cheats[offset].handler == CHEAT_HANDLER_TYPE_EMU)
         strlcpy(s, "Emulator-Handled", len);
      else
         strlcpy(s, "RetroArch-Handled", len);
   }

   return 0;
}

#ifdef HAVE_NETWORKING
static int action_bind_sublabel_netplay_room(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   uint32_t gamecrc       = 0;
   const char *ra_version = NULL;
   const char *corename   = NULL;
   const char *gamename   = NULL;
   const char *core_ver   = NULL;
   const char *frontend   = NULL;
   const char *na         = NULL;
   const char *subsystem  = NULL;

   /* This offset may cause issues if any entries are added to this menu */
   unsigned offset        = i - 3;

   if (i < 1 || offset > (unsigned)netplay_room_count)
      return -1;

   ra_version = netplay_room_list[offset].retroarch_version;
   corename   = netplay_room_list[offset].corename;
   gamename   = netplay_room_list[offset].gamename;
   core_ver   = netplay_room_list[offset].coreversion;
   gamecrc    = netplay_room_list[offset].gamecrc;
   frontend   = netplay_room_list[offset].frontend;
   subsystem  = netplay_room_list[offset].subsystem_name;
   na         = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);

   if (string_is_empty(subsystem) || string_is_equal(subsystem, "N/A"))
   {
      snprintf(s, len,
         "RetroArch: %s (%s)\nCore: %s (%s)\nGame: %s (%08x)",
         string_is_empty(ra_version)    ? na : ra_version,
         string_is_empty(frontend)      ? na : frontend,
         corename, core_ver,
         !string_is_equal(gamename, na) ? gamename : na,
         gamecrc);
   }
   else
   {
      if (strstr(gamename, "|"))
      {
         char buf[4096];
         unsigned i               = 0;
         struct string_list *list = string_split(gamename, "|");

         buf[0] = '\0';
         for (i = 0; i < list->size; i++)
         {
            strlcat(buf, "   ", sizeof(buf));
            strlcat(buf, list->elems[i].data, sizeof(buf));
            /* Never terminate a UI string with a newline */
            if (i != list->size - 1)
               strlcat(buf, "\n", sizeof(buf));
         }
         snprintf(s, len,
            "RetroArch: %s (%s)\nCore: %s (%s)\nSubsystem: %s\nGames:\n%s",
            string_is_empty(ra_version)    ? na : ra_version,
            string_is_empty(frontend)      ? na : frontend,
            corename, core_ver, subsystem,
            !string_is_equal(gamename, na) ? buf : na
            );
         string_list_free(list);
      }
      else
      {
         snprintf(s, len,
            "RetroArch: %s (%s)\nCore: %s (%s)\nSubsystem: %s\nGame: %s (%08x)",
            string_is_empty(ra_version)    ? na : ra_version,
            string_is_empty(frontend)      ? na : frontend,
            corename, core_ver, subsystem,
            !string_is_equal(gamename, na) ? gamename : na,
            gamecrc);
      }
   }
   return 0;
}
#endif

static int action_bind_sublabel_playlist_entry(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   settings_t *settings               = config_get_ptr();
   playlist_t *playlist               = NULL;
   const struct playlist_entry *entry = NULL;
   
   if (!settings->bools.playlist_show_sublabels || string_is_equal(settings->arrays.menu_driver, "ozone"))
      return 0;

   /* Get current playlist */
   playlist = playlist_get_cached();

   if (!playlist)
      return 0;

   if (i >= playlist_get_size(playlist))
      return 0;
   
   /* Read playlist entry */
   playlist_get_index(playlist, i, &entry);
   
   /* Only add sublabel if a core is currently assigned */
   if (string_is_empty(entry->core_name) || string_is_equal(entry->core_name, "DETECT"))
      return 0;
   
   /* Add core name */
   strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE), len);
   strlcat(s, " ", len);
   strlcat(s, entry->core_name, len);
   
   /* Get runtime info *if* required runtime log is enabled
    * *and* this is a valid playlist type */
   if (((settings->uints.playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_PER_CORE) &&
         !settings->bools.content_runtime_log) ||
       ((settings->uints.playlist_sublabel_runtime_type == PLAYLIST_RUNTIME_AGGREGATE) &&
         !settings->bools.content_runtime_log_aggregate))
      return 0;
   
   /* Note: This looks heavy, but each string_is_equal() call will
    * return almost immediately */
   if (!string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY)) &&
       !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB)) &&
       !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_FAVORITES_LIST)) &&
       !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)) &&
       !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_PLAYLIST_LIST)) &&
       !string_is_equal(label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)))
      return 0;
   
   /* Check whether runtime info should be loaded from log file */
   if (entry->runtime_status == PLAYLIST_RUNTIME_UNKNOWN)
      runtime_update_playlist(playlist, i);
   
   /* Check whether runtime info is valid */
   if (entry->runtime_status == PLAYLIST_RUNTIME_VALID)
   {
      int n = 0;
      char tmp[64];
      
      tmp[0  ] = '\n';
      tmp[1  ] = '\0';

      n = strlcat(tmp, entry->runtime_str, sizeof(tmp));

      tmp[n  ] = '\n';
      tmp[n+1] = '\0';
      
      /* Runtime/last played strings are now cached in the
       * playlist, so we can add both in one go */
      n = strlcat(tmp, entry->last_played_str, sizeof(tmp));
      
      if ((n < 0) || (n >= 64))
         n = 0; /* Silence GCC warnings... */
      
      if (!string_is_empty(tmp))
         strlcat(s, tmp, len);
   }
   
   return 0;
}

static int action_bind_sublabel_core_option(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   core_option_manager_t *opt = NULL;
   const char *info           = NULL;

   if (!rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, &opt))
      return 0;

   info = core_option_manager_get_info(opt, type - MENU_SETTINGS_CORE_OPTION_START);

   if (!string_is_empty(info))
      strlcpy(s, info, len);

   return 0;
}

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

   if (type >= MENU_SETTINGS_INPUT_DESC_KBD_BEGIN
      && type <= MENU_SETTINGS_INPUT_DESC_KBD_END)
   {
      BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_remap_kbd_sublabel);
   }
#ifdef HAVE_AUDIOMIXER
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_END)
   {
      BIND_ACTION_SUBLABEL(cbs,
         menu_action_sublabel_setting_audio_mixer_stream_play);
      return 0;
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_LOOPED_END)
   {
      BIND_ACTION_SUBLABEL(cbs,
         menu_action_sublabel_setting_audio_mixer_stream_play_looped);
      return 0;
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_PLAY_SEQUENTIAL_END)
   {
      BIND_ACTION_SUBLABEL(cbs,
         menu_action_sublabel_setting_audio_mixer_stream_play_sequential);
      return 0;
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_REMOVE_END)
   {
      BIND_ACTION_SUBLABEL(cbs,
         menu_action_sublabel_setting_audio_mixer_stream_remove);
      return 0;
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_STOP_END)
   {
      BIND_ACTION_SUBLABEL(cbs,
         menu_action_sublabel_setting_audio_mixer_stream_stop);
      return 0;
   }
   else if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_BEGIN
         && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_ACTIONS_VOLUME_END)
   {
      BIND_ACTION_SUBLABEL(cbs,
         menu_action_sublabel_setting_audio_mixer_stream_volume);
      return 0;
   }
#endif

   if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
      && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_remap_sublabel);
   }

   if (type >= MENU_SETTINGS_CHEAT_BEGIN
      && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_desc);
   }

#ifdef HAVE_AUDIOMIXER
   if (type >= MENU_SETTINGS_AUDIO_MIXER_STREAM_BEGIN
      && type <= MENU_SETTINGS_AUDIO_MIXER_STREAM_END)
   {
      BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mixer_stream);
      return 0;
   }
#endif

   if ((type >= MENU_SETTINGS_CORE_OPTION_START) &&
       (type < MENU_SETTINGS_CHEEVOS_START))
   {
      BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_option);
      return 0;
   }

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      settings_t *settings; /* config_get_ptr is called only when needed */

      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            BIND_ACTION_SUBLABEL(cbs, menu_action_sublabel_file_browser_core);
            break;
         case MENU_ENUM_LABEL_ADD_TO_MIXER:
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, menu_action_sublabel_setting_audio_mixer_add_to_mixer);
#endif
            break;
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_PLAY:
         case MENU_ENUM_LABEL_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, menu_action_sublabel_setting_audio_mixer_add_to_mixer_and_play);
#endif
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_START_STREAMING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_start_streaming);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_START_RECORDING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_start_recording);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_STOP_STREAMING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_stop_streaming);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_STOP_RECORDING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_stop_recording);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_OVERRIDE_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_override_options);
            break;
         case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_crt_switchres);
            break;
         case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_crt_switchres_super);
            break;
         case MENU_ENUM_LABEL_CRT_SWITCH_X_AXIS_CENTERING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_crt_switchres_x_axis_centering);
            break;
         case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_crt_switchres_use_custom_refresh_rate);
            break;
         case MENU_ENUM_LABEL_AUDIO_RESAMPLER_QUALITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_resampler_quality);
            break;
         case MENU_ENUM_LABEL_MATERIALUI_ICONS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_materialui_icons_enable);
            break;
         case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_viewport_custom_height);
            break;
         case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_viewport_custom_width);
            break;
         case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_viewport_custom_x);
            break;
         case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_viewport_custom_y);
            break;
         case MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_aspect_ratio);
            break;
         case MENU_ENUM_LABEL_CORE_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_information);
            break;
         case MENU_ENUM_LABEL_DISC_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_disc_information);
            break;
         case MENU_ENUM_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_save_as);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_load);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD_APPEND:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_load_append);
            break;
         case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_apply_changes);
            break;
         case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_num_passes);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_parameters);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_parameters);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_remove);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_as);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_global);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_core);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_PARENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_parent);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_game);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_remove_global);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_remove_core);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_remove_parent);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_REMOVE_GAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_remove_game);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_num_passes);
            break;
         case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_apply_changes);
            break;
         case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_watch_for_changes);
            break;
         case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_font_path);
            break;
         case MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_recording_config_directory);
            break;
         case MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_recording_output_directory);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_DIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_shader_directory);
            break;
         case MENU_ENUM_LABEL_AUDIO_FILTER_DIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_filter_directory);
            break;
         case MENU_ENUM_LABEL_VIDEO_FILTER_DIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_filter_directory);
            break;
         case MENU_ENUM_LABEL_CHEAT_DATABASE_PATH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheatfile_directory);
            break;
         case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_overlay_directory);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_layout_directory);
            break;
#endif
         case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_screenshot_directory);
            break;
         case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_savefile_directory);
            break;
         case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_savestate_directory);
            break;
         case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_assets_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_database_directory);
            break;
         case MENU_ENUM_LABEL_CURSOR_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cursor_directory);
            break;
         case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cache_directory);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlists_directory);
            break;
         case MENU_ENUM_LABEL_RUNTIME_LOG_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_runtime_log_directory);
            break;
         case MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_joypad_autoconfig_directory);
            break;
         case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_info_directory);
            break;
         case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_directory);
            break;
         case MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_assets_directory);
            break;
         case MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_remapping_directory);
            break;
         case MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_dpi_override_enable);
            break;
         case MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_dpi_override_value);
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_footer_opacity);
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_header_opacity);
            break;
         case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rgui_show_start_screen);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_ADD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_import_content_tab);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_PLAYLISTS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_playlist_tabs);
            break;
         case MENU_ENUM_LABEL_XMB_MAIN_MENU_ENABLE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_main_menu_enable_settings);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_HISTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_history_tab);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_settings_tab);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_SETTINGS_PASSWORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_settings_tab_enable_password);
            break;
         case MENU_ENUM_LABEL_GOTO_IMAGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_goto_images);
            break;
         case MENU_ENUM_LABEL_GOTO_MUSIC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_goto_music);
            break;
         case MENU_ENUM_LABEL_GOTO_VIDEO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_goto_video);
            break;
         case MENU_ENUM_LABEL_GOTO_FAVORITES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_goto_favorites);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_DRIVERS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_drivers);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_VIDEO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_video);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_AUDIO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_audio);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_INPUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_input);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_LATENCY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_latency);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_core);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_CONFIGURATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_configuration);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_SAVING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_saving);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_LOGGING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_logging)
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_FRAME_THROTTLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_frame_throttle);
            break;
         case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_frame_time_counter_reset_after_fastforwarding);
            break;
         case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_frame_time_counter_reset_after_load_state);
            break;
         case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_frame_time_counter_reset_after_save_state);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_RECORDING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_recording);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_onscreen_display);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_USER_INTERFACE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_user_interface);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_AI_SERVICE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_ai_service);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_POWER_MANAGEMENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_power_management);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_ACHIEVEMENTS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_achievements);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_NETWORK:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_network);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_PLAYLISTS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_playlists);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_USER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_user);
            break;
         case MENU_ENUM_LABEL_SETTINGS_SHOW_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_show_directory);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESUME_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_resume_content);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESTART_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_restart_content);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CLOSE_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_close_content);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_take_screenshot);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_save_load_state);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_undo_save_load_state);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_add_to_favorites);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_RECORDING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_start_recording);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_START_STREAMING:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_start_streaming);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_set_core_association);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_reset_core_association);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_options);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CONTROLS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_controls);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_CHEATS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_cheats);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_LATENCY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_show_latency);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_REWIND:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_show_rewind);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_OVERLAYS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_show_overlays);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO_LAYOUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_show_video_layout);
            break;
#endif
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SHADERS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_shaders);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_save_core_overrides);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_save_game_overrides);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_information);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_show_download_thumbnails);
            break;
         case MENU_ENUM_LABEL_MENU_ENABLE_KIOSK_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_enable_kiosk_mode);
            break;
         case MENU_ENUM_LABEL_MENU_DISABLE_KIOSK_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_disable_kiosk_mode);
            break;
         case MENU_ENUM_LABEL_MENU_KIOSK_MODE_PASSWORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_kiosk_mode_password);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_FAVORITES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_favorites_tab);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_IMAGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_images_tab);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_MUSIC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_music_tab);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_LOAD_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_load_core);
            break;
         case MENU_ENUM_LABEL_LOAD_DISC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_load_disc);
            break;
         case MENU_ENUM_LABEL_DUMP_DISC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_dump_disc);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_LOAD_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_load_content);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_LOAD_DISC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_load_disc);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_DUMP_DISC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_dump_disc);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_information);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_CONFIGURATIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_configurations);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_HELP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_help);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_QUIT_RETROARCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_quit_retroarch);
            break;
#ifndef HAVE_LAKKA
         case MENU_ENUM_LABEL_MENU_SHOW_RESTART_RETROARCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_restart_retroarch);
            break;
#endif
         case MENU_ENUM_LABEL_MENU_SHOW_REBOOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_reboot);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_SHUTDOWN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_shutdown);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_online_updater);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_core_updater);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_legacy_thumbnail_updater);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_NETPLAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_netplay_tab);
            break;
         case MENU_ENUM_LABEL_CONTENT_SHOW_VIDEO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_video_tab);
            break;
         case MENU_ENUM_LABEL_XMB_FONT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_font);
            break;
         case MENU_ENUM_LABEL_XMB_RIBBON_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_ribbon_enable);
            break;
         case MENU_ENUM_LABEL_MENU_FRAMEBUFFER_OPACITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_framebuffer_opacity);
            break;
         case MENU_ENUM_LABEL_MENU_HORIZONTAL_ANIMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_horizontal_animation);
            break;
         case MENU_ENUM_LABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_animation_horizontal_higlight);
            break;
         case MENU_ENUM_LABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_animation_move_up_down);
            break;
         case MENU_ENUM_LABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_animation_opening_main_menu);
            break;
         case MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_wallpaper_opacity);
            break;
         case MENU_ENUM_LABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_use_preferred_system_color_theme);
            break;
         case MENU_ENUM_LABEL_OZONE_MENU_COLOR_THEME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_menu_color_theme);
            break;
         case MENU_ENUM_LABEL_OZONE_COLLAPSE_SIDEBAR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_collapse_sidebar);
            break;
         case MENU_ENUM_LABEL_OZONE_TRUNCATE_PLAYLIST_NAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_truncate_playlist_name);
            break;
         case MENU_ENUM_LABEL_OZONE_SCROLL_CONTENT_METADATA:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ozone_scroll_content_metadata);
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME:
         case MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_color_theme);
            break;
         case MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_shadows_enable);
            break;
         case MENU_ENUM_LABEL_XMB_VERTICAL_THUMBNAILS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_vertical_thumbnails);
            break;
         case MENU_ENUM_LABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_xmb_thumbnail_scale_factor);
            break;
         case MENU_ENUM_LABEL_XMB_LAYOUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_layout);
            break;
         case MENU_ENUM_LABEL_XMB_THEME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_icon_theme);
            break;
         case MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_throttle_framerate);
            break;
         case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_disk_image_append);
            break;
         case MENU_ENUM_LABEL_SUBSYSTEM_ADD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_subsystem_add);
            break;
         case MENU_ENUM_LABEL_SUBSYSTEM_LOAD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_subsystem_load);
            break;
         case MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_disk_cycle_tray_status);
            break;
         case MENU_ENUM_LABEL_DISK_INDEX:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_disk_index);
            break;
         case MENU_ENUM_LABEL_DISK_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_disk_options);
            break;
         case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_latency_frames_range);
            break;
         case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_latency_frames);
            break;
         case MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_rgui_config_directory);
            break;
         case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_thumbnails_directory);
            break;
         case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_dynamic_wallpapers_directory);
            break;
         case MENU_ENUM_LABEL_CONTENT_DIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_dir);
            break;
         case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rgui_browser_directory);
            break;
         case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_system_directory);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_ENTRY_RENAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_entry_rename);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_ENTRY_REMOVE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_entry_remove);
            break;
         case MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_threaded_data_runloop_enable);
            break;
         case MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_show_advanced_settings);
            break;
         case MENU_ENUM_LABEL_CORE_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_options);
            break;
         case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_input_remapping_options);
            break;
         case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_cheat_options);
            break;
         case MENU_ENUM_LABEL_SHADER_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_options);
            break;
         case MENU_ENUM_LABEL_RESET_TO_DEFAULT_CONFIG:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_reset_to_default_config);
            break;
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_save_current_config_override_game);
            break;
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_save_current_config_override_core);
            break;
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_save_current_config_override_content_dir);
            break;
         case MENU_ENUM_LABEL_RESTART_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_restart_content);
            break;
         case MENU_ENUM_LABEL_REWIND_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_rewind);
            break;
         case MENU_ENUM_LABEL_CHEAT_DETAILS_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_cheat_details);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_cheat_search);
            break;
         case MENU_ENUM_LABEL_ACCOUNTS_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_accounts_list);
            break;
         case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_accounts_retro_achievements);
            break;
         case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_undo_save_state);
            break;
         case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_undo_load_state);
            break;
         case MENU_ENUM_LABEL_STATE_SLOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_state_slot);
            break;
         case MENU_ENUM_LABEL_RESUME:
         case MENU_ENUM_LABEL_RESUME_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_resume_content);
            break;
         case MENU_ENUM_LABEL_SAVE_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_save_state);
            break;
         case MENU_ENUM_LABEL_LOAD_STATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_load_state);
            break;
         case MENU_ENUM_LABEL_CLOSE_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_close_content);
            break;
         case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_take_screenshot);
            break;
         case MENU_ENUM_LABEL_CURSOR_MANAGER:
         case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cursor_manager);
            break;
         case MENU_ENUM_LABEL_DATABASE_MANAGER:
         case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_database_manager);
            break;
         case MENU_ENUM_LABEL_CORE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_enable);
            break;
         case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_game_specific_options);
            break;
         case MENU_ENUM_LABEL_GLOBAL_CORE_OPTIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_global_core_options);
            break;
         case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_auto_overrides_enable);
            break;
         case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_auto_remaps_enable);
            break;
         case MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_filebrowser_settings);
            break;
         case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_filebrowser_open_uwp_permissions);
            break;
         case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_filebrowser_open_picker);
            break;
         case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_add_to_favorites);
            break;
         case MENU_ENUM_LABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_download_pl_entry_thumbnails);
            break;
         case MENU_ENUM_LABEL_RUN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_run);
            break;
         case MENU_ENUM_LABEL_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_information);
            break;
         case MENU_ENUM_LABEL_RENAME_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rename_entry);
            break;
         case MENU_ENUM_LABEL_DELETE_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_delete_entry);
            break;
         case MENU_ENUM_LABEL_NETPLAY_REFRESH_ROOMS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_refresh_rooms);
            break;
         case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_updater_auto_extract_archive);
            break;
         case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_updater_buildbot_url);
            break;
         case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_updater_buildbot_assets_url);
            break;
         case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_sort_savefiles_enable);
            break;
         case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_sort_savestates_enable);
            break;
         case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_swap_interval);
            break;
         case MENU_ENUM_LABEL_SCAN_FILE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_scan_file);
            break;
         case MENU_ENUM_LABEL_SCAN_DIRECTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_scan_directory);
            break;
         case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_disconnect);
            break;
         case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_enable_client);
            break;
         case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_enable_host);
            break;
         case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_navigation_wraparound);
            break;
         case MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_battery_level_enable);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_SUBLABELS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_sublabels);
            break;
         case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_timedate_enable);
            break;
         case MENU_ENUM_LABEL_TIMEDATE_STYLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_timedate_style);
            break;
         case MENU_ENUM_LABEL_THUMBNAILS:
            settings = config_get_ptr();
            if (string_is_equal(settings->arrays.menu_driver, "rgui"))
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails_rgui);
            }
            else
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails);
            }
            break;
         case MENU_ENUM_LABEL_LEFT_THUMBNAILS:
            settings = config_get_ptr();
            if (string_is_equal(settings->arrays.menu_driver, "rgui"))
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_rgui);
            }
            else if (string_is_equal(settings->arrays.menu_driver, "ozone"))
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_ozone);
            }
            else
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails);
            }
            break;
         case MENU_ENUM_LABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_thumbnail_upscale_threshold);
            break;
         case MENU_ENUM_LABEL_MOUSE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_mouse_enable);
            break;
         case MENU_ENUM_LABEL_POINTER_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_pointer_enable);
            break;
         case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_stdin_cmd_enable);
            break;
         case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_public_announce);
            break;
         case MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_nat_traversal);
            break;
         case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_check_frames);
            break;
         case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_start_as_spectator);
            break;
         case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_allow_slaves);
            break;
         case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_require_slaves);
            break;
         case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_stateless_mode);
            break;
         case MENU_ENUM_LABEL_NETPLAY_PASSWORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_password);
            break;
         case MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_spectate_password);
            break;
         case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_tcp_udp_port);
            break;
         case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_ip_address);
            break;
         case MENU_ENUM_LABEL_OVERLAY_PRESET:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_overlay_preset);
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_overlay_enable);
            break;
         case MENU_ENUM_LABEL_OVERLAY_OPACITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_overlay_opacity);
            break;
         case MENU_ENUM_LABEL_OVERLAY_SCALE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_overlay_scale);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_VIDEO_LAYOUT_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_layout_enable);
            break;
         case MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_layout_path);
            break;
#endif
         case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_dsp_plugin);
            break;
         case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_output_rate);
            break;
         case MENU_ENUM_LABEL_AUDIO_DEVICE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_device);
            break;
         case MENU_ENUM_LABEL_AUDIO_WASAPI_EXCLUSIVE_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_wasapi_exclusive_mode);
            break;
         case MENU_ENUM_LABEL_AUDIO_WASAPI_FLOAT_FORMAT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_wasapi_float_format);
            break;
         case MENU_ENUM_LABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_wasapi_sh_buffer_length);
            break;
         case MENU_ENUM_LABEL_MENU_WALLPAPER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_wallpaper);
            break;
         case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_dynamic_wallpaper);
            break;
         case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_filter_supported_extensions);
            break;
         case MENU_ENUM_LABEL_WIFI_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_wifi_driver);
            break;
         case MENU_ENUM_LABEL_RECORD_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_record_driver);
            break;
         case MENU_ENUM_LABEL_MIDI_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_midi_driver);
            break;
         case MENU_ENUM_LABEL_MENU_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_driver);
            break;
         case MENU_ENUM_LABEL_LOCATION_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_location_driver);
            break;
         case MENU_ENUM_LABEL_CAMERA_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_camera_driver);
            break;
         case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_resampler_driver);
            break;
         case MENU_ENUM_LABEL_JOYPAD_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_joypad_driver);
            break;
         case MENU_ENUM_LABEL_INPUT_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_driver);
            break;
         case MENU_ENUM_LABEL_AUDIO_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_driver);
            break;
         case MENU_ENUM_LABEL_VIDEO_DRIVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_driver);
            break;
         case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_pause_libretro);
            break;
         case MENU_ENUM_LABEL_MENU_SAVESTATE_RESUME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_savestate_resume);
            break;
         case MENU_ENUM_LABEL_MENU_INPUT_SWAP_OK_CANCEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_swap_ok_cancel);
            break;
         case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_autodetect_enable);
            break;
         case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_remap_binds_enable);
            break;
         case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_autosave_interval);
            break;
         case MENU_ENUM_LABEL_SAVESTATE_THUMBNAIL_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_savestate_thumbnail_enable);
            break;
         case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_savestate_auto_save);
            break;
         case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_savestate_auto_load);
            break;
         case MENU_ENUM_LABEL_PERFCNT_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_perfcnt_enable);
            break;
         case MENU_ENUM_LABEL_FRONTEND_LOG_LEVEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_frontend_log_level);
            break;
         case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_libretro_log_level);
            break;
         case MENU_ENUM_LABEL_REWIND_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rewind);
            break;
         case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_apply_after_toggle);
            break;
         case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rewind_granularity);
            break;
         case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rewind_buffer_size);
            break;
         case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rewind_buffer_size_step);
            break;
         case MENU_ENUM_LABEL_CHEAT_IDX:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_idx);
            break;
         case MENU_ENUM_LABEL_CHEAT_MATCH_IDX:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_match_idx);
            break;
         case MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_big_endian);
            break;
         case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_start_or_cont);
            break;
         case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_start_or_restart);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_exact);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_LT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_lt);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_GT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_gt);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EQ:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_eq);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_neq);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_eqplus);
            break;
         case MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_search_eqminus);
            break;
         case MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_repeat_count);
            break;
         case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_repeat_add_to_address);
            break;
         case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_repeat_add_to_value);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_MATCHES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_add_matches);
            break;
         case MENU_ENUM_LABEL_CHEAT_VIEW_MATCHES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_view_matches);
            break;
         case MENU_ENUM_LABEL_CHEAT_CREATE_OPTION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_create_option);
            break;
         case MENU_ENUM_LABEL_CHEAT_DELETE_OPTION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_delete_option);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_add_new_top);
            break;
         case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_reload_cheats);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_add_new_bottom);
            break;
         case MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_address_bit_position);
            break;
         case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_delete_all);
            break;
         case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_slowmotion_ratio);
            break;
         case MENU_ENUM_LABEL_RUN_AHEAD_ENABLED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_run_ahead_enabled);
            break;
         case MENU_ENUM_LABEL_RUN_AHEAD_SECONDARY_INSTANCE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_run_ahead_secondary_instance);
            break;
         case MENU_ENUM_LABEL_RUN_AHEAD_HIDE_WARNINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_run_ahead_hide_warnings);
            break;
         case MENU_ENUM_LABEL_RUN_AHEAD_FRAMES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_run_ahead_frames);
            break;
         case MENU_ENUM_LABEL_INPUT_BLOCK_TIMEOUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_block_timeout);
            break;
         case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_fastforward_ratio);
            break;
         case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_vrr_runloop_enable);
            break;
         case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_block_sram_overwrite);
            break;
         case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_savestate_auto_index);
            break;
         case MENU_ENUM_LABEL_VIDEO_GPU_RECORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_gpu_record);
            break;
         case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_fullscreen);
            break;
         case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_windowed_fullscreen);
            break;
         case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_force_srgb_enable);
            break;
         case MENU_ENUM_LABEL_VIDEO_ROTATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_rotation);
            break;
         case MENU_ENUM_LABEL_SCREEN_ORIENTATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_screen_orientation);
            break;
         case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_gpu_screenshot);
            break;
         case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_scale_integer);
            break;
         case MENU_ENUM_LABEL_PLAYLISTS_TAB:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_collection_list);
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_overlay_hide_in_menu);
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_overlay_show_physical_inputs);
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_overlay_show_physical_inputs_port);
            break;
         case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_overlay_show_mouse_cursor);
            break;
         case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_font_size);
            break;
         case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_message_pos_x);
            break;
         case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_message_pos_y);
            break;
         case MENU_ENUM_LABEL_VIDEO_WINDOW_WIDTH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_window_width);
            break;
         case MENU_ENUM_LABEL_VIDEO_WINDOW_HEIGHT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_window_height);
            break;
         case MENU_ENUM_LABEL_VIDEO_FULLSCREEN_X:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_fullscreen_x);
            break;
         case MENU_ENUM_LABEL_VIDEO_FULLSCREEN_Y:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_fullscreen_y);
            break;
         case MENU_ENUM_LABEL_VIDEO_WINDOW_SAVE_POSITION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_save_window_position);
            break;
         case MENU_ENUM_LABEL_QUIT_RETROARCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quit_retroarch);
            break;
         case MENU_ENUM_LABEL_MENU_WIDGETS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_widgets);
            break;
         case MENU_ENUM_LABEL_RESTART_RETROARCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_restart_retroarch);
            break;
         case MENU_ENUM_LABEL_NETWORK_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_network_information);
            break;
         case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_system_information);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_list);
            break;
         case MENU_ENUM_LABEL_LOAD_CONTENT_SPECIAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_special);
            break;
         case MENU_ENUM_LABEL_START_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_start_core);
            break;
         case MENU_ENUM_LABEL_CORE_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_list);
            break;
         case MENU_ENUM_LABEL_SIDELOAD_CORE_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_sideload_core_list);
            break;
         case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_download_core);
            break;
         case MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_post_filter_record);
            break;
         case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_nickname);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_username);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_password);
            break;
         case MENU_ENUM_LABEL_VIDEO_FILTER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_filter);
            break;
         case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_crop_overscan);
            break;
         case MENU_ENUM_LABEL_VIDEO_SMOOTH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_smooth);
            break;
         case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_onscreen_notifications_enable);
            break;
         case MENU_ENUM_LABEL_INPUT_UNIFIED_MENU_CONTROLS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_input_unified_controls);
            break;
         case MENU_ENUM_LABEL_QUIT_PRESS_TWICE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quit_press_twice);
            break;
         case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_max_timing_skew);
            break;
         case MENU_ENUM_LABEL_AUDIO_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_enable);
            break;
         case MENU_ENUM_LABEL_AUDIO_ENABLE_MENU:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_enable_menu);
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
         case MENU_ENUM_LABEL_VIDEO_ADAPTIVE_VSYNC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_adaptive_vsync);
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
         case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_bind_hold);
            break;
         case MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_axis_threshold);
            break;
#if defined(GEKKO)
         case MENU_ENUM_LABEL_INPUT_MOUSE_SCALE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_mouse_scale);
            break;
#endif
         case MENU_ENUM_LABEL_AUDIO_SYNC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_sync);
            break;
         case MENU_ENUM_LABEL_AUDIO_VOLUME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_volume);
            break;
         case MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mixer_volume);
#endif
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
         case MENU_ENUM_LABEL_AUDIO_MIXER_MUTE:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mixer_mute);
#endif
            break;
         case MENU_ENUM_LABEL_AUDIO_LATENCY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_latency);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_shared_context);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY:
         case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ENTRY_HARDCORE:
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
         case MENU_ENUM_LABEL_CHEEVOS_LEADERBOARDS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_leaderboards_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_BADGES_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_badges_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_verbose_enable);
            break;
         case MENU_ENUM_LABEL_CHEEVOS_AUTO_SCREENSHOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_auto_screenshot);
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
         case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_refresh_rate_polled);
            break;
         case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_monitor_index);
            break;
         case MENU_ENUM_LABEL_LOG_VERBOSITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_log_verbosity);
            break;
         case MENU_ENUM_LABEL_LOG_TO_FILE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_log_to_file);
            break;
         case MENU_ENUM_LABEL_LOG_TO_FILE_TIMESTAMP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_log_to_file_timestamp);
            break;
         case MENU_ENUM_LABEL_LOG_DIR:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_log_dir);
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
         case MENU_ENUM_LABEL_SYSTEM_INFO_CONTROLLER_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_systeminfo_controller_entry);
            break;
         case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_black_frame_insertion);
            break;
         case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_frame_delay);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_DELAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_shader_delay);
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
         case MENU_ENUM_LABEL_STATISTICS_SHOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_statistics_show);
            break;
         case MENU_ENUM_LABEL_FPS_SHOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_fps_show);
            break;
         case MENU_ENUM_LABEL_FPS_UPDATE_INTERVAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_fps_update_interval);
            break;
         case MENU_ENUM_LABEL_FRAMECOUNT_SHOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_framecount_show);
            break;
         case MENU_ENUM_LABEL_MEMORY_SHOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_memory_show);
            break;
         case MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_views_settings_list);
            break;
         case MENU_ENUM_LABEL_SETTINGS_VIEWS_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_settings_views_settings_list);
            break;
         case MENU_ENUM_LABEL_QUICK_MENU_VIEWS_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu_views_settings_list);
            break;
         case MENU_ENUM_LABEL_MENU_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_settings_list);
            break;
         case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_settings_list);
            break;
         case MENU_ENUM_LABEL_CRT_SWITCHRES_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_crt_switchres_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_MIXER_SETTINGS:
#ifdef HAVE_AUDIOMIXER
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_mixer_settings_list);
#endif
            break;
         case MENU_ENUM_LABEL_LATENCY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_latency_settings_list);
            break;
         case MENU_ENUM_LABEL_RECORDING_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_recording_settings_list);
            break;
         case MENU_ENUM_LABEL_CORE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_settings_list);
            break;
         case MENU_ENUM_LABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_automatically_add_content_to_playlist);
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
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_manager_list);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_DEFAULT_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_manager_default_core);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RESET_CORES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_manager_reset_cores);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_manager_label_display_mode);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_RIGHT_THUMBNAIL_MODE:
            settings = config_get_ptr();
            /* Uses same sublabels as MENU_ENUM_LABEL_THUMBNAILS */
            if (string_is_equal(settings->arrays.menu_driver, "rgui"))
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails_rgui);
            }
            else
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails);
            }
            break;
         case MENU_ENUM_LABEL_PLAYLIST_MANAGER_LEFT_THUMBNAIL_MODE:
            settings = config_get_ptr();
            /* Uses same sublabels as MENU_ENUM_LABEL_LEFT_THUMBNAILS */
            if (string_is_equal(settings->arrays.menu_driver, "rgui"))
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_rgui);
            }
            else if (string_is_equal(settings->arrays.menu_driver, "ozone"))
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails_ozone);
            }
            else
            {
               BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_left_thumbnails);
            }
            break;
         case MENU_ENUM_LABEL_AI_SERVICE_URL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ai_service_url);
            break;
         case MENU_ENUM_LABEL_AI_SERVICE_TARGET_LANG:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ai_service_target_lang);
            break;
         case MENU_ENUM_LABEL_AI_SERVICE_SOURCE_LANG:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ai_service_source_lang);
            break;
         case MENU_ENUM_LABEL_AI_SERVICE_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ai_service_mode);
            break;
         case MENU_ENUM_LABEL_AI_SERVICE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ai_service_enable);
            break;
         case MENU_ENUM_LABEL_AI_SERVICE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_ai_service_settings_list);
            break;
         case MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_user_interface_settings_list);
            break;
         case MENU_ENUM_LABEL_POWER_MANAGEMENT_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_power_management_settings_list);
            break;
         case MENU_ENUM_LABEL_PRIVACY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_privacy_settings_list);
            break;
         case MENU_ENUM_LABEL_MIDI_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_midi_settings_list);
            break;
         case MENU_ENUM_LABEL_DIRECTORY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_directory_settings_list);
            break;
         case MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_frame_throttle_settings_list);
            break;
         case MENU_ENUM_LABEL_FRAME_TIME_COUNTER_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_frame_time_counter_settings_list);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_onscreen_display_settings_list);
            break;
         case MENU_ENUM_LABEL_NETWORK_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_network_settings_list);
            break;
         case MENU_ENUM_LABEL_NETWORK_ON_DEMAND_THUMBNAILS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_network_on_demand_thumbnails);
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
         case MENU_ENUM_LABEL_CONTENT_FAVORITES_SIZE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_favorites_size);
            break;
         case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_use_mitm_server);
            break;
         case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_mitm_server);
            break;
         case MENU_ENUM_LABEL_CORE_DELETE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_delete);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_PAUSE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_pause_hardcode_mode);
            break;
         case MENU_ENUM_LABEL_ACHIEVEMENT_RESUME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_resume_hardcode_mode);
            break;
         case MENU_ENUM_LABEL_MIDI_INPUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_midi_input);
            break;
         case MENU_ENUM_LABEL_MIDI_OUTPUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_midi_output);
            break;
         case MENU_ENUM_LABEL_MIDI_VOLUME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_midi_volume);
            break;
         case MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_onscreen_overlay_settings_list);
            break;
#ifdef HAVE_VIDEO_LAYOUT
         case MENU_ENUM_LABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_onscreen_video_layout_settings_list);
            break;
#endif
         case MENU_ENUM_LABEL_ONSCREEN_NOTIFICATIONS_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_onscreen_notifications_settings_list);
            break;
#ifdef HAVE_QT
         case MENU_ENUM_LABEL_SHOW_WIMP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_show_wimp);
            break;
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
         case MENU_ENUM_LABEL_SWITCH_CPU_PROFILE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_cpu_profile);
            break;
#endif
#ifdef HAVE_LAKKA_SWITCH
         case MENU_ENUM_LABEL_SWITCH_GPU_PROFILE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_gpu_profile);
            break;
         case MENU_ENUM_LABEL_SWITCH_BACKLIGHT_CONTROL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_switch_backlight_control);
            break;
#endif
#if defined(_3DS)
         case MENU_ENUM_LABEL_VIDEO_3DS_LCD_BOTTOM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_3ds_lcd_bottom);
            break;
         case MENU_ENUM_LABEL_VIDEO_3DS_DISPLAY_MODE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_3ds_display_mode);
            break;
#endif
#if defined(GEKKO)
         case MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_TOP:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_overscan_correction_top);
            break;
         case MENU_ENUM_LABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_overscan_correction_bottom);
            break;
#endif
         case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_apply_after_load);
            break;
         case MENU_ENUM_LABEL_DISCORD_ALLOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_discord_allow);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_ENTRY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_entry);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SHOW_SUBLABELS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_show_sublabels);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_border_filler_enable);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_border_filler_thickness_enable);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_background_filler_thickness_enable);
            break;
         case MENU_ENUM_LABEL_MENU_LINEAR_FILTER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_linear_filter);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO_LOCK:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_aspect_ratio_lock);
            break;
         case MENU_ENUM_LABEL_RGUI_MENU_COLOR_THEME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rgui_menu_color_theme);
            break;
         case MENU_ENUM_LABEL_RGUI_MENU_THEME_PRESET:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rgui_menu_theme_preset);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_SHADOWS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_shadows);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_particle_effect);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_particle_effect_speed);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_INLINE_THUMBNAILS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_inline_thumbnails);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_SWAP_THUMBNAILS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_swap_thumbnails);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_thumbnail_downscaler);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_THUMBNAIL_DELAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_thumbnail_delay);
            break;
         case MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_runtime_log);
            break;
         case MENU_ENUM_LABEL_SCAN_WITHOUT_CORE_MATCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_scan_without_core_match);
            break;
         case MENU_ENUM_LABEL_CONTENT_RUNTIME_LOG_AGGREGATE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_content_runtime_log_aggregate);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_sublabel_runtime_type);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_sublabel_last_played_style);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_internal_upscale_level);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_ASPECT_RATIO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_aspect_ratio);
            break;
         case MENU_ENUM_LABEL_MENU_TICKER_TYPE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_ticker_type);
            break;
         case MENU_ENUM_LABEL_MENU_TICKER_SPEED:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_ticker_speed);
            break;
         case MENU_ENUM_LABEL_MENU_TICKER_SMOOTH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_ticker_smooth);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SHOW_INLINE_CORE_NAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_show_inline_core_name);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_SORT_ALPHABETICAL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_sort_alphabetical);
            break;
         case MENU_ENUM_LABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_playlist_fuzzy_archive_match);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_FULL_WIDTH_LAYOUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_full_width_layout);
            break;
         case MENU_ENUM_LABEL_MENU_RGUI_EXTENDED_ASCII:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_rgui_extended_ascii);
            break;
         case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails_updater_list);
            break;
         case MENU_ENUM_LABEL_PL_THUMBNAILS_UPDATER_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_pl_thumbnails_updater_list);
            break;
         case MENU_ENUM_LABEL_HELP_SEND_DEBUG_INFO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_help_send_debug_info);
            break;
         case MENU_ENUM_LABEL_RDB_ENTRY_DETAIL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rdb_entry_detail);
            break;
         default:
         case MSG_UNKNOWN:
            return -1;
      }
   }

   return 0;
}
