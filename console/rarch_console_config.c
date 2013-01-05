/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdio.h>

#include "../conf/config_file.h"
#include "../conf/config_file_macros.h"

#include "rarch_console_config.h"

void rarch_config_load(const char *path, bool upgrade_core_succeeded)
{
      char libretro_path_tmp[PATH_MAX];

      //if a core has been upgraded, settings need to saved at the end
      if(upgrade_core_succeeded)
         snprintf(libretro_path_tmp, sizeof(libretro_path_tmp), g_settings.libretro);
      

      config_file_t * conf = config_file_new(path);

      if(!conf)
      {
#ifdef RARCH_CONSOLE
         FILE * f;
         RARCH_ERR("Config file \"%s\" doesn't exist. Creating...\n", path);
         f = fopen(path, "w");
         fclose(f);
#endif
         return;
      }

      // g_settings

      CONFIG_GET_STRING(libretro, "libretro_path");
      CONFIG_GET_STRING(system_directory, "system_directory");
#ifdef HAVE_XML
      CONFIG_GET_STRING(cheat_database, "cheat_database");
#endif
      CONFIG_GET_BOOL(rewind_enable, "rewind_enable");
      CONFIG_GET_STRING(video.cg_shader_path, "video_cg_shader");
#ifdef HAVE_FBO
      CONFIG_GET_STRING(video.second_pass_shader, "video_second_pass_shader");
      CONFIG_GET_FLOAT(video.fbo.scale_x, "video_fbo_scale_x");
      CONFIG_GET_FLOAT(video.fbo.scale_y, "video_fbo_scale_y");
      CONFIG_GET_BOOL(video.render_to_texture, "video_render_to_texture");
      CONFIG_GET_BOOL(video.second_pass_smooth, "video_second_pass_smooth");
#endif
      CONFIG_GET_BOOL(video.smooth, "video_smooth");
      CONFIG_GET_BOOL(video.vsync, "video_vsync");
      CONFIG_GET_INT(video.aspect_ratio_idx, "aspect_ratio_index");
      CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");
      CONFIG_GET_STRING(audio.device, "audio_device");
      CONFIG_GET_BOOL(audio.rate_control, "audio_rate_control");
      CONFIG_GET_FLOAT(audio.rate_control_delta, "audio_rate_control_delta");

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
         snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
         CONFIG_GET_INT(input.dpad_emulation[i], cfg);
         snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
         CONFIG_GET_INT(input.device[i], cfg);
      }

      // g_extern
      CONFIG_GET_STRING_EXTERN(console.main_wrap.paths.default_rom_startup_dir, "default_rom_startup_dir");
      CONFIG_GET_BOOL_EXTERN(console.screen.gamma_correction, "gamma_correction");
      CONFIG_GET_BOOL_EXTERN(console.rmenu.state.msg_info.enable, "info_msg_enable");
      CONFIG_GET_BOOL_EXTERN(console.screen.state.screenshots.enable, "screenshots_enable");
      CONFIG_GET_BOOL_EXTERN(console.screen.state.throttle.enable, "throttle_enable");
      CONFIG_GET_BOOL_EXTERN(console.screen.state.triple_buffering.enable, "triple_buffering_enable");
      CONFIG_GET_BOOL_EXTERN(console.screen.state.overscan.enable, "overscan_enable");
      CONFIG_GET_BOOL_EXTERN(console.sound.custom_bgm.enable, "custom_bgm_enable");
      CONFIG_GET_BOOL_EXTERN(console.main_wrap.state.default_sram_dir.enable, "sram_dir_enable");
      CONFIG_GET_BOOL_EXTERN(console.main_wrap.state.default_savestate_dir.enable, "savestate_dir_enable");
      CONFIG_GET_FLOAT_EXTERN(console.screen.overscan_amount, "overscan_amount");
#ifdef _XBOX1
      CONFIG_GET_INT_EXTERN(console.screen.state.flicker_filter.enable, "flicker_filter");
      CONFIG_GET_INT_EXTERN(console.sound.volume_level, "sound_volume_level");
#endif
#ifdef HAVE_ZLIB
      CONFIG_GET_INT_EXTERN(file_state.zip_extract_mode, "zip_extract_mode");
#endif
      CONFIG_GET_INT_EXTERN(console.screen.resolutions.current.id, "current_resolution_id");
      CONFIG_GET_INT_EXTERN(state_slot, "state_slot");
      CONFIG_GET_INT_EXTERN(audio_data.mute, "audio_mute");
      CONFIG_GET_BOOL_EXTERN(console.screen.state.soft_filter.enable, "soft_display_filter_enable");
      CONFIG_GET_INT_EXTERN(console.screen.orientation, "screen_orientation");
      CONFIG_GET_INT_EXTERN(console.sound.mode, "sound_mode");
      CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.x, "custom_viewport_x");
      CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.y, "custom_viewport_y");
      CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.width, "custom_viewport_width");
      CONFIG_GET_INT_EXTERN(console.screen.viewports.custom_vp.height, "custom_viewport_height");
      CONFIG_GET_FLOAT_EXTERN(console.rmenu.font_size, "menu_font_size");

      if(upgrade_core_succeeded)
      {
         //save config file with new libretro path
         snprintf(g_settings.libretro, sizeof(g_settings.libretro), libretro_path_tmp);
         rarch_config_save(path);
      }
}

void rarch_config_save(const char *path)
{
      config_file_t * conf = config_file_new(path);

      if(!conf)
      {
         RARCH_ERR("Failed to write config file to \"%s\". Check permissions.\n", path);
         return;
      }

      // g_settings
      config_set_string(conf, "libretro_path", g_settings.libretro);
#ifdef HAVE_XML
      config_set_string(conf, "cheat_database_path", g_settings.cheat_database);
#endif
      config_set_bool(conf, "rewind_enable", g_settings.rewind_enable);
      config_set_string(conf, "video_cg_shader", g_settings.video.cg_shader_path);
      config_set_float(conf, "video_aspect_ratio", g_settings.video.aspect_ratio);
#ifdef HAVE_FBO
      config_set_float(conf, "video_fbo_scale_x", g_settings.video.fbo.scale_x);
      config_set_float(conf, "video_fbo_scale_y", g_settings.video.fbo.scale_y);
      config_set_string(conf, "video_second_pass_shader", g_settings.video.second_pass_shader);
      config_set_bool(conf, "video_render_to_texture", g_settings.video.render_to_texture);
      config_set_bool(conf, "video_second_pass_smooth", g_settings.video.second_pass_smooth);
#endif
      config_set_bool(conf, "video_smooth", g_settings.video.smooth);
      config_set_bool(conf, "video_vsync", g_settings.video.vsync);
      config_set_int(conf, "aspect_ratio_index", g_settings.video.aspect_ratio_idx);
      config_set_string(conf, "audio_device", g_settings.audio.device);
      config_set_bool(conf, "audio_rate_control", g_settings.audio.rate_control);
      config_set_float(conf, "audio_rate_control_delta", g_settings.audio.rate_control_delta);
      config_set_string(conf, "system_directory", g_settings.system_directory);

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
         snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
         config_set_int(conf, cfg, g_settings.input.dpad_emulation[i]);
         snprintf(cfg, sizeof(cfg), "input_device_p%u", i + 1);
         config_set_int(conf, cfg, g_settings.input.device[i]);
      }

      config_set_bool(conf, "overscan_enable", g_extern.console.screen.state.overscan.enable);
      config_set_bool(conf, "screenshots_enable", g_extern.console.screen.state.screenshots.enable);
      config_set_bool(conf, "gamma_correction", g_extern.console.screen.gamma_correction);
#ifdef _XBOX1
      config_set_int(conf, "flicker_filter", g_extern.console.screen.state.flicker_filter.value);
      config_set_int(conf, "sound_volume_level", g_extern.console.sound.volume_level);
#endif
      config_set_bool(conf, "throttle_enable", g_extern.console.screen.state.throttle.enable);
      config_set_bool(conf, "triple_buffering_enable", g_extern.console.screen.state.triple_buffering.enable);
      config_set_bool(conf, "info_msg_enable", g_extern.console.rmenu.state.msg_info.enable);
      config_set_int(conf, "current_resolution_id", g_extern.console.screen.resolutions.current.id);
      config_set_int(conf, "custom_viewport_width", g_extern.console.screen.viewports.custom_vp.width);
      config_set_int(conf, "custom_viewport_height", g_extern.console.screen.viewports.custom_vp.height);
      config_set_int(conf, "custom_viewport_x", g_extern.console.screen.viewports.custom_vp.x);
      config_set_int(conf, "custom_viewport_y", g_extern.console.screen.viewports.custom_vp.y);
      config_set_string(conf, "default_rom_startup_dir", g_extern.console.main_wrap.paths.default_rom_startup_dir);
      config_set_float(conf, "menu_font_size", g_extern.console.rmenu.font_size);
      config_set_float(conf, "overscan_amount", g_extern.console.screen.overscan_amount);
#ifdef HAVE_ZLIB
      config_set_int(conf, "zip_extract_mode", g_extern.file_state.zip_extract_mode);
#endif

      // g_extern
      config_set_int(conf, "sound_mode", g_extern.console.sound.mode);
      config_set_int(conf, "state_slot", g_extern.state_slot);
      config_set_int(conf, "audio_mute", g_extern.audio_data.mute);
      config_set_bool(conf, "soft_display_filter_enable", g_extern.console.screen.state.soft_filter.enable);
      config_set_int(conf, "screen_orientation", g_extern.console.screen.orientation);
      config_set_bool(conf, "custom_bgm_enable", g_extern.console.sound.custom_bgm.enable);

      config_set_bool(conf, "sram_dir_enable", g_extern.console.main_wrap.state.default_sram_dir.enable);
      config_set_bool(conf, "savestate_dir_enable", g_extern.console.main_wrap.state.default_savestate_dir.enable);

      if (!config_file_write(conf, path))
         RARCH_ERR("Failed to write config file to \"%s\". Check permissions.\n", path);

      free(conf);
}
