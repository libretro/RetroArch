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

#ifndef __RETROARCH_RUNLOOP_H
#define __RETROARCH_RUNLOOP_H

#include <file/nbio.h>
#include <formats/image.h>
#include <formats/rpng.h>
#include <queues/message_queue.h>
#include <setjmp.h>
#include "libretro.h"
#include "core_info.h"
#include "core_options.h"
#include "driver.h"
#include "rewind.h"
#include "autosave.h"
#include "movie.h"
#include "cheats.h"

#ifndef AUDIO_BUFFER_FREE_SAMPLES_COUNT
#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)
#endif

#ifndef MEASURE_FRAME_TIME_SAMPLES_COUNT
#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)
#endif

#ifndef MAX_USERS
#define MAX_USERS 16
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum runloop_data_type
{
   DATA_TYPE_NONE = 0,
   DATA_TYPE_FILE,
   DATA_TYPE_IMAGE,
   DATA_TYPE_HTTP,
#ifdef HAVE_OVERLAY
   DATA_TYPE_OVERLAY,
#endif
};

/* All libretro runloop-related globals go here. */

typedef struct runloop
{
   /* Lifecycle state checks. */
   bool is_paused;
   bool is_idle;
   bool is_menu;
   bool is_slowmotion;

   struct
   {
      struct
      {
         unsigned count;
         unsigned max;
         struct
         {
            struct
            {
               struct
               {
                  bool is_updated;
               } label;

               struct
               {
                  bool is_active;
               } animation;

               struct
               {
                  bool dirty;
               } framebuf;

               struct
               {
                  bool active;
               } action;
            } menu;
         } current;
      } video;

      struct
      {
         retro_time_t minimum_time;
         retro_time_t last_time;
      } limit;
   } frames;

   struct
   {
      unsigned buffer_free_samples[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
      uint64_t buffer_free_samples_count;

      retro_time_t frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
      uint64_t frame_time_samples_count;
   } measure_data;

   msg_queue_t *msg_queue;
} runloop_t;

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

runloop_t *rarch_main_get_ptr(void);

global_t *global_get_ptr(void);

/**
 * rarch_main_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on successful run, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop. 
 **/
int rarch_main_iterate(void);

void rarch_main_msg_queue_push(const char *msg, unsigned prio,
      unsigned duration, bool flush);

const char *rarch_main_msg_queue_pull(void);

void rarch_main_msg_queue_free(void);

void rarch_main_msg_queue_init(void);

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush);

void rarch_main_clear_state(void);

bool rarch_main_verbosity(void);

FILE *rarch_main_log_file(void);

bool rarch_main_is_idle(void);

void rarch_main_data_clear_state(void);

void rarch_main_data_iterate(void);

void rarch_main_data_deinit(void);

void rarch_main_data_init_queues(void);

/* Public data structures. */
extern global_t g_extern;

#ifdef __cplusplus
}
#endif

#endif
