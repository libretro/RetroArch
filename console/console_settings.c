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
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "../general.h"
#include "console_settings.h"

void rarch_settings_change(unsigned setting)
{
   switch(setting)
   {
      case S_ASPECT_RATIO_DECREMENT:
         if(g_console.aspect_ratio_index > 0)
            g_console.aspect_ratio_index--;
         break;
      case S_ASPECT_RATIO_INCREMENT:
         if(g_console.aspect_ratio_index < LAST_ASPECT_RATIO)
            g_console.aspect_ratio_index++;
         break;
      case S_FRAME_ADVANCE:
         g_console.frame_advance_enable = true;
         g_console.menu_enable = false;
         g_console.mode_switch = MODE_EMULATION;
         break;
      case S_HW_TEXTURE_FILTER:
         g_settings.video.smooth = !g_settings.video.smooth;
         break;
      case S_HW_TEXTURE_FILTER_2:
         g_settings.video.second_pass_smooth = !g_settings.video.second_pass_smooth;
         break;
      case S_OVERSCAN_DECREMENT:
         g_console.overscan_amount -= 0.01f;
         g_console.overscan_enable = true;
         if(g_console.overscan_amount == 0.0f)
            g_console.overscan_enable = false;
         break;
      case S_OVERSCAN_INCREMENT:
         g_console.overscan_amount += 0.01f;
         g_console.overscan_enable = true;
         if(g_console.overscan_amount == 0.0f)
            g_console.overscan_enable = 0;
         break;
      case S_RESOLUTION_PREVIOUS:
         if (g_console.current_resolution_index)
         {
            g_console.current_resolution_index--;
            g_console.current_resolution_id = g_console.supported_resolutions[g_console.current_resolution_index];
         }
         break;
      case S_RESOLUTION_NEXT:
	 if (g_console.current_resolution_index + 1 < g_console.supported_resolutions_count)
	 {
            g_console.current_resolution_index++;
	    g_console.current_resolution_id = g_console.supported_resolutions[g_console.current_resolution_index];
	 }
         break;
      case S_QUIT:
         g_console.menu_enable = false;
         g_console.ingame_menu_enable = false;
         g_console.mode_switch = MODE_EXIT;
         break;
      case S_RETURN_TO_DASHBOARD:
         g_console.menu_enable = false;
         g_console.initialize_rarch_enable = false;
         g_console.mode_switch = MODE_EXIT;
         break;
      case S_RETURN_TO_GAME:
         g_console.frame_advance_enable = false;
         //g_console.ingame_menu_item = 0;
         g_console.menu_enable = false;
         g_console.mode_switch = MODE_EMULATION;
         break;
      case S_RETURN_TO_LAUNCHER:
         g_console.return_to_launcher = true;
         g_console.menu_enable = false;
         g_console.mode_switch = MODE_EXIT;
         break;
      case S_RETURN_TO_MENU:
         g_console.menu_enable = false;
         g_console.ingame_menu_item = 0;
         g_console.mode_switch = MODE_MENU;
         break;
      case S_ROTATION_DECREMENT:
         if(g_console.screen_orientation > 0)
            g_console.screen_orientation--;
         break;
      case S_ROTATION_INCREMENT:
         if(g_console.screen_orientation < LAST_ORIENTATION)
            g_console.screen_orientation++;
         break;
      case S_START_RARCH:
         g_console.menu_enable = false;
         g_console.initialize_rarch_enable = 1;
         g_console.mode_switch = MODE_EMULATION;
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
         g_console.fbo_enabled = !g_console.fbo_enabled;
         break;
      case S_SCALE_FACTOR_DECREMENT:
         g_settings.video.fbo_scale_x -= 1.0f;
         g_settings.video.fbo_scale_y -= 1.0f;
         break;
      case S_SCALE_FACTOR_INCREMENT:
         g_settings.video.fbo_scale_x += 1.0f;
         g_settings.video.fbo_scale_y += 1.0f;
         break;
      case S_THROTTLE:
         g_console.throttle_enable = !g_console.throttle_enable;
         break;
      case S_TRIPLE_BUFFERING:
         g_console.triple_buffering_enable = !g_console.triple_buffering_enable;
         break; 
   }
}

void rarch_settings_default(unsigned setting)
{
   switch(setting)
   {
      case S_DEF_ASPECT_RATIO:
         g_console.aspect_ratio_index = ASPECT_RATIO_4_3;
         break;
      case S_DEF_HW_TEXTURE_FILTER:
         g_settings.video.smooth = 1;
         break;
      case S_DEF_HW_TEXTURE_FILTER_2:
         g_settings.video.second_pass_smooth = 1;
         break;
      case S_DEF_OVERSCAN:
         g_console.overscan_amount = 0.0f;
         g_console.overscan_enable = false;
         break;
	  case S_DEF_ROTATION:
         g_console.screen_orientation = ORIENTATION_NORMAL;
         break;
      case S_DEF_THROTTLE:
         g_console.throttle_enable = true;
         break;
      case S_DEF_TRIPLE_BUFFERING:
         g_console.triple_buffering_enable = true;
         break;
      case S_DEF_SAVE_STATE:
         g_extern.state_slot = 0;
         break;
      case S_DEF_SCALE_ENABLED:
         g_console.fbo_enabled = true;
         g_settings.video.fbo_scale_x = 2.0f;
         g_settings.video.fbo_scale_y = 2.0f;
         break;
      case S_DEF_SCALE_FACTOR:
         g_settings.video.fbo_scale_x = 2.0f;
         g_settings.video.fbo_scale_y = 2.0f;
         break;
   }
}

void rarch_settings_msg(unsigned setting, unsigned delay)
{
   char str[PATH_MAX];
   msg_queue_clear(g_extern.msg_queue);

   switch(setting)
   {
      case S_MSG_CACHE_PARTITION:
         snprintf(str, sizeof(str), "INFO - All the contents of the ZIP files you have selected in the filebrowser\nare extracted to this partition.");
         break;
      case S_MSG_CHANGE_CONTROLS:
         snprintf(str, sizeof(str), "INFO - Press LEFT/RIGHT to change the controls, and press\n[RetroPad Start] to reset a button to default values.");
         break;
      case S_MSG_EXTRACTED_ZIPFILE:
         snprintf(str, sizeof(str), "INFO - ZIP file successfully extracted to cache partition.");
         break;
      case S_MSG_NOT_IMPLEMENTED:
         snprintf(str, sizeof(str), "TODO - Not yet implemented.");
         break;
	  case S_MSG_RESIZE_SCREEN:
         snprintf(str, sizeof(str), "INFO - Resize the screen by moving around the two analog sticks.\nPress [RetroPad X] to reset to default values, and [RetroPad A] to go back.\nTo select the resized screen mode, set Aspect Ratio to: 'Custom'.");
         break;
	  case S_MSG_RESTART_RARCH:
         snprintf(str, sizeof(str), "INFO - You need to restart RetroArch for this change to take effect.");
         break;
	  case S_MSG_SELECT_LIBRETRO_CORE:
         snprintf(str, sizeof(str), "INFO - Select a Libretro core from the menu by pressing [RetroPad B].");
         break;
	  case S_MSG_SELECT_SHADER:
         snprintf(str, sizeof(str), "INFO - Select a shader from the menu by pressing [RetroPad A].");
         break;
	  case S_MSG_SHADER_LOADING_SUCCEEDED:
         snprintf(str, sizeof(str), "INFO - Shader successfully loaded.");
         break;
   }

   msg_queue_push(g_extern.msg_queue, str, 1, delay);
}

void rarch_settings_create_menu_item_label_w(wchar_t *strwbuf, unsigned setting, size_t size)
{
   char str[PATH_MAX];

   rarch_settings_create_menu_item_label(str, setting, sizeof(str));
   rarch_convert_char_to_wchar(strwbuf, str, size);
}

void rarch_settings_create_menu_item_label(char * str, unsigned setting, size_t size)
{
   switch (setting)
   {
      case S_LBL_ASPECT_RATIO:
         snprintf(str, size, "Aspect Ratio: %s", aspectratio_lut[g_console.aspect_ratio_index].name);
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
         snprintf(str, size, "Scale Factor: %f (X) / %f (Y)", g_settings.video.fbo_scale_x, g_settings.video.fbo_scale_y);
         break;
	  case S_LBL_ROTATION:
         snprintf(str, size, "Rotation: %s", rotation_lut[g_console.screen_orientation]);
         break;
	  case S_LBL_LOAD_STATE_SLOT:
         snprintf(str, size, "Load State #%d", g_extern.state_slot);
         break;
	  case S_LBL_SAVE_STATE_SLOT:
		 snprintf(str, size, "Save State #%d", g_extern.state_slot);
         break;
   }
}
