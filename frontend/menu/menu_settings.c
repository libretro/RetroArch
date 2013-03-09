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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "../../general.h"
#include "menu_settings.h"

void menu_settings_set(unsigned setting)
{
   switch(setting)
   {
      case S_ASPECT_RATIO_DECREMENT:
         if(g_settings.video.aspect_ratio_idx > 0)
            g_settings.video.aspect_ratio_idx--;
         break;
      case S_ASPECT_RATIO_INCREMENT:
         if(g_settings.video.aspect_ratio_idx < LAST_ASPECT_RATIO)
            g_settings.video.aspect_ratio_idx++;
         break;
      case S_AUDIO_MUTE:
         g_extern.audio_data.mute = !g_extern.audio_data.mute;
         break;
      case S_AUDIO_CONTROL_RATE_DECREMENT:
         if (g_settings.audio.rate_control_delta > 0.0)
            g_settings.audio.rate_control_delta -= 0.001;
         if (g_settings.audio.rate_control_delta == 0.0)
            g_settings.audio.rate_control = false;
         else
            g_settings.audio.rate_control = true;
         break;
      case S_AUDIO_CONTROL_RATE_INCREMENT:
         if (g_settings.audio.rate_control_delta < 0.2)
            g_settings.audio.rate_control_delta += 0.001;
         g_settings.audio.rate_control = true;
         break;
      case S_FRAME_ADVANCE:
         g_extern.lifecycle_state |= (1ULL << RARCH_FRAMEADVANCE);
         g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         break;
      case S_HW_TEXTURE_FILTER:
         g_settings.video.smooth = !g_settings.video.smooth;
         break;
      case S_HW_TEXTURE_FILTER_2:
         g_settings.video.second_pass_smooth = !g_settings.video.second_pass_smooth;
         break;
      case S_OVERSCAN_DECREMENT:
         g_extern.console.screen.overscan_amount -= 0.01f;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_OVERSCAN_ENABLE);
         if(g_extern.console.screen.overscan_amount == 0.0f)
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_OVERSCAN_ENABLE);
         break;
      case S_OVERSCAN_INCREMENT:
         g_extern.console.screen.overscan_amount += 0.01f;
         g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_OVERSCAN_ENABLE);
         if(g_extern.console.screen.overscan_amount == 0.0f)
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_OVERSCAN_ENABLE);
         break;
      case S_RESOLUTION_PREVIOUS:
         if (g_extern.console.screen.resolutions.current.idx)
         {
            g_extern.console.screen.resolutions.current.idx--;
            g_extern.console.screen.resolutions.current.id = g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
         }
         break;
      case S_RESOLUTION_NEXT:
         if (g_extern.console.screen.resolutions.current.idx + 1 < g_extern.console.screen.resolutions.count)
         {
            g_extern.console.screen.resolutions.current.idx++;
            g_extern.console.screen.resolutions.current.id = g_extern.console.screen.resolutions.list[g_extern.console.screen.resolutions.current.idx];
         }
         break;
      case S_ROTATION_DECREMENT:
         if(g_extern.console.screen.orientation > 0)
            g_extern.console.screen.orientation--;
         break;
      case S_ROTATION_INCREMENT:
         if(g_extern.console.screen.orientation < LAST_ORIENTATION)
            g_extern.console.screen.orientation++;
         break;
      case S_REWIND:
         g_settings.rewind_enable = !g_settings.rewind_enable;
         break;
      case S_SAVESTATE_DECREMENT:
         if(g_extern.state_slot != 0)
            g_extern.state_slot--;
         break;
      case S_SAVESTATE_INCREMENT:
         g_extern.state_slot++;
         break;
      case S_SCALE_ENABLED:
         g_settings.video.render_to_texture = !g_settings.video.render_to_texture;
         break;
      case S_SCALE_FACTOR_DECREMENT:
         g_settings.video.fbo.scale_x -= 1.0f;
         g_settings.video.fbo.scale_y -= 1.0f;
         break;
      case S_SCALE_FACTOR_INCREMENT:
         g_settings.video.fbo.scale_x += 1.0f;
         g_settings.video.fbo.scale_y += 1.0f;
         break;
      case S_THROTTLE:
         if(!g_extern.system.force_nonblock)
         {
            if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_THROTTLE_ENABLE))
               g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_THROTTLE_ENABLE);
            else
               g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_THROTTLE_ENABLE);
         }
         break;
      case S_TRIPLE_BUFFERING:
         if (g_extern.lifecycle_mode_state & (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE))
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE);
         else
            g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE);
         break;
      case S_REFRESH_RATE_DECREMENT:
         g_settings.video.refresh_rate -= 0.01f;
         break;
      case S_REFRESH_RATE_INCREMENT:
         g_settings.video.refresh_rate += 0.01f;
         break;
      case S_INFO_DEBUG_MSG_TOGGLE:
         if (g_extern.lifecycle_mode_state & (1ULL << MODE_FPS_DRAW))
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_FPS_DRAW);
         else
            g_extern.lifecycle_mode_state |= (1ULL << MODE_FPS_DRAW);
         break;
      case S_INFO_MSG_TOGGLE:
         if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
            g_extern.lifecycle_mode_state &= ~(1ULL << MODE_INFO_DRAW);
         else
            g_extern.lifecycle_mode_state |= (1ULL << MODE_INFO_DRAW);
         break;
      default:
         RARCH_WARN("menu_settings_set - unhandled action.\n");
   }
}

void menu_settings_set_default(unsigned setting)
{
   switch(setting)
   {
      case S_DEF_ASPECT_RATIO:
         g_settings.video.aspect_ratio_idx = ASPECT_RATIO_4_3;
         break;
      case S_DEF_AUDIO_MUTE:
         g_extern.audio_data.mute = false;
         break;
      case S_DEF_AUDIO_CONTROL_RATE:
#ifdef GEKKO
         g_settings.audio.rate_control_delta = 0.006;
         g_settings.audio.rate_control = true;
#else
         g_settings.audio.rate_control_delta = 0.0;
         g_settings.audio.rate_control = false;
#endif
         break;
      case S_DEF_HW_TEXTURE_FILTER:
         g_settings.video.smooth = 1;
         break;
      case S_DEF_HW_TEXTURE_FILTER_2:
         g_settings.video.second_pass_smooth = 1;
         break;
      case S_DEF_OVERSCAN:
         g_extern.console.screen.overscan_amount = 0.0f;
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_VIDEO_OVERSCAN_ENABLE);
         break;
      case S_DEF_ROTATION:
         g_extern.console.screen.orientation = ORIENTATION_NORMAL;
         break;
      case S_DEF_THROTTLE:
         if(!g_extern.system.force_nonblock)
            g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_THROTTLE_ENABLE);
         break;
      case S_DEF_TRIPLE_BUFFERING:
         g_extern.lifecycle_mode_state |= (1ULL << MODE_VIDEO_TRIPLE_BUFFERING_ENABLE);
         break;
      case S_DEF_SAVE_STATE:
         g_extern.state_slot = 0;
         break;
      case S_DEF_SCALE_ENABLED:
         g_settings.video.render_to_texture = true;
         g_settings.video.fbo.scale_x = 2.0f;
         g_settings.video.fbo.scale_y = 2.0f;
         break;
      case S_DEF_SCALE_FACTOR:
         g_settings.video.fbo.scale_x = 2.0f;
         g_settings.video.fbo.scale_y = 2.0f;
         break;
      case S_DEF_REFRESH_RATE:
#if defined(RARCH_CONSOLE)
         g_settings.video.refresh_rate = 59.92;
#else
         g_settings.video.refresh_rate = 59.95;
#endif
         break;
      case S_DEF_INFO_DEBUG_MSG:
         g_extern.lifecycle_mode_state &= ~(1ULL << MODE_FPS_DRAW);
         break;
      case S_DEF_INFO_MSG:
         g_extern.lifecycle_mode_state |= (1ULL << MODE_INFO_DRAW);
         break;
      default:
         RARCH_WARN("menu_settings_set_default: unhandled action.\n");
   }
}

void menu_settings_msg(unsigned setting, unsigned delay)
{
   char str[PATH_MAX], tmp[PATH_MAX];
   msg_queue_clear(g_extern.msg_queue);

   (void)tmp;

   switch(setting)
   {
      case S_MSG_CACHE_PARTITION:
         snprintf(str, sizeof(str), "INFO - All the contents of the ZIP files you have selected\nare extracted to this partition.");
         break;
      case S_MSG_CHANGE_CONTROLS:
         snprintf(str, sizeof(str), "INFO - Press LEFT/RIGHT to change the controls, and press\n[RetroPad Start] to reset a button to default values.");
         break;
      case S_MSG_LOADING_ROM:
         fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
         snprintf(str, sizeof(str), "INFO - Loading %s...", tmp);
         break;
      case S_MSG_DIR_LOADING_ERROR:
         snprintf(str, sizeof(str), "ERROR - Failed to open selected directory.");
         break;
      case S_MSG_ROM_LOADING_ERROR:
         snprintf(str, sizeof(str), "ERROR - An error occurred during ROM loading.");
         break;
      case S_MSG_NOT_IMPLEMENTED:
         snprintf(str, sizeof(str), "TODO - Not yet implemented.");
         break;
      case S_MSG_RESIZE_SCREEN:
         snprintf(str, sizeof(str), "INFO - Resize the screen by moving around the two analog sticks.\nPress [RetroPad X] to reset to default values, and [RetroPad A] to go back.\nTo select the resized screen mode, set Aspect Ratio to: 'Custom'.");
         break;
      case S_MSG_RESTART_RARCH:
         snprintf(str, sizeof(str), "INFO - You need to restart RetroArch.");
         break;
      case S_MSG_SELECT_LIBRETRO_CORE:
         snprintf(str, sizeof(str), "INFO - Select a Libretro core from the menu.");
         break;
      case S_MSG_SELECT_SHADER:
         snprintf(str, sizeof(str), "INFO - Select a shader from the menu.");
         break;
      case S_MSG_SHADER_LOADING_SUCCEEDED:
         snprintf(str, sizeof(str), "INFO - Shader successfully loaded.");
         break;
   }

   msg_queue_push(g_extern.msg_queue, str, 1, delay);
}

void menu_settings_create_menu_item_label(char * str, unsigned setting, size_t size)
{
   switch (setting)
   {
      case S_LBL_ASPECT_RATIO:
         snprintf(str, size, "Aspect Ratio: %s", aspectratio_lut[g_settings.video.aspect_ratio_idx].name);
         break;
      case S_LBL_SHADER:
         snprintf(str, size, "Shader #1: %s", g_settings.video.cg_shader_path);
         break;
      case S_LBL_SHADER_2:
         snprintf(str, size, "Shader #2: %s", g_settings.video.second_pass_shader);
         break;
      case S_LBL_RARCH_VERSION:
         snprintf(str, size, "RetroArch %s", PACKAGE_VERSION);
         break;
      case S_LBL_SCALE_FACTOR:
         snprintf(str, size, "Scale Factor: %f (X) / %f (Y)", g_settings.video.fbo.scale_x, g_settings.video.fbo.scale_y);
         break;
      case S_LBL_ROTATION:
         snprintf(str, size, "Rotation: %s", rotation_lut[g_extern.console.screen.orientation]);
         break;
      case S_LBL_LOAD_STATE_SLOT:
         snprintf(str, size, "Load State #%d", g_extern.state_slot);
         break;
      case S_LBL_SAVE_STATE_SLOT:
         snprintf(str, size, "Save State #%d", g_extern.state_slot);
         break;
      case S_LBL_REWIND_GRANULARITY:
         snprintf(str, size, "Rewind granularity: %d", g_settings.rewind_granularity);
	 break;
   }
}
