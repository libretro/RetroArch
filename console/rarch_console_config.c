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

void rarch_config_load (void)
{
   config_file_t *conf = NULL;

   if (*g_extern.config_path)
      conf = config_file_new(g_extern.config_path);
   else
      conf = config_file_new(NULL);

   if (!conf)
   {
      RARCH_ERR("Couldn't find config at path: \"%s\"\n", g_extern.config_path);
      rarch_fail(1, "rarch_config_load()");
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

   for (unsigned i = 0; i < MAX_PADS; i++)
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
}
