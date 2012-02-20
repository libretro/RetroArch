/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef __SSNES_GENERAL_H
#define __SSNES_GENERAL_H

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
#include "audio/ext/ssnes_dsp.h"
#include "strl.h"

#ifdef __CELLOS_LV2__
#include <sys/timer.h>
#endif

#ifdef XENON
#include <time/time.h>
#endif

#if defined(XENON) || defined(__CELLOS_LV2__)
#undef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#include "audio/hermite.h"

#if defined(_WIN32) && !defined(_XBOX)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "msvc/msvc_compat.h"
#elif defined(_XBOX)
#include <xtl.h>
#include "360/msvc_compat.h"
#endif

#define MAX_PLAYERS 8

enum ssnes_shader_type
{
   SSNES_SHADER_CG,
   SSNES_SHADER_BSNES,
   SSNES_SHADER_AUTO,
   SSNES_SHADER_NONE
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
      char cg_shader_path[PATH_MAX];
      char bsnes_shader_path[PATH_MAX];
      char filter_path[PATH_MAX];
      enum ssnes_shader_type shader_type;
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
      bool post_filter_record;

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
   } audio;

   struct
   {
      char driver[32];
      struct snes_keybind binds[MAX_PLAYERS][SSNES_BIND_LIST_END];
      float axis_threshold;
      int joypad_map[MAX_PLAYERS];
      bool netplay_client_swap_input;
   } input;

   char libsnes[PATH_MAX];
   char cheat_database[PATH_MAX];
   char cheat_settings_path[PATH_MAX];

   char screenshot_directory[PATH_MAX];

   bool rewind_enable;
   size_t rewind_buffer_size;
   unsigned rewind_granularity;

   bool pause_nonactive;
   unsigned autosave_interval;

   bool block_sram_overwrite;
   bool savestate_auto_index;
};

// Settings and/or global state that is specific to a console-style implementation.
#ifdef SSNES_CONSOLE
struct console_settings
{
   bool autostart_game;
   bool block_config_read;
   bool default_sram_dir_enable;
   bool default_savestate_dir_enable;
   bool external_launcher_support;
   bool frame_advance_enable;
   bool initialize_ssnes_enable;
   bool ingame_menu_enable;
   bool menu_enable;
   bool overscan_enable;
   bool return_to_launcher;
   bool screenshots_enable;
   bool throttle_enable;
   bool triple_buffering_enable;
   float overscan_amount;
   uint32_t aspect_ratio_index;
   uint32_t emulator_initialized;
   uint32_t screen_orientation;
   uint32_t current_resolution_index;
   uint32_t current_resolution_id;
   uint32_t ingame_menu_item;
   uint32_t initial_resolution_id;
   uint32_t mode_switch;
   uint32_t *supported_resolutions;
   uint32_t supported_resolutions_count;
   uint32_t timer_expiration_frame_count;
#ifdef _XBOX
   DWORD volume_device_type;
#endif
   char rom_path[PATH_MAX];
   char aspect_ratio_name[PATH_MAX];
   char default_rom_startup_dir[PATH_MAX];
   char default_savestate_dir[PATH_MAX];
   char default_sram_dir[PATH_MAX];
   float menu_font_size;
};
#endif

enum ssnes_game_type
{
   SSNES_CART_NORMAL = 0,
   SSNES_CART_SGB,
   SSNES_CART_BSX,
   SSNES_CART_BSX_SLOTTED,
   SSNES_CART_SUFAMI
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
   enum ssnes_game_type game_type;
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
   char savefile_name_srm[PATH_MAX];
   char savefile_name_rtc[PATH_MAX]; // Make sure that fill_pathname has space.
   char savefile_name_psrm[PATH_MAX];
   char savefile_name_asrm[PATH_MAX];
   char savefile_name_bsrm[PATH_MAX];
   char savestate_name[PATH_MAX];
   char xml_name[PATH_MAX];

   bool ups_pref;
   bool bps_pref;
   char ups_name[PATH_MAX];
   char bps_name[PATH_MAX];

   unsigned state_slot;

   struct
   {
      struct snes_geometry geom;
      unsigned pitch; // If 0, has classic libsnes semantics.
      char fullpath[PATH_MAX];
      struct snes_system_timing timing;
      bool timing_set;
      bool need_fullpath;

      char *environment;
      char *environment_split;
   } system;

   struct
   {
      hermite_resampler_t *source;

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
      const ssnes_dsp_plugin_t *dsp_plugin;
      void *dsp_handle;
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

   bool sram_load_disable;
   bool sram_save_disable;
   bool use_sram;

   // Pausing support
   bool is_paused;
   bool is_oneshot;

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
      const uint16_t *data;
      unsigned width;
      unsigned height;
   } frame_cache;

   char title_buf[64];

   struct
   {
      char **elems;
      size_t size;
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

#ifdef HAVE_CONFIGFILE
#include "conf/config_file.h"
bool config_load_file(const char *path);
void config_read_keybinds(config_file_t *conf);
#endif

void ssnes_game_reset(void);
void ssnes_main_clear_state(void);
int ssnes_main_init(int argc, char *argv[]);
bool ssnes_main_iterate(void);
void ssnes_main_deinit(void);
void ssnes_render_cached_frame(void);

void ssnes_load_state(void);
void ssnes_save_state(void);
void ssnes_state_slot_increase(void);
void ssnes_state_slot_decrease(void);
/////////

// Public data structures
extern struct settings g_settings;
extern struct global g_extern;
#ifdef SSNES_CONSOLE
extern struct console_settings g_console;
#endif
/////////

#define SSNES_LOG(...) do { \
   if (g_extern.verbose) \
      fprintf(stderr, "SSNES: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

#define SSNES_ERR(...) do { \
      fprintf(stderr, "SSNES [ERROR] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

#define SSNES_WARN(...) do { \
      fprintf(stderr, "SSNES [WARN] :: " __VA_ARGS__); \
      fflush(stderr); \
   } while (0)

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define SSNES_SCALE_BASE 256

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

#ifdef GEKKO
#include <unistd.h>
#endif

static inline void ssnes_sleep(unsigned msec)
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

#define ssnes_assert(cond) \
   if (!(cond)) { SSNES_ERR("Assertion failed at %s:%d.\n", __FILE__, __LINE__); exit(2); }

static inline void ssnes_fail(int error_code, const char *error)
{
   // We cannot longjmp unless we're in ssnes_main_init().
   // If not, something went very wrong, and we should just exit right away.
   ssnes_assert(g_extern.error_in_init);

   strlcpy(g_extern.error_string, error, sizeof(g_extern.error_string));
   longjmp(g_extern.error_sjlj_context, error_code);
}

#endif


