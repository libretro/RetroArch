/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

#include "general.h"
#include "conf/config_file.h"
#include <assert.h>
#include <string.h>
#include "hqflt/filters.h"
#include "config.def.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

struct settings g_settings;

#ifdef HAVE_CONFIGFILE
static void read_keybinds(config_file_t *conf);
#endif

static void set_defaults(void)
{
   const char *def_video = NULL;
   const char *def_audio = NULL;
   const char *def_input = NULL;

   switch (VIDEO_DEFAULT_DRIVER)
   {
      case VIDEO_GL:
         def_video = "gl";
         break;
      default:
         break;
   }

   switch (AUDIO_DEFAULT_DRIVER)
   {
      case AUDIO_RSOUND:
         def_audio = "rsound";
         break;
      case AUDIO_OSS:
         def_audio = "oss";
         break;
      case AUDIO_ALSA:
         def_audio = "alsa";
         break;
      case AUDIO_ROAR:
         def_audio = "roar";
         break;
      case AUDIO_AL:
         def_audio = "openal";
         break;
      case AUDIO_SDL:
         def_audio = "sdl";
         break;
      case AUDIO_XAUDIO:
         def_audio = "xaudio";
         break;
      case AUDIO_PULSE:
         def_audio = "pulse";
         break;
      default:
         break;
   }

   switch (INPUT_DEFAULT_DRIVER)
   {
      case INPUT_SDL:
         def_input = "sdl";
         break;
      default:
         break;
   }

   if (def_video)
      strncpy(g_settings.video.driver, def_video, sizeof(g_settings.video.driver) - 1);
   if (def_audio)
      strncpy(g_settings.audio.driver, def_audio, sizeof(g_settings.audio.driver) - 1);
   if (def_input)
      strncpy(g_settings.input.driver, def_input, sizeof(g_settings.input.driver) - 1);

   g_settings.video.xscale = xscale;
   g_settings.video.yscale = yscale;
   g_settings.video.fullscreen = fullscreen;
   g_settings.video.fullscreen_x = fullscreen_x;
   g_settings.video.fullscreen_y = fullscreen_y;
   g_settings.video.vsync = vsync;
   g_settings.video.smooth = video_smooth;
   g_settings.video.force_aspect = force_aspect;
   g_settings.video.aspect_ratio = SNES_ASPECT_RATIO;
#ifdef HAVE_FILTER
   g_settings.video.filter = FILTER_NONE;
#endif

#ifdef HAVE_FREETYPE
   g_settings.video.font_size = font_size;
   g_settings.video.msg_pos_x = message_pos_offset_x;
   g_settings.video.msg_pos_y = message_pos_offset_y;
#endif

   g_settings.audio.enable = audio_enable;
   g_settings.audio.out_rate = out_rate;
   g_settings.audio.in_rate = in_rate;
   g_settings.audio.rate_step = audio_rate_step;
   if (audio_device)
      strncpy(g_settings.audio.device, audio_device, sizeof(g_settings.audio.device));
   g_settings.audio.latency = out_latency;
   g_settings.audio.sync = audio_sync;
   g_settings.audio.src_quality = SAMPLERATE_QUALITY;

   g_settings.rewind_enable = rewind_enable;
   g_settings.rewind_buffer_size = rewind_buffer_size;
   g_settings.rewind_granularity = rewind_granularity;

   assert(sizeof(g_settings.input.binds[0]) >= sizeof(snes_keybinds_1));
   assert(sizeof(g_settings.input.binds[1]) >= sizeof(snes_keybinds_2));
   assert(sizeof(g_settings.input.binds[2]) >= sizeof(snes_keybinds_3));
   assert(sizeof(g_settings.input.binds[3]) >= sizeof(snes_keybinds_4));
   assert(sizeof(g_settings.input.binds[4]) >= sizeof(snes_keybinds_5));
   memcpy(g_settings.input.binds[0], snes_keybinds_1, sizeof(snes_keybinds_1));
   memcpy(g_settings.input.binds[1], snes_keybinds_2, sizeof(snes_keybinds_2));
   memcpy(g_settings.input.binds[2], snes_keybinds_3, sizeof(snes_keybinds_3));
   memcpy(g_settings.input.binds[3], snes_keybinds_4, sizeof(snes_keybinds_4));
   memcpy(g_settings.input.binds[4], snes_keybinds_5, sizeof(snes_keybinds_5));

   g_settings.input.axis_threshold = AXIS_THRESHOLD;
   for (int i = 0; i < MAX_PLAYERS; i++)
      g_settings.input.joypad_map[i] = i;
}

#ifdef HAVE_CONFIGFILE
static void parse_config_file(void);
#endif

void parse_config(void)
{
   memset(&g_settings, 0, sizeof(struct settings));
   set_defaults();

#ifdef HAVE_CONFIGFILE
   parse_config_file();
#endif
}

// Macros to ease config getting.
#define CONFIG_GET_BOOL(var, key) if (config_get_bool(conf, key, &tmp_bool)) \
   g_settings.var = tmp_bool

#define CONFIG_GET_INT(var, key) if (config_get_int(conf, key, &tmp_int)) \
   g_settings.var = tmp_int

#define CONFIG_GET_DOUBLE(var, key) if (config_get_double(conf, key, &tmp_double)) \
   g_settings.var = tmp_double

#define CONFIG_GET_STRING(var, key) do { \
   if (config_get_string(conf, key, &tmp_str)) \
   { \
      strncpy(g_settings.var, tmp_str, sizeof(g_settings.var) - 1); \
      free(tmp_str); \
   } \
} while(0)

#ifdef HAVE_CONFIGFILE
static void parse_config_file(void)
{
   config_file_t *conf = NULL;

   if (strlen(g_extern.config_path) > 0)
   {
      conf = config_file_new(g_extern.config_path);
      if (!conf)
      {
         SSNES_ERR("Couldn't find config at path: \"%s\"\n", g_extern.config_path);
         exit(1);
      }
   }
   else
   {
#ifdef _WIN32
      // Just do something for now.
      conf = config_file_new("ssnes.cfg");
#else
      const char *xdg = getenv("XDG_CONFIG_HOME");
      const char *home = getenv("HOME");
      if (xdg)
      {
         char conf_path[strlen(xdg) + strlen("/ssnes/ssnes.cfg ")];
         strcpy(conf_path, xdg);
         strcat(conf_path, "/ssnes/ssnes.cfg");
         conf = config_file_new(conf_path);
      }
      else if (home)
      {
         char conf_path[strlen(home) + strlen("/.ssnesrc ")];
         strcpy(conf_path, home);
         strcat(conf_path, "/.ssnesrc");
         conf = config_file_new(conf_path);
      }
      // Try this as a last chance...
      if (!conf)
         conf = config_file_new("/etc/ssnes.cfg");
#endif
   }

   if (conf == NULL)
      return;

   if (g_extern.verbose)
      config_file_dump(conf, stderr);

   int tmp_int;
   double tmp_double;
   bool tmp_bool;
   char *tmp_str;

   CONFIG_GET_DOUBLE(video.xscale, "video_xscale");
   CONFIG_GET_DOUBLE(video.yscale, "video_yscale");
   CONFIG_GET_INT(video.fullscreen_x, "video_fullscreen_x");
   CONFIG_GET_INT(video.fullscreen_y, "video_fullscreen_y");
   CONFIG_GET_BOOL(video.fullscreen, "video_fullscreen");
   CONFIG_GET_BOOL(video.vsync, "video_vsync");
   CONFIG_GET_BOOL(video.smooth, "video_smooth");
   CONFIG_GET_BOOL(video.force_aspect, "video_force_aspect");
   CONFIG_GET_DOUBLE(video.aspect_ratio, "video_aspect_ratio");

   CONFIG_GET_STRING(video.cg_shader_path, "video_cg_shader");
   CONFIG_GET_STRING(video.bsnes_shader_path, "video_bsnes_shader");

#ifdef HAVE_FREETYPE
   CONFIG_GET_STRING(video.font_path, "video_font_path");
   CONFIG_GET_INT(video.font_size, "video_font_size");
   CONFIG_GET_DOUBLE(video.msg_pos_x, "video_message_pos_x");
   CONFIG_GET_DOUBLE(video.msg_pos_y, "video_message_pos_y");
#endif

#ifdef HAVE_FILTER
   if (config_get_string(conf, "video_filter", &tmp_str))
   {
      unsigned filter = 0;
      if (strcasecmp(FILTER_HQ2X_STR, tmp_str) == 0)
         filter = FILTER_HQ2X;
      else if (strcasecmp(FILTER_HQ4X_STR, tmp_str) == 0)
         filter = FILTER_HQ4X;
      else if (strcasecmp(FILTER_GRAYSCALE_STR, tmp_str) == 0)
         filter = FILTER_GRAYSCALE;
      else if (strcasecmp(FILTER_BLEED_STR, tmp_str) == 0)
         filter = FILTER_BLEED;
      else if (strcasecmp(FILTER_NTSC_STR, tmp_str) == 0)
         filter = FILTER_NTSC;
      else
      {
         SSNES_ERR(
               "Invalid filter... Valid filters are:\n"
               "\t%s\n"
               "\t%s\n"
               "\t%s\n"
               "\t%s\n"
               "\t%s\n", 
               FILTER_HQ2X_STR, FILTER_HQ4X_STR, FILTER_GRAYSCALE_STR,
               FILTER_BLEED_STR, FILTER_NTSC_STR);
         exit(1);
      }

      free(tmp_str);
      g_settings.video.filter = filter;
   }
#endif

   CONFIG_GET_DOUBLE(input.axis_threshold, "input_axis_threshold");
   CONFIG_GET_INT(input.joypad_map[0], "input_player1_joypad_index");
   CONFIG_GET_INT(input.joypad_map[1], "input_player2_joypad_index");
   CONFIG_GET_INT(input.joypad_map[2], "input_player3_joypad_index");
   CONFIG_GET_INT(input.joypad_map[3], "input_player4_joypad_index");
   CONFIG_GET_INT(input.joypad_map[4], "input_player5_joypad_index");

   // Audio settings.
   
   CONFIG_GET_BOOL(audio.enable, "audio_enable");
   CONFIG_GET_INT(audio.out_rate, "audio_out_rate");
   CONFIG_GET_DOUBLE(audio.in_rate, "audio_in_rate");
   CONFIG_GET_DOUBLE(audio.rate_step, "audio_rate_step");
   CONFIG_GET_STRING(audio.device, "audio_device");
   CONFIG_GET_INT(audio.latency, "audio_latency");
   CONFIG_GET_BOOL(audio.sync, "audio_sync");

   if (config_get_int(conf, "audio_src_quality", &tmp_int))
   {
      int quals[] = { SRC_ZERO_ORDER_HOLD, SRC_LINEAR, SRC_SINC_FASTEST, 
         SRC_SINC_MEDIUM_QUALITY, SRC_SINC_BEST_QUALITY };

      if (tmp_int > 0 && tmp_int < 6)
         g_settings.audio.src_quality = quals[tmp_int];
   }

   CONFIG_GET_STRING(video.driver, "video_driver");
   CONFIG_GET_STRING(audio.driver, "audio_driver");
   CONFIG_GET_STRING(input.driver, "input_driver");
   CONFIG_GET_STRING(libsnes, "libsnes_path");

   CONFIG_GET_BOOL(rewind_enable, "rewind_enable");

   if (config_get_int(conf, "rewind_buffer_size", &tmp_int))
      g_settings.rewind_buffer_size = tmp_int * 1000000;

   CONFIG_GET_INT(rewind_granularity, "rewind_granularity");

   read_keybinds(conf);

   config_file_free(conf);
}

struct bind_map
{
   const char *key;
   const char *btn;
   const char *axis;
   int snes_key;
};


#define DECLARE_BIND(x, bind) { "input_" #x, "input_" #x "_btn", "input_" #x "_axis", bind },
// Big and nasty bind map... :)
static const struct bind_map bind_maps[MAX_PLAYERS][MAX_BINDS - 1] = {
   {
      DECLARE_BIND(player1_a,             SNES_DEVICE_ID_JOYPAD_A)
      DECLARE_BIND(player1_b,             SNES_DEVICE_ID_JOYPAD_B)
      DECLARE_BIND(player1_y,             SNES_DEVICE_ID_JOYPAD_Y)
      DECLARE_BIND(player1_x,             SNES_DEVICE_ID_JOYPAD_X)
      DECLARE_BIND(player1_start,         SNES_DEVICE_ID_JOYPAD_START)
      DECLARE_BIND(player1_select,        SNES_DEVICE_ID_JOYPAD_SELECT)
      DECLARE_BIND(player1_l,             SNES_DEVICE_ID_JOYPAD_L)
      DECLARE_BIND(player1_r,             SNES_DEVICE_ID_JOYPAD_R)
      DECLARE_BIND(player1_left,          SNES_DEVICE_ID_JOYPAD_LEFT)
      DECLARE_BIND(player1_right,         SNES_DEVICE_ID_JOYPAD_RIGHT)
      DECLARE_BIND(player1_up,            SNES_DEVICE_ID_JOYPAD_UP)
      DECLARE_BIND(player1_down,          SNES_DEVICE_ID_JOYPAD_DOWN)
      DECLARE_BIND(toggle_fast_forward,   SSNES_FAST_FORWARD_KEY)
      DECLARE_BIND(save_state,            SSNES_SAVE_STATE_KEY)
      DECLARE_BIND(load_state,            SSNES_LOAD_STATE_KEY)
      DECLARE_BIND(state_slot_increase,   SSNES_STATE_SLOT_PLUS)
      DECLARE_BIND(state_slot_decrease,   SSNES_STATE_SLOT_MINUS)
      DECLARE_BIND(exit_emulator,         SSNES_QUIT_KEY)
      DECLARE_BIND(toggle_fullscreen,     SSNES_FULLSCREEN_TOGGLE_KEY)
      DECLARE_BIND(rate_step_up,          SSNES_AUDIO_INPUT_RATE_PLUS)
      DECLARE_BIND(rate_step_down,        SSNES_AUDIO_INPUT_RATE_MINUS)
      DECLARE_BIND(rewind,                SSNES_REWIND)
      DECLARE_BIND(movie_record_toggle,   SSNES_MOVIE_RECORD_TOGGLE)
   },
   {
      DECLARE_BIND(player2_a,             SNES_DEVICE_ID_JOYPAD_A)
      DECLARE_BIND(player2_b,             SNES_DEVICE_ID_JOYPAD_B)
      DECLARE_BIND(player2_y,             SNES_DEVICE_ID_JOYPAD_Y)
      DECLARE_BIND(player2_x,             SNES_DEVICE_ID_JOYPAD_X)
      DECLARE_BIND(player2_start,         SNES_DEVICE_ID_JOYPAD_START)
      DECLARE_BIND(player2_select,        SNES_DEVICE_ID_JOYPAD_SELECT)
      DECLARE_BIND(player2_l,             SNES_DEVICE_ID_JOYPAD_L)
      DECLARE_BIND(player2_r,             SNES_DEVICE_ID_JOYPAD_R)
      DECLARE_BIND(player2_left,          SNES_DEVICE_ID_JOYPAD_LEFT)
      DECLARE_BIND(player2_right,         SNES_DEVICE_ID_JOYPAD_RIGHT)
      DECLARE_BIND(player2_up,            SNES_DEVICE_ID_JOYPAD_UP)
      DECLARE_BIND(player2_down,          SNES_DEVICE_ID_JOYPAD_DOWN)
      DECLARE_BIND(toggle_fast_forward,   SSNES_FAST_FORWARD_KEY)
      DECLARE_BIND(save_state,            SSNES_SAVE_STATE_KEY)
      DECLARE_BIND(load_state,            SSNES_LOAD_STATE_KEY)
      DECLARE_BIND(state_slot_increase,   SSNES_STATE_SLOT_PLUS)
      DECLARE_BIND(state_slot_decrease,   SSNES_STATE_SLOT_MINUS)
      DECLARE_BIND(exit_emulator,         SSNES_QUIT_KEY)
      DECLARE_BIND(toggle_fullscreen,     SSNES_FULLSCREEN_TOGGLE_KEY)
      DECLARE_BIND(rate_step_up,          SSNES_AUDIO_INPUT_RATE_PLUS)
      DECLARE_BIND(rate_step_down,        SSNES_AUDIO_INPUT_RATE_MINUS)
      DECLARE_BIND(rewind,                SSNES_REWIND)
      DECLARE_BIND(movie_record_toggle,   SSNES_MOVIE_RECORD_TOGGLE)
   },
   {
      DECLARE_BIND(player3_a,             SNES_DEVICE_ID_JOYPAD_A)
      DECLARE_BIND(player3_b,             SNES_DEVICE_ID_JOYPAD_B)
      DECLARE_BIND(player3_y,             SNES_DEVICE_ID_JOYPAD_Y)
      DECLARE_BIND(player3_x,             SNES_DEVICE_ID_JOYPAD_X)
      DECLARE_BIND(player3_start,         SNES_DEVICE_ID_JOYPAD_START)
      DECLARE_BIND(player3_select,        SNES_DEVICE_ID_JOYPAD_SELECT)
      DECLARE_BIND(player3_l,             SNES_DEVICE_ID_JOYPAD_L)
      DECLARE_BIND(player3_r,             SNES_DEVICE_ID_JOYPAD_R)
      DECLARE_BIND(player3_left,          SNES_DEVICE_ID_JOYPAD_LEFT)
      DECLARE_BIND(player3_right,         SNES_DEVICE_ID_JOYPAD_RIGHT)
      DECLARE_BIND(player3_up,            SNES_DEVICE_ID_JOYPAD_UP)
      DECLARE_BIND(player3_down,          SNES_DEVICE_ID_JOYPAD_DOWN)
      DECLARE_BIND(toggle_fast_forward,   SSNES_FAST_FORWARD_KEY)
      DECLARE_BIND(save_state,            SSNES_SAVE_STATE_KEY)
      DECLARE_BIND(load_state,            SSNES_LOAD_STATE_KEY)
      DECLARE_BIND(state_slot_increase,   SSNES_STATE_SLOT_PLUS)
      DECLARE_BIND(state_slot_decrease,   SSNES_STATE_SLOT_MINUS)
      DECLARE_BIND(exit_emulator,         SSNES_QUIT_KEY)
      DECLARE_BIND(toggle_fullscreen,     SSNES_FULLSCREEN_TOGGLE_KEY)
      DECLARE_BIND(rate_step_up,          SSNES_AUDIO_INPUT_RATE_PLUS)
      DECLARE_BIND(rate_step_down,        SSNES_AUDIO_INPUT_RATE_MINUS)
      DECLARE_BIND(rewind,                SSNES_REWIND)
      DECLARE_BIND(movie_record_toggle,   SSNES_MOVIE_RECORD_TOGGLE)
   },
   {
      DECLARE_BIND(player4_a,             SNES_DEVICE_ID_JOYPAD_A)
      DECLARE_BIND(player4_b,             SNES_DEVICE_ID_JOYPAD_B)
      DECLARE_BIND(player4_y,             SNES_DEVICE_ID_JOYPAD_Y)
      DECLARE_BIND(player4_x,             SNES_DEVICE_ID_JOYPAD_X)
      DECLARE_BIND(player4_start,         SNES_DEVICE_ID_JOYPAD_START)
      DECLARE_BIND(player4_select,        SNES_DEVICE_ID_JOYPAD_SELECT)
      DECLARE_BIND(player4_l,             SNES_DEVICE_ID_JOYPAD_L)
      DECLARE_BIND(player4_r,             SNES_DEVICE_ID_JOYPAD_R)
      DECLARE_BIND(player4_left,          SNES_DEVICE_ID_JOYPAD_LEFT)
      DECLARE_BIND(player4_right,         SNES_DEVICE_ID_JOYPAD_RIGHT)
      DECLARE_BIND(player4_up,            SNES_DEVICE_ID_JOYPAD_UP)
      DECLARE_BIND(player4_down,          SNES_DEVICE_ID_JOYPAD_DOWN)
      DECLARE_BIND(toggle_fast_forward,   SSNES_FAST_FORWARD_KEY)
      DECLARE_BIND(save_state,            SSNES_SAVE_STATE_KEY)
      DECLARE_BIND(load_state,            SSNES_LOAD_STATE_KEY)
      DECLARE_BIND(state_slot_increase,   SSNES_STATE_SLOT_PLUS)
      DECLARE_BIND(state_slot_decrease,   SSNES_STATE_SLOT_MINUS)
      DECLARE_BIND(exit_emulator,         SSNES_QUIT_KEY)
      DECLARE_BIND(toggle_fullscreen,     SSNES_FULLSCREEN_TOGGLE_KEY)
      DECLARE_BIND(rate_step_up,          SSNES_AUDIO_INPUT_RATE_PLUS)
      DECLARE_BIND(rate_step_down,        SSNES_AUDIO_INPUT_RATE_MINUS)
      DECLARE_BIND(rewind,                SSNES_REWIND)
      DECLARE_BIND(movie_record_toggle,   SSNES_MOVIE_RECORD_TOGGLE)
   },
   {
      DECLARE_BIND(player5_a,             SNES_DEVICE_ID_JOYPAD_A)
      DECLARE_BIND(player5_b,             SNES_DEVICE_ID_JOYPAD_B)
      DECLARE_BIND(player5_y,             SNES_DEVICE_ID_JOYPAD_Y)
      DECLARE_BIND(player5_x,             SNES_DEVICE_ID_JOYPAD_X)
      DECLARE_BIND(player5_start,         SNES_DEVICE_ID_JOYPAD_START)
      DECLARE_BIND(player5_select,        SNES_DEVICE_ID_JOYPAD_SELECT)
      DECLARE_BIND(player5_l,             SNES_DEVICE_ID_JOYPAD_L)
      DECLARE_BIND(player5_r,             SNES_DEVICE_ID_JOYPAD_R)
      DECLARE_BIND(player5_left,          SNES_DEVICE_ID_JOYPAD_LEFT)
      DECLARE_BIND(player5_right,         SNES_DEVICE_ID_JOYPAD_RIGHT)
      DECLARE_BIND(player5_up,            SNES_DEVICE_ID_JOYPAD_UP)
      DECLARE_BIND(player5_down,          SNES_DEVICE_ID_JOYPAD_DOWN)
      DECLARE_BIND(toggle_fast_forward,   SSNES_FAST_FORWARD_KEY)
      DECLARE_BIND(save_state,            SSNES_SAVE_STATE_KEY)
      DECLARE_BIND(load_state,            SSNES_LOAD_STATE_KEY)
      DECLARE_BIND(state_slot_increase,   SSNES_STATE_SLOT_PLUS)
      DECLARE_BIND(state_slot_decrease,   SSNES_STATE_SLOT_MINUS)
      DECLARE_BIND(exit_emulator,         SSNES_QUIT_KEY)
      DECLARE_BIND(toggle_fullscreen,     SSNES_FULLSCREEN_TOGGLE_KEY)
      DECLARE_BIND(rate_step_up,          SSNES_AUDIO_INPUT_RATE_PLUS)
      DECLARE_BIND(rate_step_down,        SSNES_AUDIO_INPUT_RATE_MINUS)
      DECLARE_BIND(rewind,                SSNES_REWIND)
      DECLARE_BIND(movie_record_toggle,   SSNES_MOVIE_RECORD_TOGGLE)
   },
};

struct key_map
{
   const char *str;
   int key;
};

// Edit: Not portable to different input systems atm. Might move this map into the driver itself or something.
// However, this should map nicely over to other systems aswell since the definition are mostly the same anyways.
static const struct key_map sdlk_map[] = {
   { "left", SDLK_LEFT },
   { "right", SDLK_RIGHT },
   { "up", SDLK_UP },
   { "down", SDLK_DOWN },
   { "enter", SDLK_RETURN },
   { "tab", SDLK_TAB },
   { "insert", SDLK_INSERT },
   { "del", SDLK_DELETE },
   { "rshift", SDLK_RSHIFT },
   { "shift", SDLK_LSHIFT },
   { "ctrl", SDLK_LCTRL },
   { "alt", SDLK_LALT },
   { "space", SDLK_SPACE },
   { "escape", SDLK_ESCAPE },
   { "kp_plus", SDLK_KP_PLUS },
   { "kp_minus", SDLK_KP_MINUS },
   { "f1", SDLK_F1 },
   { "f2", SDLK_F2 },
   { "f3", SDLK_F3 },
   { "f4", SDLK_F4 },
   { "f5", SDLK_F5 },
   { "f6", SDLK_F6 },
   { "f7", SDLK_F7 },
   { "f8", SDLK_F8 },
   { "f9", SDLK_F9 },
   { "f10", SDLK_F10 },
   { "f11", SDLK_F11 },
   { "f12", SDLK_F12 },
};

static struct snes_keybind *find_snes_bind(unsigned port, int id)
{
   struct snes_keybind *binds = g_settings.input.binds[port];

   for (int i = 0; binds[i].id != -1; i++)
   {
      if (id == binds[i].id)
         return &binds[i];
   }
   return NULL;
}

static int find_sdlk_bind(const char *str)
{
   for (int i = 0; i < sizeof(sdlk_map)/sizeof(struct key_map); i++)
   {
      if (strcasecmp(sdlk_map[i].str, str) == 0)
         return sdlk_map[i].key;
   }
   return -1;
}

static int find_sdlk_key(const char *str)
{
   // If the bind is a normal key-press ...
   if (strlen(str) == 1 && isalpha(*str))
      return (int)SDLK_a + (tolower(*str) - (int)'a');
   else // Check if we have a special mapping for it.
      return find_sdlk_bind(str);
}

static void read_keybinds(config_file_t *conf)
{
   char *tmp_key = NULL;
   char *tmp_btn = NULL;
   char *tmp_axis = NULL;

   for (int j = 0; j < MAX_PLAYERS; j++)
   {
      for (int i = 0; i < sizeof(bind_maps[0])/sizeof(struct bind_map); i++)
      {
         struct snes_keybind *bind = find_snes_bind(j, bind_maps[j][i].snes_key);
         if (!bind)
            continue;

         if (bind_maps[j][i].key && config_get_string(conf, bind_maps[j][i].key, &tmp_key))
         {
            int key = find_sdlk_key(tmp_key);

            if (key >= 0)
               bind->key = key;

            free(tmp_key);
            tmp_key = NULL;
         }

         if (bind_maps[j][i].btn && config_get_string(conf, bind_maps[j][i].btn, &tmp_btn))
         {
            const char *btn = tmp_btn;
            if (*btn++ == 'h')
            {
               if (isdigit(*btn))
               {
                  char *dir = NULL;
                  int hat = strtol(btn, &dir, 0);
                  int hat_dir = 0;
                  if (dir)
                  {
                     if (strcasecmp(dir, "up") == 0)
                        hat_dir = HAT_UP_MASK;
                     else if (strcasecmp(dir, "down") == 0)
                        hat_dir = HAT_DOWN_MASK;
                     else if (strcasecmp(dir, "left") == 0)
                        hat_dir = HAT_LEFT_MASK;
                     else if (strcasecmp(dir, "right") == 0)
                        hat_dir = HAT_RIGHT_MASK;

                     if (hat_dir)
                        bind->joykey = HAT_MAP(hat, hat_dir);
                  }
               }
            }
            else
               bind->joykey = strtol(tmp_btn, NULL, 0);
            free(tmp_btn);
         }

         if (bind_maps[j][i].axis && config_get_string(conf, bind_maps[j][i].axis, &tmp_axis))
         {
            if (strlen(tmp_axis) >= 2 && (*tmp_axis == '+' || *tmp_axis == '-'))
            {
               int axis = strtol(tmp_axis + 1, NULL, 0);
               if (*tmp_axis == '+')
                  bind->joyaxis = AXIS_POS(axis);
               else
                  bind->joyaxis = AXIS_NEG(axis);

            }
            free(tmp_axis);
            tmp_axis = NULL;
         }
      }
   }
}
#endif
