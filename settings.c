/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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
#include "config.def.h"
#include <assert.h>
#include <string.h>
#include "hqflt/filters.h"
#include "config.h"
#include <ctype.h>

struct settings g_settings;

static void read_keybinds(config_file_t *conf);

static void set_defaults(void)
{
   const char *def_video = NULL;
   const char *def_audio = NULL;

   switch (VIDEO_DEFAULT_DRIVER)
   {
      case VIDEO_GL:
         def_video = "glfw";
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
      default:
         break;
   }

   // No input atm ... It is in the GLFW driver.

   if (def_video)
      strncpy(g_settings.video.driver, def_video, sizeof(g_settings.video.driver) - 1);
   if (def_audio)
      strncpy(g_settings.audio.driver, def_audio, sizeof(g_settings.audio.driver) - 1);

   g_settings.video.xscale = xscale;
   g_settings.video.yscale = yscale;
   g_settings.video.fullscreen = fullscreen;
   g_settings.video.fullscreen_x = fullscreen_x;
   g_settings.video.fullscreen_y = fullscreen_y;
   g_settings.video.vsync = vsync;
   g_settings.video.smooth = video_smooth;
   g_settings.video.force_aspect = force_aspect;

   g_settings.audio.enable = audio_enable;
   g_settings.audio.out_rate = out_rate;
   g_settings.audio.in_rate = in_rate;
   if (audio_device)
      strncpy(g_settings.audio.device, audio_device, sizeof(g_settings.audio.device));
   g_settings.audio.latency = out_latency;
   g_settings.audio.sync = audio_sync;
   g_settings.audio.src_quality = SAMPLERATE_QUALITY;

   assert(sizeof(g_settings.input.binds[0]) >= sizeof(snes_keybinds_1));
   assert(sizeof(g_settings.input.binds[1]) >= sizeof(snes_keybinds_2));
   memcpy(g_settings.input.binds[0], snes_keybinds_1, sizeof(snes_keybinds_1));
   memcpy(g_settings.input.binds[1], snes_keybinds_2, sizeof(snes_keybinds_2));

   g_settings.input.save_state_key = SAVE_STATE_KEY;
   g_settings.input.load_state_key = LOAD_STATE_KEY;
   g_settings.input.toggle_fullscreen_key = TOGGLE_FULLSCREEN;
   g_settings.input.axis_threshold = AXIS_THRESHOLD;
}

void parse_config(void)
{
   memset(&g_settings, 0, sizeof(struct settings));
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
      const char *xdg = getenv("XDG_CONFIG_HOME");
      const char *home = getenv("HOME");
      if (xdg)
      {
         char conf_path[strlen(xdg) + strlen("/ssnes ")];
         strcpy(conf_path, xdg);
         strcat(conf_path, "/ssnes");
         conf = config_file_new(conf_path);
      }
      else if (home)
      {
         char conf_path[strlen(home) + strlen("/.ssnesrc ")];
         strcpy(conf_path, xdg);
         strcat(conf_path, "/.ssnesrc");
         conf = config_file_new(conf_path);
      }
      // Try this as a last chance...
      if (!conf)
         conf = config_file_new("/etc/ssnes.cfg");
   }

   set_defaults();
   if (conf == NULL)
      return;

   int tmp_int;
   double tmp_double;
   bool tmp_bool;
   char *tmp_str;

   // Video settings.
   if (config_get_double(conf, "video_xscale", &tmp_double))
      g_settings.video.xscale = tmp_double;

   if (config_get_double(conf, "video_yscale", &tmp_double))
      g_settings.video.yscale = tmp_double;

   if (config_get_int(conf, "video_fullscreen_x", &tmp_int))
      g_settings.video.fullscreen_x = tmp_int;

   if (config_get_int(conf, "video_fullscreen_y", &tmp_int))
      g_settings.video.fullscreen_y = tmp_int;

   if (config_get_bool(conf, "video_fullscreen", &tmp_bool))
      g_settings.video.fullscreen = tmp_bool;

   if (config_get_bool(conf, "video_vsync", &tmp_bool))
      g_settings.video.vsync = tmp_bool;

   if (config_get_bool(conf, "video_smooth", &tmp_bool))
      g_settings.video.smooth = tmp_bool;

   if (config_get_bool(conf, "video_force_aspect", &tmp_bool))
      g_settings.video.force_aspect = tmp_bool;

   if (config_get_string(conf, "video_cg_shader", &tmp_str))
   {
      strncpy(g_settings.video.cg_shader_path, tmp_str, sizeof(g_settings.video.cg_shader_path) - 1);
      free(tmp_str);
   }

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

   // Input Settings.
   if (config_get_double(conf, "input_axis_threshold", &tmp_double))
      g_settings.input.axis_threshold = tmp_double;

   // Audio settings.
   if (config_get_bool(conf, "audio_enable", &tmp_bool))
      g_settings.audio.enable = tmp_bool;

   if (config_get_int(conf, "audio_out_rate", &tmp_int))
      g_settings.audio.out_rate = tmp_int;

   if (config_get_int(conf, "audio_in_rate", &tmp_int))
      g_settings.audio.in_rate = tmp_int;

   if (config_get_string(conf, "audio_device", &tmp_str))
   {
      strncpy(g_settings.audio.device, tmp_str, sizeof(g_settings.audio.device) - 1);
      free(tmp_str);
   }

   if (config_get_int(conf, "audio_latency", &tmp_int))
      g_settings.audio.latency = tmp_int;

   if (config_get_bool(conf, "audio_sync", &tmp_bool))
      g_settings.audio.sync = tmp_bool;

   if (config_get_int(conf, "audio_src_quality", &tmp_int))
   {
      int quals[] = { SRC_ZERO_ORDER_HOLD, SRC_LINEAR, SRC_SINC_FASTEST, 
         SRC_SINC_MEDIUM_QUALITY, SRC_SINC_BEST_QUALITY };

      if (tmp_int > 0 && tmp_int < 6)
         g_settings.audio.src_quality = quals[tmp_int];
   }

   if (config_get_string(conf, "video_driver", &tmp_str))
   {
      strncpy(g_settings.video.driver, tmp_str, sizeof(g_settings.video.driver) - 1);
      free(tmp_str);
   }
   if (config_get_string(conf, "audio_driver", &tmp_str))
   {
      strncpy(g_settings.audio.driver, tmp_str, sizeof(g_settings.audio.driver) - 1);
      free(tmp_str);
   }

   read_keybinds(conf);

   // TODO: Keybinds.

   config_file_free(conf);
}

struct bind_map
{
   const char *key;
   const char *btn;
   const char *axis;
   int snes_key;
};

static const struct bind_map bind_maps[2][13] = {
   {
      { "input_player1_a",       "input_player1_a_btn",        NULL, SNES_DEVICE_ID_JOYPAD_A }, 
      { "input_player1_b",       "input_player1_b_btn",        NULL, SNES_DEVICE_ID_JOYPAD_B }, 
      { "input_player1_y",       "input_player1_y_btn",        NULL, SNES_DEVICE_ID_JOYPAD_Y }, 
      { "input_player1_x",       "input_player1_x_btn",        NULL, SNES_DEVICE_ID_JOYPAD_X }, 
      { "input_player1_start",   "input_player1_start_btn",    NULL, SNES_DEVICE_ID_JOYPAD_START }, 
      { "input_player1_select",  "input_player1_select_btn",   NULL, SNES_DEVICE_ID_JOYPAD_SELECT }, 
      { "input_player1_l",       "input_player1_l_btn",        NULL, SNES_DEVICE_ID_JOYPAD_L }, 
      { "input_player1_r",       "input_player1_r_btn",        NULL, SNES_DEVICE_ID_JOYPAD_R }, 
      { "input_player1_left",    "input_player1_left_btn",     "input_player1_left_axis", SNES_DEVICE_ID_JOYPAD_LEFT }, 
      { "input_player1_right",   "input_player1_right_btn",    "input_player1_right_axis", SNES_DEVICE_ID_JOYPAD_RIGHT }, 
      { "input_player1_up",      "input_player1_up_btn",       "input_player1_up_axis", SNES_DEVICE_ID_JOYPAD_UP }, 
      { "input_player1_down",    "input_player1_down_btn",     "input_player1_down_axis", SNES_DEVICE_ID_JOYPAD_DOWN }, 
      { "input_toggle_fast_forward", "input_toggle_fast_forward_btn", NULL, SNES_FAST_FORWARD_KEY }
   }, 
   {
      { "input_player2_a",       "input_player2_a_btn",        NULL, SNES_DEVICE_ID_JOYPAD_A }, 
      { "input_player2_b",       "input_player2_b_btn",        NULL, SNES_DEVICE_ID_JOYPAD_B }, 
      { "input_player2_y",       "input_player2_y_btn",        NULL, SNES_DEVICE_ID_JOYPAD_Y }, 
      { "input_player2_x",       "input_player2_x_btn",        NULL, SNES_DEVICE_ID_JOYPAD_X }, 
      { "input_player2_start",   "input_player2_start_btn",    NULL, SNES_DEVICE_ID_JOYPAD_START }, 
      { "input_player2_select",  "input_player2_select_btn",   NULL, SNES_DEVICE_ID_JOYPAD_SELECT }, 
      { "input_player2_l",       "input_player2_l_btn",        NULL, SNES_DEVICE_ID_JOYPAD_L }, 
      { "input_player2_r",       "input_player2_r_btn",        NULL, SNES_DEVICE_ID_JOYPAD_R }, 
      { "input_player2_left",    "input_player2_left_btn",     "input_player2_left_axis", SNES_DEVICE_ID_JOYPAD_LEFT }, 
      { "input_player2_right",   "input_player2_right_btn",    "input_player2_right_axis", SNES_DEVICE_ID_JOYPAD_RIGHT }, 
      { "input_player2_up",      "input_player2_up_btn",       "input_player2_up_axis", SNES_DEVICE_ID_JOYPAD_UP }, 
      { "input_player2_down",    "input_player2_down_btn",     "input_player2_down_axis", SNES_DEVICE_ID_JOYPAD_DOWN }, 
      { "input_toggle_fast_forward", "input_toggle_fast_forward_btn", NULL, SNES_FAST_FORWARD_KEY }
   }
};

struct glfw_map
{
   const char *str;
   int key;
};

// Edit: Not portable to different input systems atm. Might move this map into the driver itself or something.
static const struct glfw_map glfw_map[] = {
   { "left", GLFW_KEY_LEFT },
   { "right", GLFW_KEY_RIGHT },
   { "up", GLFW_KEY_UP },
   { "down", GLFW_KEY_DOWN },
   { "enter", GLFW_KEY_ENTER },
   { "rshift", GLFW_KEY_RSHIFT },
   { "space", GLFW_KEY_SPACE }
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

static int find_glfw_bind(const char *str)
{
   for (int i = 0; i < sizeof(glfw_map)/sizeof(struct glfw_map); i++)
   {
      if (strcasecmp(glfw_map[i].str, str) == 0)
         return glfw_map[i].key;
   }
   return -1;
}

static int find_glfw_key(const char *str)
{
   // If the bind is a normal key-press ...
   if (strlen(str) == 1 && isalpha(*str))
      return toupper(*str);
   else // Check if we have a special mapping for it.
      return find_glfw_bind(str);
}

static void read_keybinds(config_file_t *conf)
{
   char *tmp_key = NULL;
   int tmp_btn;
   char *tmp_axis = NULL;

   for (int j = 0; j < 1; j++)
   {
      for (int i = 0; i < sizeof(bind_maps[j])/sizeof(struct bind_map); i++)
      {
         struct snes_keybind *bind = find_snes_bind(j, bind_maps[j][i].snes_key);
         if (!bind)
            continue;

         if (bind_maps[j][i].key && config_get_string(conf, bind_maps[j][i].key, &tmp_key))
         {
            int key = find_glfw_key(tmp_key);

            if (key >= 0)
               bind->key = key;

            free(tmp_key);
            tmp_key = NULL;
         }

         if (bind_maps[j][i].btn && config_get_int(conf, bind_maps[j][i].btn, &tmp_btn))
         {
            if (tmp_btn >= 0)
               bind->joykey = tmp_btn;
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

   char *tmp_str;
   if (config_get_string(conf, "input_toggle_fullscreen", &tmp_str))
   {
      int key = find_glfw_key(tmp_str);
      if (key >= 0)
         g_settings.input.toggle_fullscreen_key = key;
      free(tmp_str);
   }
   if (config_get_string(conf, "input_save_state", &tmp_str))
   {
      int key = find_glfw_key(tmp_str);
      if (key >= 0)
         g_settings.input.save_state_key = key;
      free(tmp_str);
   }
   if (config_get_string(conf, "input_load_state", &tmp_str))
   {
      int key = find_glfw_key(tmp_str);
      if (key >= 0)
         g_settings.input.load_state_key = key;
      free(tmp_str);
   }
}




