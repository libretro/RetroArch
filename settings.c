#include "general.h"
#include "conf/config_file.h"
#include "config.h.def"
#include <assert.h>
#include <string.h>

struct settings g_settings;

static void set_defaults(void)
{
   g_settings.video.xscale = xscale;
   g_settings.video.yscale = yscale;
   g_settings.video.fullscreen = fullscreen;
   g_settings.video.fullscreen_x = fullscreen_x;
   g_settings.video.fullscreen_y = fullscreen_y;
   g_settings.video.vsync = vsync;
   g_settings.video.smooth = video_smooth;
   g_settings.video.force_aspect = force_aspect;
#if HAVE_CG
   strncpy(g_settings.video.cg_shader_path, cg_shader_path, sizeof(g_settings.video.cg_shader_path) - 1);
#endif
   strncpy(g_settings.video.video_filter, "foo", sizeof(g_settings.video.video_filter) - 1);

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
}

void parse_config(void)
{
   memset(&g_settings, 0, sizeof(struct settings));
   config_file_t *conf = NULL;

   const char *xdg = getenv("XDG_CONFIG_HOME");
   if (xdg)
   {
      char conf_path[strlen(xdg) + strlen("/ssnes ")];
      strcpy(conf_path, xdg);
      strcat(conf_path, "/ssnes");
      conf = config_file_new(conf_path);
   }
   else
   {
      const char *home = getenv("HOME");

      if (home)
      {
         char conf_path[strlen(home) + strlen("/.ssnesrc ")];
         strcpy(conf_path, xdg);
         strcat(conf_path, "/.ssnesrc");
         conf = config_file_new(conf_path);
      }
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

   if (config_get_bool(conf, "video_vsync", &tmp_bool))
      g_settings.video.vsync = tmp_bool;

   if (config_get_bool(conf, "video_smooth", &tmp_bool))
      g_settings.video.smooth = tmp_bool;

   if (config_get_bool(conf, "video_force_aspect", &tmp_bool))
      g_settings.video.force_aspect = tmp_bool;

   if (config_get_string(conf, "video_cg_shader_path", &tmp_str))
   {
      strncpy(g_settings.video.cg_shader_path, tmp_str, sizeof(g_settings.video.cg_shader_path) - 1);
      free(tmp_str);
   }

   if (config_get_string(conf, "video_video_filter", &tmp_str))
   {
      strncpy(g_settings.video.video_filter, tmp_str, sizeof(g_settings.video.video_filter) - 1);
      free(tmp_str);
   }

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
      int quals[] = {SRC_ZERO_ORDER_HOLD, SRC_LINEAR, SRC_SINC_FASTEST, 
         SRC_SINC_MEDIUM_QUALITY, SRC_SINC_BEST_QUALITY};

      if (tmp_int > 0 && tmp_int < 6)
         g_settings.audio.src_quality = quals[tmp_int];
   }

   // TODO: Keybinds.

   config_file_free(conf);
}
