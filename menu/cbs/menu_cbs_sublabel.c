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
#include "../../cheevos/cheevos.h"
#endif
#include "../../verbosity.h"

#include <string.h>
#include <string/stdstring.h>

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
default_sublabel_macro(action_bind_sublabel_cheevos_verbose_enable,        MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_views_settings_list,      MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS)
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
default_sublabel_macro(action_bind_sublabel_materialui_icons_enable,       MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE)
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
default_sublabel_macro(action_bind_sublabel_audio_mixer_mute,              MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE)
default_sublabel_macro(action_bind_sublabel_camera_allow,                  MENU_ENUM_SUBLABEL_CAMERA_ALLOW)
default_sublabel_macro(action_bind_sublabel_location_allow,                MENU_ENUM_SUBLABEL_LOCATION_ALLOW)
default_sublabel_macro(action_bind_sublabel_input_max_users,               MENU_ENUM_SUBLABEL_INPUT_MAX_USERS)
default_sublabel_macro(action_bind_sublabel_input_poll_type_behavior,      MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR)
default_sublabel_macro(action_bind_sublabel_input_all_users_control_menu,  MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU)
default_sublabel_macro(action_bind_sublabel_input_bind_timeout,            MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT)
default_sublabel_macro(action_bind_sublabel_audio_volume,                  MENU_ENUM_SUBLABEL_AUDIO_VOLUME)
default_sublabel_macro(action_bind_sublabel_audio_mixer_volume,            MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME)
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
default_sublabel_macro(action_bind_sublabel_video_smooth,                  MENU_ENUM_SUBLABEL_VIDEO_SMOOTH)
default_sublabel_macro(action_bind_sublabel_history_list_enable,           MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE)
default_sublabel_macro(action_bind_sublabel_content_history_size,          MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE)
default_sublabel_macro(action_bind_sublabel_menu_input_unified_controls,   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS)
default_sublabel_macro(action_bind_sublabel_onscreen_notifications_enable, MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE)
default_sublabel_macro(action_bind_sublabel_video_crop_overscan,           MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN)
default_sublabel_macro(action_bind_sublabel_video_filter,                  MENU_ENUM_SUBLABEL_VIDEO_FILTER)
default_sublabel_macro(action_bind_sublabel_netplay_nickname,              MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME)
default_sublabel_macro(action_bind_sublabel_cheevos_username,              MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME)
default_sublabel_macro(action_bind_sublabel_cheevos_password,              MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD)
default_sublabel_macro(action_bind_sublabel_video_post_filter_record,      MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD)
default_sublabel_macro(action_bind_sublabel_core_list,                     MENU_ENUM_SUBLABEL_CORE_LIST)
default_sublabel_macro(action_bind_sublabel_content_list,                  MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST)
default_sublabel_macro(action_bind_sublabel_content_special,               MENU_ENUM_SUBLABEL_LOAD_CONTENT_SPECIAL)
default_sublabel_macro(action_bind_sublabel_network_information,           MENU_ENUM_SUBLABEL_NETWORK_INFORMATION)
default_sublabel_macro(action_bind_sublabel_system_information,            MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION)
default_sublabel_macro(action_bind_sublabel_quit_retroarch,                MENU_ENUM_SUBLABEL_QUIT_RETROARCH)
default_sublabel_macro(action_bind_sublabel_video_window_width,            MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH)
default_sublabel_macro(action_bind_sublabel_video_window_height,           MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT)
default_sublabel_macro(action_bind_sublabel_video_fullscreen_x,            MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X)
default_sublabel_macro(action_bind_sublabel_video_fullscreen_y,            MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y)
default_sublabel_macro(action_bind_sublabel_video_message_pos_x,           MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X)
default_sublabel_macro(action_bind_sublabel_video_message_pos_y,           MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y)
default_sublabel_macro(action_bind_sublabel_video_font_size,               MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE)
default_sublabel_macro(action_bind_sublabel_input_overlay_hide_in_menu,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU)
default_sublabel_macro(action_bind_sublabel_content_collection_list,       MENU_ENUM_SUBLABEL_CONTENT_COLLECTION_LIST)
default_sublabel_macro(action_bind_sublabel_video_scale_integer,           MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER)
default_sublabel_macro(action_bind_sublabel_video_gpu_screenshot,          MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT)
default_sublabel_macro(action_bind_sublabel_video_rotation,                MENU_ENUM_SUBLABEL_VIDEO_ROTATION)
default_sublabel_macro(action_bind_sublabel_video_force_srgb_enable,       MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE)
default_sublabel_macro(action_bind_sublabel_video_fullscreen,              MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN)
default_sublabel_macro(action_bind_sublabel_video_windowed_fullscreen,     MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN)
default_sublabel_macro(action_bind_sublabel_video_gpu_record,              MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD)
default_sublabel_macro(action_bind_sublabel_savestate_auto_index,          MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX)
default_sublabel_macro(action_bind_sublabel_block_sram_overwrite,          MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE)
default_sublabel_macro(action_bind_sublabel_fastforward_ratio,             MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO)
default_sublabel_macro(action_bind_sublabel_slowmotion_ratio,              MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO)
default_sublabel_macro(action_bind_sublabel_rewind,                        MENU_ENUM_SUBLABEL_REWIND_ENABLE)
default_sublabel_macro(action_bind_sublabel_rewind_granularity,            MENU_ENUM_SUBLABEL_REWIND_GRANULARITY)
default_sublabel_macro(action_bind_sublabel_libretro_log_level,            MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL)
default_sublabel_macro(action_bind_sublabel_perfcnt_enable,                MENU_ENUM_SUBLABEL_PERFCNT_ENABLE)
default_sublabel_macro(action_bind_sublabel_savestate_auto_save,           MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE)
default_sublabel_macro(action_bind_sublabel_savestate_auto_load,           MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD)
default_sublabel_macro(action_bind_sublabel_savestate_thumbnail_enable,    MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE)
default_sublabel_macro(action_bind_sublabel_autosave_interval,             MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL)
default_sublabel_macro(action_bind_sublabel_input_remap_binds_enable,      MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE)
default_sublabel_macro(action_bind_sublabel_input_autodetect_enable,       MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE)
default_sublabel_macro(action_bind_sublabel_input_swap_ok_cancel,          MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL)
default_sublabel_macro(action_bind_sublabel_pause_libretro,                MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO)
default_sublabel_macro(action_bind_sublabel_video_driver,                  MENU_ENUM_SUBLABEL_VIDEO_DRIVER)
default_sublabel_macro(action_bind_sublabel_audio_driver,                  MENU_ENUM_SUBLABEL_AUDIO_DRIVER)
default_sublabel_macro(action_bind_sublabel_input_driver,                  MENU_ENUM_SUBLABEL_INPUT_DRIVER)
default_sublabel_macro(action_bind_sublabel_joypad_driver,                 MENU_ENUM_SUBLABEL_JOYPAD_DRIVER)
default_sublabel_macro(action_bind_sublabel_audio_resampler_driver,        MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER)
default_sublabel_macro(action_bind_sublabel_camera_driver,                 MENU_ENUM_SUBLABEL_CAMERA_DRIVER)
default_sublabel_macro(action_bind_sublabel_location_driver,               MENU_ENUM_SUBLABEL_LOCATION_DRIVER)
default_sublabel_macro(action_bind_sublabel_menu_driver,                   MENU_ENUM_SUBLABEL_MENU_DRIVER)
default_sublabel_macro(action_bind_sublabel_record_driver,                 MENU_ENUM_SUBLABEL_RECORD_DRIVER)
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
default_sublabel_macro(action_bind_sublabel_timedate_enable,               MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE)
default_sublabel_macro(action_bind_sublabel_battery_level_enable,          MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE)
default_sublabel_macro(action_bind_sublabel_navigation_wraparound,         MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND)
default_sublabel_macro(action_bind_sublabel_netplay_enable_host,           MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST)
default_sublabel_macro(action_bind_sublabel_netplay_enable_client,         MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT)
default_sublabel_macro(action_bind_sublabel_netplay_disconnect,            MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT)
default_sublabel_macro(action_bind_sublabel_scan_file,                     MENU_ENUM_SUBLABEL_SCAN_FILE)
default_sublabel_macro(action_bind_sublabel_scan_directory,                MENU_ENUM_SUBLABEL_SCAN_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_video_swap_interval,           MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL)
default_sublabel_macro(action_bind_sublabel_sort_savefiles_enable,         MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE)
default_sublabel_macro(action_bind_sublabel_sort_savestates_enable,        MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE)
default_sublabel_macro(action_bind_sublabel_netplay_client_swap_input,     MENU_ENUM_SUBLABEL_NETPLAY_CLIENT_SWAP_INPUT)
default_sublabel_macro(action_bind_sublabel_core_updater_buildbot_url,     MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL)
default_sublabel_macro(action_bind_sublabel_input_overlay_show_physical_inputs,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS)
default_sublabel_macro(action_bind_sublabel_input_overlay_show_physical_inputs_port,    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT)
default_sublabel_macro(action_bind_sublabel_core_updater_buildbot_assets_url,      MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL)
default_sublabel_macro(action_bind_sublabel_core_updater_auto_extract_archive,     MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE)
default_sublabel_macro(action_bind_sublabel_netplay_refresh_rooms,                 MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS)
default_sublabel_macro(action_bind_sublabel_delete_entry,                          MENU_ENUM_SUBLABEL_DELETE_ENTRY)
default_sublabel_macro(action_bind_sublabel_information,                           MENU_ENUM_SUBLABEL_INFORMATION)
default_sublabel_macro(action_bind_sublabel_run,                                   MENU_ENUM_SUBLABEL_RUN)
default_sublabel_macro(action_bind_sublabel_add_to_favorites,                      MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES)
default_sublabel_macro(action_bind_sublabel_goto_favorites,                      MENU_ENUM_SUBLABEL_GOTO_FAVORITES)
default_sublabel_macro(action_bind_sublabel_goto_images,                      MENU_ENUM_SUBLABEL_GOTO_IMAGES)
default_sublabel_macro(action_bind_sublabel_goto_music,                      MENU_ENUM_SUBLABEL_GOTO_MUSIC)
default_sublabel_macro(action_bind_sublabel_goto_video,                      MENU_ENUM_SUBLABEL_GOTO_VIDEO)
default_sublabel_macro(action_bind_sublabel_menu_filebrowser_settings,             MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS)
default_sublabel_macro(action_bind_sublabel_auto_remaps_enable,                    MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE)
default_sublabel_macro(action_bind_sublabel_auto_overrides_enable,                 MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE)
default_sublabel_macro(action_bind_sublabel_game_specific_options,                 MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS)
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
default_sublabel_macro(action_bind_sublabel_restart_content,                       MENU_ENUM_SUBLABEL_RESTART_CONTENT)
default_sublabel_macro(action_bind_sublabel_save_current_config_override_core,     MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE)
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
default_sublabel_macro(action_bind_sublabel_xmb_icon_theme,                        MENU_ENUM_SUBLABEL_XMB_THEME)
default_sublabel_macro(action_bind_sublabel_xmb_shadows_enable,                    MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_color_theme,                      MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME)
default_sublabel_macro(action_bind_sublabel_menu_wallpaper_opacity,                MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY)
default_sublabel_macro(action_bind_sublabel_menu_framebuffer_opacity,                MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY)
default_sublabel_macro(action_bind_sublabel_menu_ribbon_enable,                    MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE)
default_sublabel_macro(action_bind_sublabel_menu_font,                             MENU_ENUM_SUBLABEL_XMB_FONT)
default_sublabel_macro(action_bind_sublabel_menu_favorites_tab,                    MENU_ENUM_SUBLABEL_XMB_SHOW_FAVORITES)
default_sublabel_macro(action_bind_sublabel_menu_images_tab,                       MENU_ENUM_SUBLABEL_XMB_SHOW_IMAGES)
default_sublabel_macro(action_bind_sublabel_menu_show_online_updater,              MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER)
default_sublabel_macro(action_bind_sublabel_menu_show_core_updater,              MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER)
default_sublabel_macro(action_bind_sublabel_menu_music_tab,                        MENU_ENUM_SUBLABEL_XMB_SHOW_MUSIC)
default_sublabel_macro(action_bind_sublabel_menu_video_tab,                        MENU_ENUM_SUBLABEL_XMB_SHOW_VIDEO)
default_sublabel_macro(action_bind_sublabel_menu_netplay_tab,                      MENU_ENUM_SUBLABEL_XMB_SHOW_NETPLAY)
default_sublabel_macro(action_bind_sublabel_menu_settings_tab,                     MENU_ENUM_SUBLABEL_XMB_SHOW_SETTINGS)
default_sublabel_macro(action_bind_sublabel_menu_history_tab,                      MENU_ENUM_SUBLABEL_XMB_SHOW_HISTORY)
default_sublabel_macro(action_bind_sublabel_menu_import_content_tab,               MENU_ENUM_SUBLABEL_XMB_SHOW_ADD)
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
default_sublabel_macro(action_bind_sublabel_cache_directory,                       MENU_ENUM_SUBLABEL_CACHE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_database_directory,                    MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_cursor_directory,                      MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_assets_directory,                      MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_savefile_directory,                    MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_savestate_directory,                   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_screenshot_directory,                  MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_overlay_directory,                     MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_cheatfile_directory,                   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH)
default_sublabel_macro(action_bind_sublabel_audio_filter_directory,                MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR)
default_sublabel_macro(action_bind_sublabel_video_filter_directory,                MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR)
default_sublabel_macro(action_bind_sublabel_video_shader_directory,                MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR)
default_sublabel_macro(action_bind_sublabel_recording_output_directory,            MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_recording_config_directory,            MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY)
default_sublabel_macro(action_bind_sublabel_video_font_path,                       MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH)
default_sublabel_macro(action_bind_sublabel_shader_apply_changes,                  MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES)
default_sublabel_macro(action_bind_sublabel_shader_num_passes,                     MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES)
default_sublabel_macro(action_bind_sublabel_shader_preset,                         MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_as,                 MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_core,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE)
default_sublabel_macro(action_bind_sublabel_shader_preset_save_game,               MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME)
default_sublabel_macro(action_bind_sublabel_shader_parameters,                     MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS)
default_sublabel_macro(action_bind_sublabel_shader_preset_parameters,              MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS)
default_sublabel_macro(action_bind_sublabel_cheat_apply_changes,                   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES)
default_sublabel_macro(action_bind_sublabel_cheat_num_passes,                      MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES)
default_sublabel_macro(action_bind_sublabel_cheat_file_load,                       MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD)
default_sublabel_macro(action_bind_sublabel_cheat_file_save_as,                    MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS)
default_sublabel_macro(action_bind_sublabel_quick_menu,                            MENU_ENUM_SUBLABEL_CONTENT_SETTINGS)
default_sublabel_macro(action_bind_sublabel_core_information,                      MENU_ENUM_SUBLABEL_CORE_INFORMATION)
default_sublabel_macro(action_bind_sublabel_video_aspect_ratio,                    MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_height,          MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_width,           MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_x,               MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X)
default_sublabel_macro(action_bind_sublabel_video_viewport_custom_y,               MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y)
default_sublabel_macro(action_bind_sublabel_netplay_use_mitm_server,               MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER)
default_sublabel_macro(action_bind_sublabel_core_delete,                           MENU_ENUM_SUBLABEL_CORE_DELETE)

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

   snprintf(s,len, "RetroArch: %s\nCore: %s (%s)\nGame: %s (%08x)",
      string_is_empty(netplay_room_list[i - 3].retroarch_version) ? "n/a" : netplay_room_list[i - 3].retroarch_version,
      netplay_room_list[i - 3].corename, netplay_room_list[i - 3].coreversion,
      !string_is_equal(netplay_room_list[i - 3].gamename, "N/A") ? netplay_room_list[i - 3].gamename : "n/a", netplay_room_list[i - 3].gamecrc);
#if 0
   strlcpy(s, netplay_room_list[i - 3].corename, len);
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
         case MENU_ENUM_LABEL_CONTENT_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quick_menu);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_save_as);
            break;
         case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheat_file_load);
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
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_core);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_GAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_game);
            break;
         case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_shader_preset_save_as);
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
         case MENU_ENUM_LABEL_XMB_SHOW_ADD:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_import_content_tab);
            break;
         case MENU_ENUM_LABEL_XMB_SHOW_HISTORY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_history_tab);
            break;
         case MENU_ENUM_LABEL_XMB_SHOW_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_settings_tab);
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
         case MENU_ENUM_LABEL_XMB_SHOW_FAVORITES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_favorites_tab);
            break;
         case MENU_ENUM_LABEL_XMB_SHOW_IMAGES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_images_tab);
            break;
         case MENU_ENUM_LABEL_XMB_SHOW_MUSIC:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_music_tab);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_ONLINE_UPDATER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_online_updater);
            break;
         case MENU_ENUM_LABEL_MENU_SHOW_CORE_UPDATER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_show_core_updater);
            break;
         case MENU_ENUM_LABEL_XMB_SHOW_NETPLAY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_netplay_tab);
            break;
         case MENU_ENUM_LABEL_XMB_SHOW_VIDEO:
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
         case MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_wallpaper_opacity);
            break;
         case MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME:
         case MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_color_theme);
            break;
         case MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_xmb_shadows_enable);
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
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_save_current_config_override_game);
            break;
         case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_save_current_config_override_core);
            break;
         case MENU_ENUM_LABEL_RESTART_CONTENT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_restart_content);
            break;
         case MENU_ENUM_LABEL_REWIND_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_input_meta_rewind);
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
         case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_auto_overrides_enable);
            break;
         case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_auto_remaps_enable);
            break;
         case MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_filebrowser_settings);
            break;
         case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_add_to_favorites);
            break;
         case MENU_ENUM_LABEL_RUN:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_run);
            break;
         case MENU_ENUM_LABEL_INFORMATION:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_information);
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
         case MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_client_swap_input);
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
         case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_timedate_enable);
            break;
         case MENU_ENUM_LABEL_THUMBNAILS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_thumbnails);
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
         case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_libretro_log_level);
            break;
         case MENU_ENUM_LABEL_REWIND_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rewind);
            break;
         case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_rewind_granularity);
            break;
         case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_slowmotion_ratio);
            break;
         case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_fastforward_ratio);
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
         case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_gpu_screenshot);
            break;
         case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_scale_integer);
            break;
         case MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST:
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
         case MENU_ENUM_LABEL_QUIT_RETROARCH:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_quit_retroarch);
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
         case MENU_ENUM_LABEL_CORE_LIST:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_list);
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
         case MENU_ENUM_LABEL_AUDIO_MIXER_VOLUME:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mixer_volume);
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
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_mixer_mute);
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
         case MENU_ENUM_LABEL_CHEEVOS_VERBOSE_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_cheevos_verbose_enable);
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
         case MENU_ENUM_LABEL_MENU_VIEWS_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_menu_views_settings_list);
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
         case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_netplay_use_mitm_server);
            break;
         case MENU_ENUM_LABEL_CORE_DELETE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_core_delete);
         default:
         case MSG_UNKNOWN:
            return -1;
      }
   }

   return 0;
}
