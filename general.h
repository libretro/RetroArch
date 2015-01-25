/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __RARCH_GENERAL_H
#define __RARCH_GENERAL_H

#include <boolean.h>
#include <stdint.h>
#include <limits.h>
#include <setjmp.h>
#include "driver.h"
#include <queues/message_queue.h>
#include "rewind.h"
#include "movie.h"
#include "autosave.h"
#include "cheats.h"
#include <compat/strl.h>
#include "core_options.h"
#include "core_info.h"
#include <retro_miscellaneous.h>

#include "playlist.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.1"
#endif

/* Platform-specific headers */

/* Windows */
#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <compat/posix_string.h>
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#ifdef HAVE_NETPLAY
#include "net_http.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAX_USERS
#define MAX_USERS 16
#endif

enum sound_mode_enums
{
   SOUND_MODE_NORMAL = 0,
#ifdef HAVE_RSOUND
   SOUND_MODE_RSOUND,
#endif
#ifdef HAVE_HEADSET
   SOUND_MODE_HEADSET,
#endif
   SOUND_MODE_LAST
};

typedef struct rarch_viewport
{
   int x;
   int y;
   unsigned width;
   unsigned height;
   unsigned full_width;
   unsigned full_height;
} rarch_viewport_t;

struct defaults
{
   char menu_config_dir[PATH_MAX_LENGTH];
   char config_path[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char autoconfig_dir[PATH_MAX_LENGTH];
   char audio_filter_dir[PATH_MAX_LENGTH];
   char video_filter_dir[PATH_MAX_LENGTH];
   char assets_dir[PATH_MAX_LENGTH];
   char core_dir[PATH_MAX_LENGTH];
   char core_info_dir[PATH_MAX_LENGTH];
   char overlay_dir[PATH_MAX_LENGTH];
   char port_dir[PATH_MAX_LENGTH];
   char shader_dir[PATH_MAX_LENGTH];
   char savestate_dir[PATH_MAX_LENGTH];
   char resampler_dir[PATH_MAX_LENGTH];
   char sram_dir[PATH_MAX_LENGTH];
   char screenshot_dir[PATH_MAX_LENGTH];
   char system_dir[PATH_MAX_LENGTH];
   char playlist_dir[PATH_MAX_LENGTH];
   char content_history_dir[PATH_MAX_LENGTH];

   struct
   {
      int out_latency;
      float video_refresh_rate;
      bool video_threaded_enable;
   } settings; 

   content_playlist_t *history;
};

/* All config related settings go here. */

struct settings
{
   struct 
   {
      char driver[32];
      char context_driver[32];
      float scale;
      bool fullscreen;
      bool windowed_fullscreen;
      unsigned monitor_index;
      unsigned fullscreen_x;
      unsigned fullscreen_y;
      bool vsync;
      bool hard_sync;
      bool black_frame_insertion;
      unsigned swap_interval;
      unsigned hard_sync_frames;
      unsigned frame_delay;
#ifdef GEKKO
      unsigned viwidth;
      bool vfilter;
#endif
      bool smooth;
      bool force_aspect;
      bool crop_overscan;
      float aspect_ratio;
      bool aspect_ratio_auto;
      bool scale_integer;
      unsigned aspect_ratio_idx;
      unsigned rotation;

      char shader_path[PATH_MAX_LENGTH];
      bool shader_enable;

      char softfilter_plugin[PATH_MAX_LENGTH];
      float refresh_rate;
      bool threaded;

      char filter_dir[PATH_MAX_LENGTH];
      char shader_dir[PATH_MAX_LENGTH];

      char font_path[PATH_MAX_LENGTH];
      float font_size;
      bool font_enable;
      float msg_pos_x;
      float msg_pos_y;
      float msg_color_r;
      float msg_color_g;
      float msg_color_b;

      bool disable_composition;

      bool post_filter_record;
      bool gpu_record;
      bool gpu_screenshot;

      bool allow_rotate;
      bool shared_context;
      bool force_srgb_disable;
   } video;

   struct
   {
      bool menubar_enable;
      bool suspend_screensaver_enable;
   } ui;

#ifdef HAVE_MENU
   struct 
   {
      char driver[32];
      bool pause_libretro;
      bool mouse_enable;
      bool timedate_enable;
      char wallpaper[PATH_MAX_LENGTH];

      struct
      {
         struct
         {
            bool horizontal_enable;
            bool vertical_enable;
         } wraparound;
         struct
         {
            struct
            {
               bool supported_extensions_enable;
            } filter;
         } browser;
      } navigation;
   } menu;
#endif

   struct
   {
      char driver[32];
      char device[PATH_MAX_LENGTH];
      bool allow;
      unsigned width;
      unsigned height;
   } camera;

   struct
   {
      char driver[32];
      bool allow;
      int update_interval_ms;
      int update_interval_distance;
   } location;

   struct
   {
      char driver[32];
      bool enable;
   } osk;

   struct
   {
      char driver[32];
      bool enable;
      bool mute_enable;
      unsigned out_rate;
      unsigned block_frames;
      char device[PATH_MAX_LENGTH];
      unsigned latency;
      bool sync;

      char dsp_plugin[PATH_MAX_LENGTH];
      char filter_dir[PATH_MAX_LENGTH];

      bool rate_control;
      float rate_control_delta;
      float max_timing_skew;
      float volume; /* dB scale. */
      char resampler[32];
   } audio;

   struct
   {
      char driver[32];
      char joypad_driver[32];
      char keyboard_layout[64];

      unsigned remap_ids[MAX_USERS][RARCH_BIND_LIST_END];
      struct retro_keybind binds[MAX_USERS][RARCH_BIND_LIST_END];
      struct retro_keybind autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];

      unsigned max_users;

      /* Set by autoconfiguration in joypad_autoconfig_dir.
       * Does not override main binds. */
      bool autoconfigured[MAX_USERS];

      unsigned libretro_device[MAX_USERS];
      unsigned analog_dpad_mode[MAX_USERS];

      bool remap_binds_enable;
      float axis_threshold;
      unsigned joypad_map[MAX_USERS];
      unsigned device[MAX_USERS];
      char device_names[MAX_USERS][64];
      bool autodetect_enable;
      bool netplay_client_swap_input;

      unsigned turbo_period;
      unsigned turbo_duty_cycle;

      bool overlay_enable;
      char overlay[PATH_MAX_LENGTH];
      float overlay_opacity;
      float overlay_scale;

      char autoconfig_dir[PATH_MAX_LENGTH];
      bool autoconfig_descriptor_label_show;
      bool input_descriptor_label_show;
      bool input_descriptor_hide_unbound;

      char remapping_path[PATH_MAX_LENGTH];
   } input;

   struct
   {
      unsigned mode;
   } archive;

   struct
   {
      char buildbot_url[PATH_MAX_LENGTH];
   } network;

   int state_slot;

   char core_options_path[PATH_MAX_LENGTH];
   char content_history_path[PATH_MAX_LENGTH];
   char content_history_directory[PATH_MAX_LENGTH];
   unsigned content_history_size;

   char libretro[PATH_MAX_LENGTH];
   char libretro_directory[PATH_MAX_LENGTH];
   unsigned libretro_log_level;
   char libretro_info_path[PATH_MAX_LENGTH];
   char content_database[PATH_MAX_LENGTH];
   char cheat_database[PATH_MAX_LENGTH];
   char cheat_settings_path[PATH_MAX_LENGTH];
   char input_remapping_directory[PATH_MAX_LENGTH];

   char resampler_directory[PATH_MAX_LENGTH];
   char screenshot_directory[PATH_MAX_LENGTH];
   char system_directory[PATH_MAX_LENGTH];

   char extraction_directory[PATH_MAX_LENGTH];
   char playlist_directory[PATH_MAX_LENGTH];

   bool history_list_enable;
   bool rewind_enable;
   size_t rewind_buffer_size;
   unsigned rewind_granularity;

   float slowmotion_ratio;
   float fastforward_ratio;
   bool fastforward_ratio_throttle_enable;

   bool pause_nonactive;
   unsigned autosave_interval;

   bool block_sram_overwrite;
   bool savestate_auto_index;
   bool savestate_auto_save;
   bool savestate_auto_load;

   bool network_cmd_enable;
   uint16_t network_cmd_port;
   bool stdin_cmd_enable;

   char content_directory[PATH_MAX_LENGTH];
   char assets_directory[PATH_MAX_LENGTH];
   char menu_config_directory[PATH_MAX_LENGTH];
#if defined(HAVE_MENU)
   char menu_content_directory[PATH_MAX_LENGTH];
   bool menu_show_start_screen;
#endif
   bool fps_show;
   bool fps_monitor_enable;
   bool load_dummy_on_core_shutdown;

   bool core_specific_config;

   char username[32];
   unsigned int user_language;

   bool config_save_on_exit;
};

typedef struct rarch_resolution
{
   unsigned idx;
   unsigned id;
} rarch_resolution_t;

#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)
#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#ifdef HAVE_NETPLAY
typedef int (*http_cb_t               )(void *data, size_t len);
#endif

/* All run-time- / command line flag-related globals go here. */

struct global
{
   bool verbosity;
   bool perfcnt_enable;
   bool force_fullscreen;
   bool core_shutdown_initiated;

   struct string_list *temporary_content;

   core_info_list_t *core_info;
   core_info_t *core_info_current;

   uint32_t content_crc;

   char gb_rom_path[PATH_MAX_LENGTH];
   char bsx_rom_path[PATH_MAX_LENGTH];
   char sufami_rom_path[2][PATH_MAX_LENGTH];
   bool has_set_input_descriptors;
   bool has_set_save_path;
   bool has_set_state_path;
   bool has_set_libretro_device[MAX_USERS];
   bool has_set_libretro;
   bool has_set_libretro_directory;
   bool has_set_verbosity;

   bool has_set_netplay_mode;
   bool has_set_username;
   bool has_set_netplay_ip_address;
   bool has_set_netplay_delay_frames;
   bool has_set_netplay_ip_port;

   bool has_set_ups_pref;
   bool has_set_bps_pref;
   bool has_set_ips_pref;

   /* Config associated with global "default" config. */
   char config_path[PATH_MAX_LENGTH];
   char append_config_path[PATH_MAX_LENGTH];
   char input_config_path[PATH_MAX_LENGTH];

#ifdef HAVE_FILE_LOGGER
   char default_log_file[PATH_MAX_LENGTH];
#endif
   
   char basename[PATH_MAX_LENGTH];
   char fullpath[PATH_MAX_LENGTH];

   /* A list of save types and associated paths for all content. */
   struct string_list *savefiles;

   /* For --subsystem content. */
   char subsystem[PATH_MAX_LENGTH];
   struct string_list *subsystem_fullpaths;

   char savefile_name[PATH_MAX_LENGTH];
   char savestate_name[PATH_MAX_LENGTH];
   char cheatfile_name[PATH_MAX_LENGTH];

   /* Used on reentrancy to use a savestate dir. */
   char savefile_dir[PATH_MAX_LENGTH];
   char savestate_dir[PATH_MAX_LENGTH];

#ifdef HAVE_OVERLAY
   char overlay_dir[PATH_MAX_LENGTH];
#endif

   bool block_patch;
   bool ups_pref;
   bool bps_pref;
   bool ips_pref;
   char ups_name[PATH_MAX_LENGTH];
   char bps_name[PATH_MAX_LENGTH];
   char ips_name[PATH_MAX_LENGTH];

   struct
   {
      unsigned windowed_scale;
   } pending;

   struct
   {
      retro_time_t minimum_frame_time;
      retro_time_t last_frame_time;
   } frame_limit;

   struct
   {
      struct retro_system_info info;
      struct retro_system_av_info av_info;
      float aspect_ratio;

      unsigned rotation;
      bool shutdown;
      unsigned performance_level;
      enum retro_pixel_format pix_fmt;

      bool block_extract;
      bool force_nonblock;
      bool no_content;

      const char *input_desc_btn[MAX_USERS][RARCH_FIRST_META_KEY];
      char valid_extensions[PATH_MAX_LENGTH];
      
      retro_keyboard_event_t key_event;

      struct retro_audio_callback audio_callback;

      struct retro_disk_control_callback disk_control; 
      struct retro_hw_render_callback hw_render_callback;
      struct retro_camera_callback camera_callback;
      struct retro_location_callback location_callback;

      struct retro_frame_time_callback frame_time;
      retro_usec_t frame_time_last;

      core_option_manager_t *core_options;

      struct retro_subsystem_info *special;
      unsigned num_special;

      struct retro_controller_info *ports;
      unsigned num_ports;
   } system;

   struct
   {
      float *data;

      size_t data_ptr;
      size_t chunk_size;
      size_t nonblock_chunk_size;
      size_t block_chunk_size;

      double src_ratio;
      float in_rate;

      bool use_float;

      float *outsamples;
      int16_t *conv_outsamples;

      int16_t *rewind_buf;
      size_t rewind_ptr;
      size_t rewind_size;

      rarch_dsp_filter_t *dsp;

      bool rate_control; 
      double orig_src_ratio;
      size_t driver_buffer_size;

      float volume_gain;
   } audio_data;

   struct
   {
      unsigned buffer_free_samples[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
      uint64_t buffer_free_samples_count;

      retro_time_t frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
      uint64_t frame_time_samples_count;
   } measure_data;

   struct
   {
      rarch_softfilter_t *filter;

      void *buffer;
      unsigned scale;
      unsigned out_bpp;
      bool out_rgb32;
   } filter;

#ifdef HAVE_MENU
   struct
   {
      struct retro_system_info info;
      bool bind_mode_keyboard;
   } menu;
#endif

   msg_queue_t *msg_queue;
#ifdef HAVE_NETPLAY
   msg_queue_t *http_msg_queue;
   http_t      *http_handle;
   http_cb_t    http_cb;
#endif

   bool exec;

   /* Rewind support. */
   state_manager_t *state_manager;
   size_t state_size;
   bool frame_is_reverse;

   /* Movie playback/recording support. */
   struct
   {
      bsv_movie_t *movie;
      char movie_path[PATH_MAX_LENGTH];
      bool movie_playback;
      bool eof_exit;

      /* Immediate playback/recording. */
      char movie_start_path[PATH_MAX_LENGTH];
      bool movie_start_recording;
      bool movie_start_playback;
      bool movie_end;
   } bsv;

   struct
   {
      bool (*cb_init)(void *data);
      bool (*cb_callback)(void *data);
   } osk;

   bool sram_load_disable;
   bool sram_save_disable;
   bool use_sram;

   /* Lifecycle state checks. */
   bool is_paused;
   bool is_menu;
   bool is_slowmotion;

   /* Turbo support. */
   bool turbo_frame_enable[MAX_USERS];
   uint16_t turbo_enable[MAX_USERS];
   unsigned turbo_count;

   /* Autosave support. */
   autosave_t **autosave;
   unsigned num_autosave;

#ifdef HAVE_NETPLAY
   /* Netplay. */
   char netplay_server[PATH_MAX_LENGTH];
   bool netplay_enable;
   bool netplay_is_client;
   bool netplay_is_spectate;
   unsigned netplay_sync_frames;
   unsigned netplay_port;
#endif

   /* Recording. */
   char record_path[PATH_MAX_LENGTH];
   char record_config[PATH_MAX_LENGTH];
   bool recording_enable;
   unsigned record_width;
   unsigned record_height;

   uint8_t *record_gpu_buffer;
   size_t record_gpu_width;
   size_t record_gpu_height;

   struct
   {
      const void *data;
      unsigned width;
      unsigned height;
      size_t pitch;
   } frame_cache;

   unsigned frame_count;
   unsigned max_frames;

   char title_buf[64];

   struct
   {
      struct string_list *list;
      size_t ptr;
   } shader_dir;

   struct
   {
      struct string_list *list;
      size_t ptr;
   } filter_dir;

   cheat_manager_t *cheat;

   bool block_config_read;

   /* Settings and/or global state that is specific to 
    * a console-style implementation. */
   struct
   {
      struct
      {
         struct
         {
            rarch_resolution_t current;
            rarch_resolution_t initial;
            uint32_t *list;
            unsigned count;
            bool check;
         } resolutions;


         struct
         {
            rarch_viewport_t custom_vp;
         } viewports;

         unsigned gamma_correction;
         unsigned char flicker_filter_index;
         unsigned char soft_filter_index;
         bool pal_enable;
         bool pal60_enable;
      } screen;

      struct
      {
         unsigned mode;
         bool system_bgm_enable;
      } sound;

      bool flickerfilter_enable;
      bool softfilter_enable;
   } console;

   uint64_t lifecycle_state;

   /* If this is non-NULL. RARCH_LOG and friends 
    * will write to this file. */
   FILE *log_file;

   bool main_is_init;
   bool content_is_init;
   bool error_in_init;
   char error_string[PATH_MAX_LENGTH];
   jmp_buf error_sjlj_context;

   bool libretro_no_content;
   bool libretro_dummy;

   /* Config file associated with per-core configs. */
   char core_specific_config_path[PATH_MAX_LENGTH];

   retro_keyboard_event_t frontend_key_event;
};

/* Public data structures. */
extern struct settings g_settings;
extern struct global g_extern;
extern struct defaults g_defaults;

/* Public functions. */
int rarch_main(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

/**
 * db_to_gain:
 * @db          : Decibels.
 *
 * Converts decibels to voltage gain.
 *
 * Returns: voltage gain value.
 **/
static inline float db_to_gain(float db)
{
   return powf(10.0f, db / 20.0f);
}

/**
 * rarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
static inline void rarch_fail(int error_code, const char *error)
{
   /* We cannot longjmp unless we're in rarch_main_init().
    * If not, something went very wrong, and we should 
    * just exit right away. */
   rarch_assert(g_extern.error_in_init);

   strlcpy(g_extern.error_string, error,
         sizeof(g_extern.error_string));
   longjmp(g_extern.error_sjlj_context, error_code);
}

#ifdef HAVE_NETPLAY
void net_http_set_pending_cb(http_cb_t cb);
#endif

#endif


