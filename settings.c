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

#include "general.h"
#include "conf/config_file.h"
#include "conf/config_file_macros.h"
#include "input/keysym.h"
#include "strl.h"
#include "config.def.h"
#include "file.h"
#include "posix_string.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

struct settings g_settings;
struct global g_extern;
#ifdef SSNES_CONSOLE
struct console_settings g_console;
#endif

void config_set_defaults(void)
{
   const char *def_video = NULL;
   const char *def_audio = NULL;
   const char *def_input = NULL;

   switch (VIDEO_DEFAULT_DRIVER)
   {
      case VIDEO_GL:
         def_video = "gl";
         break;
      case VIDEO_WII:
         def_video = "wii";
         break;
      case VIDEO_XENON360:
         def_video = "xenon360";
         break;
      case VIDEO_XDK360:
         def_video = "xdk360";
         break;
      case VIDEO_XVIDEO:
         def_video = "xvideo";
         break;
      case VIDEO_SDL:
         def_video = "sdl";
         break;
      case VIDEO_EXT:
         def_video = "ext";
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
      case AUDIO_COREAUDIO:
         def_audio = "coreaudio";
         break;
      case AUDIO_AL:
         def_audio = "openal";
         break;
      case AUDIO_SDL:
         def_audio = "sdl";
         break;
      case AUDIO_DSOUND:
         def_audio = "dsound";
         break;
      case AUDIO_XAUDIO:
         def_audio = "xaudio";
         break;
      case AUDIO_PULSE:
         def_audio = "pulse";
         break;
      case AUDIO_EXT:
         def_audio = "ext";
         break;
      case AUDIO_XENON360:
         def_audio = "xenon360";
         break;
      case AUDIO_XDK360:
         def_audio = "xdk360";
         break;
      case AUDIO_PS3:
         def_audio = "ps3";
         break;
      case AUDIO_WII:
         def_audio = "wii";
         break;
      default:
         break;
   }

   switch (INPUT_DEFAULT_DRIVER)
   {
      case INPUT_PS3:
         def_input = "ps3";
         break;
      case INPUT_SDL:
         def_input = "sdl";
         break;
      case INPUT_X:
         def_input = "x";
         break;
      case INPUT_XENON360:
         def_input = "xenon360";
         break;
      case INPUT_XDK360:
         def_input = "xdk360";
         break;
      case INPUT_WII:
         def_input = "wii";
         break;
      default:
         break;
   }

   if (def_video)
      strlcpy(g_settings.video.driver, def_video, sizeof(g_settings.video.driver));
   if (def_audio)
      strlcpy(g_settings.audio.driver, def_audio, sizeof(g_settings.audio.driver));
   if (def_input)
      strlcpy(g_settings.input.driver, def_input, sizeof(g_settings.input.driver));

   g_settings.video.xscale = xscale;
   g_settings.video.yscale = yscale;
   g_settings.video.fullscreen = g_extern.force_fullscreen ? true : fullscreen;
   g_settings.video.fullscreen_x = fullscreen_x;
   g_settings.video.fullscreen_y = fullscreen_y;
   g_settings.video.force_16bit = force_16bit;
   g_settings.video.disable_composition = disable_composition;
   g_settings.video.vsync = vsync;
   g_settings.video.smooth = video_smooth;
   g_settings.video.force_aspect = force_aspect;
   g_settings.video.crop_overscan = crop_overscan;
   g_settings.video.aspect_ratio = -1.0f; // Automatic
   g_settings.video.shader_type = SSNES_SHADER_AUTO;

#ifdef HAVE_FREETYPE
   g_settings.video.font_enable = font_enable;
   g_settings.video.font_size = font_size;
   g_settings.video.font_scale = font_scale;
   g_settings.video.msg_pos_x = message_pos_offset_x;
   g_settings.video.msg_pos_y = message_pos_offset_y;
   
   g_settings.video.msg_color_r = ((message_color >> 16) & 0xff) / 255.0f;
   g_settings.video.msg_color_g = ((message_color >>  8) & 0xff) / 255.0f;
   g_settings.video.msg_color_b = ((message_color >>  0) & 0xff) / 255.0f;
#endif

#if defined(HAVE_CG) || defined(HAVE_XML)
   g_settings.video.render_to_texture = render_to_texture;
   g_settings.video.fbo_scale_x = fbo_scale_x;
   g_settings.video.fbo_scale_y = fbo_scale_y;
   g_settings.video.second_pass_smooth = second_pass_smooth;
#endif

   g_settings.video.refresh_rate = refresh_rate;
   g_settings.video.hires_record = hires_record;
   g_settings.video.post_filter_record = post_filter_record;

   g_settings.audio.enable = audio_enable;
   g_settings.audio.out_rate = out_rate;
   g_settings.audio.rate_step = audio_rate_step;
   if (audio_device)
      strlcpy(g_settings.audio.device, audio_device, sizeof(g_settings.audio.device));
   g_settings.audio.latency = out_latency;
   g_settings.audio.sync = audio_sync;
   g_settings.audio.rate_control = rate_control;
   g_settings.audio.rate_control_delta = rate_control_delta;

   g_settings.rewind_enable = rewind_enable;
   g_settings.rewind_buffer_size = rewind_buffer_size;
   g_settings.rewind_granularity = rewind_granularity;
   g_settings.pause_nonactive = pause_nonactive;
   g_settings.autosave_interval = autosave_interval;

   g_settings.block_sram_overwrite = block_sram_overwrite;
   g_settings.savestate_auto_index = savestate_auto_index;

   ssnes_assert(sizeof(g_settings.input.binds[0]) >= sizeof(snes_keybinds_1));
   ssnes_assert(sizeof(g_settings.input.binds[1]) >= sizeof(snes_keybinds_rest));
   memcpy(g_settings.input.binds[0], snes_keybinds_1, sizeof(snes_keybinds_1));
   for (unsigned i = 1; i < MAX_PLAYERS; i++)
      memcpy(g_settings.input.binds[i], snes_keybinds_rest, sizeof(snes_keybinds_rest));

   // Verify that binds are in proper order.
   for (int i = 0; i < MAX_PLAYERS; i++)
      for (int j = 0; j < SSNES_BIND_LIST_END; j++)
         if (g_settings.input.binds[i][j].valid)
            ssnes_assert(j == g_settings.input.binds[i][j].id);

   g_settings.input.axis_threshold = axis_threshold;
   g_settings.input.netplay_client_swap_input = netplay_client_swap_input;
   for (int i = 0; i < MAX_PLAYERS; i++)
      g_settings.input.joypad_map[i] = i;
}

#ifdef HAVE_CONFIGFILE
static void parse_config_file(void);
#endif

void config_load(void)
{
#ifdef SSNES_CONSOLE
   if (!g_console.block_config_read)
#endif
   {
      config_set_defaults();

#ifdef HAVE_CONFIGFILE
      parse_config_file();
#endif
   }
}

#ifdef HAVE_CONFIGFILE
static config_file_t *open_default_config_file(void)
{
   config_file_t *conf = NULL;
#if defined(_WIN32) && !defined(_XBOX)
   // Just do something for now.
   conf = config_file_new("ssnes.cfg");
   if (!conf)
   {
      const char *appdata = getenv("APPDATA");
      if (appdata)
      {
         char conf_path[PATH_MAX];
         strlcpy(conf_path, appdata, sizeof(conf_path));
         strlcat(conf_path, "/ssnes.cfg", sizeof(conf_path));
         conf = config_file_new(conf_path);
      }
   }
#elif defined(__APPLE__)
   const char *home = getenv("HOME");
   if (home)
   {
      char conf_path[PATH_MAX];
      strlcpy(conf_path, home, sizeof(conf_path));
      strlcat(conf_path, "/.ssnes.cfg", sizeof(conf_path));
      conf = config_file_new(conf_path);
   }
   if (!conf)
      conf = config_file_new("/etc/ssnes.cfg");
#elif defined(__CELLOS_LV2__) || defined(_XBOX)
   return NULL;
#else
   const char *xdg = getenv("XDG_CONFIG_HOME");
   if (!xdg)
      SSNES_WARN("XDG_CONFIG_HOME is not defined. Will look for config in $HOME/.ssnes.cfg ...\n");

   const char *home = getenv("HOME");
   if (xdg)
   {
      char conf_path[PATH_MAX];
      strlcpy(conf_path, xdg, sizeof(conf_path));
      strlcat(conf_path, "/ssnes/ssnes.cfg", sizeof(conf_path));
      conf = config_file_new(conf_path);
   }
   else if (home)
   {
      char conf_path[PATH_MAX];
      strlcpy(conf_path, home, sizeof(conf_path));
      strlcat(conf_path, "/.ssnes.cfg", sizeof(conf_path));
      conf = config_file_new(conf_path);
   }
   // Try this as a last chance...
   if (!conf)
      conf = config_file_new("/etc/ssnes.cfg");
#endif

   return conf;
}
#endif


#ifdef HAVE_CONFIGFILE
static void parse_config_file(void)
{
   bool ret;
   if (*g_extern.config_path)
      ret = config_load_file(g_extern.config_path);
   else
      ret = config_load_file(NULL);

   if (!ret)
   {
      SSNES_ERR("Couldn't find config at path: \"%s\"\n", g_extern.config_path);
      ssnes_fail(1, "parse_config_file()");
   }
}

bool config_load_file(const char *path)
{
   config_file_t *conf = NULL;

   if (path)
   {
      conf = config_file_new(path);
      if (!conf)
         return false;
   }
   else
      conf = open_default_config_file();

   if (conf == NULL)
      return true;

   if (g_extern.verbose)
   {
      fprintf(stderr, "=== Config ===\n");
      config_file_dump_all(conf, stderr);
      fprintf(stderr, "=== Config end ===\n");
   }

   char tmp_str[PATH_MAX];

   CONFIG_GET_FLOAT(video.xscale, "video_xscale");
   CONFIG_GET_FLOAT(video.yscale, "video_yscale");
   CONFIG_GET_INT(video.fullscreen_x, "video_fullscreen_x");
   CONFIG_GET_INT(video.fullscreen_y, "video_fullscreen_y");

   if (!g_extern.force_fullscreen)
      CONFIG_GET_BOOL(video.fullscreen, "video_fullscreen");

   CONFIG_GET_BOOL(video.force_16bit, "video_force_16bit");
   CONFIG_GET_BOOL(video.disable_composition, "video_disable_composition");
   CONFIG_GET_BOOL(video.vsync, "video_vsync");
   CONFIG_GET_BOOL(video.smooth, "video_smooth");
   CONFIG_GET_BOOL(video.force_aspect, "video_force_aspect");
   CONFIG_GET_BOOL(video.crop_overscan, "video_crop_overscan");
   CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");
   CONFIG_GET_FLOAT(video.refresh_rate, "video_refresh_rate");

   CONFIG_GET_STRING(video.cg_shader_path, "video_cg_shader");
   CONFIG_GET_STRING(video.bsnes_shader_path, "video_bsnes_shader");
   CONFIG_GET_STRING(video.second_pass_shader, "video_second_pass_shader");
   CONFIG_GET_BOOL(video.render_to_texture, "video_render_to_texture");
   CONFIG_GET_FLOAT(video.fbo_scale_x, "video_fbo_scale_x");
   CONFIG_GET_FLOAT(video.fbo_scale_y, "video_fbo_scale_y");
   CONFIG_GET_BOOL(video.second_pass_smooth, "video_second_pass_smooth");

#ifdef HAVE_FREETYPE
   CONFIG_GET_STRING(video.font_path, "video_font_path");
   CONFIG_GET_INT(video.font_size, "video_font_size");
   CONFIG_GET_BOOL(video.font_enable, "video_font_enable");
   CONFIG_GET_BOOL(video.font_scale, "video_font_scale");
   CONFIG_GET_FLOAT(video.msg_pos_x, "video_message_pos_x");
   CONFIG_GET_FLOAT(video.msg_pos_y, "video_message_pos_y");

   unsigned msg_color;
   if (config_get_hex(conf, "video_message_color", &msg_color))
   {
      g_settings.video.msg_color_r = ((msg_color >> 16) & 0xff) / 255.0f;
      g_settings.video.msg_color_g = ((msg_color >>  8) & 0xff) / 255.0f;
      g_settings.video.msg_color_b = ((msg_color >>  0) & 0xff) / 255.0f;
   }
#endif

   CONFIG_GET_BOOL(video.hires_record, "video_hires_record");
   CONFIG_GET_BOOL(video.post_filter_record, "video_post_filter_record");

#ifdef HAVE_DYLIB
   CONFIG_GET_STRING(video.filter_path, "video_filter");
   CONFIG_GET_STRING(video.external_driver, "video_external_driver");
   CONFIG_GET_STRING(audio.external_driver, "audio_external_driver");
#endif

#if defined(HAVE_CG) || defined(HAVE_XML)
   if (config_get_array(conf, "video_shader_type", tmp_str, sizeof(tmp_str)))
   {
      if (strcmp("cg", tmp_str) == 0)
         g_settings.video.shader_type = SSNES_SHADER_CG;
      else if (strcmp("bsnes", tmp_str) == 0)
         g_settings.video.shader_type = SSNES_SHADER_BSNES;
      else if (strcmp("auto", tmp_str) == 0)
         g_settings.video.shader_type = SSNES_SHADER_AUTO;
      else if (strcmp("none", tmp_str) == 0)
         g_settings.video.shader_type = SSNES_SHADER_NONE;
   }
#endif

#if defined(HAVE_XML)
   CONFIG_GET_STRING(video.shader_dir, "video_shader_dir");
#endif

   CONFIG_GET_FLOAT(input.axis_threshold, "input_axis_threshold");
   CONFIG_GET_BOOL(input.netplay_client_swap_input, "netplay_client_swap_input");

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      char buf[64];
      snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i);
      CONFIG_GET_INT(input.joypad_map[i], buf);
   }

   // Audio settings.
   CONFIG_GET_BOOL(audio.enable, "audio_enable");
   CONFIG_GET_INT(audio.out_rate, "audio_out_rate");
   CONFIG_GET_FLOAT(audio.rate_step, "audio_rate_step");
   CONFIG_GET_STRING(audio.device, "audio_device");
   CONFIG_GET_INT(audio.latency, "audio_latency");
   CONFIG_GET_BOOL(audio.sync, "audio_sync");
   CONFIG_GET_BOOL(audio.rate_control, "audio_rate_control");
   CONFIG_GET_FLOAT(audio.rate_control_delta, "audio_rate_control_delta");

   CONFIG_GET_STRING(video.driver, "video_driver");
   CONFIG_GET_STRING(audio.driver, "audio_driver");
   CONFIG_GET_STRING(audio.dsp_plugin, "audio_dsp_plugin");
   CONFIG_GET_STRING(input.driver, "input_driver");

   if (!*g_settings.libsnes)
      CONFIG_GET_STRING(libsnes, "libsnes_path");

   CONFIG_GET_STRING(screenshot_directory, "screenshot_directory");
   if (*g_settings.screenshot_directory && !path_is_directory(g_settings.screenshot_directory))
   {
      SSNES_WARN("screenshot_directory is not an existing directory, ignoring ...\n");
      *g_settings.screenshot_directory = '\0';
   }

   CONFIG_GET_BOOL(rewind_enable, "rewind_enable");

   int buffer_size;
   if (config_get_int(conf, "rewind_buffer_size", &buffer_size))
      g_settings.rewind_buffer_size = buffer_size * UINT64_C(1000000);

   CONFIG_GET_INT(rewind_granularity, "rewind_granularity");

   CONFIG_GET_BOOL(pause_nonactive, "pause_nonactive");
   CONFIG_GET_INT(autosave_interval, "autosave_interval");

   CONFIG_GET_STRING(cheat_database, "cheat_database_path");
   CONFIG_GET_STRING(cheat_settings_path, "cheat_settings_path");

   CONFIG_GET_BOOL(block_sram_overwrite, "block_sram_overwrite");
   CONFIG_GET_BOOL(savestate_auto_index, "savestate_auto_index");

   if (config_get_string(conf, "environment_variables",
            &g_extern.system.environment))
   {
      g_extern.system.environment_split = strdup(g_extern.system.environment);
      if (!g_extern.system.environment_split)
      {
         SSNES_ERR("Failed to allocate environment variables. Will ignore them.\n");
         free(g_extern.system.environment);
         g_extern.system.environment = NULL;
      }
   }

   if (!g_extern.has_set_save_path && config_get_array(conf, "savefile_directory", tmp_str, sizeof(tmp_str)))
   {
      if (path_is_directory(tmp_str))
      {
         strlcpy(g_extern.savefile_name_srm, tmp_str, sizeof(g_extern.savefile_name_srm));
         fill_pathname_dir(g_extern.savefile_name_srm, g_extern.basename, ".srm", sizeof(g_extern.savefile_name_srm));
      }
      else
         SSNES_WARN("savefile_directory is not a directory, ignoring ....\n");
   }
   if (!g_extern.has_set_state_path && config_get_array(conf, "savestate_directory", tmp_str, sizeof(tmp_str)))
   {
      if (path_is_directory(tmp_str))
      {
         strlcpy(g_extern.savestate_name, tmp_str, sizeof(g_extern.savestate_name));
         fill_pathname_dir(g_extern.savestate_name, g_extern.basename, ".state", sizeof(g_extern.savestate_name));
      }
      else
         SSNES_WARN("savestate_directory is not a directory, ignoring ...\n");
   }

   config_read_keybinds(conf);

   config_file_free(conf);
   return true;
}

struct bind_map
{
   bool valid;
   const char *key;
   const char *btn;
   const char *axis;
   int snes_key;
};

#define DECLARE_BIND(x, bind) { true, "input_" #x, "input_" #x "_btn", "input_" #x "_axis", bind }
#define DECL_PLAYER(P) \
   { \
      DECLARE_BIND(player##P##_b,             SNES_DEVICE_ID_JOYPAD_B), \
      DECLARE_BIND(player##P##_y,             SNES_DEVICE_ID_JOYPAD_Y), \
      DECLARE_BIND(player##P##_select,        SNES_DEVICE_ID_JOYPAD_SELECT), \
      DECLARE_BIND(player##P##_start,         SNES_DEVICE_ID_JOYPAD_START), \
      DECLARE_BIND(player##P##_up,            SNES_DEVICE_ID_JOYPAD_UP), \
      DECLARE_BIND(player##P##_down,          SNES_DEVICE_ID_JOYPAD_DOWN), \
      DECLARE_BIND(player##P##_left,          SNES_DEVICE_ID_JOYPAD_LEFT), \
      DECLARE_BIND(player##P##_right,         SNES_DEVICE_ID_JOYPAD_RIGHT), \
      DECLARE_BIND(player##P##_a,             SNES_DEVICE_ID_JOYPAD_A), \
      DECLARE_BIND(player##P##_x,             SNES_DEVICE_ID_JOYPAD_X), \
      DECLARE_BIND(player##P##_l,             SNES_DEVICE_ID_JOYPAD_L), \
      DECLARE_BIND(player##P##_r,             SNES_DEVICE_ID_JOYPAD_R), \
   }

// Big and nasty bind map... :)
static const struct bind_map bind_maps[MAX_PLAYERS][SSNES_BIND_LIST_END] = {
   {
      DECLARE_BIND(player1_b,             SNES_DEVICE_ID_JOYPAD_B),
      DECLARE_BIND(player1_y,             SNES_DEVICE_ID_JOYPAD_Y),
      DECLARE_BIND(player1_select,        SNES_DEVICE_ID_JOYPAD_SELECT),
      DECLARE_BIND(player1_start,         SNES_DEVICE_ID_JOYPAD_START),
      DECLARE_BIND(player1_up,            SNES_DEVICE_ID_JOYPAD_UP),
      DECLARE_BIND(player1_down,          SNES_DEVICE_ID_JOYPAD_DOWN),
      DECLARE_BIND(player1_left,          SNES_DEVICE_ID_JOYPAD_LEFT),
      DECLARE_BIND(player1_right,         SNES_DEVICE_ID_JOYPAD_RIGHT),
      DECLARE_BIND(player1_a,             SNES_DEVICE_ID_JOYPAD_A),
      DECLARE_BIND(player1_x,             SNES_DEVICE_ID_JOYPAD_X),
      DECLARE_BIND(player1_l,             SNES_DEVICE_ID_JOYPAD_L),
      DECLARE_BIND(player1_r,             SNES_DEVICE_ID_JOYPAD_R),

      DECLARE_BIND(toggle_fast_forward,   SSNES_FAST_FORWARD_KEY),
      DECLARE_BIND(hold_fast_forward,     SSNES_FAST_FORWARD_HOLD_KEY),
      DECLARE_BIND(load_state,            SSNES_LOAD_STATE_KEY),
      DECLARE_BIND(save_state,            SSNES_SAVE_STATE_KEY),
      DECLARE_BIND(toggle_fullscreen,     SSNES_FULLSCREEN_TOGGLE_KEY),
      DECLARE_BIND(exit_emulator,         SSNES_QUIT_KEY),
      DECLARE_BIND(state_slot_increase,   SSNES_STATE_SLOT_PLUS),
      DECLARE_BIND(state_slot_decrease,   SSNES_STATE_SLOT_MINUS),
      DECLARE_BIND(rate_step_up,          SSNES_AUDIO_INPUT_RATE_PLUS),
      DECLARE_BIND(rate_step_down,        SSNES_AUDIO_INPUT_RATE_MINUS),
      DECLARE_BIND(rewind,                SSNES_REWIND),
      DECLARE_BIND(movie_record_toggle,   SSNES_MOVIE_RECORD_TOGGLE),
      DECLARE_BIND(pause_toggle,          SSNES_PAUSE_TOGGLE),
      DECLARE_BIND(frame_advance,         SSNES_FRAMEADVANCE),
      DECLARE_BIND(reset,                 SSNES_RESET),
      DECLARE_BIND(shader_next,           SSNES_SHADER_NEXT),
      DECLARE_BIND(shader_prev,           SSNES_SHADER_PREV),
      DECLARE_BIND(cheat_index_plus,      SSNES_CHEAT_INDEX_PLUS),
      DECLARE_BIND(cheat_index_minus,     SSNES_CHEAT_INDEX_MINUS),
      DECLARE_BIND(cheat_toggle,          SSNES_CHEAT_TOGGLE),
      DECLARE_BIND(screenshot,            SSNES_SCREENSHOT),
      DECLARE_BIND(dsp_config,            SSNES_DSP_CONFIG),
      DECLARE_BIND(audio_mute,            SSNES_MUTE),
      DECLARE_BIND(netplay_flip_players,  SSNES_NETPLAY_FLIP),
   },

   DECL_PLAYER(2),
   DECL_PLAYER(3),
   DECL_PLAYER(4),
   DECL_PLAYER(5),
   DECL_PLAYER(6),
   DECL_PLAYER(7),
   DECL_PLAYER(8),
};

struct key_map
{
   const char *str;
   int key;
};

// Edit: Not portable to different input systems atm. Might move this map into the driver itself or something.
// However, this should map nicely over to other systems aswell since the definition are mostly the same anyways.
static const struct key_map sk_map[] = {
   { "left", SK_LEFT },
   { "right", SK_RIGHT },
   { "up", SK_UP },
   { "down", SK_DOWN },
   { "enter", SK_RETURN },
   { "kp_enter", SK_KP_ENTER },
   { "tab", SK_TAB },
   { "insert", SK_INSERT },
   { "del", SK_DELETE },
   { "end", SK_END },
   { "home", SK_HOME },
   { "rshift", SK_RSHIFT },
   { "shift", SK_LSHIFT },
   { "ctrl", SK_LCTRL },
   { "alt", SK_LALT },
   { "space", SK_SPACE },
   { "escape", SK_ESCAPE },
   { "add", SK_KP_PLUS },
   { "subtract", SK_KP_MINUS },
   { "kp_plus", SK_KP_PLUS },
   { "kp_minus", SK_KP_MINUS },
   { "f1", SK_F1 },
   { "f2", SK_F2 },
   { "f3", SK_F3 },
   { "f4", SK_F4 },
   { "f5", SK_F5 },
   { "f6", SK_F6 },
   { "f7", SK_F7 },
   { "f8", SK_F8 },
   { "f9", SK_F9 },
   { "f10", SK_F10 },
   { "f11", SK_F11 },
   { "f12", SK_F12 },
   { "num0", SK_0 },
   { "num1", SK_1 },
   { "num2", SK_2 },
   { "num3", SK_3 },
   { "num4", SK_4 },
   { "num5", SK_5 },
   { "num6", SK_6 },
   { "num7", SK_7 },
   { "num8", SK_8 },
   { "num9", SK_9 },
   { "pageup", SK_PAGEUP },
   { "pagedown", SK_PAGEDOWN },
   { "keypad0", SK_KP0 },
   { "keypad1", SK_KP1 },
   { "keypad2", SK_KP2 },
   { "keypad3", SK_KP3 },
   { "keypad4", SK_KP4 },
   { "keypad5", SK_KP5 },
   { "keypad6", SK_KP6 },
   { "keypad7", SK_KP7 },
   { "keypad8", SK_KP8 },
   { "keypad9", SK_KP9 },
   { "period", SK_PERIOD },
   { "capslock", SK_CAPSLOCK },
   { "numlock", SK_NUMLOCK },
   { "backspace", SK_BACKSPACE },
   { "multiply", SK_KP_MULTIPLY },
   { "divide", SK_KP_DIVIDE },
   { "print_screen", SK_PRINT },
   { "scroll_lock", SK_SCROLLOCK },
   { "tilde", SK_BACKQUOTE },
   { "backquote", SK_BACKQUOTE },
   { "pause", SK_PAUSE },
   { "nul", SK_UNKNOWN },
};

static struct snes_keybind *find_snes_bind(unsigned port, int id)
{
   struct snes_keybind *binds = g_settings.input.binds[port];
   return binds[id].valid ? &binds[id] : NULL;
}

static int find_sk_bind(const char *str)
{
   for (size_t i = 0; i < sizeof(sk_map) / sizeof(struct key_map); i++)
   {
      if (strcasecmp(sk_map[i].str, str) == 0)
         return sk_map[i].key;
   }

   return -1;
}

static int find_sk_key(const char *str)
{
   if (strlen(str) == 1 && isalpha(*str))
      return (int)SK_a + (tolower(*str) - (int)'a');
   else
      return find_sk_bind(str);
}

static void read_keybinds_keyboard(config_file_t *conf, unsigned player, unsigned index, struct snes_keybind *bind)
{
   char tmp[64];

   if (bind_maps[player][index].key &&
         config_get_array(conf, bind_maps[player][index].key, tmp, sizeof(tmp)))
   {
      int key = find_sk_key(tmp);

      if (key >= 0)
         bind->key = (enum ssnes_key)key;
   }
}

static void parse_hat(struct snes_keybind *bind, const char *str)
{
   if (isdigit(*str))
   {
      char *dir = NULL;
      int hat = strtol(str, &dir, 0);
      int hat_dir = 0;

      if (dir)
      {
         if (strcasecmp(str, "up") == 0)
            hat_dir = HAT_UP_MASK;
         else if (strcasecmp(str, "down") == 0)
            hat_dir = HAT_DOWN_MASK;
         else if (strcasecmp(str, "left") == 0)
            hat_dir = HAT_LEFT_MASK;
         else if (strcasecmp(str, "right") == 0)
            hat_dir = HAT_RIGHT_MASK;

         if (hat_dir)
            bind->joykey = HAT_MAP(hat, hat_dir);
      }
   }
}

static void read_keybinds_button(config_file_t *conf, unsigned player, unsigned index, struct snes_keybind *bind)
{
   char tmp[64];
   if (bind_maps[player][index].btn &&
         config_get_array(conf, bind_maps[player][index].btn, tmp, sizeof(tmp)))
   {
      const char *btn = tmp;
      if (strcmp(btn, "nul") == 0)
         bind->joykey = NO_BTN;
      else
      {
         if (*btn == 'h')
            parse_hat(bind, btn + 1);
         else
            bind->joykey = strtol(tmp, NULL, 0);
      }
   }
}

static void read_keybinds_axis(config_file_t *conf, unsigned player, unsigned index, struct snes_keybind *bind)
{
   char tmp[64];
   if (bind_maps[player][index].axis &&
         config_get_array(conf, bind_maps[player][index].axis, tmp, sizeof(tmp)))
   {
      if (strcmp(tmp, "nul") == 0)
         bind->joyaxis = AXIS_NONE;
      else if (strlen(tmp) >= 2 && (*tmp == '+' || *tmp == '-'))
      {
         int axis = strtol(tmp + 1, NULL, 0);
         if (*tmp == '+')
            bind->joyaxis = AXIS_POS(axis);
         else
            bind->joyaxis = AXIS_NEG(axis);

      }
   }
}

static void read_keybinds_player(config_file_t *conf, unsigned player)
{
   for (unsigned i = 0; bind_maps[player][i].valid; i++)
   {
      struct snes_keybind *bind = find_snes_bind(player, bind_maps[player][i].snes_key);
      ssnes_assert(bind);

      read_keybinds_keyboard(conf, player, i, bind);
      read_keybinds_button(conf, player, i, bind);
      read_keybinds_axis(conf, player, i, bind);
   }
}

void config_read_keybinds(config_file_t *conf)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      read_keybinds_player(conf, i);
}

#endif

