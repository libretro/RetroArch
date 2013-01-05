/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "general.h"
#include "conf/config_file.h"
#include "conf/config_file_macros.h"
#include "compat/strl.h"
#include "config.def.h"
#include "file.h"
#include "compat/posix_string.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

struct settings g_settings;
struct global g_extern;

const char *config_get_default_audio(void)
{
   switch (AUDIO_DEFAULT_DRIVER)
   {
      case AUDIO_RSOUND:
         return "rsound";
      case AUDIO_OSS:
         return "oss";
      case AUDIO_ALSA:
         return "alsa";
      case AUDIO_ALSATHREAD:
         return "alsathread";
      case AUDIO_ROAR:
         return "roar";
      case AUDIO_COREAUDIO:
         return "coreaudio";
      case AUDIO_AL:
         return "openal";
      case AUDIO_SL:
         return "opensl";
      case AUDIO_SDL:
         return "sdl";
      case AUDIO_DSOUND:
         return "dsound";
      case AUDIO_XAUDIO:
         return "xaudio";
      case AUDIO_PULSE:
         return "pulse";
      case AUDIO_EXT:
         return "ext";
      case AUDIO_XENON360:
         return "xenon360";
      case AUDIO_XDK360:
         return "xdk360";
      case AUDIO_PS3:
         return "ps3";
      case AUDIO_WII:
         return "gx";
      case AUDIO_NULL:
         return "null";
      default:
         return NULL;
   }
}

const char *config_get_default_video(void)
{
   switch (VIDEO_DEFAULT_DRIVER)
   {
      case VIDEO_GL:
         return "gl";
      case VIDEO_WII:
         return "gx";
      case VIDEO_XENON360:
         return "xenon360";
      case VIDEO_XDK_D3D:
         return "xdk_d3d";
      case VIDEO_D3D9:
         return "d3d9";
      case VIDEO_PSP1:
         return "psp1";
      case VIDEO_VITA:
         return "vita";
      case VIDEO_XVIDEO:
         return "xvideo";
      case VIDEO_SDL:
         return "sdl";
      case VIDEO_EXT:
         return "ext";
      case VIDEO_VG:
         return "vg";
      case VIDEO_NULL:
         return "null";
      default:
         return NULL;
   }
}

const char *config_get_default_input(void)
{
   switch (INPUT_DEFAULT_DRIVER)
   {
      case INPUT_ANDROID:
         return "android_input";
      case INPUT_PS3:
         return "ps3";
      case INPUT_PSP:
         return "psp";
      case INPUT_SDL:
         return "sdl";
      case INPUT_DINPUT:
         return "dinput";
      case INPUT_X:
         return "x";
      case INPUT_XENON360:
         return "xenon360";
      case INPUT_XINPUT:
         return "xinput";
      case INPUT_WII:
         return "gx";
      case INPUT_LINUXRAW:
         return "linuxraw";
      case INPUT_NULL:
         return "null";
      default:
         return NULL;
   }
}

void config_set_defaults(void)
{
   const char *def_video = config_get_default_video();
   const char *def_audio = config_get_default_audio();
   const char *def_input = config_get_default_input();

   if (def_video)
      strlcpy(g_settings.video.driver, def_video, sizeof(g_settings.video.driver));
   if (def_audio)
      strlcpy(g_settings.audio.driver, def_audio, sizeof(g_settings.audio.driver));
   if (def_input)
      strlcpy(g_settings.input.driver, def_input, sizeof(g_settings.input.driver));

   g_settings.video.xscale = xscale;
   g_settings.video.yscale = yscale;
   g_settings.video.fullscreen = g_extern.force_fullscreen ? true : fullscreen;
   g_settings.video.windowed_fullscreen = windowed_fullscreen;
   g_settings.video.monitor_index = monitor_index;
   g_settings.video.fullscreen_x = fullscreen_x;
   g_settings.video.fullscreen_y = fullscreen_y;
   g_settings.video.disable_composition = disable_composition;
   g_settings.video.vsync = vsync;
   g_settings.video.smooth = video_smooth;
   g_settings.video.force_aspect = force_aspect;
   g_settings.video.crop_overscan = crop_overscan;
   g_settings.video.aspect_ratio = aspect_ratio;
   g_settings.video.aspect_ratio_auto = aspect_ratio_auto; // Let implementation decide if automatic, or 1:1 PAR.
   g_settings.video.shader_type = RARCH_SHADER_AUTO;
   g_settings.video.allow_rotate = allow_rotate;

   g_settings.video.font_enable = font_enable;
   g_settings.video.font_size = font_size;
   g_settings.video.font_scale = font_scale;
   g_settings.video.msg_pos_x = message_pos_offset_x;
   g_settings.video.msg_pos_y = message_pos_offset_y;
   
   g_settings.video.msg_color_r = ((message_color >> 16) & 0xff) / 255.0f;
   g_settings.video.msg_color_g = ((message_color >>  8) & 0xff) / 255.0f;
   g_settings.video.msg_color_b = ((message_color >>  0) & 0xff) / 255.0f;

#if defined(HAVE_CG) || defined(HAVE_GLSL)
   g_settings.video.render_to_texture = render_to_texture;
   g_settings.video.fbo.scale_x = fbo_scale_x;
   g_settings.video.fbo.scale_y = fbo_scale_y;
   g_settings.video.second_pass_smooth = second_pass_smooth;
#endif

   g_settings.video.refresh_rate = refresh_rate;
   g_settings.video.post_filter_record = post_filter_record;
   g_settings.video.gpu_record = gpu_record;
   g_settings.video.gpu_screenshot = gpu_screenshot;

   g_settings.audio.enable = audio_enable;
   g_settings.audio.out_rate = out_rate;
   g_settings.audio.rate_step = audio_rate_step;
   if (audio_device)
      strlcpy(g_settings.audio.device, audio_device, sizeof(g_settings.audio.device));
   g_settings.audio.latency = out_latency;
   g_settings.audio.sync = audio_sync;
   g_settings.audio.rate_control = rate_control;
   g_settings.audio.rate_control_delta = rate_control_delta;
   g_settings.audio.volume = audio_volume;

   g_settings.rewind_enable = rewind_enable;
   g_settings.rewind_buffer_size = rewind_buffer_size;
   g_settings.rewind_granularity = rewind_granularity;
   g_settings.slowmotion_ratio = slowmotion_ratio;
   g_settings.pause_nonactive = pause_nonactive;
   g_settings.autosave_interval = autosave_interval;

   g_settings.block_sram_overwrite = block_sram_overwrite;
   g_settings.savestate_auto_index = savestate_auto_index;
   g_settings.savestate_auto_save  = savestate_auto_save;
   g_settings.network_cmd_enable   = network_cmd_enable;
   g_settings.network_cmd_port     = network_cmd_port;
   g_settings.stdin_cmd_enable     = stdin_cmd_enable;

   rarch_assert(sizeof(g_settings.input.binds[0]) >= sizeof(retro_keybinds_1));
   rarch_assert(sizeof(g_settings.input.binds[1]) >= sizeof(retro_keybinds_rest));
   memcpy(g_settings.input.binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));
   for (unsigned i = 1; i < MAX_PLAYERS; i++)
      memcpy(g_settings.input.binds[i], retro_keybinds_rest, sizeof(retro_keybinds_rest));

   // Verify that binds are in proper order.
   for (int i = 0; i < MAX_PLAYERS; i++)
      for (int j = 0; j < RARCH_BIND_LIST_END; j++)
         if (g_settings.input.binds[i][j].valid)
            rarch_assert(j == g_settings.input.binds[i][j].id);

   g_settings.input.axis_threshold = axis_threshold;
   g_settings.input.netplay_client_swap_input = netplay_client_swap_input;
   g_settings.input.turbo_period = turbo_period;
   g_settings.input.turbo_duty_cycle = turbo_duty_cycle;
   for (int i = 0; i < MAX_PLAYERS; i++)
      g_settings.input.joypad_map[i] = i;

   rarch_init_msg_queue();
}

static void parse_config_file(void);

void config_load(void)
{
#ifdef RARCH_CONSOLE
   if (!g_extern.console.block_config_read)
#endif
   {
      config_set_defaults();
      parse_config_file();
   }
}

static config_file_t *open_default_config_file(void)
{
   config_file_t *conf = NULL;

#if defined(_WIN32) && !defined(_XBOX)
   // Just do something for now.
   char conf_path[PATH_MAX];
   conf = config_file_new("retroarch.cfg");
   if (!conf)
   {
      const char *appdata = getenv("APPDATA");
      if (appdata)
      {
         snprintf(conf_path, sizeof(conf_path), "%s/retroarch.cfg", appdata);
         conf = config_file_new(conf_path);
      }
   }
#elif defined(__APPLE__)
   char conf_path[PATH_MAX];
   const char *home = getenv("HOME");
   if (home)
   {
      snprintf(conf_path, sizeof(conf_path), "%s/.retroarch.cfg", home);
      conf = config_file_new(conf_path);
   }
   if (!conf)
      conf = config_file_new("/etc/retroarch.cfg");
#elif !defined(__CELLOS_LV2__) && !defined(_XBOX)
   char conf_path[PATH_MAX];
   const char *xdg = getenv("XDG_CONFIG_HOME");
   if (!xdg)
      RARCH_WARN("XDG_CONFIG_HOME is not defined. Will look for config in $HOME/.retroarch.cfg ...\n");

   const char *home = getenv("HOME");
   if (xdg)
   {
      snprintf(conf_path, sizeof(conf_path), "%s/retroarch/retroarch.cfg", xdg);
      conf = config_file_new(conf_path);
   }
   else if (home)
   {
      snprintf(conf_path, sizeof(conf_path), "%s/.retroarch.cfg", home);
      conf = config_file_new(conf_path);
   }
   // Try this as a last chance...
   if (!conf)
      conf = config_file_new("/etc/retroarch.cfg");
#endif

   return conf;
}

static void config_read_keybinds_conf(config_file_t *conf);

static void parse_config_file(void)
{
   bool ret;
   if (*g_extern.config_path)
      ret = config_load_file(g_extern.config_path);
   else
      ret = config_load_file(NULL);

   if (!ret)
   {
      RARCH_ERR("Couldn't find config at path: \"%s\"\n", g_extern.config_path);
      rarch_fail(1, "parse_config_file()");
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

   char *save;
   char tmp_append_path[PATH_MAX]; // Don't destroy append_config_path.
   strlcpy(tmp_append_path, g_extern.append_config_path, sizeof(tmp_append_path));
   const char *extra_path = strtok_r(tmp_append_path, ",", &save);
   while (extra_path)
   {
      RARCH_LOG("Appending config \"%s\"\n", extra_path);
      bool ret = config_append_file(conf, extra_path);
      if (!ret)
         RARCH_ERR("Failed to append config \"%s\"\n", extra_path);
      extra_path = strtok_r(NULL, ";", &save);
   }

   if (g_extern.verbose)
   {
      RARCH_LOG_OUTPUT("=== Config ===\n");
      config_file_dump_all(conf, stderr);
      RARCH_LOG_OUTPUT("=== Config end ===\n");
   }

   char tmp_str[PATH_MAX];

   CONFIG_GET_FLOAT(video.xscale, "video_xscale");
   CONFIG_GET_FLOAT(video.yscale, "video_yscale");
   CONFIG_GET_INT(video.fullscreen_x, "video_fullscreen_x");
   CONFIG_GET_INT(video.fullscreen_y, "video_fullscreen_y");

   if (!g_extern.force_fullscreen)
      CONFIG_GET_BOOL(video.fullscreen, "video_fullscreen");

   CONFIG_GET_BOOL(video.windowed_fullscreen, "video_windowed_fullscreen");
   CONFIG_GET_INT(video.monitor_index, "video_monitor_index");
   CONFIG_GET_BOOL(video.disable_composition, "video_disable_composition");
   CONFIG_GET_BOOL(video.vsync, "video_vsync");
   CONFIG_GET_BOOL(video.smooth, "video_smooth");
   CONFIG_GET_BOOL(video.force_aspect, "video_force_aspect");
   CONFIG_GET_BOOL(video.crop_overscan, "video_crop_overscan");
   CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");
   CONFIG_GET_BOOL(video.aspect_ratio_auto, "video_aspect_ratio_auto");
   CONFIG_GET_FLOAT(video.refresh_rate, "video_refresh_rate");

   CONFIG_GET_PATH(video.cg_shader_path, "video_cg_shader");
   CONFIG_GET_PATH(video.bsnes_shader_path, "video_bsnes_shader");
   CONFIG_GET_PATH(video.second_pass_shader, "video_second_pass_shader");
   CONFIG_GET_BOOL(video.render_to_texture, "video_render_to_texture");
   CONFIG_GET_FLOAT(video.fbo.scale_x, "video_fbo_scale_x");
   CONFIG_GET_FLOAT(video.fbo.scale_y, "video_fbo_scale_y");
   CONFIG_GET_BOOL(video.second_pass_smooth, "video_second_pass_smooth");
   CONFIG_GET_BOOL(video.allow_rotate, "video_allow_rotate");

#ifdef HAVE_FREETYPE
   CONFIG_GET_PATH(video.font_path, "video_font_path");
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

   CONFIG_GET_BOOL(video.post_filter_record, "video_post_filter_record");
   CONFIG_GET_BOOL(video.gpu_record, "video_gpu_record");
   CONFIG_GET_BOOL(video.gpu_screenshot, "video_gpu_screenshot");

#ifdef HAVE_DYLIB
   CONFIG_GET_PATH(video.filter_path, "video_filter");
#endif

   if (config_get_array(conf, "video_shader_type", tmp_str, sizeof(tmp_str)))
   {
      if (strcmp("cg", tmp_str) == 0)
         g_settings.video.shader_type = RARCH_SHADER_CG;
      else if (strcmp("bsnes", tmp_str) == 0)
         g_settings.video.shader_type = RARCH_SHADER_BSNES;
      else if (strcmp("auto", tmp_str) == 0)
         g_settings.video.shader_type = RARCH_SHADER_AUTO;
      else if (strcmp("none", tmp_str) == 0)
         g_settings.video.shader_type = RARCH_SHADER_NONE;
   }

   CONFIG_GET_PATH(video.shader_dir, "video_shader_dir");

   CONFIG_GET_FLOAT(input.axis_threshold, "input_axis_threshold");
   CONFIG_GET_BOOL(input.netplay_client_swap_input, "netplay_client_swap_input");

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      char buf[64];
      snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
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
   CONFIG_GET_FLOAT(audio.volume, "audio_volume");

   CONFIG_GET_STRING(video.driver, "video_driver");
   CONFIG_GET_STRING(audio.driver, "audio_driver");
   CONFIG_GET_PATH(audio.dsp_plugin, "audio_dsp_plugin");
   CONFIG_GET_STRING(input.driver, "input_driver");

   if (!*g_settings.libretro)
      CONFIG_GET_PATH(libretro, "libretro_path");

   CONFIG_GET_PATH(screenshot_directory, "screenshot_directory");
   if (*g_settings.screenshot_directory && !path_is_directory(g_settings.screenshot_directory))
   {
      RARCH_WARN("screenshot_directory is not an existing directory, ignoring ...\n");
      *g_settings.screenshot_directory = '\0';
   }

   CONFIG_GET_BOOL(rewind_enable, "rewind_enable");

   int buffer_size = 0;
   if (config_get_int(conf, "rewind_buffer_size", &buffer_size))
      g_settings.rewind_buffer_size = buffer_size * UINT64_C(1000000);

   CONFIG_GET_INT(rewind_granularity, "rewind_granularity");
   CONFIG_GET_FLOAT(slowmotion_ratio, "slowmotion_ratio");
   if (g_settings.slowmotion_ratio < 1.0f)
      g_settings.slowmotion_ratio = 1.0f;

   CONFIG_GET_BOOL(pause_nonactive, "pause_nonactive");
   CONFIG_GET_INT(autosave_interval, "autosave_interval");

   CONFIG_GET_PATH(cheat_database, "cheat_database_path");
   CONFIG_GET_PATH(cheat_settings_path, "cheat_settings_path");

   CONFIG_GET_BOOL(block_sram_overwrite, "block_sram_overwrite");
   CONFIG_GET_BOOL(savestate_auto_index, "savestate_auto_index");
   CONFIG_GET_BOOL(savestate_auto_save, "savestate_auto_save");

   CONFIG_GET_BOOL(network_cmd_enable, "network_cmd_enable");
   CONFIG_GET_INT(network_cmd_port, "network_cmd_port");
   CONFIG_GET_BOOL(stdin_cmd_enable, "stdin_cmd_enable");

   CONFIG_GET_INT(input.turbo_period, "input_turbo_period");
   CONFIG_GET_INT(input.turbo_duty_cycle, "input_duty_cycle");

   CONFIG_GET_PATH(input.overlay, "input_overlay");
#ifdef ANDROID
   CONFIG_GET_BOOL(input.autodetect_enable, "input_autodetect_enable");
#endif

   if (config_get_string(conf, "environment_variables",
            &g_extern.system.environment))
   {
      g_extern.system.environment_split = strdup(g_extern.system.environment);
      if (!g_extern.system.environment_split)
      {
         RARCH_ERR("Failed to allocate environment variables. Will ignore them.\n");
         free(g_extern.system.environment);
         g_extern.system.environment = NULL;
      }
   }

   if (!g_extern.has_set_save_path && config_get_path(conf, "savefile_directory", tmp_str, sizeof(tmp_str)))
   {
      if (path_is_directory(tmp_str))
      {
         strlcpy(g_extern.savefile_name_srm, tmp_str, sizeof(g_extern.savefile_name_srm));
         fill_pathname_dir(g_extern.savefile_name_srm, g_extern.basename, ".srm", sizeof(g_extern.savefile_name_srm));
      }
      else
         RARCH_WARN("savefile_directory is not a directory, ignoring ....\n");
   }

   if (!g_extern.has_set_state_path && config_get_path(conf, "savestate_directory", tmp_str, sizeof(tmp_str)))
   {
      if (path_is_directory(tmp_str))
      {
         strlcpy(g_extern.savestate_name, tmp_str, sizeof(g_extern.savestate_name));
         fill_pathname_dir(g_extern.savestate_name, g_extern.basename, ".state", sizeof(g_extern.savestate_name));
      }
      else
         RARCH_WARN("savestate_directory is not a directory, ignoring ...\n");
   }

   if (!config_get_path(conf, "system_directory", g_settings.system_directory, sizeof(g_settings.system_directory)))
   {
      RARCH_WARN("system_directory is not set in config. Assuming system directory is same folder as game: \"%s\".\n",
            g_settings.system_directory);
   }

   config_read_keybinds_conf(conf);

   config_file_free(conf);
   return true;
}

struct bind_map
{
   bool valid;
   const char *key;
   const char *btn;
   const char *axis;
   int retro_key;
};

#define DECLARE_BIND(x, bind) { true, "input_" #x, "input_" #x "_btn", "input_" #x "_axis", bind }
#define DECL_PLAYER(P) \
      DECLARE_BIND(player##P##_b,         RETRO_DEVICE_ID_JOYPAD_B), \
      DECLARE_BIND(player##P##_y,         RETRO_DEVICE_ID_JOYPAD_Y), \
      DECLARE_BIND(player##P##_select,    RETRO_DEVICE_ID_JOYPAD_SELECT), \
      DECLARE_BIND(player##P##_start,     RETRO_DEVICE_ID_JOYPAD_START), \
      DECLARE_BIND(player##P##_up,        RETRO_DEVICE_ID_JOYPAD_UP), \
      DECLARE_BIND(player##P##_down,      RETRO_DEVICE_ID_JOYPAD_DOWN), \
      DECLARE_BIND(player##P##_left,      RETRO_DEVICE_ID_JOYPAD_LEFT), \
      DECLARE_BIND(player##P##_right,     RETRO_DEVICE_ID_JOYPAD_RIGHT), \
      DECLARE_BIND(player##P##_a,         RETRO_DEVICE_ID_JOYPAD_A), \
      DECLARE_BIND(player##P##_x,         RETRO_DEVICE_ID_JOYPAD_X), \
      DECLARE_BIND(player##P##_l,         RETRO_DEVICE_ID_JOYPAD_L), \
      DECLARE_BIND(player##P##_r,         RETRO_DEVICE_ID_JOYPAD_R), \
      DECLARE_BIND(player##P##_l2,        RETRO_DEVICE_ID_JOYPAD_L2), \
      DECLARE_BIND(player##P##_r2,        RETRO_DEVICE_ID_JOYPAD_R2), \
      DECLARE_BIND(player##P##_l3,        RETRO_DEVICE_ID_JOYPAD_L3), \
      DECLARE_BIND(player##P##_r3,        RETRO_DEVICE_ID_JOYPAD_R3), \
      DECLARE_BIND(player##P##_turbo,     RARCH_TURBO_ENABLE), \
      DECLARE_BIND(player##P##_l_x_plus,  RARCH_ANALOG_LEFT_X_PLUS), \
      DECLARE_BIND(player##P##_l_x_minus, RARCH_ANALOG_LEFT_X_MINUS), \
      DECLARE_BIND(player##P##_l_y_plus,  RARCH_ANALOG_LEFT_Y_PLUS), \
      DECLARE_BIND(player##P##_l_y_minus, RARCH_ANALOG_LEFT_Y_MINUS), \
      DECLARE_BIND(player##P##_r_x_plus,  RARCH_ANALOG_RIGHT_X_PLUS), \
      DECLARE_BIND(player##P##_r_x_minus, RARCH_ANALOG_RIGHT_X_MINUS), \
      DECLARE_BIND(player##P##_r_y_plus,  RARCH_ANALOG_RIGHT_Y_PLUS), \
      DECLARE_BIND(player##P##_r_y_minus, RARCH_ANALOG_RIGHT_Y_MINUS)

// Big and nasty bind map... :)
static const struct bind_map bind_maps[MAX_PLAYERS][RARCH_BIND_LIST_END_NULL] = {
   {
      DECL_PLAYER(1),

      DECLARE_BIND(toggle_fast_forward,   RARCH_FAST_FORWARD_KEY),
      DECLARE_BIND(hold_fast_forward,     RARCH_FAST_FORWARD_HOLD_KEY),
      DECLARE_BIND(load_state,            RARCH_LOAD_STATE_KEY),
      DECLARE_BIND(save_state,            RARCH_SAVE_STATE_KEY),
      DECLARE_BIND(toggle_fullscreen,     RARCH_FULLSCREEN_TOGGLE_KEY),
      DECLARE_BIND(exit_emulator,         RARCH_QUIT_KEY),
      DECLARE_BIND(state_slot_increase,   RARCH_STATE_SLOT_PLUS),
      DECLARE_BIND(state_slot_decrease,   RARCH_STATE_SLOT_MINUS),
      DECLARE_BIND(rewind,                RARCH_REWIND),
      DECLARE_BIND(movie_record_toggle,   RARCH_MOVIE_RECORD_TOGGLE),
      DECLARE_BIND(pause_toggle,          RARCH_PAUSE_TOGGLE),
      DECLARE_BIND(frame_advance,         RARCH_FRAMEADVANCE),
      DECLARE_BIND(reset,                 RARCH_RESET),
      DECLARE_BIND(shader_next,           RARCH_SHADER_NEXT),
      DECLARE_BIND(shader_prev,           RARCH_SHADER_PREV),
      DECLARE_BIND(cheat_index_plus,      RARCH_CHEAT_INDEX_PLUS),
      DECLARE_BIND(cheat_index_minus,     RARCH_CHEAT_INDEX_MINUS),
      DECLARE_BIND(cheat_toggle,          RARCH_CHEAT_TOGGLE),
      DECLARE_BIND(screenshot,            RARCH_SCREENSHOT),
      DECLARE_BIND(dsp_config,            RARCH_DSP_CONFIG),
      DECLARE_BIND(audio_mute,            RARCH_MUTE),
      DECLARE_BIND(netplay_flip_players,  RARCH_NETPLAY_FLIP),
      DECLARE_BIND(slowmotion,            RARCH_SLOWMOTION),
      DECLARE_BIND(enable_hotkey,         RARCH_ENABLE_HOTKEY),
      DECLARE_BIND(volume_up,             RARCH_VOLUME_UP),
      DECLARE_BIND(volume_down,           RARCH_VOLUME_DOWN),
      DECLARE_BIND(overlay_next,          RARCH_OVERLAY_NEXT),
   },

   { DECL_PLAYER(2) },
   { DECL_PLAYER(3) },
   { DECL_PLAYER(4) },
   { DECL_PLAYER(5) },
   { DECL_PLAYER(6) },
   { DECL_PLAYER(7) },
   { DECL_PLAYER(8) },
};

struct key_map
{
   const char *str;
   int key;
};

static const struct key_map sk_map[] = {
   { "left", RETROK_LEFT },
   { "right", RETROK_RIGHT },
   { "up", RETROK_UP },
   { "down", RETROK_DOWN },
   { "enter", RETROK_RETURN },
   { "kp_enter", RETROK_KP_ENTER },
   { "tab", RETROK_TAB },
   { "insert", RETROK_INSERT },
   { "del", RETROK_DELETE },
   { "end", RETROK_END },
   { "home", RETROK_HOME },
   { "rshift", RETROK_RSHIFT },
   { "shift", RETROK_LSHIFT },
   { "ctrl", RETROK_LCTRL },
   { "alt", RETROK_LALT },
   { "space", RETROK_SPACE },
   { "escape", RETROK_ESCAPE },
   { "add", RETROK_KP_PLUS },
   { "subtract", RETROK_KP_MINUS },
   { "kp_plus", RETROK_KP_PLUS },
   { "kp_minus", RETROK_KP_MINUS },
   { "f1", RETROK_F1 },
   { "f2", RETROK_F2 },
   { "f3", RETROK_F3 },
   { "f4", RETROK_F4 },
   { "f5", RETROK_F5 },
   { "f6", RETROK_F6 },
   { "f7", RETROK_F7 },
   { "f8", RETROK_F8 },
   { "f9", RETROK_F9 },
   { "f10", RETROK_F10 },
   { "f11", RETROK_F11 },
   { "f12", RETROK_F12 },
   { "num0", RETROK_0 },
   { "num1", RETROK_1 },
   { "num2", RETROK_2 },
   { "num3", RETROK_3 },
   { "num4", RETROK_4 },
   { "num5", RETROK_5 },
   { "num6", RETROK_6 },
   { "num7", RETROK_7 },
   { "num8", RETROK_8 },
   { "num9", RETROK_9 },
   { "pageup", RETROK_PAGEUP },
   { "pagedown", RETROK_PAGEDOWN },
   { "keypad0", RETROK_KP0 },
   { "keypad1", RETROK_KP1 },
   { "keypad2", RETROK_KP2 },
   { "keypad3", RETROK_KP3 },
   { "keypad4", RETROK_KP4 },
   { "keypad5", RETROK_KP5 },
   { "keypad6", RETROK_KP6 },
   { "keypad7", RETROK_KP7 },
   { "keypad8", RETROK_KP8 },
   { "keypad9", RETROK_KP9 },
   { "period", RETROK_PERIOD },
   { "capslock", RETROK_CAPSLOCK },
   { "numlock", RETROK_NUMLOCK },
   { "backspace", RETROK_BACKSPACE },
   { "multiply", RETROK_KP_MULTIPLY },
   { "divide", RETROK_KP_DIVIDE },
   { "print_screen", RETROK_PRINT },
   { "scroll_lock", RETROK_SCROLLOCK },
   { "tilde", RETROK_BACKQUOTE },
   { "backquote", RETROK_BACKQUOTE },
   { "pause", RETROK_PAUSE },
   { "nul", RETROK_UNKNOWN },
};

static struct retro_keybind *find_retro_bind(unsigned port, int id)
{
   struct retro_keybind *binds = g_settings.input.binds[port];
   return binds[id].valid ? &binds[id] : NULL;
}

static int find_sk_bind(const char *str)
{
   for (size_t i = 0; i < ARRAY_SIZE(sk_map); i++)
   {
      if (strcasecmp(sk_map[i].str, str) == 0)
         return sk_map[i].key;
   }

   return -1;
}

static int find_sk_key(const char *str)
{
   if (strlen(str) == 1 && isalpha(*str))
      return (int)RETROK_a + (tolower(*str) - (int)'a');
   else
      return find_sk_bind(str);
}

static void read_keybinds_keyboard(config_file_t *conf, unsigned player, unsigned index, struct retro_keybind *bind)
{
   char tmp[64];

   if (bind_maps[player][index].key &&
         config_get_array(conf, bind_maps[player][index].key, tmp, sizeof(tmp)))
   {
      int key = find_sk_key(tmp);

      if (key >= 0)
         bind->key = (enum retro_key)key;
   }
}

static void parse_hat(struct retro_keybind *bind, const char *str)
{
   if (!isdigit(*str))
      return;

   char *dir = NULL;
   uint16_t hat = strtoul(str, &dir, 0);
   uint16_t hat_dir = 0;

   if (!dir)
   {
      RARCH_WARN("Found invalid hat in config!\n");
      return;
   }

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

static void read_keybinds_button(config_file_t *conf, unsigned player, unsigned index, struct retro_keybind *bind)
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
            bind->joykey = strtoull(tmp, NULL, 0);
      }
   }
}

static void read_keybinds_axis(config_file_t *conf, unsigned player, unsigned index, struct retro_keybind *bind)
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
      struct retro_keybind *bind = find_retro_bind(player, bind_maps[player][i].retro_key);
      rarch_assert(bind);

      read_keybinds_keyboard(conf, player, i, bind);
      read_keybinds_button(conf, player, i, bind);
      read_keybinds_axis(conf, player, i, bind);
   }
}

static void config_read_keybinds_conf(config_file_t *conf)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      read_keybinds_player(conf, i);
}

bool config_read_keybinds(const char *path)
{
   config_file_t *conf = config_file_new(path);
   if (!conf)
      return false;
   config_read_keybinds_conf(conf);
   config_file_free(conf);
   return true;
}

static void save_keybind_key(config_file_t *conf,
      const struct bind_map *map, const struct retro_keybind *bind)
{
   char ascii[2] = {0};
   const char *btn = ascii;

   if (bind->key >= RETROK_a && bind->key <= RETROK_z)
      ascii[0] = 'a' + (bind->key - RETROK_a);
   else
   {
      for (unsigned i = 0; i < ARRAY_SIZE(sk_map); i++)
      {
         if (sk_map[i].key == bind->key)
         {
            btn = sk_map[i].str;
            break;
         }
      }
   }

   config_set_string(conf, map->key, btn);
}

#ifndef RARCH_CONSOLE
static void save_keybind_hat(config_file_t *conf,
      const struct bind_map *map, const struct retro_keybind *bind)
{
   unsigned hat = GET_HAT(bind->joykey);
   const char *dir = NULL;

   switch (GET_HAT_DIR(hat))
   {
      case HAT_UP_MASK:
         dir = "up";
         break;

      case HAT_DOWN_MASK:
         dir = "down";
         break;

      case HAT_LEFT_MASK:
         dir = "left";
         break;

      case HAT_RIGHT_MASK:
         dir = "right";
         break;

      default:
         rarch_assert(0);
   }

   char config[16];
   snprintf(config, sizeof(config), "h%u%s", hat, dir);
   config_set_string(conf, map->btn, config);
}
#endif

static void save_keybind_joykey(config_file_t *conf,
      const struct bind_map *map, const struct retro_keybind *bind)
{
   if (bind->joykey == NO_BTN)
      config_set_string(conf, map->btn, "nul");
#ifndef RARCH_CONSOLE // Consoles don't understand hats.
   else if (GET_HAT_DIR(bind->joykey))
      save_keybind_hat(conf, map, bind);
#endif
   else
      config_set_uint64(conf, map->btn, bind->joykey);
}

static void save_keybind_axis(config_file_t *conf,
      const struct bind_map *map, const struct retro_keybind *bind)
{
   unsigned axis = 0;
   char dir = '\0';

   if (bind->joyaxis == AXIS_NONE)
      config_set_string(conf, map->axis, "nul");
   else if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }

   char config[16];
   snprintf(config, sizeof(config), "%c%u", dir, axis);
   config_set_string(conf, map->axis, config);
}

static void save_keybind(config_file_t *conf,
      const struct bind_map *map, const struct retro_keybind *bind)
{
   if (!map->valid)
      return;

   save_keybind_key(conf, map, bind);
   save_keybind_joykey(conf, map, bind);
   save_keybind_axis(conf, map, bind);
}

static void save_keybinds_player(config_file_t *conf, unsigned i)
{
   for (unsigned j = 0; j < RARCH_BIND_LIST_END; j++)
      save_keybind(conf, &bind_maps[i][j], &g_settings.input.binds[i][j]);
}

bool config_save_keybinds(const char *path)
{
   config_file_t *conf = config_file_new(path);
   if (!conf)
      conf = config_file_new(NULL);
   if (!conf)
      return NULL;

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      save_keybinds_player(conf, i);

   config_file_write(conf, path);
   config_file_free(conf);
   return true;
}

