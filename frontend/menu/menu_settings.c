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
#include "../../gfx/gfx_common.h"
#include "../../file.h"
#include "menu_settings.h"

void menu_settings_msg(unsigned setting, unsigned delay)
{
   char str[PATH_MAX];
   msg_queue_clear(g_extern.msg_queue);

   switch(setting)
   {
      case S_MSG_CHANGE_CONTROLS:
         snprintf(str, sizeof(str), "INFO - Press LEFT/RIGHT to change the controls, and press\n[RetroPad Start] to reset a button to default values.");
         break;
      case S_MSG_LOADING_ROM:
         {
            char tmp[PATH_MAX];
            fill_pathname_base(tmp, g_extern.fullpath, sizeof(tmp));
            snprintf(str, sizeof(str), "INFO - Loading %s...", g_extern.fullpath);
         }
         break;
      case S_MSG_DIR_LOADING_ERROR:
         strlcpy(str, "ERROR - Failed to open directory.", sizeof(str));
         break;
      case S_MSG_ROM_LOADING_ERROR:
         strlcpy(str, "ERROR - An error occurred during ROM loading.", sizeof(str));
         break;
      case S_MSG_NOT_IMPLEMENTED:
         strlcpy(str, "TODO - Not yet implemented.", sizeof(str));
         break;
      case S_MSG_RESIZE_SCREEN:
         snprintf(str, sizeof(str), "INFO - Resize the screen by moving around the two analog sticks.\n");
         break;
      case S_MSG_RESTART_RARCH:
         strlcpy(str, "INFO - You need to restart RetroArch.", sizeof(str));
         break;
      case S_MSG_SELECT_LIBRETRO_CORE:
         strlcpy(str, "INFO - Select a Libretro core from the menu.", sizeof(str));
         break;
      case S_MSG_SELECT_SHADER:
         strlcpy(str, "INFO - Select a shader from the menu.", sizeof(str));
         break;
      case S_MSG_SHADER_LOADING_SUCCEEDED:
         strlcpy(str, "INFO - Shader successfully loaded.", sizeof(str));
         break;
   }

   msg_queue_push(g_extern.msg_queue, str, 1, delay);
}
