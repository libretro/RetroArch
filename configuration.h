/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2016 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __RARCH_CONFIGURATION_H__
#define __RARCH_CONFIGURATION_H__

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include "gfx/video_defines.h"
#include "input/input_defines.h"
#include "led/led_defines.h"

#define configuration_set_float(settings, var, newvar) \
{ \
   settings->modified = true; \
   var = newvar; \
}

#define configuration_set_bool(settings, var, newvar) \
{ \
   settings->modified = true; \
   var = newvar; \
}

#define configuration_set_uint(settings, var, newvar) \
{ \
   settings->modified = true; \
   var = newvar; \
}

#define configuration_set_int(settings, var, newvar) \
{ \
   settings->modified = true; \
   var = newvar; \
}

enum crt_switch_type
{
   CRT_SWITCH_NONE = 0,
   CRT_SWITCH_15KHZ,
   CRT_SWITCH_31KHZ
};

enum override_type
{
   OVERRIDE_NONE = 0,
   OVERRIDE_CORE,
   OVERRIDE_CONTENT_DIR,
   OVERRIDE_GAME
};

RETRO_BEGIN_DECLS

typedef struct settings
{
   struct
   {
      bool placeholder;

      /* Video */
      bool video_fullscreen;
      bool video_windowed_fullscreen;
      bool video_vsync;
      bool video_adaptive_vsync;
      bool video_hard_sync;
      bool video_black_frame_insertion;
      bool video_vfilter;
      bool video_smooth;
      bool video_force_aspect;
      bool video_crop_overscan;
      bool video_aspect_ratio_auto;
      bool video_scale_integer;
      bool video_shader_enable;
      bool video_shader_watch_files;
      bool video_threaded;
      bool video_font_enable;
      bool video_disable_composition;
      bool video_post_filter_record;
      bool video_gpu_record;
      bool video_gpu_screenshot;
      bool video_allow_rotate;
      bool video_shared_context;
      bool video_force_srgb_disable;
      bool video_fps_show;
      bool video_statistics_show;
      bool video_framecount_show;
      bool video_memory_show;
      bool video_msg_bgcolor_enable;
      bool video_3ds_lcd_bottom;
#ifdef HAVE_VIDEO_LAYOUT
      bool video_layout_enable;
#endif

      /* Audio */
      bool audio_enable;
      bool audio_enable_menu;
      bool audio_enable_menu_ok;
      bool audio_enable_menu_cancel;
      bool audio_enable_menu_notice;
      bool audio_enable_menu_bgm;
      bool audio_sync;
      bool audio_rate_control;
      bool audio_wasapi_exclusive_mode;
      bool audio_wasapi_float_format;

      /* Input */
      bool input_remap_binds_enable;
      bool input_autodetect_enable;
      bool input_overlay_enable;
      bool input_overlay_enable_autopreferred;
      bool input_overlay_hide_in_menu;
      bool input_overlay_show_physical_inputs;
      bool input_overlay_show_mouse_cursor;
      bool input_descriptor_label_show;
      bool input_descriptor_hide_unbound;
      bool input_all_users_control_menu;
      bool input_menu_swap_ok_cancel_buttons;
      bool input_backtouch_enable;
      bool input_backtouch_toggle;
      bool input_small_keyboard_enable;
      bool input_keyboard_gamepad_enable;

      /* Frame time counter */
      bool frame_time_counter_reset_after_fastforwarding;
      bool frame_time_counter_reset_after_load_state;
      bool frame_time_counter_reset_after_save_state;

      /* Menu */
      bool filter_by_current_core;
      bool menu_enable_widgets;
      bool menu_show_start_screen;
      bool menu_pause_libretro;
      bool menu_savestate_resume;
      bool menu_timedate_enable;
      bool menu_battery_level_enable;
      bool menu_core_enable;
      bool menu_show_sublabels;
      bool menu_dynamic_wallpaper_enable;
      bool menu_throttle;
      bool menu_mouse_enable;
      bool menu_pointer_enable;
      bool menu_navigation_wraparound_enable;
      bool menu_navigation_browser_filter_supported_extensions_enable;
      bool menu_dpi_override_enable;
      bool menu_show_advanced_settings;
      bool menu_throttle_framerate;
      bool menu_linear_filter;
      bool menu_horizontal_animation;
      bool menu_show_online_updater;
      bool menu_show_core_updater;
      bool menu_show_load_core;
      bool menu_show_load_content;
      bool menu_show_load_disc;
      bool menu_show_dump_disc;
      bool menu_show_information;
      bool menu_show_configurations;
      bool menu_show_help;
      bool menu_show_quit_retroarch;
      bool menu_show_restart_retroarch;
      bool menu_show_reboot;
      bool menu_show_shutdown;
      bool menu_show_latency;
      bool menu_show_rewind;
      bool menu_show_overlays;
      bool menu_show_legacy_thumbnail_updater;
#ifdef HAVE_VIDEO_LAYOUT
      bool menu_show_video_layout;
#endif
      bool menu_materialui_icons_enable;
      bool menu_rgui_background_filler_thickness_enable;
      bool menu_rgui_border_filler_thickness_enable;
      bool menu_rgui_border_filler_enable;
      bool menu_rgui_full_width_layout;
      bool menu_rgui_shadows;
      bool menu_rgui_inline_thumbnails;
      bool menu_rgui_swap_thumbnails;
      bool menu_rgui_extended_ascii;
      bool menu_xmb_shadows_enable;
      bool menu_xmb_vertical_thumbnails;
      bool menu_content_show_settings;
      bool menu_content_show_favorites;
      bool menu_content_show_images;
      bool menu_content_show_music;
      bool menu_content_show_video;
      bool menu_content_show_netplay;
      bool menu_content_show_history;
      bool menu_content_show_add;
      bool menu_content_show_playlists;
      bool menu_use_preferred_system_color_theme;
      bool menu_preferred_system_color_theme_set;
      bool menu_unified_controls;
      bool menu_ticker_smooth;
      bool settings_show_drivers;
      bool settings_show_video;
      bool settings_show_audio;
      bool settings_show_input;
      bool settings_show_latency;
      bool settings_show_core;
      bool settings_show_configuration;
      bool settings_show_saving;
      bool settings_show_logging;
      bool settings_show_frame_throttle;
      bool settings_show_recording;
      bool settings_show_onscreen_display;
      bool settings_show_user_interface;
      bool settings_show_ai_service;
      bool settings_show_power_management;
      bool settings_show_achievements;
      bool settings_show_network;
      bool settings_show_playlists;
      bool settings_show_user;
      bool settings_show_directory;
      bool quick_menu_show_resume_content;
      bool quick_menu_show_restart_content;
      bool quick_menu_show_close_content;
      bool quick_menu_show_take_screenshot;
      bool quick_menu_show_save_load_state;
      bool quick_menu_show_undo_save_load_state;
      bool quick_menu_show_add_to_favorites;
      bool quick_menu_show_start_recording;
      bool quick_menu_show_start_streaming;
      bool quick_menu_show_set_core_association;
      bool quick_menu_show_reset_core_association;
      bool quick_menu_show_options;
      bool quick_menu_show_controls;
      bool quick_menu_show_cheats;
      bool quick_menu_show_shaders;
      bool quick_menu_show_save_core_overrides;
      bool quick_menu_show_save_game_overrides;
      bool quick_menu_show_save_content_dir_overrides;
      bool quick_menu_show_information;
      bool quick_menu_show_recording;
      bool quick_menu_show_streaming;
      bool quick_menu_show_download_thumbnails;
      bool kiosk_mode_enable;

      bool crt_switch_custom_refresh_enable;

      /* Netplay */
      bool netplay_public_announce;
      bool netplay_start_as_spectator;
      bool netplay_allow_slaves;
      bool netplay_require_slaves;
      bool netplay_stateless_mode;
      bool netplay_nat_traversal;
      bool netplay_use_mitm_server;
      bool netplay_request_devices[MAX_USERS];

      /* Network */
      bool network_buildbot_auto_extract_archive;
      bool network_on_demand_thumbnails;

      /* UI */
      bool ui_menubar_enable;
      bool ui_suspend_screensaver_enable;
      bool ui_companion_start_on_boot;
      bool ui_companion_enable;
      bool ui_companion_toggle;
      bool desktop_menu_enable;

      /* Cheevos */
      bool cheevos_enable;
      bool cheevos_test_unofficial;
      bool cheevos_hardcore_mode_enable;
      bool cheevos_leaderboards_enable;
      bool cheevos_badges_enable;
      bool cheevos_verbose_enable;
      bool cheevos_auto_screenshot;

      /* Camera */
      bool camera_allow;

      /* WiFi */
      bool wifi_allow;

      /* Location */
      bool location_allow;

      /* Multimedia */
      bool multimedia_builtin_mediaplayer_enable;
      bool multimedia_builtin_imageviewer_enable;

      /* Bundle */
      bool bundle_finished;
      bool bundle_assets_extract_enable;

      /* Misc. */
      bool discord_enable;
      bool threaded_data_runloop_enable;
      bool set_supports_no_game_enable;
      bool auto_screenshot_filename;
      bool history_list_enable;
      bool playlist_entry_rename;
      bool rewind_enable;
      bool vrr_runloop_enable;
      bool apply_cheats_after_toggle;
      bool apply_cheats_after_load;
      bool run_ahead_enabled;
      bool run_ahead_secondary_instance;
      bool run_ahead_hide_warnings;
      bool pause_nonactive;
      bool block_sram_overwrite;
      bool savestate_auto_index;
      bool savestate_auto_save;
      bool savestate_auto_load;
      bool savestate_thumbnail_enable;
      bool network_cmd_enable;
      bool stdin_cmd_enable;
      bool keymapper_enable;
      bool network_remote_enable;
      bool network_remote_enable_user[MAX_USERS];
      bool load_dummy_on_core_shutdown;
      bool check_firmware_before_loading;

      bool game_specific_options;
      bool auto_overrides_enable;
      bool auto_remaps_enable;
      bool global_core_options;
      bool auto_shaders_enable;

      bool sort_savefiles_enable;
      bool sort_savestates_enable;
      bool config_save_on_exit;
      bool show_hidden_files;

      bool savefiles_in_content_dir;
      bool savestates_in_content_dir;
      bool screenshots_in_content_dir;
      bool systemfiles_in_content_dir;
      bool ssh_enable;
      bool samba_enable;
      bool bluetooth_enable;

      bool automatically_add_content_to_playlist;
      bool video_window_show_decorations;
      bool video_window_save_positions;

      bool sustained_performance_mode;
      bool playlist_use_old_format;
      bool content_runtime_log;
      bool content_runtime_log_aggregate;

      bool playlist_sort_alphabetical;
      bool playlist_show_sublabels;
      bool playlist_fuzzy_archive_match;

      bool quit_press_twice;
      bool vibrate_on_keypress;
      bool enable_device_vibration;
      bool ozone_collapse_sidebar;
      bool ozone_truncate_playlist_name;
      bool ozone_scroll_content_metadata;

      bool log_to_file;
      bool log_to_file_timestamp;

      bool scan_without_core_match;

      bool ai_service_enable;
   } bools;

   struct
   {
      float placeholder;
      float video_scale;
      float video_aspect_ratio;
      float video_refresh_rate;
      float crt_video_refresh_rate;
      float video_font_size;
      float video_msg_pos_x;
      float video_msg_pos_y;
      float video_msg_color_r;
      float video_msg_color_g;
      float video_msg_color_b;
      float video_msg_bgcolor_opacity;

      float menu_wallpaper_opacity;
      float menu_framebuffer_opacity;
      float menu_footer_opacity;
      float menu_header_opacity;
      float menu_ticker_speed;
      float menu_rgui_particle_effect_speed;

      float audio_max_timing_skew;
      float audio_volume; /* dB scale. */
      float audio_mixer_volume; /* dB scale. */

      float input_overlay_opacity;
      float input_overlay_scale;

      float slowmotion_ratio;
      float fastforward_ratio;
      float input_analog_deadzone;
      float input_analog_sensitivity;
   } floats;

   struct
   {
      int placeholder;
      int netplay_check_frames;
      int location_update_interval_ms;
      int location_update_interval_distance;
      int state_slot;
      int audio_wasapi_sh_buffer_length;
      int crt_switch_center_adjust;
#ifdef HAVE_VULKAN
      int vulkan_gpu_index;
#endif
#ifdef HAVE_D3D10
      int d3d10_gpu_index;
#endif
#ifdef HAVE_D3D11
      int d3d11_gpu_index;
#endif
#ifdef HAVE_D3D12
      int d3d12_gpu_index;
#endif
      int content_favorites_size;
   } ints;

   struct
   {
      unsigned placeholder;
      unsigned audio_out_rate;
      unsigned audio_block_frames;
      unsigned audio_latency;

      unsigned fps_update_interval;

      unsigned input_block_timeout;

      unsigned audio_resampler_quality;

      unsigned input_turbo_period;
      unsigned input_turbo_duty_cycle;

      unsigned input_bind_timeout;
      unsigned input_bind_hold;
#ifdef GEKKO
      unsigned input_mouse_scale;
#endif
      unsigned input_menu_toggle_gamepad_combo;
      unsigned input_keyboard_gamepad_mapping_type;
      unsigned input_poll_type_behavior;
      unsigned netplay_port;
      unsigned netplay_input_latency_frames_min;
      unsigned netplay_input_latency_frames_range;
      unsigned netplay_share_digital;
      unsigned netplay_share_analog;
      unsigned bundle_assets_extract_version_current;
      unsigned bundle_assets_extract_last_version;
      unsigned content_history_size;
      unsigned frontend_log_level;
      unsigned libretro_log_level;
      unsigned rewind_granularity;
      unsigned rewind_buffer_size_step;
      unsigned autosave_interval;
      unsigned network_cmd_port;
      unsigned network_remote_base_port;
      unsigned keymapper_port;
      unsigned video_window_opacity;
      unsigned crt_switch_resolution;
      unsigned crt_switch_resolution_super;
      unsigned video_monitor_index;
      unsigned video_fullscreen_x;
      unsigned video_fullscreen_y;
      unsigned video_max_swapchain_images;
      unsigned video_swap_interval;
      unsigned video_hard_sync_frames;
      unsigned video_frame_delay;
      unsigned video_viwidth;
      unsigned video_aspect_ratio_idx;
      unsigned video_rotation;
      unsigned screen_orientation;
      unsigned video_msg_bgcolor_red;
      unsigned video_msg_bgcolor_green;
      unsigned video_msg_bgcolor_blue;
      unsigned video_stream_port;
      unsigned video_record_quality;
      unsigned video_stream_quality;
      unsigned video_record_scale_factor;
      unsigned video_stream_scale_factor;
      unsigned video_3ds_display_mode;
#ifdef HAVE_VIDEO_LAYOUT
      unsigned video_layout_selected_view;
#endif
#ifdef GEKKO
      unsigned video_overscan_correction_top;
      unsigned video_overscan_correction_bottom;
#endif
      unsigned video_shader_delay;

      unsigned menu_timedate_style;
      unsigned menu_thumbnails;
      unsigned menu_left_thumbnails;
      unsigned menu_thumbnail_upscale_threshold;
      unsigned menu_rgui_thumbnail_downscaler;
      unsigned menu_rgui_thumbnail_delay;
      unsigned menu_dpi_override_value;
      unsigned menu_rgui_color_theme;
      unsigned menu_xmb_animation_opening_main_menu;
      unsigned menu_xmb_animation_horizontal_highlight;
      unsigned menu_xmb_animation_move_up_down;
      unsigned menu_xmb_layout;
      unsigned menu_xmb_shader_pipeline;
      unsigned menu_xmb_scale_factor;
      unsigned menu_xmb_alpha_factor;
      unsigned menu_xmb_theme;
      unsigned menu_xmb_color_theme;
      unsigned menu_xmb_thumbnail_scale_factor;
      unsigned menu_materialui_color_theme;
      unsigned menu_ozone_color_theme;
      unsigned menu_font_color_red;
      unsigned menu_font_color_green;
      unsigned menu_font_color_blue;
      unsigned menu_rgui_internal_upscale_level;
      unsigned menu_rgui_aspect_ratio;
      unsigned menu_rgui_aspect_ratio_lock;
      unsigned menu_rgui_particle_effect;
      unsigned menu_ticker_type;

      unsigned playlist_entry_remove_enable;
      unsigned playlist_show_inline_core_name;
      unsigned playlist_sublabel_runtime_type;
      unsigned playlist_sublabel_last_played_style;

      unsigned camera_width;
      unsigned camera_height;

      unsigned input_overlay_show_physical_inputs_port;

      unsigned input_split_joycon[MAX_USERS];
      unsigned input_joypad_map[MAX_USERS];
      unsigned input_device[MAX_USERS];
      unsigned input_mouse_index[MAX_USERS];
      /* Set by autoconfiguration in joypad_autoconfig_dir.
       * Does not override main binds. */
      unsigned input_libretro_device[MAX_USERS];
      unsigned input_analog_dpad_mode[MAX_USERS];

      unsigned input_keymapper_ids[MAX_USERS][RARCH_CUSTOM_BIND_LIST_END];

      unsigned input_remap_ids[MAX_USERS][RARCH_CUSTOM_BIND_LIST_END];

      unsigned led_map[MAX_LEDS];

      unsigned run_ahead_frames;

      unsigned midi_volume;
      unsigned streaming_mode;

      unsigned window_position_x;
      unsigned window_position_y;
      unsigned window_position_width;
      unsigned window_position_height;

      unsigned video_record_threads;

      unsigned libnx_overclock;
      unsigned ai_service_mode;
      unsigned ai_service_target_lang;
      unsigned ai_service_source_lang;
   } uints;

   struct
   {
      size_t placeholder;
      size_t rewind_buffer_size;
   } sizes;

   struct
   {
      char placeholder;

      char video_driver[32];
      char record_driver[32];
      char camera_driver[32];
      char wifi_driver[32];
      char led_driver[32];
      char location_driver[32];
      char menu_driver[32];
      char cheevos_username[32];
      char cheevos_password[32];
      char cheevos_token[32];
      char video_context_driver[32];
      char audio_driver[32];
      char audio_resampler[32];
      char input_driver[32];
      char input_joypad_driver[32];
      char midi_driver[32];

      char input_keyboard_layout[64];

      char audio_device[255];
      char camera_device[255];

      char bundle_assets_src[PATH_MAX_LENGTH];
      char bundle_assets_dst[PATH_MAX_LENGTH];
      char bundle_assets_dst_subdir[PATH_MAX_LENGTH];

      char netplay_mitm_server[255];

      char midi_input[32];
      char midi_output[32];

      char youtube_stream_key[PATH_MAX_LENGTH];
      char twitch_stream_key[PATH_MAX_LENGTH];

      char discord_app_id[PATH_MAX_LENGTH];
      char translation_service_url[2048];
      char ai_service_url[PATH_MAX_LENGTH];
   } arrays;

   struct
   {
      char placeholder;

      char username[32];
      char netplay_password[128];
      char netplay_spectate_password[128];
      char netplay_server[255];
      char network_buildbot_url[255];
      char network_buildbot_assets_url[255];
      char browse_url[4096];
      char path_stream_url[8192];

      char path_menu_xmb_font[PATH_MAX_LENGTH];
      char menu_content_show_settings_password[PATH_MAX_LENGTH];
      char kiosk_mode_password[PATH_MAX_LENGTH];
      char path_cheat_database[PATH_MAX_LENGTH];
      char path_content_database[PATH_MAX_LENGTH];
      char path_overlay[PATH_MAX_LENGTH];
#ifdef HAVE_VIDEO_LAYOUT
      char path_video_layout[PATH_MAX_LENGTH];
#endif
      char path_record_config[PATH_MAX_LENGTH];
      char path_stream_config[PATH_MAX_LENGTH];
      char path_menu_wallpaper[PATH_MAX_LENGTH];
      char path_audio_dsp_plugin[PATH_MAX_LENGTH];
      char path_softfilter_plugin[PATH_MAX_LENGTH];
      char path_core_options[PATH_MAX_LENGTH];
      char path_content_history[PATH_MAX_LENGTH];
      char path_content_favorites[PATH_MAX_LENGTH];
      char path_content_music_history[PATH_MAX_LENGTH];
      char path_content_image_history[PATH_MAX_LENGTH];
      char path_content_video_history[PATH_MAX_LENGTH];
      char path_libretro_info[PATH_MAX_LENGTH];
      char path_cheat_settings[PATH_MAX_LENGTH];
      char path_font[PATH_MAX_LENGTH];
      char path_rgui_theme_preset[PATH_MAX_LENGTH];

      char directory_audio_filter[PATH_MAX_LENGTH];
      char directory_autoconfig[PATH_MAX_LENGTH];
      char directory_video_filter[PATH_MAX_LENGTH];
      char directory_video_shader[PATH_MAX_LENGTH];
      char directory_content_history[PATH_MAX_LENGTH];
      char directory_content_favorites[PATH_MAX_LENGTH];
      char directory_libretro[PATH_MAX_LENGTH];
      char directory_cursor[PATH_MAX_LENGTH];
      char directory_input_remapping[PATH_MAX_LENGTH];
      char directory_overlay[PATH_MAX_LENGTH];
#ifdef HAVE_VIDEO_LAYOUT
      char directory_video_layout[PATH_MAX_LENGTH];
#endif
      char directory_resampler[PATH_MAX_LENGTH];
      char directory_screenshot[PATH_MAX_LENGTH];
      char directory_system[PATH_MAX_LENGTH];
      char directory_cache[PATH_MAX_LENGTH];
      char directory_playlist[PATH_MAX_LENGTH];
      char directory_runtime_log[PATH_MAX_LENGTH];
      char directory_core_assets[PATH_MAX_LENGTH];
      char directory_assets[PATH_MAX_LENGTH];
      char directory_dynamic_wallpapers[PATH_MAX_LENGTH];
      char directory_thumbnails[PATH_MAX_LENGTH];
      char directory_menu_config[PATH_MAX_LENGTH];
      char directory_menu_content[PATH_MAX_LENGTH];
      char streaming_title[PATH_MAX_LENGTH];

      char log_dir[PATH_MAX_LENGTH];
   } paths;

   bool modified;

   video_viewport_t video_viewport_custom;

} settings_t;

/**
 * config_get_default_camera:
 *
 * Gets default camera driver.
 *
 * Returns: Default camera driver.
 **/
const char *config_get_default_camera(void);

/**
 * config_get_default_wifi:
 *
 * Gets default wifi driver.
 *
 * Returns: Default wifi driver.
 **/
const char *config_get_default_wifi(void);

/**
 * config_get_default_location:
 *
 * Gets default location driver.
 *
 * Returns: Default location driver.
 **/
const char *config_get_default_location(void);

/**
 * config_get_default_video:
 *
 * Gets default video driver.
 *
 * Returns: Default video driver.
 **/
const char *config_get_default_video(void);

/**
 * config_get_default_audio:
 *
 * Gets default audio driver.
 *
 * Returns: Default audio driver.
 **/
const char *config_get_default_audio(void);

/**
 * config_get_default_audio_resampler:
 *
 * Gets default audio resampler driver.
 *
 * Returns: Default audio resampler driver.
 **/
const char *config_get_default_audio_resampler(void);

/**
 * config_get_default_input:
 *
 * Gets default input driver.
 *
 * Returns: Default input driver.
 **/
const char *config_get_default_input(void);

/**
 * config_get_default_joypad:
 *
 * Gets default input joypad driver.
 *
 * Returns: Default input joypad driver.
 **/
const char *config_get_default_joypad(void);

/**
 * config_get_default_menu:
 *
 * Gets default menu driver.
 *
 * Returns: Default menu driver.
 **/
const char *config_get_default_menu(void);

const char *config_get_default_midi(void);
const char *config_get_midi_driver_options(void);

const char *config_get_default_record(void);

/**
 * config_parse_file:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_parse_file(void);

/**
 * config_load_override:
 *
 * Tries to append game-specific and core-specific configuration.
 * These settings will always have precedence, thus this feature
 * can be used to enforce overrides.
 *
 * Returns: false if there was an error or no action was performed.
 *
 */
bool config_load_override(void);

/**
 * config_unload_override:
 *
 * Unloads configuration overrides if overrides are active.
 *
 *
 * Returns: false if there was an error.
 */
bool config_unload_override(void);

/**
 * config_load_remap:
 *
 * Tries to append game-specific and core-specific remap files.
 *
 * Returns: false if there was an error or no action was performed.
 *
 */
bool config_load_remap(const char *directory_input_remapping);

/**
 * config_save_autoconf_profile:
 * @path            : Path that shall be written to.
 * @user              : Controller number to save
 * Writes a controller autoconf file to disk.
 **/
bool config_save_autoconf_profile(const char *path, unsigned user);

/**
 * config_save_file:
 * @path            : Path that shall be written to.
 *
 * Writes a config file to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_file(const char *path);

/**
 * config_save_overrides:
 * @path            : Path that shall be written to.
 *
 * Writes a config file override to disk.
 *
 * Returns: true (1) on success, otherwise returns false (0).
 **/
bool config_save_overrides(int override_type);

/* Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly. */
bool config_replace(bool config_save_on_exit, char *path);

bool config_overlay_enable_default(void);

void config_set_defaults(void);

settings_t *config_get_ptr(void);

RETRO_END_DECLS

#endif
