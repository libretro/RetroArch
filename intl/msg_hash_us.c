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

#include <stdint.h>
#include <stddef.h>

#include <string/stdstring.h>

#include "../msg_hash.h"

#ifdef HAVE_MENU
static const char *menu_hash_to_str_us_label_enum(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MENU_ENUM_LABEL_CORE_SETTINGS:
         return "core_settings";
      case MENU_ENUM_LABEL_CB_MENU_WALLPAPER:
         return "cb_menu_wallpaper";
      case MENU_ENUM_LABEL_CB_LAKKA_LIST:
         return "cb_lakka_list";
      case MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_LIST:
         return "cb_thumbnails_updater_list";
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_LIST:
         return "cb_core_updater_list";
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_LIST:
         return "cb_core_content_list";
      case MENU_ENUM_LABEL_CB_CORE_THUMBNAILS_DOWNLOAD:
         return "cb_core_thumbnails_download";
      case MENU_ENUM_LABEL_CB_CORE_UPDATER_DOWNLOAD:
         return "cb_core_updater_download";
      case MENU_ENUM_LABEL_CB_UPDATE_CHEATS:
         return "cb_update_cheats";
      case MENU_ENUM_LABEL_CB_UPDATE_OVERLAYS:
         return "cb_update_overlays";
      case MENU_ENUM_LABEL_CB_UPDATE_DATABASES:
         return "cb_update_databases";
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_GLSL:
         return "cb_update_shaders_glsl";
      case MENU_ENUM_LABEL_CB_UPDATE_SHADERS_CG:
         return "cb_update_shaders_cg";
      case MENU_ENUM_LABEL_CB_UPDATE_CORE_INFO_FILES:
         return "cb_update_core_info_files";
      case MENU_ENUM_LABEL_CB_CORE_CONTENT_DOWNLOAD:
         return "cb_core_content_download";
      case MENU_ENUM_LABEL_CB_LAKKA_DOWNLOAD:
         return "cb_lakka_download";
      case MENU_ENUM_LABEL_CB_UPDATE_ASSETS:
         return "cb_update_assets";
      case MENU_ENUM_LABEL_CB_UPDATE_AUTOCONFIG_PROFILES:
         return "cb_update_autoconfig_profiles";
      case MENU_ENUM_LABEL_CB_THUMBNAILS_UPDATER_DOWNLOAD:
         return "cb_thumbnails_updater_download";
      case MENU_ENUM_LABEL_CONTENT_ACTIONS:
         return "content_actions";
      case MENU_ENUM_LABEL_CPU_ARCHITECTURE:
         return "system_information_cpu_architecture";
      case MENU_ENUM_LABEL_CPU_CORES:
         return "system_information_cpu_cores";
      case MENU_ENUM_LABEL_NO_ITEMS:
         return "no_items";
      case MENU_ENUM_LABEL_SETTINGS_TAB:
         return "settings_tab";
      case MENU_ENUM_LABEL_HISTORY_TAB:
         return "history_tab";
      case MENU_ENUM_LABEL_ADD_TAB:
         return "add_tab";
      case MENU_ENUM_LABEL_PLAYLISTS_TAB:
         return "playlists_tab";
      case MENU_ENUM_LABEL_HORIZONTAL_MENU:
         return "horizontal_menu";
      case MENU_ENUM_LABEL_PARENT_DIRECTORY:
         return "parent_directory";
      case MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE:
         return "input_player%u_analog_dpad_mode";
      case MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE:
         return "input_libretro_device_p%u";
      case MENU_ENUM_LABEL_RUN:
         return "collection";
      case MENU_ENUM_LABEL_INPUT_USER_1_BINDS:
         return "1_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_2_BINDS:
         return "2_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_3_BINDS:
         return "3_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_4_BINDS:
         return "4_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_5_BINDS:
         return "5_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_6_BINDS:
         return "6_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_7_BINDS:
         return "7_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_8_BINDS:
         return "8_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_9_BINDS:
         return "9_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_10_BINDS:
         return "10_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_11_BINDS:
         return "11_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_12_BINDS:
         return "12_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_13_BINDS:
         return "13_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_14_BINDS:
         return "14_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_15_BINDS:
         return "15_input_binds_list";
      case MENU_ENUM_LABEL_INPUT_USER_16_BINDS:
         return "16_input_binds_list";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X:
         return "video_viewport_custom_x";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y:
         return "video_viewport_custom_y";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH:
         return "video_viewport_custom_width";
      case MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
         return "video_viewport_custom_height";
      case MENU_ENUM_LABEL_NO_CORES_AVAILABLE:
         return "no_cores_available";
      case MENU_ENUM_LABEL_NO_CORE_OPTIONS_AVAILABLE:
         return "no_core_options_available";
      case MENU_ENUM_LABEL_NO_CORE_INFORMATION_AVAILABLE:
         return "no_core_information_available";
      case MENU_ENUM_LABEL_NO_PERFORMANCE_COUNTERS:
         return "no_performance_counters";
      case MENU_ENUM_LABEL_NO_ENTRIES_TO_DISPLAY:
         return "no_entries_to_display";
      case MENU_ENUM_LABEL_CHEEVOS_UNLOCKED_ACHIEVEMENTS:
         return "cheevos_unlocked_achievements";
      case MENU_ENUM_LABEL_CHEEVOS_LOCKED_ACHIEVEMENTS:
         return "cheevos_locked_achievements";
      case MENU_ENUM_LABEL_MAIN_MENU:
         return "main_menu";
      case MENU_ENUM_LABEL_MENU_LINEAR_FILTER:
         return "menu_linear_filter";
      case MENU_ENUM_LABEL_MENU_ENUM_THROTTLE_FRAMERATE:
         return "menu_throttle_framerate";
      case MENU_ENUM_LABEL_START_CORE:
         return "start_core";
      case MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "cheevos_hardcore_mode_enable";
      case MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL:
         return "cheevos_test_unofficial";
      case MENU_ENUM_LABEL_CHEEVOS_ENABLE:
         return "cheevos_enable";
      case MENU_ENUM_LABEL_INPUT_ICADE_ENABLE:
         return "input_icade_enable";
      case MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE:
         return "keyboard_gamepad_mapping_type";
      case MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE:
         return "input_small_keyboard_enable";
      case MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG:
         return "save_current_config";
      case MENU_ENUM_LABEL_STATE_SLOT:
         return "state_slot";
      case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
         return "cheevos_username";
      case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
         return "cheevos_password";
      case MENU_ENUM_LABEL_ACCOUNTS_CHEEVOS_USERNAME:
         return "accounts_cheevos_username";
      case MENU_ENUM_LABEL_ACCOUNTS_CHEEVOS_PASSWORD:
         return "accounts_cheevos_password";
      case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
         return "retro_achievements";
      case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_CHEEVOS_LIST:
         return "deferred_accounts_cheevos_list";
      case MENU_ENUM_LABEL_DEFERRED_USER_BINDS_LIST:
         return "deferred_user_binds_list";
      case MENU_ENUM_LABEL_DEFERRED_ACCOUNTS_LIST:
         return "deferred_accounts_list";
      case MENU_ENUM_LABEL_DEFERRED_INPUT_SETTINGS_LIST:
         return "deferred_input_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_DRIVER_SETTINGS_LIST:
         return "deferred_driver_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_AUDIO_SETTINGS_LIST:
         return "deferred_audio_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_SETTINGS_LIST:
         return "deferred_core_settings_list";
      case MENU_ENUM_LABEL_DEFERRED_VIDEO_SETTINGS_LIST:
         return "deferred_video_settings_list";
      case MENU_ENUM_LABEL_ACCOUNTS_LIST:
         return "accounts_list";
      case MENU_ENUM_LABEL_DEFERRED_INPUT_HOTKEY_BINDS_LIST:
         return "deferred_input_hotkey_binds";
      case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS:
         return "input_hotkey_binds";
      case MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS_BEGIN:
         return "input_hotkey_binds_begin";
      case MENU_ENUM_LABEL_INPUT_SETTINGS_BEGIN:
         return "input_settings_begin";
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS_BEGIN:
         return "playlist_settings_begin";
      case MENU_ENUM_LABEL_PLAYLIST_SETTINGS:
         return "playlist_settings";
      case MENU_ENUM_LABEL_DEFERRED_PLAYLIST_SETTINGS_LIST:
         return "deferred_playlist_settings";
      case MENU_ENUM_LABEL_INPUT_SETTINGS:
         return "input_settings";
      case MENU_ENUM_LABEL_DRIVER_SETTINGS:
         return "driver_settings";
      case MENU_ENUM_LABEL_VIDEO_SETTINGS:
         return "video_settings";
      case MENU_ENUM_LABEL_AUDIO_SETTINGS:
         return "audio_settings";
      case MENU_ENUM_LABEL_DEBUG_PANEL_ENABLE:
         return "debug_panel_enable";
      case MENU_ENUM_LABEL_HELP_SCANNING_CONTENT:
         return "help_scanning_content";
      case MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION:
         return "cheevos_description";
      case MENU_ENUM_LABEL_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "help_audio_video_troubleshooting";
      case MENU_ENUM_LABEL_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "help_change_virtual_gamepad";
      case MENU_ENUM_LABEL_HELP_WHAT_IS_A_CORE:
         return "help_what_is_a_core";
      case MENU_ENUM_LABEL_HELP_LOADING_CONTENT:
         return "help_loading_content";
      case MENU_ENUM_LABEL_HELP_LIST:
         return "help_list";
      case MENU_ENUM_LABEL_HELP_CONTROLS:
         return "help_controls";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
         return "deferred_archive_open_detect_core";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_OPEN:
         return "deferred_archive_open";
      case MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE:
         return "load_archive_detect_core";
      case MENU_ENUM_LABEL_LOAD_ARCHIVE:
         return "load_archive";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE:
         return "deferred_archive_action_detect_core";
      case MENU_ENUM_LABEL_DEFERRED_ARCHIVE_ACTION:
         return "deferred_archive_action";
      case MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE:
         return "open_archive_detect_core";
      case MENU_ENUM_LABEL_OPEN_ARCHIVE:
         return "open_archive";
      case MENU_ENUM_LABEL_INPUT_BACK_AS_MENU_ENUM_TOGGLE_ENABLE:
         return "back_as_menu_toggle_enable";
      case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "input_menu_toggle_gamepad_combo";
      case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
         return "overlay_hide_in_menu";
      case MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "no_playlist_entries_available";
      case MENU_ENUM_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "downloaded_file_detect_core_list";
      case MENU_ENUM_LABEL_UPDATE_CORE_INFO_FILES:
         return "update_core_info_files";
      case MENU_ENUM_LABEL_DEFERRED_CORE_CONTENT_LIST:
         return "deferred_core_content_list";
      case MENU_ENUM_LABEL_DEFERRED_LAKKA_LIST:
         return "deferred_lakka_list";
      case MENU_ENUM_LABEL_DOWNLOAD_CORE_CONTENT:
         return "download_core_content";
      case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
         return "scan_this_directory";
      case MENU_ENUM_LABEL_SCAN_FILE:
         return "scan_file";
      case MENU_ENUM_LABEL_SCAN_DIRECTORY:
         return "scan_directory";
      case MENU_ENUM_LABEL_ADD_CONTENT_LIST:
         return "add_content";
      case MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED:
         return "overlay_autoload_preferred";
      case MENU_ENUM_LABEL_INFORMATION_LIST:
         return "information_list";
      case MENU_ENUM_LABEL_USE_BUILTIN_PLAYER:
         return "use_builtin_player";
      case MENU_ENUM_LABEL_CONTENT_SETTINGS:
         return "quick_menu";
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         return "load_content";
      case MENU_ENUM_LABEL_NO_SETTINGS_FOUND:
         return "menu_label_no_settings_found";
      case MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE:
         return "system_bgm_enable";
      case MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES:
         return "audio_block_frames";
      case MENU_ENUM_LABEL_INPUT_BIND_MODE:
         return "input_bind_mode";
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "input_descriptor_label_show";
      case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "input_descriptor_hide_unbound";
      case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
         return "video_font_enable";
      case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
         return "video_font_path";
      case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
         return "video_font_size";
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
         return "video_message_pos_x";
      case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
         return "video_message_pos_y";
      case MENU_ENUM_LABEL_VIDEO_SOFT_FILTER:
         return "soft_filter";
      case MENU_ENUM_LABEL_VIDEO_FILTER_FLICKER:
         return "video_filter_flicker";
      case MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY:
         return "input_remapping_directory";
      case MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR:
         return "joypad_autoconfig_dir";
      case MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY:
         return "recording_config_directory";
      case MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY:
         return "recording_output_directory";
      case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
         return "screenshot_directory";
      case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
         return "playlist_directory";
      case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
         return "savefile_directory";
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         return "savestate_directory";
      case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
         return "stdin_commands";
      case MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE:
         return "network_remote_enable";
      case MENU_ENUM_LABEL_NETWORK_REMOTE_PORT:
         return "network_remote_base_port";
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         return "video_driver";
      case MENU_ENUM_LABEL_RECORD_ENABLE:
         return "record_enable";
      case MENU_ENUM_LABEL_VIDEO_GPU_RECORD:
         return "video_gpu_record";
      case MENU_ENUM_LABEL_RECORD_PATH:
         return "record_path";
      case MENU_ENUM_LABEL_RECORD_USE_OUTPUT_DIRECTORY:
         return "record_use_output_directory";
      case MENU_ENUM_LABEL_RECORD_CONFIG:
         return "record_config";
      case MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD:
         return "video_post_filter_record";
      case MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY:
         return "core_assets_directory";
      case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
         return "assets_directory";
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "dynamic_wallpapers_directory";
      case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
         return "thumbnails_directory";
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
         return "rgui_browser_directory";
      case MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY:
         return "rgui_config_directory";
      case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
         return "libretro_info_path";
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         return "libretro_dir_path";
      case MENU_ENUM_LABEL_CURSOR_DIRECTORY:
         return "cursor_directory";
      case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
         return "content_database_path";
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         return "system_directory";
      case MENU_ENUM_LABEL_CACHE_DIRECTORY:
         return "cache_directory";
      case MENU_ENUM_LABEL_CHEAT_DATABASE_PATH:
         return "cheat_database_path";
      case MENU_ENUM_LABEL_AUDIO_FILTER_DIR:
         return "audio_filter_dir";
      case MENU_ENUM_LABEL_VIDEO_FILTER_DIR:
         return "video_filter_dir";
      case MENU_ENUM_LABEL_VIDEO_SHADER_DIR:
         return "video_shader_dir";
      case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
         return "overlay_directory";
      case MENU_ENUM_LABEL_OSK_OVERLAY_DIRECTORY:
         return "osk_overlay_directory";
      case MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT:
         return "netplay_client_swap_input";
      case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "netplay_spectator_mode_enable";
      case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
         return "netplay_ip_address";
      case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
         return "netplay_tcp_udp_port";
      case MENU_ENUM_LABEL_NETPLAY_ENABLE:
         return "netplay_enable";
      case MENU_ENUM_LABEL_SSH_ENABLE:
         return "ssh_enable";
      case MENU_ENUM_LABEL_SAMBA_ENABLE:
         return "samba_enable";
      case MENU_ENUM_LABEL_BLUETOOTH_ENABLE:
         return "bluetooth_enable";
      case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
         return "netplay_delay_frames";
      case MENU_ENUM_LABEL_NETPLAY_MODE:
         return "netplay_mode";
      case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
         return "rgui_show_start_screen";
      case MENU_ENUM_LABEL_TITLE_COLOR:
         return "menu_title_color";
      case MENU_ENUM_LABEL_ENTRY_HOVER_COLOR:
         return "menu_entry_hover_color";
      case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
         return "menu_timedate_enable";
      case MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE:
         return "threaded_data_runloop_enable";
      case MENU_ENUM_LABEL_ENTRY_NORMAL_COLOR:
         return "menu_entry_normal_color";
      case MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS:
         return "menu_show_advanced_settings";
      case MENU_ENUM_LABEL_MOUSE_ENABLE:
         return "menu_mouse_enable";
      case MENU_ENUM_LABEL_POINTER_ENABLE:
         return "menu_pointer_enable";
      case MENU_ENUM_LABEL_CORE_ENABLE:
         return "menu_core_enable";
      case MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE:
         return "dpi_override_enable";
      case MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE:
         return "dpi_override_value";
      case MENU_ENUM_LABEL_XMB_FONT:
         return "xmb_font";
      case MENU_ENUM_LABEL_XMB_THEME:
         return "xmb_theme";
      case MENU_ENUM_LABEL_XMB_GRADIENT:
         return "xmb_gradient";
      case MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE:
         return "xmb_shadows_enable";
      case MENU_ENUM_LABEL_XMB_RIBBON_ENABLE:
         return "xmb_ribbon_enable";
      case MENU_ENUM_LABEL_XMB_SCALE_FACTOR:
         return "xmb_scale_factor";
      case MENU_ENUM_LABEL_XMB_ALPHA_FACTOR:
         return "xmb_alpha_factor";
      case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
         return "suspend_screensaver_enable";
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         return "video_disable_composition";
      case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
         return "pause_nonactive";
      case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
         return "ui_companion_start_on_boot";
      case MENU_ENUM_LABEL_UI_COMPANION_ENABLE:
         return "ui_companion_enable";
      case MENU_ENUM_LABEL_UI_MENUBAR_ENABLE:
         return "ui_menubar_enable";
      case MENU_ENUM_LABEL_ARCHIVE_MODE:
         return "archive_mode";
      case MENU_ENUM_LABEL_NETWORK_CMD_ENABLE:
         return "network_cmd_enable";
      case MENU_ENUM_LABEL_NETWORK_CMD_PORT:
         return "network_cmd_port";
      case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
         return "history_list_enable";
      case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
         return "Content History Size";
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         return "video_refresh_rate_auto";
      case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         return "dummy_on_core_shutdown";
      case MENU_ENUM_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "core_set_supports_no_content_enable";
      case MENU_ENUM_LABEL_FRAME_THROTTLE_ENABLE:
         return "fastforward_ratio_throttle_enable";
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
         return "fastforward_ratio";
      case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
         return "auto_remaps_enable";
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         return "slowmotion_ratio";
      case MENU_ENUM_LABEL_CORE_SPECIFIC_CONFIG:
         return "core_specific_config";
      case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
         return "game_specific_options";
      case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
         return "auto_overrides_enable";
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
         return "config_save_on_exit";
      case MENU_ENUM_LABEL_VIDEO_SMOOTH:
         return "video_smooth";
      case MENU_ENUM_LABEL_VIDEO_GAMMA:
         return "video_gamma";
      case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
         return "video_allow_rotate";
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         return "video_hard_sync";
      case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
         return "video_swap_interval";
      case MENU_ENUM_LABEL_VIDEO_VSYNC:
         return "video_vsync";
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         return "video_threaded";
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         return "video_rotation";
      case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
         return "video_gpu_screenshot";
      case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
         return "video_crop_overscan";
      case MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX:
         return "aspect_ratio_index";
      case MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_AUTO:
         return "video_aspect_ratio_auto";
      case MENU_ENUM_LABEL_VIDEO_FORCE_ASPECT:
         return "video_force_aspect";
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE: 
         return "video_refresh_rate";
      case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
         return "video_force_srgb_disable";
      case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
         return "video_windowed_fullscreen";
      case MENU_ENUM_LABEL_PAL60_ENABLE:
         return "pal60_enable";
      case MENU_ENUM_LABEL_VIDEO_VFILTER:
         return "video_vfilter";
      case MENU_ENUM_LABEL_VIDEO_VI_WIDTH:
         return "video_vi_width";
      case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         return "video_black_frame_insertion";
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
         return "video_hard_sync_frames";
      case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
         return "sort_savefiles_enable";
      case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
         return "sort_savestates_enable";
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         return "video_fullscreen";
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         return "perfcnt_enable";
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         return "video_scale";
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         return "video_scale_integer";
      case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
         return "libretro_log_level";
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         return "log_verbosity";
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         return "savestate_auto_save";
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
         return "savestate_auto_load";
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
         return "savestate_auto_index";
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
         return "autosave_interval";
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         return "block_sram_overwrite";
      case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
         return "video_shared_context";
      case MENU_ENUM_LABEL_RESTART_RETROARCH:
         return "restart_retroarch";
      case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
         return "netplay_nickname";
      case MENU_ENUM_LABEL_USER_LANGUAGE:
         return "user_language";
      case MENU_ENUM_LABEL_CAMERA_ALLOW:
         return "camera_allow";
      case MENU_ENUM_LABEL_LOCATION_ALLOW:
         return "location_allow";
      case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
         return "menu_pause_libretro";
      case MENU_ENUM_LABEL_INPUT_OSK_OVERLAY_ENABLE:
         return "input_osk_overlay_enable";
      case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
         return "input_overlay_enable";
      case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
         return "video_monitor_index";
      case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
         return "video_frame_delay";
      case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
         return "input_duty_cycle";
      case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
         return "input_turbo_period";
      case MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD:
         return "input_axis_threshold";
      case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
         return "input_remap_binds_enable";
      case MENU_ENUM_LABEL_INPUT_MAX_USERS:
         return "input_max_users";
      case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
         return "input_autodetect_enable";
      case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
         return "audio_output_rate";
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         return "audio_max_timing_skew";
      case MENU_ENUM_LABEL_CHEAT_APPLY_CHANGES:
         return "cheat_apply_changes";
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_CORE:
         return "remap_file_save_core";
      case MENU_ENUM_LABEL_REMAP_FILE_SAVE_GAME:
         return "remap_file_save_game";
      case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
         return "cheat_num_passes";
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         return "shader_apply_changes";
      case MENU_ENUM_LABEL_COLLECTION:
         return "collection";
      case MENU_ENUM_LABEL_REWIND_ENABLE:
         return "rewind_enable";
      case MENU_ENUM_LABEL_CONTENT_COLLECTION_LIST:
         return "select_from_collection";
      case MENU_ENUM_LABEL_DETECT_CORE_LIST:
         return "detect_core_list";
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         return "load_recent";
      case MENU_ENUM_LABEL_AUDIO_ENABLE:
         return "audio_enable";
      case MENU_ENUM_LABEL_FPS_SHOW:
         return "fps_show";
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         return "audio_mute_enable";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
         return "video_shader_pass";
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         return "audio_volume";
      case MENU_ENUM_LABEL_AUDIO_SYNC:
         return "audio_sync";
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
         return "audio_rate_control_delta";
      case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
         return "video_shader_filter_pass";
      case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
         return "video_shader_scale_pass";
      case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
         return "video_shader_num_passes";
      case MENU_ENUM_LABEL_RDB_ENTRY_DESCRIPTION:
         return "rdb_entry_description";
      case MENU_ENUM_LABEL_RDB_ENTRY_GENRE:
         return "rdb_entry_genre";
      case MENU_ENUM_LABEL_RDB_ENTRY_ORIGIN:
         return "rdb_entry_origin";
      case MENU_ENUM_LABEL_RDB_ENTRY_PUBLISHER:
         return "rdb_entry_publisher";
      case MENU_ENUM_LABEL_RDB_ENTRY_DEVELOPER:
         return "rdb_entry_developer";
      case MENU_ENUM_LABEL_RDB_ENTRY_FRANCHISE:
         return "rdb_entry_franchise";
      case MENU_ENUM_LABEL_RDB_ENTRY_MAX_USERS:
         return "rdb_entry_max_users";
      case MENU_ENUM_LABEL_RDB_ENTRY_NAME:
         return "rdb_entry_name";
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "rdb_entry_edge_magazine_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW:
         return "rdb_entry_edge_magazine_review";
      case MENU_ENUM_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "rdb_entry_famitsu_magazine_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_TGDB_RATING:
         return "rdb_entry_tgdb_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         return "rdb_entry_edge_magazine_issue";
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_MONTH:
         return "rdb_entry_releasemonth";
      case MENU_ENUM_LABEL_RDB_ENTRY_RELEASE_YEAR:
         return "rdb_entry_releaseyear";
      case MENU_ENUM_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
         return "rdb_entry_enhancement_hw";
      case MENU_ENUM_LABEL_RDB_ENTRY_SHA1:
         return "rdb_entry_sha1";
      case MENU_ENUM_LABEL_RDB_ENTRY_CRC32:
         return "rdb_entry_crc32";
      case MENU_ENUM_LABEL_RDB_ENTRY_MD5:
         return "rdb_entry_md5";
      case MENU_ENUM_LABEL_RDB_ENTRY_BBFC_RATING:
         return "rdb_entry_bbfc_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_ESRB_RATING:
         return "rdb_entry_esrb_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_ELSPA_RATING:
         return "rdb_entry_elspa_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_PEGI_RATING:
         return "rdb_entry_pegi_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_CERO_RATING:
         return "rdb_entry_cero_rating";
      case MENU_ENUM_LABEL_RDB_ENTRY_ANALOG:
         return "rdb_entry_analog";
      case MENU_ENUM_LABEL_CONFIGURATIONS:
         return "configurations";
      case MENU_ENUM_LABEL_LOAD_OPEN_ZIP:
         return "load_open_zip";
      case MENU_ENUM_LABEL_REWIND_GRANULARITY:
         return "rewind_granularity";
      case MENU_ENUM_LABEL_REMAP_FILE_LOAD:
         return "remap_file_load";
      case MENU_ENUM_LABEL_CUSTOM_RATIO:
         return "custom_ratio";
      case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
         return "use_this_directory";
      case MENU_ENUM_LABEL_RDB_ENTRY_START_CONTENT:
         return "rdb_entry_start_content";
      case MENU_ENUM_LABEL_CUSTOM_BIND:
         return "custom_bind";
      case MENU_ENUM_LABEL_CUSTOM_BIND_ALL:
         return "custom_bind_all";
      case MENU_ENUM_LABEL_DISK_OPTIONS:
         return "core_disk_options";
      case MENU_ENUM_LABEL_CORE_CHEAT_OPTIONS:
         return "core_cheat_options";
      case MENU_ENUM_LABEL_CORE_OPTIONS:
         return "core_options";
      case MENU_ENUM_LABEL_DATABASE_MANAGER_LIST:
         return "database_manager_list";
      case MENU_ENUM_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
         return "deferred_database_manager_list";
      case MENU_ENUM_LABEL_CURSOR_MANAGER_LIST:
         return "cursor_manager_list";
      case MENU_ENUM_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
         return "deferred_cursor_manager_list";
      case MENU_ENUM_LABEL_CHEAT_FILE_LOAD:
         return "cheat_file_load";
      case MENU_ENUM_LABEL_CHEAT_FILE_SAVE_AS:
         return "cheat_file_save_as";
      case MENU_ENUM_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
         return "deferred_rdb_entry_detail";
      case MENU_ENUM_LABEL_FRONTEND_COUNTERS:
         return "frontend_counters";
      case MENU_ENUM_LABEL_CORE_COUNTERS:
         return "core_counters";
      case MENU_ENUM_LABEL_DISK_CYCLE_TRAY_STATUS:
         return "disk_cycle_tray_status";
      case MENU_ENUM_LABEL_DISK_IMAGE_APPEND:
         return "disk_image_append";
      case MENU_ENUM_LABEL_DEFERRED_CORE_LIST:
         return "deferred_core_list";
      case MENU_ENUM_LABEL_DEFERRED_CORE_LIST_SET:
         return "deferred_core_list_set";
      case MENU_ENUM_LABEL_INFO_SCREEN:
         return "info_screen";
      case MENU_ENUM_LABEL_SETTINGS:
         return "settings";
      case MENU_ENUM_LABEL_QUIT_RETROARCH:
         return "quit_retroarch";
      case MENU_ENUM_LABEL_SHUTDOWN:
         return "shutdown";
      case MENU_ENUM_LABEL_REBOOT:
         return "reboot";
      case MENU_ENUM_LABEL_HELP:
         return "help";
      case MENU_ENUM_LABEL_SAVE_NEW_CONFIG:
         return "save_new_config";
      case MENU_ENUM_LABEL_RESTART_CONTENT:
         return "restart_content";
      case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
         return "take_screenshot";
      case MENU_ENUM_LABEL_CORE_UPDATER_LIST:
         return "core_updater_list";
      case MENU_ENUM_LABEL_START_NET_RETROPAD:
         return "menu_start_net_retropad";
      case MENU_ENUM_LABEL_THUMBNAILS_UPDATER_LIST:
         return "thumbnails_updater_list";
      case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
         return "core_updater_buildbot_url";
      case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
         return "buildbot_assets_url";
      case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
         return "menu_navigation_wraparound_enable";
      case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "menu_navigation_browser_filter_supported_extensions_enable";
      case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "core_updater_auto_extract_archive";
      case MENU_ENUM_LABEL_DEBUG_INFORMATION:
         return "debug_information";
      case MENU_ENUM_LABEL_ACHIEVEMENT_LIST:
         return "achievement_list";
      case MENU_ENUM_LABEL_SYSTEM_INFORMATION:
         return "system_information";
      case MENU_ENUM_LABEL_NETWORK_INFORMATION:
         return "network_information";
      case MENU_ENUM_LABEL_ONLINE_UPDATER:
         return "online_updater";
      case MENU_ENUM_LABEL_CORE_INFORMATION:
         return "core_information";
      case MENU_ENUM_LABEL_CORE_LIST:
         return "load_core";
      case MENU_ENUM_LABEL_LOAD_CONTENT:
         return "load_content_default";
      case MENU_ENUM_LABEL_CLOSE_CONTENT:
         return "unload_core";
      case MENU_ENUM_LABEL_MANAGEMENT:
         return "database_settings";
      case MENU_ENUM_LABEL_SAVE_STATE:
         return "savestate";
      case MENU_ENUM_LABEL_LOAD_STATE:
         return "loadstate";
      case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
         return "undoloadstate";
      case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
         return "undosavestate";
      case MENU_ENUM_LABEL_RESUME_CONTENT:
         return "resume_content";
      case MENU_ENUM_LABEL_INPUT_DRIVER:
         return "input_driver";
      case MENU_ENUM_LABEL_AUDIO_DRIVER:
         return "audio_driver";
      case MENU_ENUM_LABEL_JOYPAD_DRIVER:
         return "input_joypad_driver";
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         return "audio_resampler_driver";
      case MENU_ENUM_LABEL_RECORD_DRIVER:
         return "record_driver";
      case MENU_ENUM_LABEL_MENU_DRIVER:
         return "menu_driver";
      case MENU_ENUM_LABEL_CAMERA_DRIVER:
         return "camera_driver";
      case MENU_ENUM_LABEL_LOCATION_DRIVER:
         return "location_driver";
      case MENU_ENUM_LABEL_OVERLAY_SCALE:
         return "input_overlay_scale";
      case MENU_ENUM_LABEL_OVERLAY_PRESET:
         return "input_overlay";
      case MENU_ENUM_LABEL_KEYBOARD_OVERLAY_PRESET:
         return "input_osk_overlay";
      case MENU_ENUM_LABEL_AUDIO_DEVICE:
         return "audio_device";
      case MENU_ENUM_LABEL_AUDIO_LATENCY:
         return "audio_latency";
      case MENU_ENUM_LABEL_OVERLAY_OPACITY:
         return "input_overlay_opacity";
      case MENU_ENUM_LABEL_MENU_WALLPAPER:
         return "menu_wallpaper";
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
         return "menu_dynamic_wallpaper_enable";
      case MENU_ENUM_LABEL_THUMBNAILS:
         return "thumbnails";
      case MENU_ENUM_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return "core_input_remapping_options";
      case MENU_ENUM_LABEL_SHADER_OPTIONS:
         return "shader_options";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         return "video_shader_parameters";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         return "video_shader_preset_parameters";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         return "video_shader_preset_save_as";
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
         return "video_shader_preset";
      case MENU_ENUM_LABEL_VIDEO_FILTER:
         return "video_filter";
      case MENU_ENUM_LABEL_DEFERRED_VIDEO_FILTER:
         return "deferred_video_filter";
      case MENU_ENUM_LABEL_DEFERRED_CORE_UPDATER_LIST:
         return "core_updater";
      case MENU_ENUM_LABEL_DEFERRED_THUMBNAILS_UPDATER_LIST:
         return "deferred_thumbnails_updater_list";
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         return "audio_dsp_plugin";
      case MENU_ENUM_LABEL_UPDATE_ASSETS:
         return "update_assets";
      case MENU_ENUM_LABEL_UPDATE_LAKKA:
         return "update_lakka";
      case MENU_ENUM_LABEL_UPDATE_CHEATS:
         return "update_cheats";
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES:
         return "update_autoconfig_profiles";
      case MENU_ENUM_LABEL_UPDATE_AUTOCONFIG_PROFILES_HID:
         return "update_autoconfig_profiles_hid";
      case MENU_ENUM_LABEL_UPDATE_DATABASES:
         return "update_databases";
      case MENU_ENUM_LABEL_UPDATE_OVERLAYS:
         return "update_overlays";
      case MENU_ENUM_LABEL_UPDATE_CG_SHADERS:
         return "update_cg_shaders";
      case MENU_ENUM_LABEL_UPDATE_GLSL_SHADERS:
         return "update_glsl_shaders";
      case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
         return "screen_resolution";
      case MENU_ENUM_LABEL_USE_BUILTIN_IMAGE_VIEWER:
         return "use_builtin_image_viewer";
      case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         return "input_poll_type_behavior";
      default:
         break;
   }

   return "null";
}
#endif

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
#ifdef HAVE_MENU
   const char *ret = menu_hash_to_str_us_label_enum(msg);

   if (ret && !string_is_equal(ret, "null"))
      return ret;
#endif

   switch (msg)
   {
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_MOVIE_RECORD_STOPPED:
         return "Stopping movie record.";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "Movie playback ended.";
      case MSG_AUTOSAVE_FAILED:
         return "Could not initialize autosave.";
      case MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED:
         return "Movie playback has started. Cannot start netplay.";
      case MSG_NETPLAY_FAILED:
         return "Failed to initialize netplay.";
      case MSG_LIBRETRO_ABI_BREAK:
         return "is compiled against a different version of libretro than this libretro implementation.";
      case MSG_REWIND_INIT_FAILED_NO_SAVESTATES:
         return "Implementation does not support save states. Cannot use rewind.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "Implementation uses threaded audio. Cannot use rewind.";
      case MSG_REWIND_INIT_FAILED:
         return "Failed to initialize rewind buffer. Rewinding will be disabled.";
      case MSG_REWIND_INIT:
         return "Initializing rewind buffer with size";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Custom timing given";
      case MSG_VIEWPORT_SIZE_CALCULATION_FAILED:
         return "Viewport size calculation failed! Will continue using raw data. This will probably not work right ...";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Libretro core is hardware rendered. Must use post-shaded recording as well.";
      case MSG_RECORDING_TO:
         return "Recording to";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Detected viewport of";
      case MSG_TAKING_SCREENSHOT:
         return "Taking screenshot.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Failed to take screenshot.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Failed to start recording.";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Recording terminated due to resize.";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Using libretro dummy core. Skipping recording.";
      case MSG_UNKNOWN:
         return "Unknown";
      case MSG_LOADING_CONTENT_FILE:
         return "Loading content file";
      case MSG_RECEIVED:
         return "received";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Unrecognized command";
      case MSG_SENDING_COMMAND:
         return "Sending command";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Got invalid disk index.";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Failed to remove disk from tray.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Removed disk from tray.";
      case MSG_VIRTUAL_DISK_TRAY:
         return "virtual disk tray.";
      case MSG_FAILED_TO:
         return "Failed to";
      case MSG_TO:
         return "to";
      case MSG_SAVING_RAM_TYPE:
         return "Saving RAM type";
      case MSG_SAVING_STATE:
         return "Saving state";
      case MSG_LOADING_STATE:
         return "Loading state";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Failed to load movie file";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Failed to load content";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Could not read content file";
      case MSG_GRAB_MOUSE_STATE:
         return "Grab mouse state";
      case MSG_PAUSED:
         return "Paused.";
      case MSG_UNPAUSED:
         return "Unpaused.";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Failed to load overlay.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Failed to unmute audio.";
      case MSG_AUDIO_MUTED:
         return "Audio muted.";
      case MSG_AUDIO_UNMUTED:
         return "Audio unmuted.";
      case MSG_RESET:
         return "Reset";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Failed to load state from";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Failed to save state to";
      case MSG_FAILED_TO_LOAD_UNDO:
         return "No undo state found";
      case MSG_FAILED_TO_SAVE_UNDO:
         return "Failed to save undo information";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Failed to save SRAM";
      case MSG_STATE_SIZE:
         return "State size";
      case MSG_FOUND_SHADER:
         return "Found shader";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "SRAM will not be saved.";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Blocking SRAM Overwrite";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "Core does not support save states.";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Saved state to slot";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Saved successfully to";
      case MSG_BYTES:
         return "bytes";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Config directory not set. Cannot save new config.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Skipping SRAM load.";
      case MSG_APPENDED_DISK:
         return "Appended disk";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Starting movie playback.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Failed to remove temporary file";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Removing temporary content file";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Loaded state from slot";
      case MSG_COULD_NOT_PROCESS_ZIP_FILE:
         return "Could not process ZIP file.";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Scanning of directory finished";
      case MSG_SCANNING:
         return "Scanning";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Redirecting cheat file to";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Redirecting save file to";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Redirecting savestate to";
      case MSG_SHADER:
         return "Shader";
      case MSG_APPLYING_SHADER:
         return "Applying shader";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Failed to apply shader.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Starting movie record to";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Failed to start movie record.";
      case MSG_STATE_SLOT:
         return "State slot";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Restarting recording due to driver reinit.";
      case MSG_SLOW_MOTION:
         return "Slow motion.";
      case MSG_SLOW_MOTION_REWIND:
         return "Slow motion rewind.";
      case MSG_REWINDING:
         return "Rewinding.";
      case MSG_REWIND_REACHED_END:
         return "Reached end of rewind buffer.";
      case MSG_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "Hardcore Mode Enabled: savestate & rewind were disabled.";
      case MSG_TASK_FAILED:
         return "Failed";
      case MSG_DOWNLOADING:
         return "Downloading";
      case MSG_EXTRACTING:
         return "Extracting";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "Edge Magazine Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW:
         return "Edge Magazine Review";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "Famitsu Magazine Rating";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING:
         return "TGDB Rating";
      case MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE:
         return "CPU Architecture:";
      case MENU_ENUM_LABEL_VALUE_CPU_CORES:
         return "CPU Cores:";
      case MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS:
         return "Internal storage status";
      case MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY:
         return "..";
      case MENU_ENUM_LABEL_VALUE_RUN:
         return "Run";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X:
         return "Custom Viewport X";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y:
         return "Custom Viewport Y";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH:
         return "Custom Viewport Width";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT:
         return "Custom Viewport Height";
      case MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY:
         return "No entries to display.";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ACHIEVEMENTS:
         return "Unlocked Achievements:";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ACHIEVEMENTS:
         return "Locked Achievements:";
      case MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD:
         return "Start Remote RetroPad";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST:
         return "Thumbnails Updater";
      case MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER:
         return "Menu Linear Filter";
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE:
         return "Throttle Menu Framerate";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "Hardcore Mode";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL:
         return "Test unofficial";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS:
         return "Retro Achievements";
      case MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE:
         return "Keyboard Gamepad Mapping Enable";
      case MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE:
         return "Keyboard Gamepad Mapping Type";
      case MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE:
         return "Small Keyboard Enable";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG:
         return "Save Current Config";
      case MENU_ENUM_LABEL_VALUE_STATE_SLOT:
         return "State Slot";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS:
         return "Accounts Cheevos";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME:
         return "Username";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD:
         return "Password";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS:
         return "Retro Achievements";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST:
         return "Accounts";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END:
         return "Accounts List Endpoint";
      case MENU_ENUM_LABEL_VALUE_DEBUG_PANEL_ENABLE:
         return "Debug Panel Enable";
      case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT:
         return "Scanning For Content";
      case MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION:
         return "Description";
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "Audio/Video Troubleshooting";
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "Changing Virtual Gamepad Overlay";
      case MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE:
         return "What Is A Core?";
      case MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT:
         return "Loading Content";
      case MENU_ENUM_LABEL_VALUE_HELP_LIST:
         return "Help";
      case MENU_ENUM_LABEL_VALUE_HELP_CONTROLS:
         return "Basic Menu Controls";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS:
         return "Basic menu controls";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP:
         return "Scroll Up";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_DOWN:
         return "Scroll Down";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM:
         return "Confirm/OK";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_BACK:
         return "Back";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START:
         return "Defaults";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO:
         return "Info";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU:
         return "Toggle Menu";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT:
         return "Quit";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD:
         return "Toggle Keyboard";
      case MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE:
         return "Open Archive As Folder";
      case MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE:
         return "Load Archive With Core";
      case MENU_ENUM_LABEL_VALUE_INPUT_BACK_AS_MENU_ENUM_TOGGLE_ENABLE:
         return "Back As Menu Toggle Enable";
      case MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "Menu Toggle Gamepad Combo";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU:
         return "Hide Overlay In Menu";
      case MENU_ENUM_LABEL_VALUE_LANG_POLISH:
         return "Polish";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "Autoload Preferred Overlay";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "Update Core Info Files";
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "Download Content";
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE:
         return "Download Core...";
      case MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<Scan This Directory>";
      case MENU_ENUM_LABEL_VALUE_SCAN_FILE:
         return "Scan File";
      case MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY:
         return "Scan Directory";
      case MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST:
         return "Add Content";
      case MENU_ENUM_LABEL_VALUE_INFORMATION_LIST:
         return "Information";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Use Builtin Media Player";
      case MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS:
         return "Quick Menu";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Load Content";
      case MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE:
         return "Ask";
      case MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Privacy";
      case MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU:
         return "Horizontal Menu";
      case MENU_ENUM_LABEL_VALUE_SETTINGS_TAB:
         return "Settings";
      case MENU_ENUM_LABEL_VALUE_HISTORY_TAB:
         return "History";
      case MENU_ENUM_LABEL_VALUE_ADD_TAB:
         return "Add tab";
      case MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB:
         return "Playlists";
      case MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "No settings found.";
      case MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "No performance counters.";
      case MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS:
         return "Driver";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Configuration";
      case MENU_ENUM_LABEL_VALUE_CORE_SETTINGS:
         return "Core";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS:
         return "Video";
      case MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS:
         return "Logging";
      case MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS:
         return "Saving";
      case MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS:
         return "Rewind";
      case MENU_ENUM_LABEL_VALUE_SHADER:
         return "Shader";
      case MENU_ENUM_LABEL_VALUE_CHEAT:
         return "Cheat";
      case MENU_ENUM_LABEL_VALUE_USER:
         return "User";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "System BGM Enable";
      case MENU_ENUM_LABEL_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_ENUM_LABEL_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Block Frames";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_MODE:
         return "Bind Mode";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Display Input Descriptor Labels";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Hide Unbound Core Input Descriptors";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Display OSD Message";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH:
         return "OSD Message Font";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "OSD Message Size";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "OSD Message X Position";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "OSD Message Y Position";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Soft Filter Enable";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Flicker filter";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT:
         return "<Content dir>";
      case MENU_ENUM_LABEL_VALUE_UNKNOWN:
         return "Unknown";
      case MENU_ENUM_LABEL_VALUE_DONT_CARE:
         return "Don't care";
      case MENU_ENUM_LABEL_VALUE_LINEAR:
         return "Linear";
      case MENU_ENUM_LABEL_VALUE_NEAREST:
         return "Nearest";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT:
         return "<Default>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE:
         return "<None>";
      case MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Input Remapping Dir";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Input Device Autoconfig Dir";
      case MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Recording Config Dir";
      case MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Recording Output Dir";
      case MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Screenshot Dir";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Playlist Dir";
      case MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Savefile Dir";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Savestate Dir";
      case MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "stdin Commands";
      case MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE:
         return "Network Gamepad";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER:
         return "Video Driver";
      case MENU_ENUM_LABEL_VALUE_RECORD_ENABLE:
         return "Record Enable";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "GPU Record Enable";
      case MENU_ENUM_LABEL_VALUE_RECORD_PATH:
         return "Output File";
      case MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Use Output Dir";
      case MENU_ENUM_LABEL_VALUE_RECORD_CONFIG:
         return "Record Config";
      case MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Post filter record Enable";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Downloads Dir";
      case MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Assets Dir";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Dynamic Wallpapers Dir";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY:
         return "Thumbnails Dir";
      case MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "File Browser Dir";
      case MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Config Dir";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Core Info Dir";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Core Dir";
      case MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Cursor Dir";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Content Database Dir";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "System/BIOS Dir";
      case MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Cheat File Dir";
      case MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY:
         return "Cache Dir";
      case MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Audio Filter Dir";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Video Shader Dir";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Video Filter Dir";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Overlay Dir";
      case MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "OSK Overlay Dir";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Swap Netplay Input";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Netplay Spectator Enable";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "IP Address";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Netplay TCP/UDP Port";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE:
         return "Netplay Enable";
      case MENU_ENUM_LABEL_VALUE_SSH_ENABLE:
         return "SSH Enable";
      case MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE:
         return "SAMBA Enable";
      case MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE:
         return "Bluetooth Enable";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Netplay Delay Frames";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_MODE:
         return "Netplay Client Enable";
      case MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Show Start Screen";
      case MENU_ENUM_LABEL_VALUE_TITLE_COLOR:
         return "Menu title color";
      case MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Menu entry hover color";
      case MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Display time / date";
      case MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Threaded data runloop";
      case MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Menu entry normal color";
      case MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Show Advanced Settings";
      case MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE:
         return "Mouse Support";
      case MENU_ENUM_LABEL_VALUE_POINTER_ENABLE:
         return "Touch Support";
      case MENU_ENUM_LABEL_VALUE_CORE_ENABLE:
         return "Display core name";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "DPI Override Enable";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "DPI Override";
      case MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR:
         return "XMB Scale Factor";
      case MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR:
         return "XMB Alpha Factor";
      case MENU_ENUM_LABEL_VALUE_XMB_FONT:
         return "XMB Font";
      case MENU_ENUM_LABEL_VALUE_XMB_THEME:
         return "XMB Theme";
      case MENU_ENUM_LABEL_VALUE_XMB_GRADIENT:
         return "Background Gradient";
      case MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE:
         return "Icon Shadows Enable";
      case MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE:
         return "Menu Shader Pipeline";
      case MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Suspend Screensaver";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Disable Desktop Composition";
      case MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Don't run in background";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI Companion Start On Boot";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE:
         return "UI Companion Enable";
      case MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Menubar";
      case MENU_ENUM_LABEL_VALUE_ARCHIVE_MODE:
         return "Archive File Assocation Action";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Network Commands";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Network Command Port";
      case MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "History List Enable";
      case MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "History List Size";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Estimated Monitor Framerate";
      case MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Dummy On Core Shutdown";
      case MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "Automatically start a core";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limit Maximum Run Speed";
      case MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Maximum Run Speed";
      case MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Load Remap Files Automatically";
      case MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Slow-Motion Ratio";
      case MENU_ENUM_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configuration Per-Core";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS:
         return "Use per-game core options if available";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE:
         return "Create game-options file";
      case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE:
         return "Game-options file";
      case MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Load Override Files Automatically";
      case MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Save Configuration On Exit";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH:
         return "HW Bilinear Filtering";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA:
         return "Video Gamma";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Allow rotation";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Hard GPU Sync";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "VSync Swap Interval";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC:
         return "VSync";
      case MENU_ENUM_LABEL_VALUE_VIDEO_THREADED:
         return "Threaded Video";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotation";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "GPU Screenshot Enable";
      case MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Crop Overscan (Reload)";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Aspect Ratio Index";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Auto Aspect Ratio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Force aspect ratio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Refresh Rate";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Force-disable sRGB FBO";
      case MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Windowed Fullscreen Mode";
      case MENU_ENUM_LABEL_VALUE_PAL60_ENABLE:
         return "Use PAL60 Mode";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER:
         return "Deflicker";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Set VI Screen Width";
      case MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Black Frame Insertion";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Hard GPU Sync Frames";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sort Saves In Folders";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sort Savestates In Folders";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Use Fullscreen Mode";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE:
         return "Windowed Scale";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Integer Scale";
      case MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE:
         return "Performance Counters";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Core Logging Level";
      case MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY:
         return "Logging Verbosity";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Auto Load State";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Save State Auto Index";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Auto Save State";
      case MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "SaveRAM Autosave Interval";
      case MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Don't overwrite SaveRAM on loading savestate";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "HW Shared Context Enable";
      case MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH:
         return "Restart RetroArch";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Username";
      case MENU_ENUM_LABEL_VALUE_USER_LANGUAGE:
         return "Language";
      case MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW:
         return "Allow Camera";
      case MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW:
         return "Allow Location";
      case MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pause when menu activated";
      case MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Display Keyboard Overlay";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Display Overlay";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Monitor Index";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Frame Delay";
      case MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Duty Cycle";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Turbo Period";
      case MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Input Axis Threshold";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Remap Binds Enable";
      case MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS:
         return "Max Users";
      case MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Autoconfig Enable";
      case MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Audio Output Rate (KHz)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Audio Maximum Timing Skew";
      case MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Cheat Passes";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Save Core Remap File";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Save Game Remap File";
      case MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Apply Cheat Changes";
      case MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Apply Shader Changes";
      case MENU_ENUM_LABEL_VALUE_REWIND_ENABLE:
         return "Rewind Enable";
      case MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Select From Collection";
      case MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST:
         return "Select File And Detect Core";
      case MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "Select Downloaded File And Detect Core";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Load Recent";
      case MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE:
         return "Audio Enable";
      case MENU_ENUM_LABEL_VALUE_FPS_SHOW:
         return "Display Framerate";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MUTE:
         return "Audio Mute";
      case MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME:
         return "Audio Volume Level (dB)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SYNC:
         return "Audio Sync Enable";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Audio Rate Control Delta";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Shader Passes";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATIONS:
         return "Load Configuration";
      case MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY:
         return "Rewind Granularity";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Load Remap File";
      case MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO:
         return "Custom Ratio";
      case MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Use this directory>";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Start Content";
      case MENU_ENUM_LABEL_VALUE_DISK_OPTIONS:
         return "Disk Control";
      case MENU_ENUM_LABEL_VALUE_CORE_OPTIONS:
         return "Options";
      case MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Cheats";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Cheat File Load";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Cheat File Save As";
      case MENU_ENUM_LABEL_VALUE_CORE_COUNTERS:
         return "Core Counters";
      case MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Take Screenshot";
      case MENU_ENUM_LABEL_VALUE_RESUME:
         return "Resume";
      case MENU_ENUM_LABEL_VALUE_DISK_INDEX:
         return "Disk Index";
      case MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Frontend Counters";
      case MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Disk Image Append";
      case MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Disk Cycle Tray Status";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "No playlist entries available.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "No core information available.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "No core options available.";
      case MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "No cores available.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE:
         return "No Core";
      case MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER:
         return "Database Manager";
      case MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER:
         return "Cursor Manager";
      case MENU_ENUM_LABEL_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_ENUM_LABEL_VALUE_SETTINGS:
         return "Settings";
      case MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH:
         return "Quit RetroArch";
      case MENU_ENUM_LABEL_VALUE_SHUTDOWN:
         return "Shutdown";
      case MENU_ENUM_LABEL_VALUE_REBOOT:
         return "Reboot";
      case MENU_ENUM_LABEL_VALUE_HELP:
         return "help";
      case MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Save New Config";
      case MENU_ENUM_LABEL_VALUE_RESTART_CONTENT:
         return "Restart";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Core Updater";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "Buildbot Cores URL";
      case MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "Buildbot Assets URL";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Navigation Wrap-Around";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filter by supported extensions";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Automatically extract downloaded archive";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION:
         return "System Information";
      case MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION:
         return "Network Information";
      case MENU_ENUM_LABEL_VALUE_DEBUG_INFORMATION:
         return "Debug Information";
      case MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST:
         return "Achievement List";
      case MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER:
         return "Online Updater";
      case MENU_ENUM_LABEL_VALUE_CORE_INFORMATION:
         return "Core Information";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Directory not found.";
      case MENU_ENUM_LABEL_VALUE_NO_ITEMS:
         return "No items.";
      case MENU_ENUM_LABEL_VALUE_CORE_LIST:
         return "Load Core";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT:
         return "Select File";
      case MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT:
         return "Close";
      case MENU_ENUM_LABEL_VALUE_MANAGEMENT:
         return "Database Settings";
      case MENU_ENUM_LABEL_VALUE_SAVE_STATE:
         return "Save State";
      case MENU_ENUM_LABEL_VALUE_LOAD_STATE:
         return "Load State";
      case MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE:
         return "Undo Load State";
      case MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE:
         return "Undo Save State";
      case MENU_ENUM_LABEL_VALUE_RESUME_CONTENT:
         return "Resume";
      case MENU_ENUM_LABEL_VALUE_INPUT_DRIVER:
         return "Input Driver";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER:
         return "Audio Driver";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER:
         return "Joypad Driver";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Audio Resampler Driver";
      case MENU_ENUM_LABEL_VALUE_RECORD_DRIVER:
         return "Record Driver";
      case MENU_ENUM_LABEL_VALUE_MENU_DRIVER:
         return "Menu Driver";
      case MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER:
         return "Camera Driver";
      case MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER:
         return "Location Driver";
      case MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Unable to read compressed file.";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE:
         return "Overlay Scale";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Preset";
      case MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY:
         return "Audio Latency (ms)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE:
         return "Audio Device";
      case MENU_ENUM_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Keyboard Overlay Preset";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY:
         return "Overlay Opacity";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER:
         return "Menu Wallpaper";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Dynamic Wallpaper";
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS:
         return "Thumbnails";
      case MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Controls";
      case MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS:
         return "Shaders";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Preview Shader Parameters";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Menu Shader Parameters";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Shader Preset Save As";
      case MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No shader parameters.";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Load Shader Preset";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER:
         return "Video Filter";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Audio DSP Plugin";
      case MENU_ENUM_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Starting download: ";
      case MENU_ENUM_LABEL_VALUE_SECONDS:
         return "seconds";
      case MENU_ENUM_LABEL_VALUE_OFF:
         return "OFF";
      case MENU_ENUM_LABEL_VALUE_ON:
         return "ON";
      case MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS:
         return "Update Assets";
      case MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA:
         return "Update Lakka";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS:
         return "Update Cheats";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Update Autoconfig Profiles";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES_HID:
         return "Update Autoconfig Profiles (HID)";
      case MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES:
         return "Update Databases";
      case MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Update Overlays";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Update Cg Shaders";
      case MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Update GLSL Shaders";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Core name";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Core label";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "System name";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "System manufacturer";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Categories";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Authors";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Permissions";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES:
         return "License(s)";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Supported extensions";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Core notes";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Build date";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Git version";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "CPU Features";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Frontend identifier";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Frontend name";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "Frontend OS";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "RetroRating level";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Power source";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "No source";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Charging";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Charged";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Discharging";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Video context driver";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Display metric width (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Display metric height (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Display metric DPI";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "LibretroDB support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Overlay support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Command interface support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT:
         return "Network Gamepad support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Network Command interface support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Cocoa support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "PNG support (RPNG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT:
         return "JPEG support (RJPEG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT:
         return "BMP support (RBMP)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT:
         return "RTGA support (RTGA)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "SDL1.2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "SDL2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT:
         return "Vulkan support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "OpenGL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "OpenGL ES support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Threading support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "KMS/EGL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Udev support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "OpenVG support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "EGL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "X11 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Wayland support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "XVideo support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "ALSA support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "OSS support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "OpenAL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "OpenSL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "RSound support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "RoarAudio support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "JACK support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "PulseAudio support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "DirectSound support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "XAudio2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Zlib support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "7zip support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Dynamic library support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Cg support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "GLSL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "HLSL support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "libxml2 XML parsing support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "SDL image support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "OpenGL/Direct3D render-to-texture (multi-pass shaders) support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT:
         return "Dynamic run-time loading of libretro library";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "FFmpeg support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "CoreText support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "FreeType support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Netplay (peer-to-peer) support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Python (script support in shaders) support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Video4Linux2 support";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Libusb support";
      case MENU_ENUM_LABEL_VALUE_YES:
         return "Yes";
      case MENU_ENUM_LABEL_VALUE_NO:
         return "No";
      case MENU_ENUM_LABEL_VALUE_BACK:
         return "BACK";
      case MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Screen Resolution";
      case MENU_ENUM_LABEL_VALUE_DISABLED:
         return "Disabled";
      case MENU_ENUM_LABEL_VALUE_PORT:
         return "Port";
      case MENU_ENUM_LABEL_VALUE_NONE:
         return "None";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Developer";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Publisher";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Description";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE:
         return "Genre";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Name";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Origin";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franchise";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Releasedate Month";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Releasedate Year";
      case MENU_ENUM_LABEL_VALUE_TRUE:
         return "True";
      case MENU_ENUM_LABEL_VALUE_FALSE:
         return "False";
      case MENU_ENUM_LABEL_VALUE_MISSING:
         return "Missing";
      case MENU_ENUM_LABEL_VALUE_PRESENT:
         return "Present";
      case MENU_ENUM_LABEL_VALUE_OPTIONAL:
         return "Optional";
      case MENU_ENUM_LABEL_VALUE_REQUIRED:
         return "Required";
      case MENU_ENUM_LABEL_VALUE_STATUS:
         return "Status";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS:
         return "Audio";
      case MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS:
         return "Input";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Onscreen Display";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Onscreen Overlay";
      case MENU_ENUM_LABEL_VALUE_MENU_SETTINGS:
         return "Menu";
      case MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Multimedia";
      case MENU_ENUM_LABEL_VALUE_UI_SETTINGS:
         return "User Interface";
      case MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Menu File Browser";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Updater";
      case MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS:
         return "Network";
      case MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES:
         return "Lakka Services";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Playlists";
      case MENU_ENUM_LABEL_VALUE_USER_SETTINGS:
         return "User";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Directory";
      case MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS:
         return "Recording";
      case MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "No information is available.";
      case MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS:
         return "Input User %u Binds";
      case MENU_ENUM_LABEL_VALUE_LANG_ENGLISH:
         return "English";
      case MENU_ENUM_LABEL_VALUE_LANG_JAPANESE:
         return "Japanese";
      case MENU_ENUM_LABEL_VALUE_LANG_FRENCH:
         return "French";
      case MENU_ENUM_LABEL_VALUE_LANG_SPANISH:
         return "Spanish";
      case MENU_ENUM_LABEL_VALUE_LANG_GERMAN:
         return "German";
      case MENU_ENUM_LABEL_VALUE_LANG_ITALIAN:
         return "Italian";
      case MENU_ENUM_LABEL_VALUE_LANG_DUTCH:
         return "Dutch";
      case MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE:
         return "Portuguese";
      case MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN:
         return "Russian";
      case MENU_ENUM_LABEL_VALUE_LANG_KOREAN:
         return "Korean";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Chinese (Traditional)";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Chinese (Simplified)";
      case MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO:
         return "Esperanto";
      case MENU_ENUM_LABEL_VALUE_LEFT_ANALOG:
         return "Left Analog";
      case MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG:
         return "Right Analog";
      case MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Input Hotkey Binds";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Frame Throttle";
      case MENU_ENUM_LABEL_VALUE_SEARCH:
         return "Search:";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Use Builtin Image Viewer";
      case MENU_ENUM_LABEL_VALUE_ENABLE:
         return "Enable";
      case MENU_ENUM_LABEL_VALUE_START_CORE:
         return "Start Core";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR:
         return "Poll Type Behavior";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP:
         return "Scroll Up";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN:
         return "Scroll Down";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM:
         return "Confirm";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK:
         return "Back";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START:
         return "Start";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO:
         return "Info";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU:
         return "Toggle Menu";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT:
         return "Quit";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD:
         return "Toggle Keyboard";
      default:
         break;
   }

   return "null";
}
