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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gfx/video_defines.h"
#include "led/led_defines.h"

#ifdef HAVE_LAKKA
#include "lakka.h"
#endif

#include "msg_hash.h"

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

#define configuration_set_string(settings, var, newvar) \
{ \
   settings->modified = true; \
   strlcpy(var, newvar, sizeof(var)); \
}

RETRO_BEGIN_DECLS

enum crt_switch_type
{
   CRT_SWITCH_NONE = 0,
   CRT_SWITCH_15KHZ,
   CRT_SWITCH_31KHZ,
   CRT_SWITCH_32_120,
   CRT_SWITCH_INI
};

enum override_type
{
   OVERRIDE_NONE = 0,
   OVERRIDE_AS,
   OVERRIDE_CORE,
   OVERRIDE_CONTENT_DIR,
   OVERRIDE_GAME
};

typedef struct settings
{
   struct
   {
      size_t placeholder;
      size_t rewind_buffer_size;
   } sizes;

   video_viewport_t video_viewport_custom; /* int alignment */

   struct
   {
      int placeholder;
      int netplay_check_frames;
      int location_update_interval_ms;
      int location_update_interval_distance;
      int state_slot;
      int replay_slot;
      int crt_switch_center_adjust;
      int crt_switch_porch_adjust;
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
#ifdef HAVE_WINDOW_OFFSET
      int video_window_offset_x;
      int video_window_offset_y;
#endif
      int content_favorites_size;
#ifdef _3DS
      int bottom_font_color_red;
      int bottom_font_color_green;
      int bottom_font_color_blue;
      int bottom_font_color_opacity;
#endif
#ifdef HAVE_XMB
      int menu_xmb_title_margin;
      int menu_xmb_title_margin_horizontal_offset;
#endif
   } ints;

   struct
   {
      unsigned placeholder;

      unsigned input_split_joycon[MAX_USERS];
      unsigned input_joypad_index[MAX_USERS];
      unsigned input_device[MAX_USERS];
      unsigned input_mouse_index[MAX_USERS];

      unsigned input_libretro_device[MAX_USERS];
      unsigned input_analog_dpad_mode[MAX_USERS];

      unsigned input_remap_ports[MAX_USERS];
      unsigned input_remap_ids[MAX_USERS][RARCH_CUSTOM_BIND_LIST_END];
      unsigned input_keymapper_ids[MAX_USERS][RARCH_CUSTOM_BIND_LIST_END];
      unsigned input_remap_port_map[MAX_USERS][MAX_USERS + 1];

      unsigned led_map[MAX_LEDS];

      unsigned audio_output_sample_rate;
      unsigned audio_block_frames;
      unsigned audio_latency;

#ifdef HAVE_WASAPI
      unsigned audio_wasapi_sh_buffer_length;
#endif

#ifdef HAVE_MICROPHONE
      unsigned microphone_sample_rate;
      unsigned microphone_block_frames;
      unsigned microphone_latency;
      unsigned microphone_resampler_quality;
#ifdef HAVE_WASAPI
      unsigned microphone_wasapi_sh_buffer_length;
#endif
#endif

      unsigned fps_update_interval;
      unsigned memory_update_interval;

      unsigned input_block_timeout;

      unsigned audio_resampler_quality;

      unsigned input_turbo_period;
      unsigned input_turbo_duty_cycle;
      unsigned input_turbo_mode;
      unsigned input_turbo_default_button;

      unsigned input_bind_timeout;
      unsigned input_bind_hold;
#ifdef GEKKO
      unsigned input_mouse_scale;
#endif
      unsigned input_touch_scale;
      unsigned input_hotkey_block_delay;
      unsigned input_quit_gamepad_combo;
      unsigned input_menu_toggle_gamepad_combo;
      unsigned input_keyboard_gamepad_mapping_type;
      unsigned input_poll_type_behavior;
      unsigned input_rumble_gain;
      unsigned input_auto_game_focus;
      unsigned input_max_users;

      unsigned netplay_port;
      unsigned netplay_max_connections;
      unsigned netplay_max_ping;
      unsigned netplay_chat_color_name;
      unsigned netplay_chat_color_msg;
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
      unsigned replay_checkpoint_interval;
      unsigned replay_max_keep;
      unsigned savestate_max_keep;
      unsigned network_cmd_port;
      unsigned network_remote_base_port;
      unsigned keymapper_port;
      unsigned video_window_opacity;
      unsigned crt_switch_resolution;
      unsigned crt_switch_resolution_super;
      unsigned screen_brightness;
      unsigned video_monitor_index;
      unsigned video_fullscreen_x;
      unsigned video_fullscreen_y;
      unsigned video_scale;
      unsigned video_max_swapchain_images;
      unsigned video_max_frame_latency;
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
      unsigned video_dingux_ipu_filter_type;
      unsigned video_dingux_refresh_rate;
      unsigned video_dingux_rs90_softfilter_type;
#ifdef GEKKO
      unsigned video_overscan_correction_top;
      unsigned video_overscan_correction_bottom;
#endif
      unsigned video_shader_delay;
#ifdef HAVE_SCREENSHOTS
      unsigned notification_show_screenshot_duration;
      unsigned notification_show_screenshot_flash;
#endif

      /* Accessibility */
      unsigned accessibility_narrator_speech_speed;

      unsigned menu_timedate_style;
      unsigned menu_timedate_date_separator;
      unsigned gfx_thumbnails;
      unsigned menu_left_thumbnails;
      unsigned gfx_thumbnail_upscale_threshold;
      unsigned menu_rgui_thumbnail_downscaler;
      unsigned menu_rgui_thumbnail_delay;
      unsigned menu_rgui_color_theme;
      unsigned menu_xmb_animation_opening_main_menu;
      unsigned menu_xmb_animation_horizontal_highlight;
      unsigned menu_xmb_animation_move_up_down;
      unsigned menu_xmb_layout;
      unsigned menu_xmb_shader_pipeline;
      unsigned menu_xmb_alpha_factor;
      unsigned menu_xmb_theme;
      unsigned menu_xmb_color_theme;
      unsigned menu_xmb_thumbnail_scale_factor;
      unsigned menu_xmb_vertical_fade_factor;
      unsigned menu_materialui_color_theme;
      unsigned menu_materialui_transition_animation;
      unsigned menu_materialui_thumbnail_view_portrait;
      unsigned menu_materialui_thumbnail_view_landscape;
      unsigned menu_materialui_landscape_layout_optimization;
      unsigned menu_ozone_color_theme;
      unsigned menu_font_color_red;
      unsigned menu_font_color_green;
      unsigned menu_font_color_blue;
      unsigned menu_rgui_internal_upscale_level;
      unsigned menu_rgui_aspect_ratio;
      unsigned menu_rgui_aspect_ratio_lock;
      unsigned menu_rgui_particle_effect;
      unsigned menu_ticker_type;
      unsigned menu_scroll_delay;
      unsigned menu_content_show_add_entry;
      unsigned menu_content_show_contentless_cores;
      unsigned menu_screensaver_timeout;
      unsigned menu_screensaver_animation;
      unsigned menu_remember_selection;

      unsigned playlist_entry_remove_enable;
      unsigned playlist_show_inline_core_name;
      unsigned playlist_show_history_icons;
      unsigned playlist_sublabel_runtime_type;
      unsigned playlist_sublabel_last_played_style;

      unsigned camera_width;
      unsigned camera_height;

#ifdef HAVE_OVERLAY
      unsigned input_overlay_show_inputs;
      unsigned input_overlay_show_inputs_port;
      unsigned input_overlay_dpad_diagonal_sensitivity;
      unsigned input_overlay_abxy_diagonal_sensitivity;
#endif

      unsigned run_ahead_frames;

      unsigned midi_volume;
      unsigned streaming_mode;

      unsigned window_position_x;
      unsigned window_position_y;
      unsigned window_position_width;
      unsigned window_position_height;
      unsigned window_auto_width_max;
      unsigned window_auto_height_max;

      unsigned video_record_threads;

      unsigned libnx_overclock;
      unsigned ai_service_mode;
      unsigned ai_service_target_lang;
      unsigned ai_service_source_lang;
      unsigned ai_service_poll_delay;
      unsigned ai_service_text_position;
      unsigned ai_service_text_padding;

      unsigned core_updater_auto_backup_history_size;
      unsigned video_black_frame_insertion;
      unsigned video_autoswitch_refresh_rate;
      unsigned quit_on_close_content;

#ifdef HAVE_LAKKA
      unsigned cpu_scaling_mode;
      unsigned cpu_min_freq;
      unsigned cpu_max_freq;
#endif

#ifdef HAVE_MIST
      unsigned steam_rich_presence_format;
#endif

      unsigned cheevos_appearance_anchor;
      unsigned cheevos_visibility_summary;
   } uints;

   struct
   {
      float placeholder;
      float video_aspect_ratio;
      float video_refresh_rate;
      float video_autoswitch_pal_threshold;
      float crt_video_refresh_rate;
      float video_font_size;
      float video_msg_pos_x;
      float video_msg_pos_y;
      float video_msg_color_r;
      float video_msg_color_g;
      float video_msg_color_b;
      float video_msg_bgcolor_opacity;
      float video_hdr_max_nits;
      float video_hdr_paper_white_nits;
      float video_hdr_display_contrast;

      float menu_scale_factor;
      float menu_widget_scale_factor;
      float menu_widget_scale_factor_windowed;
      float menu_wallpaper_opacity;
      float menu_framebuffer_opacity;
      float menu_footer_opacity;
      float menu_header_opacity;
      float menu_ticker_speed;
      float menu_rgui_particle_effect_speed;
      float menu_screensaver_animation_speed;
      float ozone_thumbnail_scale_factor;

      float cheevos_appearance_padding_h;
      float cheevos_appearance_padding_v;

      float audio_max_timing_skew;
      float audio_volume; /* dB scale. */
      float audio_mixer_volume; /* dB scale. */

      float input_overlay_opacity;
      float input_osk_overlay_opacity;

      float input_overlay_scale_landscape;
      float input_overlay_aspect_adjust_landscape;
      float input_overlay_x_separation_landscape;
      float input_overlay_y_separation_landscape;
      float input_overlay_x_offset_landscape;
      float input_overlay_y_offset_landscape;

      float input_overlay_scale_portrait;
      float input_overlay_aspect_adjust_portrait;
      float input_overlay_x_separation_portrait;
      float input_overlay_y_separation_portrait;
      float input_overlay_x_offset_portrait;
      float input_overlay_y_offset_portrait;

      float slowmotion_ratio;
      float fastforward_ratio;
      float input_analog_deadzone;
      float input_axis_threshold;
      float input_analog_sensitivity;
#ifdef _3DS
      float bottom_font_scale;
#endif
   } floats;

   struct
   {
      char placeholder;

      char video_driver[32];
      char record_driver[32];
      char camera_driver[32];
      char bluetooth_driver[32];
      char wifi_driver[32];
      char led_driver[32];
      char location_driver[32];
      char cloud_sync_driver[32];
      char menu_driver[32];
      char cheevos_username[32];
      char cheevos_password[256];
      char cheevos_token[32];
      char cheevos_leaderboards_enable[32];
      char cheevos_custom_host[64];
      char video_context_driver[32];
      char audio_driver[32];
      char audio_resampler[32];
      char input_driver[32];
      char input_joypad_driver[32];
      char midi_driver[32];
      char midi_input[32];
      char midi_output[32];

      char input_keyboard_layout[64];

#ifdef HAVE_MICROPHONE
      char microphone_driver[32];
      char microphone_resampler[32];
      char microphone_device[255];
#endif

#ifdef ANDROID
      char input_android_physical_keyboard[255];
#endif

      char audio_device[255];
      char camera_device[255];
      char netplay_mitm_server[255];

      char translation_service_url[2048];

      char webdav_url[255];
      char webdav_username[255];
      char webdav_password[255];

      char youtube_stream_key[PATH_MAX_LENGTH];
      char twitch_stream_key[PATH_MAX_LENGTH];
      char facebook_stream_key[PATH_MAX_LENGTH];
      char discord_app_id[PATH_MAX_LENGTH];
      char ai_service_url[PATH_MAX_LENGTH];

      char crt_switch_timings[255];
#ifdef HAVE_LAKKA
      char timezone[TIMEZONE_LENGTH];
      char cpu_main_gov[32];
      char cpu_menu_gov[32];
#endif
   } arrays;

   struct
   {
      char placeholder;

      char username[32];

      char netplay_password[128];
      char netplay_spectate_password[128];

      char netplay_server[255];
      char netplay_custom_mitm_server[255];
      char network_buildbot_url[255];
      char network_buildbot_assets_url[255];

      char browse_url[4096];

      char path_stream_url[8192];

      char bundle_assets_src[PATH_MAX_LENGTH];
      char bundle_assets_dst[PATH_MAX_LENGTH];
      char bundle_assets_dst_subdir[PATH_MAX_LENGTH];
      char path_menu_xmb_font[PATH_MAX_LENGTH];
      char menu_content_show_settings_password[PATH_MAX_LENGTH];
      char kiosk_mode_password[PATH_MAX_LENGTH];
      char path_cheat_database[PATH_MAX_LENGTH];
      char path_content_database[PATH_MAX_LENGTH];
      char path_overlay[PATH_MAX_LENGTH];
      char path_osk_overlay[PATH_MAX_LENGTH];
      char path_record_config[PATH_MAX_LENGTH];
      char path_stream_config[PATH_MAX_LENGTH];
      char path_menu_wallpaper[PATH_MAX_LENGTH];
      char path_audio_dsp_plugin[PATH_MAX_LENGTH];
      char path_softfilter_plugin[PATH_MAX_LENGTH];
      char path_core_options[PATH_MAX_LENGTH];
      char path_content_favorites[PATH_MAX_LENGTH];
      char path_content_history[PATH_MAX_LENGTH];
      char path_content_image_history[PATH_MAX_LENGTH];
      char path_content_music_history[PATH_MAX_LENGTH];
      char path_content_video_history[PATH_MAX_LENGTH];
      char path_libretro_info[PATH_MAX_LENGTH];
      char path_cheat_settings[PATH_MAX_LENGTH];
      char path_font[PATH_MAX_LENGTH];
      char path_rgui_theme_preset[PATH_MAX_LENGTH];

      char directory_audio_filter[PATH_MAX_LENGTH];
      char directory_autoconfig[PATH_MAX_LENGTH];
      char directory_video_filter[PATH_MAX_LENGTH];
      char directory_video_shader[PATH_MAX_LENGTH];
      char directory_libretro[PATH_MAX_LENGTH];
      char directory_input_remapping[PATH_MAX_LENGTH];
      char directory_overlay[PATH_MAX_LENGTH];
      char directory_osk_overlay[PATH_MAX_LENGTH];
      char directory_resampler[PATH_MAX_LENGTH];
      char directory_screenshot[PATH_MAX_LENGTH];
      char directory_system[PATH_MAX_LENGTH];
      char directory_cache[PATH_MAX_LENGTH];
      char directory_playlist[PATH_MAX_LENGTH];
      char directory_content_favorites[PATH_MAX_LENGTH];
      char directory_content_history[PATH_MAX_LENGTH];
      char directory_content_image_history[PATH_MAX_LENGTH];
      char directory_content_music_history[PATH_MAX_LENGTH];
      char directory_content_video_history[PATH_MAX_LENGTH];
      char directory_runtime_log[PATH_MAX_LENGTH];
      char directory_core_assets[PATH_MAX_LENGTH];
      char directory_assets[PATH_MAX_LENGTH];
      char directory_dynamic_wallpapers[PATH_MAX_LENGTH];
      char directory_thumbnails[PATH_MAX_LENGTH];
      char directory_menu_config[PATH_MAX_LENGTH];
      char directory_menu_content[PATH_MAX_LENGTH];
      char streaming_title[PATH_MAX_LENGTH];
#ifdef _3DS
      char directory_bottom_assets[PATH_MAX_LENGTH];
#endif
      char log_dir[PATH_MAX_LENGTH];
   } paths;

   bool modified;

   struct
   {
      bool placeholder;

      /* Video */
      bool video_fullscreen;
      bool video_windowed_fullscreen;
      bool video_vsync;
      bool video_adaptive_vsync;
      bool video_hard_sync;
      bool video_waitable_swapchains;
      bool video_vfilter;
      bool video_smooth;
      bool video_ctx_scaling;
      bool video_force_aspect;
      bool video_frame_delay_auto;
      bool video_frame_rest;
      bool video_crop_overscan;
      bool video_aspect_ratio_auto;
      bool video_dingux_ipu_keep_aspect;
      bool video_scale_integer;
      bool video_scale_integer_overscale;
      bool video_shader_enable;
      bool video_shader_watch_files;
      bool video_shader_remember_last_dir;
      bool video_shader_preset_save_reference_enable;
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
#ifdef _3DS
      bool video_3ds_lcd_bottom;
#endif
      bool video_wiiu_prefer_drc;
      bool video_notch_write_over_enable;
      bool video_hdr_enable;
      bool video_hdr_expand_gamut;

      /* Accessibility */
      bool accessibility_enable;

      /* Audio */
      bool audio_enable;
      bool audio_enable_menu;
      bool audio_enable_menu_ok;
      bool audio_enable_menu_cancel;
      bool audio_enable_menu_notice;
      bool audio_enable_menu_bgm;
      bool audio_enable_menu_scroll;
      bool audio_sync;
      bool audio_rate_control;
      bool audio_fastforward_mute;
      bool audio_fastforward_speedup;

#ifdef HAVE_WASAPI
      bool audio_wasapi_exclusive_mode;
      bool audio_wasapi_float_format;
#endif

#ifdef HAVE_MICROPHONE
      /* Microphone */
      bool microphone_enable;
#ifdef HAVE_WASAPI
      bool microphone_wasapi_exclusive_mode;
      bool microphone_wasapi_float_format;
#endif
#endif

      /* Input */
      bool input_remap_binds_enable;
      bool input_autodetect_enable;
      bool input_sensors_enable;
      bool input_overlay_enable;
      bool input_overlay_enable_autopreferred;
      bool input_overlay_behind_menu;
      bool input_overlay_hide_in_menu;
      bool input_overlay_hide_when_gamepad_connected;
      bool input_overlay_show_mouse_cursor;
      bool input_overlay_auto_rotate;
      bool input_overlay_auto_scale;
      bool input_osk_overlay_auto_scale;
      bool input_descriptor_label_show;
      bool input_descriptor_hide_unbound;
      bool input_all_users_control_menu;
      bool input_menu_swap_ok_cancel_buttons;
      bool input_menu_swap_scroll_buttons;
      bool input_backtouch_enable;
      bool input_backtouch_toggle;
      bool input_small_keyboard_enable;
      bool input_keyboard_gamepad_enable;
      bool input_auto_mouse_grab;
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
      bool input_nowinkey_enable;
#endif
#ifdef UDEV_TOUCH_SUPPORT
      bool input_touch_vmouse_pointer;
      bool input_touch_vmouse_mouse;
      bool input_touch_vmouse_touchpad;
      bool input_touch_vmouse_trackball;
      bool input_touch_vmouse_gesture;
#endif

      /* Frame time counter */
      bool frame_time_counter_reset_after_fastforwarding;
      bool frame_time_counter_reset_after_load_state;
      bool frame_time_counter_reset_after_save_state;

      /* Menu */
      bool filter_by_current_core;
      bool menu_enable_widgets;
      bool menu_show_load_content_animation;
      bool notification_show_autoconfig;
      bool notification_show_cheats_applied;
      bool notification_show_patch_applied;
      bool notification_show_remap_load;
      bool notification_show_config_override_load;
      bool notification_show_set_initial_disk;
      bool notification_show_save_state;
      bool notification_show_fast_forward;
#ifdef HAVE_SCREENSHOTS
      bool notification_show_screenshot;
#endif
      bool notification_show_refresh_rate;
      bool notification_show_netplay_extra;
#ifdef HAVE_MENU
      bool notification_show_when_menu_is_alive;
#endif
      bool menu_widget_scale_auto;
      bool menu_show_start_screen;
      bool menu_pause_libretro;
      bool menu_savestate_resume;
      bool menu_insert_disk_resume;
      bool menu_timedate_enable;
      bool menu_battery_level_enable;
      bool menu_core_enable;
      bool menu_show_sublabels;
      bool menu_dynamic_wallpaper_enable;
      bool menu_mouse_enable;
      bool menu_pointer_enable;
      bool menu_navigation_wraparound_enable;
      bool menu_navigation_browser_filter_supported_extensions_enable;
      bool menu_show_advanced_settings;
      bool menu_linear_filter;
      bool menu_horizontal_animation;
      bool menu_scroll_fast;
      bool menu_show_online_updater;
#ifdef HAVE_MIST
      bool menu_show_core_manager_steam;
#endif
      bool menu_show_core_updater;
      bool menu_show_load_core;
      bool menu_show_load_content;
      bool menu_show_load_disc;
      bool menu_show_dump_disc;
#ifdef HAVE_LAKKA
      bool menu_show_eject_disc;
#endif
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
      bool menu_materialui_icons_enable;
      bool menu_materialui_playlist_icons_enable;
      bool menu_materialui_switch_icons;
      bool menu_materialui_show_nav_bar;
      bool menu_materialui_auto_rotate_nav_bar;
      bool menu_materialui_dual_thumbnail_list_view_enable;
      bool menu_materialui_thumbnail_background_enable;
      bool menu_rgui_background_filler_thickness_enable;
      bool menu_rgui_border_filler_thickness_enable;
      bool menu_rgui_border_filler_enable;
      bool menu_rgui_full_width_layout;
      bool menu_rgui_transparency;
      bool menu_rgui_shadows;
      bool menu_rgui_inline_thumbnails;
      bool menu_rgui_swap_thumbnails;
      bool menu_rgui_extended_ascii;
      bool menu_rgui_switch_icons;
      bool menu_rgui_particle_effect_screensaver;
      bool menu_xmb_shadows_enable;
      bool menu_xmb_show_title_header;
      bool menu_xmb_switch_icons;
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
      bool menu_content_show_explore;
      bool menu_use_preferred_system_color_theme;
      bool menu_preferred_system_color_theme_set;
      bool menu_unified_controls;
      bool menu_disable_info_button;
      bool menu_disable_search_button;
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
      bool settings_show_file_browser;
      bool settings_show_frame_throttle;
      bool settings_show_recording;
      bool settings_show_onscreen_display;
      bool settings_show_user_interface;
      bool settings_show_ai_service;
      bool settings_show_accessibility;
      bool settings_show_power_management;
      bool settings_show_achievements;
      bool settings_show_network;
      bool settings_show_playlists;
      bool settings_show_user;
      bool settings_show_directory;
#ifdef HAVE_MIST
      bool settings_show_steam;
#endif
      bool quick_menu_show_resume_content;
      bool quick_menu_show_restart_content;
      bool quick_menu_show_close_content;
      bool quick_menu_show_take_screenshot;
      bool quick_menu_show_savestate_submenu;
      bool quick_menu_show_save_load_state;
      bool quick_menu_show_replay;
      bool quick_menu_show_undo_save_load_state;
      bool quick_menu_show_add_to_favorites;
      bool quick_menu_show_start_recording;
      bool quick_menu_show_start_streaming;
      bool quick_menu_show_set_core_association;
      bool quick_menu_show_reset_core_association;
      bool quick_menu_show_options;
      bool quick_menu_show_core_options_flush;
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
      bool crt_switch_hires_menu;

      /* Netplay */
      bool netplay_show_only_connectable;
      bool netplay_show_only_installed_cores;
      bool netplay_show_passworded;
      bool netplay_public_announce;
      bool netplay_start_as_spectator;
      bool netplay_fade_chat;
      bool netplay_allow_pausing;
      bool netplay_allow_slaves;
      bool netplay_require_slaves;
      bool netplay_nat_traversal;
      bool netplay_use_mitm_server;
      bool netplay_request_devices[MAX_USERS];
      bool netplay_ping_show;

      /* Network */
      bool network_buildbot_auto_extract_archive;
      bool network_buildbot_show_experimental_cores;
      bool network_on_demand_thumbnails;
      bool core_updater_auto_backup;

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
      bool cheevos_richpresence_enable;
      bool cheevos_badges_enable;
      bool cheevos_verbose_enable;
      bool cheevos_auto_screenshot;
      bool cheevos_start_active;
      bool cheevos_unlock_sound_enable;
      bool cheevos_challenge_indicators;
      bool cheevos_appearance_padding_auto;
      bool cheevos_visibility_unlock;
      bool cheevos_visibility_mastery;
      bool cheevos_visibility_account;
      bool cheevos_visibility_lboard_start;
      bool cheevos_visibility_lboard_submit;
      bool cheevos_visibility_lboard_cancel;
      bool cheevos_visibility_lboard_trackers;
      bool cheevos_visibility_progress_tracker;

      /* Camera */
      bool camera_allow;

      /* Bluetooth */
      bool bluetooth_allow;

      /* WiFi */
      bool wifi_allow;
      bool wifi_enabled;

      /* Location */
      bool location_allow;

      /* Multimedia */
      bool multimedia_builtin_mediaplayer_enable;
      bool multimedia_builtin_imageviewer_enable;

      /* Bundle */
      bool bundle_finished;
      bool bundle_assets_extract_enable;

      /* Driver */
      bool driver_switch_enable;

#ifdef HAVE_MIST
      /* Steam */
      bool steam_rich_presence_enable;
#endif

      /* Cloud Sync */
      bool cloud_sync_enable;
      bool cloud_sync_destructive;

      /* Misc. */
      bool discord_enable;
      bool threaded_data_runloop_enable;
      bool set_supports_no_game_enable;
      bool auto_screenshot_filename;
      bool history_list_enable;
      bool playlist_entry_rename;
      bool rewind_enable;
      bool fastforward_frameskip;
      bool vrr_runloop_enable;
      bool menu_throttle_framerate;
      bool apply_cheats_after_toggle;
      bool apply_cheats_after_load;
      bool run_ahead_enabled;
      bool run_ahead_secondary_instance;
      bool run_ahead_hide_warnings;
      bool preemptive_frames_enable;
      bool preemptive_frames_hide_warnings;
      bool pause_nonactive;
      bool pause_on_disconnect;
      bool block_sram_overwrite;
      bool replay_auto_index;
      bool savestate_auto_index;
      bool savestate_auto_save;
      bool savestate_auto_load;
      bool savestate_thumbnail_enable;
      bool save_file_compression;
      bool savestate_file_compression;
      bool network_cmd_enable;
      bool stdin_cmd_enable;
      bool keymapper_enable;
      bool network_remote_enable;
      bool network_remote_enable_user[MAX_USERS];
      bool load_dummy_on_core_shutdown;
      bool check_firmware_before_loading;
      bool core_option_category_enable;
      bool core_info_cache_enable;
      bool core_info_savestate_bypass;
#ifndef HAVE_DYNAMIC
      bool always_reload_core_on_run_content;
#endif

      bool game_specific_options;
      bool auto_overrides_enable;
      bool auto_remaps_enable;
      bool global_core_options;
      bool auto_shaders_enable;

      bool sort_savefiles_enable;
      bool sort_savestates_enable;
      bool sort_savefiles_by_content_enable;
      bool sort_savestates_by_content_enable;
      bool sort_screenshots_by_content_enable;
      bool config_save_on_exit;
      bool remap_save_on_exit;
      bool show_hidden_files;
      bool use_last_start_directory;

      bool savefiles_in_content_dir;
      bool savestates_in_content_dir;
      bool screenshots_in_content_dir;
      bool systemfiles_in_content_dir;
      bool ssh_enable;
#ifdef HAVE_LAKKA_SWITCH
      bool switch_oc;
      bool switch_cec;
      bool bluetooth_ertm_disable;
#endif
      bool samba_enable;
      bool bluetooth_enable;
      bool localap_enable;

      bool video_window_show_decorations;
      bool video_window_save_positions;
      bool video_window_custom_size_enable;

      bool sustained_performance_mode;
      bool playlist_use_old_format;
      bool playlist_compression;
      bool content_runtime_log;
      bool content_runtime_log_aggregate;

      bool playlist_sort_alphabetical;
      bool playlist_show_sublabels;
      bool playlist_show_entry_idx;
      bool playlist_fuzzy_archive_match;
      bool playlist_portable_paths;
      bool playlist_use_filename;

      bool quit_press_twice;
      bool vibrate_on_keypress;
      bool enable_device_vibration;
      bool ozone_collapse_sidebar;
      bool ozone_truncate_playlist_name;
      bool ozone_sort_after_truncate_playlist_name;
      bool ozone_scroll_content_metadata;

      bool log_to_file;
      bool log_to_file_timestamp;

      bool scan_without_core_match;

      bool ai_service_enable;
      bool ai_service_pause;

      bool gamemode_enable;
#ifdef _3DS
      bool new3ds_speedup_enable;
      bool bottom_font_enable;
#endif

#ifdef ANDROID
      bool android_input_disconnect_workaround;
#endif

#if defined(HAVE_COCOATOUCH) && defined(TARGET_OS_TV)
      bool gcdwebserver_alert;
#endif
   } bools;

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
 * config_get_default_bluetooth:
 *
 * Gets default bluetooth driver.
 *
 * Returns: Default bluetooth driver.
 **/
const char *config_get_default_bluetooth(void);

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

#if defined(HAVE_MICROPHONE)
/**
 * config_get_default_microphone:
 *
 * Gets default microphone driver.
 *
 * Returns: Default microphone driver.
 **/
const char *config_get_default_microphone(void);
#endif

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

#ifdef HAVE_CONFIGFILE
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
bool config_load_override(void *data);

/**
 * config_load_override_file:
 *
 * Tries to load specified configuration file.
 * These settings will always have precedence, thus this feature
 * can be used to enforce overrides.
 *
 * Returns: false if there was an error or no action was performed.
 *
 */
bool config_load_override_file(const char *path);

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
bool config_load_remap(const char *directory_input_remapping,
      void *data);

/**
 * config_save_autoconf_profile:
 * @device_name       : Input device name
 * @user              : Controller number to save
 * Writes a controller autoconf file to disk.
 **/
bool config_save_autoconf_profile(const char *device_name, unsigned user);

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
 * Returns: true (1) on success, (-1) if nothing to write, otherwise returns false (0).
 **/
int8_t config_save_overrides(enum override_type type,
      void *data, bool remove, const char *path);

/* Replaces currently loaded configuration file with
 * another one. Will load a dummy core to flush state
 * properly. */
bool config_replace(bool config_save_on_exit, char *path);
#endif

bool config_overlay_enable_default(void);

void config_set_defaults(void *data);

void config_load(void *data);

#if !defined(HAVE_DYNAMIC)
/* Salamander config file contains a single
 * entry (libretro_path), which is linked to
 * RARCH_PATH_CORE
 * > Used to select which core to load
 *   when launching a salamander build */
void config_load_file_salamander(void);
void config_save_file_salamander(void);
#endif

void retroarch_config_init(void);

void retroarch_config_deinit(void);

settings_t *config_get_ptr(void);

#ifdef HAVE_LAKKA
const char *config_get_all_timezones(void);
void config_set_timezone(char *timezone);
#endif

bool input_config_bind_map_get_valid(unsigned bind_index);

void input_config_parse_joy_button(
      char *s,
      void *data, const char *prefix,
      const char *btn, void *bind_data);

void input_config_parse_joy_axis(
      char *s,
      void *conf_data, const char *prefix,
      const char *axis, void *bind_data);

void input_config_parse_mouse_button(
      char *s,
      void *conf_data, const char *prefix,
      const char *btn, void *bind_data);

const char *input_config_get_prefix(unsigned user, bool meta);

RETRO_END_DECLS

#endif
