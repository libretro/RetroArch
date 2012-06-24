/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
#include <time.h>
#include <limits.h>
#include <setjmp.h>
#include "driver.h"
#include "record/ffemu.h"
#include "message.h"
#include "rewind.h"
#include "movie.h"
#include "autosave.h"
#include "dynamic.h"
#include "cheats.h"
#include "audio/ext/rarch_dsp.h"
#include "compat/strl.h"

#ifdef __CELLOS_LV2__
#include <sys/timer.h>
#include "ps3/ps3_input.h"
#endif

#ifdef XENON
#include <time/time.h>
#endif

#if defined(XENON) || defined(__CELLOS_LV2__) || defined(_XBOX1)
#undef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#ifdef HAVE_NETWORK_CMD
#include "network_cmd.h"
#endif

#include "audio/resampler.h"

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(_XBOX360)
#include <xtl.h>
#endif

#if defined(_WIN32)
#include "msvc/msvc_compat.h"
#endif

#define MAX_PLAYERS 8

enum rarch_shader_type
{
   RARCH_SHADER_CG,
   RARCH_SHADER_BSNES,
   RARCH_SHADER_AUTO,
   RARCH_SHADER_NONE
};

// All config related settings go here.
struct settings
{
   struct 
   {
      char driver[32];
      float xscale;
      float yscale;
      bool fullscreen;
      unsigned fullscreen_x;
      unsigned fullscreen_y;
      bool vsync;
      bool smooth;
      bool force_aspect;
      bool crop_overscan;
      float aspect_ratio;
      bool aspect_ratio_auto;
      char cg_shader_path[PATH_MAX];
      char bsnes_shader_path[PATH_MAX];
      char filter_path[PATH_MAX];
      enum rarch_shader_type shader_type;
      float refresh_rate;

      bool render_to_texture;
      float fbo_scale_x;
      float fbo_scale_y;
      char second_pass_shader[PATH_MAX];
      bool second_pass_smooth;
      char shader_dir[PATH_MAX];

      char font_path[PATH_MAX];
      unsigned font_size;
      bool font_enable;
      bool font_scale;
      float msg_pos_x;
      float msg_pos_y;
      float msg_color_r;
      float msg_color_g;
      float msg_color_b;

      bool force_16bit;
      bool disable_composition;

      bool hires_record;
      bool h264_record;
      bool post_filter_record;

      bool allow_rotate;
      char external_driver[PATH_MAX];
   } video;

   struct
   {
      char driver[32];
      bool enable;
      unsigned out_rate;
      float in_rate;
      float rate_step;
      char device[PATH_MAX];
      unsigned latency;
      bool sync;

      char dsp_plugin[PATH_MAX];
      char external_driver[PATH_MAX];

      bool rate_control;
      float rate_control_delta;
   } audio;

   struct
   {
      char driver[32];
      struct snes_keybind binds[MAX_PLAYERS][RARCH_BIND_LIST_END];
      float axis_threshold;
      int joypad_map[MAX_PLAYERS];
#ifdef RARCH_CONSOLE
      unsigned currently_selected_controller_no;
      unsigned dpad_emulation[MAX_PLAYERS];
#endif
      bool netplay_client_swap_input;
   } input;

   char libretro[PATH_MAX];
   char cheat_database[PATH_MAX];
   char cheat_settings_path[PATH_MAX];

   char screenshot_directory[PATH_MAX];
   char system_directory[PATH_MAX];

   bool rewind_enable;
   size_t rewind_buffer_size;
   unsigned rewind_granularity;

   float slowmotion_ratio;

   bool pause_nonactive;
   unsigned autosave_interval;

   bool block_sram_overwrite;
   bool savestate_auto_index;
   bool savestate_auto_save;

   bool network_cmd_enable;
   uint16_t network_cmd_port;
};

// Settings and/or global state that is specific to a console-style implementation.
#ifdef RARCH_CONSOLE
typedef struct
{
   uint32_t x;
   uint32_t y;
   uint32_t width;
   uint32_t height;
} rarch_viewport_t;

struct console_settings
{
#ifdef __CELLOS_LV2__
   bool custom_bgm_enable;
#endif
   bool check_available_resolutions;
   bool block_config_read;
   bool default_sram_dir_enable;
   bool default_savestate_dir_enable;
   bool fbo_enabled;
   bool frame_advance_enable;
#ifdef _XBOX
   bool menus_hd_enable;
#endif
   bool initialize_rarch_enable;
   bool info_msg_enable;
   bool gamma_correction_enable;
   bool ingame_menu_enable;
   bool menu_enable;
   bool overscan_enable;
   bool return_to_launcher;
   bool screenshots_enable;
   bool throttle_enable;
   bool triple_buffering_enable;
   float overscan_amount;
   uint32_t aspect_ratio_index;
   struct
   {
      rarch_viewport_t custom_vp;
   } viewports;
   uint32_t emulator_initialized;
   uint32_t external_launcher_support;
   uint32_t screen_orientation;
   uint32_t current_resolution_index;
   uint32_t current_resolution_id;
   uint32_t ingame_menu_item;
   uint32_t initial_resolution_id;
   uint32_t map_dpad_to_stick;
   uint32_t mode_switch;
   uint32_t sound_mode;
   uint32_t *supported_resolutions;
   uint32_t supported_resolutions_count;
   uint32_t control_timer_expiration_frame_count;
   uint32_t timer_expiration_frame_count;
#ifdef RARCH_CONSOLE
   uint32_t input_loop;
#endif
#ifdef _XBOX360
   uint32_t color_format;
   DWORD volume_device_type;
#endif
   char cgp_path[PATH_MAX];
   char input_cfg_path[PATH_MAX];
   char rom_path[PATH_MAX];
   char default_rom_startup_dir[PATH_MAX];
   char default_savestate_dir[PATH_MAX];
   char default_sram_dir[PATH_MAX];
   char launch_app_on_exit[PATH_MAX];
   float menu_font_size;
#ifdef __CELLOS_LV2__
   oskutil_params oskutil_handle;
#endif
};
#endif

enum rarch_game_type
{
   RARCH_CART_NORMAL = 0,
   RARCH_CART_SGB,
   RARCH_CART_BSX,
   RARCH_CART_BSX_SLOTTED,
   RARCH_CART_SUFAMI
};

// All run-time- / command line flag-related globals go here.
struct global
{
   bool verbose;
   bool audio_active;
   bool video_active;
   bool force_fullscreen;

   bool has_mouse[2];
   bool has_scope[2];
   bool has_justifier;
   bool has_justifiers;
   bool has_multitap;
   bool disconnect_device[2];

   FILE *rom_file;
   enum rarch_game_type game_type;
   uint32_t cart_crc;

   char gb_rom_path[PATH_MAX];
   char bsx_rom_path[PATH_MAX];
   char sufami_rom_path[2][PATH_MAX];
   bool has_set_save_path;
   bool has_set_state_path;

#ifdef HAVE_CONFIGFILE
   char config_path[PATH_MAX];
#endif
   
   char basename[PATH_MAX];
   char fullpath[PATH_MAX];
   char savefile_name_srm[PATH_MAX];
   char savefile_name_rtc[PATH_MAX]; // Make sure that fill_pathname has space.
   char savefile_name_psrm[PATH_MAX];
   char savefile_name_asrm[PATH_MAX];
   char savefile_name_bsrm[PATH_MAX];
   char savestate_name[PATH_MAX];
   char xml_name[PATH_MAX];

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
      struct retro_system_info info;
      struct retro_system_av_info av_info;

      char *environment;
      char *environment_split;

      unsigned rotation;
      bool shutdown;
      unsigned performance_level;
      bool rgb32;
   } system;

   struct
   {
      rarch_resampler_t *source;

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
   } audio_data;

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
   } filter;

   msg_queue_t *msg_queue;

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

   bool sram_load_disable;
   bool sram_save_disable;
   bool use_sram;

   // Pausing support
   bool is_paused;
   bool is_oneshot;
   bool is_slowmotion;

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
   bool recording;
   unsigned record_width;
   unsigned record_height;
#endif

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

   char sha256[64 + 1];

#ifdef HAVE_XML
   cheat_manager_t *cheat;
#endif

   bool error_in_init;
   char error_string[1024];
   jmp_buf error_sjlj_context;
};

// Public functions
void config_load(void);
void config_set_defaults(void);
const char *config_get_default_video(void);
const char *config_get_default_audio(void);
const char *config_get_default_input(void);

#ifdef HAVE_CONFIGFILE
#include "conf/config_file.h"
bool config_load_file(const char *path);
bool config_read_keybinds(const char *path);
bool config_save_keybinds(const char *path);
#endif

void rarch_game_reset(void);
void rarch_main_clear_state(void);
int rarch_main_init(int argc, char *argv[]);
bool rarch_main_iterate(void);
void rarch_main_deinit(void);
void rarch_render_cached_frame(void);
void rarch_init_msg_queue(void);
void rarch_deinit_msg_queue(void);

void rarch_load_state(void);
void rarch_save_state(void);
void rarch_state_slot_increase(void);
void rarch_state_slot_decrease(void);
/////////

// Public data structures
extern struct settings g_settings;
extern struct global g_extern;
#ifdef RARCH_CONSOLE
extern struct console_settings g_console;
#endif
/////////

#if defined(RARCH_CONSOLE) && (defined(HAVE_LOGGER) || defined(HAVE_FILE_LOGGER))
#include "logger_override.h"
#else

#ifndef RARCH_LOG
#define RARCH_LOG(...) do { \
   if (g_extern.verbose) \
      fprintf(stderr, "RetroArch: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif

#ifndef RARCH_ERR
#define RARCH_ERR(...) do { \
      fprintf(stderr, "RetroArch [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif

#ifndef RARCH_WARN
#define RARCH_WARN(...) do { \
      fprintf(stderr, "RetroArch [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)
#endif
#endif

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define RARCH_SCALE_BASE 256

static inline uint32_t next_pow2(uint32_t v)
{
   v--;
   v |= v >> 1;
   v |= v >> 2;
   v |= v >> 4;
   v |= v >> 8;
   v |= v >> 16;
   v++;
   return v;
}

static inline uint8_t is_little_endian(void)
{
   union
   {
      uint16_t x;
      uint8_t y[2];
   } u;

   u.x = 1;
   return u.y[0];
}

static inline uint32_t swap_if_big32(uint32_t val)
{
   if (is_little_endian()) // Little-endian
      return val;
   else
      return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
}

static inline uint32_t swap_if_little32(uint32_t val)
{
   if (is_little_endian())
      return (val >> 24) | ((val >> 8) & 0xFF00) | ((val << 8) & 0xFF0000) | (val << 24);
   else
      return val;
}

static inline uint16_t swap_if_big16(uint16_t val)
{
   if (is_little_endian())
      return val;
   else
      return (val >> 8) | (val << 8);
}

static inline uint16_t swap_if_little16(uint16_t val)
{
   if (is_little_endian())
      return (val >> 8) | (val << 8);
   else
      return val;
}

#ifdef GEKKO
#include <unistd.h>
#endif

static inline void rarch_sleep(unsigned msec)
{
#ifdef __CELLOS_LV2__
   sys_timer_usleep(1000 * msec);
#elif defined(_WIN32)
   Sleep(msec);
#elif defined(XENON)
   udelay(1000 * msec);
#elif defined(GEKKO)
   usleep(1000 * msec);
#else
   struct timespec tv = {0};
   tv.tv_sec = msec / 1000;
   tv.tv_nsec = (msec % 1000) * 1000000;
   nanosleep(&tv, NULL);
#endif
}

#define rarch_assert(cond) \
   if (!(cond)) { RARCH_ERR("Assertion failed at %s:%d.\n", __FILE__, __LINE__); exit(2); }

static inline void rarch_fail(int error_code, const char *error)
{
   // We cannot longjmp unless we're in rarch_main_init().
   // If not, something went very wrong, and we should just exit right away.
   rarch_assert(g_extern.error_in_init);

   strlcpy(g_extern.error_string, error, sizeof(g_extern.error_string));
   longjmp(g_extern.error_sjlj_context, error_code);
}

#endif


