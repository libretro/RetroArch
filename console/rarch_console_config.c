/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include "rarch_console_libretro_mgmt.h"

void rarch_config_load(const char * conf_name, const char * libretro_dir_path, const char * exe_ext, bool find_libretro_path)
{
      config_file_t * conf = config_file_new(conf_name);

      if(!conf)
         return;

#ifdef HAVE_LIBRETRO_MANAGEMENT
      if(find_libretro_path)
      {
         CONFIG_GET_STRING(libretro, "libretro_path");

         if(strcmp(g_settings.libretro, "") == 0)
         {
            char first_file[PATH_MAX];
            rarch_manage_libretro_set_first_file(first_file, sizeof(first_file), libretro_dir_path, exe_ext);
            if(first_file != NULL)
               strlcpy(g_settings.libretro, first_file, sizeof(g_settings.libretro));
         }
      }
#endif

      // g_settings

      CONFIG_GET_STRING(system_directory, "system_directory");
#ifdef HAVE_XML
      CONFIG_GET_STRING(cheat_database, "cheat_database");
#endif
      CONFIG_GET_BOOL(rewind_enable, "rewind_enable");
      CONFIG_GET_STRING(video.cg_shader_path, "video_cg_shader");
#ifdef HAVE_FBO
      CONFIG_GET_STRING(video.second_pass_shader, "video_second_pass_shader");
      CONFIG_GET_FLOAT(video.fbo_scale_x, "video_fbo_scale_x");
      CONFIG_GET_FLOAT(video.fbo_scale_y, "video_fbo_scale_y");
      CONFIG_GET_BOOL(video.render_to_texture, "video_render_to_texture");
      CONFIG_GET_BOOL(video.second_pass_smooth, "video_second_pass_smooth");
#endif
#ifdef _XBOX360
      CONFIG_GET_BOOL_CONSOLE(gamma_correction_enable, "gamma_correction_enable");
      CONFIG_GET_INT_CONSOLE(color_format, "color_format");
#endif
      CONFIG_GET_BOOL(video.smooth, "video_smooth");
      CONFIG_GET_BOOL(video.vsync, "video_vsync");
      CONFIG_GET_FLOAT(video.aspect_ratio, "video_aspect_ratio");
      CONFIG_GET_STRING(audio.device, "audio_device");
      CONFIG_GET_BOOL(audio.rate_control, "audio_rate_control");
      CONFIG_GET_FLOAT(audio.rate_control_delta, "audio_rate_control_delta");

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
         snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
         CONFIG_GET_INT(input.dpad_emulation[i], cfg);
      }

      // g_console

#ifdef HAVE_FBO
      CONFIG_GET_BOOL_CONSOLE(fbo_enabled, "fbo_enabled");
#endif
#ifdef __CELLOS_LV2__
      CONFIG_GET_BOOL_CONSOLE(custom_bgm_enable, "custom_bgm_enable");
#endif
      CONFIG_GET_BOOL_CONSOLE(overscan_enable, "overscan_enable");
      CONFIG_GET_BOOL_CONSOLE(screenshots_enable, "screenshots_enable");
      CONFIG_GET_BOOL_CONSOLE(throttle_enable, "throttle_enable");
      CONFIG_GET_BOOL_CONSOLE(triple_buffering_enable, "triple_buffering_enable");
      CONFIG_GET_BOOL_CONSOLE(info_msg_enable, "info_msg_enable");
      CONFIG_GET_INT_CONSOLE(aspect_ratio_index, "aspect_ratio_index");
      CONFIG_GET_INT_CONSOLE(current_resolution_id, "current_resolution_id");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.x, "custom_viewport_x");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.y, "custom_viewport_y");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.width, "custom_viewport_width");
      CONFIG_GET_INT_CONSOLE(viewports.custom_vp.height, "custom_viewport_height");
      CONFIG_GET_INT_CONSOLE(screen_orientation, "screen_orientation");
      CONFIG_GET_INT_CONSOLE(sound_mode, "sound_mode");
#ifdef HAVE_ZLIB
      CONFIG_GET_INT_CONSOLE(zip_extract_mode, "zip_extract_mode");
#endif
      CONFIG_GET_STRING_CONSOLE(default_rom_startup_dir, "default_rom_startup_dir");
      CONFIG_GET_FLOAT_CONSOLE(menu_font_size, "menu_font_size");
      CONFIG_GET_FLOAT_CONSOLE(overscan_amount, "overscan_amount");

      // g_extern
      CONFIG_GET_INT_EXTERN(state_slot, "state_slot");
      CONFIG_GET_INT_EXTERN(audio_data.mute, "audio_mute");
}

void rarch_config_save(const char * conf_name)
{
      config_file_t * conf = config_file_new(conf_name);

      if(!conf)
         return;

      // g_settings
      config_set_string(conf, "libretro_path", g_settings.libretro);
#ifdef HAVE_XML
      config_set_string(conf, "cheat_database_path", g_settings.cheat_database);
#endif
      config_set_bool(conf, "rewind_enable", g_settings.rewind_enable);
      config_set_string(conf, "video_cg_shader", g_settings.video.cg_shader_path);
      config_set_float(conf, "video_aspect_ratio", g_settings.video.aspect_ratio);
#ifdef HAVE_FBO
      config_set_float(conf, "video_fbo_scale_x", g_settings.video.fbo_scale_x);
      config_set_float(conf, "video_fbo_scale_y", g_settings.video.fbo_scale_y);
      config_set_string(conf, "video_second_pass_shader", g_settings.video.second_pass_shader);
      config_set_bool(conf, "video_render_to_texture", g_settings.video.render_to_texture);
      config_set_bool(conf, "video_second_pass_smooth", g_settings.video.second_pass_smooth);
#endif
      config_set_bool(conf, "video_smooth", g_settings.video.smooth);
      config_set_bool(conf, "video_vsync", g_settings.video.vsync);
      config_set_string(conf, "audio_device", g_settings.audio.device);
      config_set_bool(conf, "audio_rate_control", g_settings.audio.rate_control);
      config_set_float(conf, "audio_rate_control_delta", g_settings.audio.rate_control_delta);

      for (unsigned i = 0; i < 7; i++)
      {
         char cfg[64];
         snprintf(cfg, sizeof(cfg), "input_dpad_emulation_p%u", i + 1);
         config_set_int(conf, cfg, g_settings.input.dpad_emulation[i]);
      }

#ifdef RARCH_CONSOLE
      config_set_bool(conf, "fbo_enabled", g_console.fbo_enabled);
#ifdef __CELLOS_LV2__
      config_set_bool(conf, "custom_bgm_enable", g_console.custom_bgm_enable);
#endif
      config_set_bool(conf, "overscan_enable", g_console.overscan_enable);
      config_set_bool(conf, "screenshots_enable", g_console.screenshots_enable);
#ifdef _XBOX
      config_set_bool(conf, "gamma_correction_enable", g_console.gamma_correction_enable);
      config_set_int(conf, "color_format", g_console.color_format);
#endif
      config_set_bool(conf, "throttle_enable", g_console.throttle_enable);
      config_set_bool(conf, "triple_buffering_enable", g_console.triple_buffering_enable);
      config_set_bool(conf, "info_msg_enable", g_console.info_msg_enable);
      config_set_int(conf, "sound_mode", g_console.sound_mode);
      config_set_int(conf, "aspect_ratio_index", g_console.aspect_ratio_index);
      config_set_int(conf, "current_resolution_id", g_console.current_resolution_id);
      config_set_int(conf, "custom_viewport_width", g_console.viewports.custom_vp.width);
      config_set_int(conf, "custom_viewport_height", g_console.viewports.custom_vp.height);
      config_set_int(conf, "custom_viewport_x", g_console.viewports.custom_vp.x);
      config_set_int(conf, "custom_viewport_y", g_console.viewports.custom_vp.y);
      config_set_int(conf, "screen_orientation", g_console.screen_orientation);
      config_set_string(conf, "default_rom_startup_dir", g_console.default_rom_startup_dir);
      config_set_float(conf, "menu_font_size", g_console.menu_font_size);
      config_set_float(conf, "overscan_amount", g_console.overscan_amount);
#ifdef HAVE_ZLIB
      config_set_int(conf, "zip_extract_mode", g_console.zip_extract_mode);
#endif
#endif

      // g_extern
      config_set_int(conf, "state_slot", g_extern.state_slot);
      config_set_int(conf, "audio_mute", g_extern.audio_data.mute);

      if (!config_file_write(conf, conf_name))
         RARCH_ERR("Failed to write config file to \"%s\". Check permissions.\n", conf_name);

      free(conf);
}
