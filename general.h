/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "boolean.h"
#include <stdio.h>
#include <limits.h>
#include <setjmp.h>
#include "driver.h"
#include "record/ffemu.h"
#include "message_queue.h"
#include "rewind.h"
#include "movie.h"
#include "autosave.h"
#include "dynamic.h"
#include "cheats.h"
#include "audio/ext/rarch_dsp.h"
#include "compat/strl.h"
#include "performance.h"
#include "core_options.h"
#include "miscellaneous.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PACKAGE_VERSION
#ifdef __QNX__
/* FIXME - avoid too many decimal points in number error */
#define PACKAGE_VERSION "1000"
#else
#define PACKAGE_VERSION "1.0.0.0"
#endif
#endif

// Platform-specific headers
// Windows
#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "msvc/msvc_compat.h"
#endif

// Wii and PSL1GHT - for usleep (among others)
#if defined(GEKKO) || defined(__PSL1GHT__) || defined(__QNX__)
#include <unistd.h>
#endif

// PSP
#if defined(PSP)
#include <pspthreadman.h>
#endif
//////////////

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#include "audio/resampler.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PLAYERS 8

enum menu_enums
{
   MODE_GAME = 0,
   MODE_LOAD_GAME,
   MODE_MENU,
   MODE_EXIT,
   MODE_MENU_WIDESCREEN,
   MODE_MENU_HD,
   MODE_MENU_PREINIT,
   MODE_EXTLAUNCH_MULTIMAN,
   MODE_EXITSPAWN,
   MODE_EXITSPAWN_START_GAME,
   MODE_EXITSPAWN_MULTIMAN,
   MODE_INPUT_XPERIA_PLAY_HACK,
   MODE_VIDEO_TRIPLE_BUFFERING_ENABLE,
   MODE_VIDEO_FLICKER_FILTER_ENABLE,
   MODE_VIDEO_SOFT_FILTER_ENABLE,
   MODE_VIDEO_PAL_ENABLE,
   MODE_VIDEO_PAL_TEMPORAL_ENABLE,
   MODE_AUDIO_CUSTOM_BGM_ENABLE,
   MODE_OSK_ENTRY_SUCCESS,
   MODE_OSK_ENTRY_FAIL,
};

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

// All config related settings go here.
struct settings
{
   struct 
   {
      char driver[32];
      char gl_context[32];
      float xscale;
      float yscale;
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
      bool smooth;
      bool force_aspect;
      bool crop_overscan;
      float aspect_ratio;
      bool aspect_ratio_auto;
      bool scale_integer;
      unsigned aspect_ratio_idx;
      unsigned rotation;

      char shader_path[PATH_MAX];
      bool shader_enable;

      char filter_path[PATH_MAX];
      float refresh_rate;
      bool threaded;

      char shader_dir[PATH_MAX];

      char font_path[PATH_MAX];
      float font_size;
      bool font_enable;
      bool font_scale;
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
   } video;

#ifdef HAVE_CAMERA
   struct
   {
      char driver[32];
      char device[PATH_MAX];
      unsigned width;
      unsigned height;
   } camera;
#endif

#ifdef HAVE_LOCATION
   struct
   {
      char driver[32];
      int update_interval_ms;
      int update_interval_distance;
   } location;
#endif

#ifdef HAVE_OSK
   struct
   {
      char driver[32];
   } osk;
#endif

   struct
   {
      char driver[32];
      bool enable;
      unsigned out_rate;
      unsigned block_frames;
      float in_rate;
      char device[PATH_MAX];
      unsigned latency;
      bool sync;

      char dsp_plugin[PATH_MAX];

      bool rate_control;
      float rate_control_delta;
      float volume; // dB scale
      char resampler[32];
   } audio;

   struct
   {
      char driver[32];
      char joypad_driver[32];
      char keyboard_layout[64];
      struct retro_keybind binds[MAX_PLAYERS][RARCH_BIND_LIST_END];
#ifdef RARCH_CONSOLE
      struct retro_keybind menu_binds[RARCH_BIND_LIST_END];
#endif

      // Set by autoconfiguration in joypad_autoconfig_dir. Does not override main binds.
      struct retro_keybind autoconf_binds[MAX_PLAYERS][RARCH_BIND_LIST_END];
      bool autoconfigured[MAX_PLAYERS];

      unsigned libretro_device[MAX_PLAYERS];

      float axis_threshold;
      int joypad_map[MAX_PLAYERS];
      unsigned device[MAX_PLAYERS];
      char device_names[MAX_PLAYERS][64];
      bool debug_enable;
      bool autodetect_enable;
#ifdef ANDROID
      unsigned back_behavior;
      unsigned icade_profile[MAX_PLAYERS];
      unsigned icade_count;
#endif
      bool netplay_client_swap_input;

      unsigned turbo_period;
      unsigned turbo_duty_cycle;

      char overlay[PATH_MAX];
      float overlay_opacity;
      float overlay_scale;

      char autoconfig_dir[PATH_MAX];
   } input;

   char core_options_path[PATH_MAX];
   char game_history_path[PATH_MAX];
   unsigned game_history_size;

   char libretro[PATH_MAX];
   char libretro_info_path[PATH_MAX];
   char cheat_database[PATH_MAX];
   char cheat_settings_path[PATH_MAX];

   char screenshot_directory[PATH_MAX];
   char system_directory[PATH_MAX];

   bool rewind_enable;
   size_t rewind_buffer_size;
   unsigned rewind_granularity;

   float slowmotion_ratio;
   float fastforward_ratio;

   bool pause_nonactive;
   unsigned autosave_interval;

   bool block_sram_overwrite;
   bool savestate_auto_index;
   bool savestate_auto_save;
   bool savestate_auto_load;

   bool network_cmd_enable;
   uint16_t network_cmd_port;
   bool stdin_cmd_enable;

   char content_directory[PATH_MAX];
#if defined(HAVE_MENU)
   char rgui_content_directory[PATH_MAX];
   char rgui_config_directory[PATH_MAX];
   bool rgui_show_start_screen;
#endif
   bool fps_show;

   bool core_specific_config;
};

enum rarch_game_type
{
   RARCH_CART_NORMAL = 0,
   RARCH_CART_SGB,
   RARCH_CART_BSX,
   RARCH_CART_BSX_SLOTTED,
   RARCH_CART_SUFAMI
};

typedef struct rarch_resolution
{
   unsigned idx;
   unsigned id;
} rarch_resolution_t;

typedef struct rarch_viewport
{
   int x;
   int y;
   unsigned width;
   unsigned height;
   unsigned full_width;
   unsigned full_height;
} rarch_viewport_t;

// All run-time- / command line flag-related globals go here.
struct global
{
   bool verbose;
   bool audio_active;
   bool video_active;
#ifdef HAVE_CAMERA
   bool camera_active;
#endif
#ifdef HAVE_LOCATION
   bool location_active;
#endif
#ifdef HAVE_OSK
   bool osk_active;
#endif
   bool force_fullscreen;

   bool rom_file_temporary;
   char last_rom[PATH_MAX];
   enum rarch_game_type game_type;
   uint32_t cart_crc;

   char gb_rom_path[PATH_MAX];
   char bsx_rom_path[PATH_MAX];
   char sufami_rom_path[2][PATH_MAX];
   bool has_set_save_path;
   bool has_set_state_path;
   bool has_set_libretro_device[MAX_PLAYERS];
   bool has_set_libretro;

#ifdef HAVE_RMENU
   char menu_texture_path[PATH_MAX];
#endif
   char config_path[PATH_MAX];
   char append_config_path[PATH_MAX];
   char input_config_path[PATH_MAX];

#ifdef HAVE_FILE_LOGGER
   char default_log_file[PATH_MAX];
#endif
   
   char basename[PATH_MAX];
   char fullpath[PATH_MAX];
   char savefile_name_srm[PATH_MAX];
   char savefile_name_rtc[PATH_MAX]; // Make sure that fill_pathname has space.
   char savefile_name_psrm[PATH_MAX];
   char savefile_name_asrm[PATH_MAX];
   char savefile_name_bsrm[PATH_MAX];
   char savestate_name[PATH_MAX];

   // Used on reentrancy to use a savestate dir.
   char savefile_dir[PATH_MAX];
   char savestate_dir[PATH_MAX];

#ifdef HAVE_OVERLAY
   char overlay_dir[PATH_MAX];
#endif

   bool block_patch;
   bool ups_pref;
   bool bps_pref;
   bool ips_pref;
   char ups_name[PATH_MAX];
   char bps_name[PATH_MAX];
   char ips_name[PATH_MAX];

   unsigned state_slot;

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
      bool no_game;

      const char *input_desc_btn[MAX_PLAYERS][RARCH_FIRST_CUSTOM_BIND];
      char valid_extensions[PATH_MAX];
      
      retro_keyboard_event_t key_event;

      struct retro_audio_callback audio_callback;

      struct retro_disk_control_callback disk_control; 
      struct retro_hw_render_callback hw_render_callback;
      struct retro_camera_callback camera_callback;
      struct retro_location_callback location_callback;

      struct retro_frame_time_callback frame_time;
      retro_usec_t frame_time_last;

      core_option_manager_t *core_options;
   } system;

   struct
   {
      void *resampler_data;
      const rarch_resampler_t *resampler;

      float *data;

      size_t data_ptr;
      size_t chunk_size;
      size_t nonblock_chunk_size;
      size_t block_chunk_size;

      double src_ratio;

      bool use_float;
      bool mute;

      float *outsamples;
      int16_t *conv_outsamples;

      int16_t *rewind_buf;
      size_t rewind_ptr;
      size_t rewind_size;

      dylib_t dsp_lib;
      const rarch_dsp_plugin_t *dsp_plugin;
      void *dsp_handle;

      bool rate_control; 
      double orig_src_ratio;
      size_t driver_buffer_size;

      float volume_db;
      float volume_gain;
   } audio_data;

   struct
   {
#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)
      unsigned buffer_free_samples[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
      uint64_t buffer_free_samples_count;

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)
      retro_time_t frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
      uint64_t frame_time_samples_count;
   } measure_data;

   struct
   {
      bool active;
      uint32_t *buffer;
      uint32_t *colormap;
      unsigned pitch;
      dylib_t lib;
      unsigned scale;

      void (*psize)(unsigned *width, unsigned *height);
      void (*prender)(uint32_t *colormap, uint32_t *output, unsigned outpitch,
            const uint16_t *input, unsigned pitch, unsigned width, unsigned height);

      // CPU filters only work on *XRGB1555*. We have to convert to XRGB1555 first.
      struct scaler_ctx scaler;
      uint16_t *scaler_out;
   } filter;

   msg_queue_t *msg_queue;

   bool exec;

   // Rewind support.
   state_manager_t *state_manager;
   void *state_buf;
   size_t state_size;
   bool frame_is_reverse;

#ifdef HAVE_BSV_MOVIE
   // Movie playback/recording support.
   struct
   {
      bsv_movie_t *movie;
      char movie_path[PATH_MAX];
      bool movie_playback;

      // Immediate playback/recording.
      char movie_start_path[PATH_MAX];
      bool movie_start_recording;
      bool movie_start_playback;
      bool movie_end;
   } bsv;
#endif

#ifdef HAVE_OSK
   struct
   {
      bool (*cb_init)(void *data);
      bool (*cb_callback)(void *data);
   } osk;
#endif

   bool sram_load_disable;
   bool sram_save_disable;
   bool use_sram;

   // Pausing support
   bool is_paused;
   bool is_oneshot;
   bool is_slowmotion;

   // Turbo support
   bool turbo_frame_enable[MAX_PLAYERS];
   uint16_t turbo_enable[MAX_PLAYERS];
   unsigned turbo_count;

   // Autosave support.
   autosave_t *autosave[2];

   // Netplay.
#ifdef HAVE_NETPLAY
   netplay_t *netplay;
   char netplay_server[PATH_MAX];
   bool netplay_enable;
   bool netplay_is_client;
   bool netplay_is_spectate;
   unsigned netplay_sync_frames;
   uint16_t netplay_port;
   char netplay_nick[32];
#endif

   // FFmpeg record.
#ifdef HAVE_FFMPEG
   ffemu_t *rec;
   char record_path[PATH_MAX];
   char record_config[PATH_MAX];
   bool recording;
   unsigned record_width;
   unsigned record_height;

   uint8_t *record_gpu_buffer;
   size_t record_gpu_width;
   size_t record_gpu_height;
#endif

   struct
   {
      const void *data;
      unsigned width;
      unsigned height;
      size_t pitch;
   } frame_cache;

   unsigned frame_count;
   char title_buf[64];

   struct
   {
      struct string_list *list;
      size_t ptr;
   } shader_dir;

   char sha256[64 + 1];

   cheat_manager_t *cheat;

   bool block_config_read;

   // Settings and/or global state that is specific to a console-style implementation.
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
      } screen;

      struct
      {
         unsigned mode;
#ifdef RARCH_CONSOLE
         unsigned volume_level;
#endif
      } sound;
   } console;

   uint64_t lifecycle_state;

   // If this is non-NULL. RARCH_LOG and friends will write to this file.
   FILE *log_file;

   bool main_is_init;
   bool error_in_init;
   bool config_save_on_exit;
   char error_string[1024];
   jmp_buf error_sjlj_context;
   unsigned menu_toggle_behavior;

   bool libretro_no_rom;
   bool libretro_dummy;

   char original_config_path[PATH_MAX];
   char core_specific_config_path[PATH_MAX];
};

struct rarch_main_wrap
{
   const char *rom_path;
   const char *sram_path;
   const char *state_path;
   const char *config_path;
   const char *libretro_path;
   bool verbose;
   bool no_rom;
};

// Public functions
void config_load(void);
void config_set_defaults(void);
#ifdef HAVE_CAMERA
const char *config_get_default_camera(void);
#endif
#ifdef HAVE_LOCATION
const char *config_get_default_location(void);
#endif
#ifdef HAVE_OSK
const char *config_get_default_osk(void);
#endif
const char *config_get_default_video(void);
const char *config_get_default_audio(void);
const char *config_get_default_input(void);

#include "conf/config_file.h"
bool config_load_file(const char *path);
bool config_save_file(const char *path);
bool config_read_keybinds(const char *path);

void rarch_game_reset(void);
void rarch_main_clear_state(void);
void rarch_init_system_info(void);
int rarch_main(int argc, char *argv[]);
int rarch_main_init_wrap(const struct rarch_main_wrap *args);
int rarch_main_init(int argc, char *argv[]);
bool rarch_main_idle_iterate(void);
bool rarch_main_iterate(void);
void rarch_main_deinit(void);
void rarch_render_cached_frame(void);
void rarch_init_msg_queue(void);
void rarch_deinit_msg_queue(void);
void rarch_input_poll(void);
void rarch_check_overlay(void);
void rarch_check_block_hotkey(void);
void rarch_init_rewind(void);
void rarch_deinit_rewind(void);
void rarch_set_fullscreen(bool fullscreen);
void rarch_disk_control_set_eject(bool state, bool log);
void rarch_disk_control_set_index(unsigned index);
void rarch_disk_control_append_image(const char *path);
bool rarch_set_rumble_state(unsigned port, enum retro_rumble_effect effect, bool enable);
void rarch_init_autosave(void);
void rarch_deinit_autosave(void);
void rarch_take_screenshot(void);

void rarch_load_state(void);
void rarch_save_state(void);
void rarch_state_slot_increase(void);
void rarch_state_slot_decrease(void);
/////////

// Public data structures
extern struct settings g_settings;
extern struct global g_extern;
/////////

#ifdef __cplusplus
}
#endif

static inline float db_to_gain(float db)
{
   return powf(10.0f, db / 20.0f);
}

static inline void rarch_fail(int error_code, const char *error)
{
   // We cannot longjmp unless we're in rarch_main_init().
   // If not, something went very wrong, and we should just exit right away.
   rarch_assert(g_extern.error_in_init);

   strlcpy(g_extern.error_string, error, sizeof(g_extern.error_string));
   longjmp(g_extern.error_sjlj_context, error_code);
}

#endif


