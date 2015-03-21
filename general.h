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
#include "rewind.h"
#include "movie.h"
#include "autosave.h"
#include "cheats.h"
#include <compat/strl.h>
#include <retro_inline.h>
#include "core_options.h"
#include "core_info.h"
#include "gfx/video_viewport.h"
#include "configuration.h"
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
   char osk_overlay_dir[PATH_MAX_LENGTH];
   char port_dir[PATH_MAX_LENGTH];
   char shader_dir[PATH_MAX_LENGTH];
   char savestate_dir[PATH_MAX_LENGTH];
   char resampler_dir[PATH_MAX_LENGTH];
   char sram_dir[PATH_MAX_LENGTH];
   char screenshot_dir[PATH_MAX_LENGTH];
   char system_dir[PATH_MAX_LENGTH];
   char playlist_dir[PATH_MAX_LENGTH];
   char content_history_dir[PATH_MAX_LENGTH];
   char extraction_dir[PATH_MAX_LENGTH];
   char database_dir[PATH_MAX_LENGTH];
   char cursor_dir[PATH_MAX_LENGTH];
   char cheats_dir[PATH_MAX_LENGTH];

   struct
   {
      int out_latency;
      float video_refresh_rate;
      bool video_threaded_enable;
   } settings; 

   content_playlist_t *history;
};

typedef struct rarch_resolution
{
   unsigned idx;
   unsigned id;
} rarch_resolution_t;

/* All run-time- / command line flag-related globals go here. */

typedef struct global
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
   char osk_overlay_dir[PATH_MAX_LENGTH];
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


   bool exec;

   struct
   {
      /* Rewind support. */
      state_manager_t *state;
      size_t size;
      bool frame_is_reverse;
   } rewind;

   struct
   {
      /* Movie playback/recording support. */
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

   bool sram_load_disable;
   bool sram_save_disable;
   bool use_sram;


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
   struct
   {
      char path[PATH_MAX_LENGTH];
      char config[PATH_MAX_LENGTH];
      bool enable;
      unsigned width;
      unsigned height;

      uint8_t *gpu_buffer;
      size_t gpu_width;
      size_t gpu_height;
   } record;

   struct
   {
      const void *data;
      unsigned width;
      unsigned height;
      size_t pitch;
   } frame_cache;


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
            video_viewport_t custom_vp;
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
} global_t;

/* Public data structures. */
extern global_t g_extern;
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
static INLINE float db_to_gain(float db)
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
static INLINE void rarch_fail(int error_code, const char *error)
{
   /* We cannot longjmp unless we're in rarch_main_init().
    * If not, something went very wrong, and we should 
    * just exit right away. */
   rarch_assert(g_extern.error_in_init);

   strlcpy(g_extern.error_string, error,
         sizeof(g_extern.error_string));
   longjmp(g_extern.error_sjlj_context, error_code);
}

#endif


