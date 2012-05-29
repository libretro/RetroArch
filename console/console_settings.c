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
         if(g_console.screen_orientation > ORIENTATION_NORMAL)
            g_console.screen_orientation--;
         break;
      case S_ROTATION_INCREMENT:
         if((g_console.screen_orientation+1) < ORIENTATION_END)
            g_console.screen_orientation++;
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
