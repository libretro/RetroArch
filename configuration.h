/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2016 - Jean-André Santoni
 *  Copyright (C) 2016 - Brad Parker
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

#include "gfx/video_driver.h"
#include "input/input_defines.h"

enum override_type
{
   OVERRIDE_NONE = 0,
   OVERRIDE_CORE,
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
      bool video_hard_sync;
      bool video_black_frame_insertion;
#ifdef GEKKO
      bool video_vfilter;
#endif
      bool video_smooth;
      bool video_force_aspect;
      bool video_crop_overscan;
      bool video_aspect_ratio_auto;
      bool video_scale_integer;
      bool video_shader_enable;
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

      /* Audio */
      bool audio_enable;
      bool audio_sync;
      bool audio_rate_control;
#ifdef HAVE_WASAPI
      bool audio_wasapi_exclusive_mode;
      bool audio_wasapi_float_format;
#endif

      /* Input */
      bool input_remap_binds_enable;
      bool input_autodetect_enable;
      bool input_overlay_enable;
      bool input_overlay_enable_autopreferred;
      bool input_overlay_hide_in_menu;
      bool input_overlay_show_physical_inputs;
      bool input_descriptor_label_show;
      bool input_descriptor_hide_unbound;
      bool input_all_users_control_menu;
      bool input_menu_swap_ok_cancel_buttons;
#if defined(VITA)
      bool input_backtouch_enable;
      bool input_backtouch_toggle;
#endif
#if TARGET_OS_IPHONE
      bool input_small_keyboard_enable;
#endif
      bool input_keyboard_gamepad_enable;

#ifdef HAVE_MENU
      /* Menu */
      bool filter_by_current_core;
      bool menu_show_start_screen;
      bool menu_pause_libretro;
      bool menu_timedate_enable;
      bool menu_battery_level_enable;
      bool menu_core_enable;
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
      bool menu_materialui_icons_enable;
      bool menu_xmb_shadows_enable;
      bool menu_xmb_show_settings;
      bool menu_xmb_show_favorites;
      bool menu_xmb_show_images;
      bool menu_xmb_show_music;
      bool menu_xmb_show_video;
      bool menu_xmb_show_netplay;
      bool menu_xmb_show_history;
      bool menu_xmb_show_add;
      bool menu_unified_controls;
#endif

#ifdef HAVE_NETWORKING
      /* Netplay */
      bool netplay_public_announce;
      bool netplay_start_as_spectator;
      bool netplay_allow_slaves;
      bool netplay_require_slaves;
      bool netplay_stateless_mode;
      bool netplay_swap_input;
      bool netplay_nat_traversal;
      bool netplay_use_mitm_server;
#endif

      /* Network */
      bool network_buildbot_auto_extract_archive;

      /* UI */
      bool ui_menubar_enable;
      bool ui_suspend_screensaver_enable;
      bool ui_companion_start_on_boot;
      bool ui_companion_enable;

#ifdef HAVE_CHEEVOS
      /* Cheevos */
      bool cheevos_enable;
      bool cheevos_test_unofficial;
      bool cheevos_hardcore_mode_enable;
      bool cheevos_verbose_enable;
#endif

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
#ifdef HAVE_THREADS
      bool threaded_data_runloop_enable;
#endif
      bool set_supports_no_game_enable;
      bool auto_screenshot_filename;
      bool history_list_enable;
      bool playlist_entry_remove;
      bool rewind_enable;
      bool pause_nonactive;
      bool block_sram_overwrite;
      bool savestate_auto_index;
      bool savestate_auto_save;
      bool savestate_auto_load;
      bool savestate_thumbnail_enable;
      bool network_cmd_enable;
      bool stdin_cmd_enable;
      bool network_remote_enable;
      bool network_remote_enable_user[MAX_USERS];
      bool load_dummy_on_core_shutdown;
      bool check_firmware_before_loading;

      bool game_specific_options;
      bool auto_overrides_enable;
      bool auto_remaps_enable;
      bool auto_shaders_enable;

      bool sort_savefiles_enable;
      bool sort_savestates_enable;
      bool config_save_on_exit;
      bool show_hidden_files;

      bool savefiles_in_content_dir;
      bool savestates_in_content_dir;
      bool screenshots_in_content_dir;
      bool systemfiles_in_content_dir;
#ifdef HAVE_LAKKA
      bool ssh_enable;
      bool samba_enable;
      bool bluetooth_enable;
#endif
   } bools;

   struct
   {
      float placeholder;
      float video_scale;
      float video_aspect_ratio;
      float video_refresh_rate;
      float video_font_size;
      float video_msg_pos_x;
      float video_msg_pos_y;
      float video_msg_color_r;
      float video_msg_color_g;
      float video_msg_color_b;

      float menu_wallpaper_opacity;
      float menu_framebuffer_opacity;
      float menu_footer_opacity;
      float menu_header_opacity;

      float audio_max_timing_skew;
      float audio_volume; /* dB scale. */
      float audio_mixer_volume; /* dB scale. */

      float input_overlay_opacity;
      float input_overlay_scale;

      float slowmotion_ratio;
      float fastforward_ratio;
   } floats;

   struct
   {
      int placeholder;
      int netplay_check_frames;
      int location_update_interval_ms;
      int location_update_interval_distance;
      int state_slot;

#ifdef HAVE_WASAPI
      int audio_wasapi_sh_buffer_length;
#endif
   } ints;

   struct
   {
      unsigned placeholder;
      unsigned audio_out_rate;
      unsigned audio_block_frames;
      unsigned audio_latency;
      unsigned input_remap_ids[MAX_USERS][RARCH_BIND_LIST_END];

      /* Set by autoconfiguration in joypad_autoconfig_dir.
       * Does not override main binds. */
      unsigned input_libretro_device[MAX_USERS];
      unsigned input_analog_dpad_mode[MAX_USERS];

      unsigned input_joypad_map[MAX_USERS];
      unsigned input_device[MAX_USERS];
      unsigned input_mouse_index[MAX_USERS];

      unsigned input_turbo_period;
      unsigned input_turbo_duty_cycle;

      unsigned input_bind_timeout;

      unsigned input_menu_toggle_gamepad_combo;
      unsigned input_keyboard_gamepad_mapping_type;
      unsigned input_poll_type_behavior;
      unsigned netplay_port;
      unsigned netplay_input_latency_frames_min;
      unsigned netplay_input_latency_frames_range;
      unsigned bundle_assets_extract_version_current;
      unsigned bundle_assets_extract_last_version;
      unsigned content_history_size;
      unsigned libretro_log_level;
      unsigned rewind_granularity;
      unsigned autosave_interval;
      unsigned network_cmd_port;
      unsigned network_remote_base_port;
      unsigned video_window_x;
      unsigned video_window_y;
      unsigned video_monitor_index;
      unsigned video_fullscreen_x;
      unsigned video_fullscreen_y;
      unsigned video_max_swapchain_images;
      unsigned video_swap_interval;
      unsigned video_hard_sync_frames;
      unsigned video_frame_delay;
#ifdef GEKKO
      unsigned video_viwidth;
#endif
      unsigned video_aspect_ratio_idx;
      unsigned video_rotation;

      unsigned menu_thumbnails;
      unsigned menu_dpi_override_value;
      unsigned menu_entry_normal_color;
      unsigned menu_entry_hover_color;
      unsigned menu_title_color;
      unsigned menu_xmb_shader_pipeline;
      unsigned menu_xmb_scale_factor;
      unsigned menu_xmb_alpha_factor;
      unsigned menu_xmb_theme;
      unsigned menu_xmb_color_theme;
      unsigned menu_materialui_color_theme;

      unsigned camera_width;
      unsigned camera_height;

      unsigned input_overlay_show_physical_inputs_port;
   } uints;

   struct
   {
      char placeholder;

      char playlist_names[PATH_MAX_LENGTH];
      char playlist_cores[PATH_MAX_LENGTH];
      char video_driver[32];
      char record_driver[32];
      char camera_driver[32];
      char wifi_driver[32];
      char location_driver[32];
#ifdef HAVE_MENU
      char menu_driver[32];
#endif
      char audio_device[255];
      char camera_device[255];
#ifdef HAVE_CHEEVOS
      char cheevos_username[32];
      char cheevos_password[32];
#endif
      char video_context_driver[32];
      char audio_driver[32];
      char audio_resampler[32];
      char input_driver[32];
      char input_joypad_driver[32];
      char input_keyboard_layout[64];
      char bundle_assets_src[PATH_MAX_LENGTH];
      char bundle_assets_dst[PATH_MAX_LENGTH];
      char bundle_assets_dst_subdir[PATH_MAX_LENGTH];
   } arrays;

   struct
   {
      char placeholder;


      char browse_url[4096];

#ifdef HAVE_MENU
      char path_menu_xmb_font[PATH_MAX_LENGTH];
#endif
      char path_cheat_database[PATH_MAX_LENGTH];
      char path_content_database[PATH_MAX_LENGTH];
      char path_overlay[PATH_MAX_LENGTH];
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
      char path_shader[PATH_MAX_LENGTH];
      char path_font[PATH_MAX_LENGTH];

      char network_buildbot_url[255];
      char network_buildbot_assets_url[255];

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
      char directory_resampler[PATH_MAX_LENGTH];
      char directory_screenshot[PATH_MAX_LENGTH];
      char directory_system[PATH_MAX_LENGTH];
      char directory_cache[PATH_MAX_LENGTH];
      char directory_playlist[PATH_MAX_LENGTH];
      char directory_core_assets[PATH_MAX_LENGTH];
      char directory_assets[PATH_MAX_LENGTH];
      char directory_dynamic_wallpapers[PATH_MAX_LENGTH];
      char directory_thumbnails[PATH_MAX_LENGTH];
      char directory_menu_config[PATH_MAX_LENGTH];
      char directory_menu_content[PATH_MAX_LENGTH];
#ifdef HAVE_NETWORKING
      char netplay_server[255];
      char netplay_password[128];
      char netplay_spectate_password[128];
#endif
      char username[32];
   } paths;

   bool modified;

   video_viewport_t video_viewport_custom;

   size_t rewind_buffer_size;
} settings_t;

#define configuration_set_float(settings, var, newvar) \
   settings->modified = true; \
   var = newvar

#define configuration_set_bool(settings, var, newvar) \
   settings->modified = true; \
   var = newvar

#define configuration_set_uint(settings, var, newvar) \
   settings->modified = true; \
   var = newvar

#define configuration_set_int(settings, var, newvar) \
   settings->modified = true; \
   var = newvar

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

#ifdef HAVE_MENU
/**
 * config_get_default_menu:
 *
 * Gets default menu driver.
 *
 * Returns: Default menu driver.
 **/
const char *config_get_default_menu(void);
#endif

const char *config_get_default_record(void);

/**
 * config_load:
 *
 * Loads a config file and reads all the values into memory.
 *
 */
void config_load(void);

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
bool config_load_remap(void);

/**
 * config_load_shader_preset:
 *
 * Tries to append game-specific and core-specific shader presets.
 *
 * Returns: false if there was an error or no action was performed.
 *
 */
bool config_load_shader_preset(void);

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

bool config_init(void);

bool config_overlay_enable_default(void);

void config_free(void);

settings_t *config_get_ptr(void);

RETRO_END_DECLS

#endif
