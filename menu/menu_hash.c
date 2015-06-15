/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
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

#include <stdint.h>

#include "menu_hash.h"

const char *menu_hash_to_str(uint32_t hash)
{
   switch (hash)
   {
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Recording Settings";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_VALUE_SETTINGS:
         return "Settings";
      case MENU_LABEL_QUIT_RETROARCH:
         return "Quit RetroArch";
      case MENU_LABEL_HELP:
         return "Help";
      case MENU_LABEL_SAVE_NEW_CONFIG:
         return "Save New Config";
      case MENU_LABEL_RESTART_CONTENT:
         return "Restart Content";
      case MENU_LABEL_TAKE_SCREENSHOT:
         return "Take Screenshot";
      case MENU_LABEL_CORE_UPDATER_LIST:
         return "Core Updater";
      case MENU_LABEL_SYSTEM_INFORMATION:
         return "System Information";
      case MENU_LABEL_OPTIONS:
         return "Options";
      case MENU_LABEL_CORE_INFORMATION:
         return "Core Information";
      case MENU_LABEL_CORE_LIST:
         return "Load Core";
      case MENU_LABEL_UNLOAD_CORE:
         return "Unload Core";
      case MENU_LABEL_MANAGEMENT:
         return "Advanced Management";
      case MENU_LABEL_PERFORMANCE_COUNTERS:
         return "Performance Counters";
      case MENU_LABEL_SAVE_STATE:
         return "Save State";
      case MENU_LABEL_LOAD_STATE:
         return "Load State";
      case MENU_LABEL_RESUME_CONTENT:
         return "Resume Content";
      case MENU_LABEL_DRIVER_SETTINGS:
         return "Driver Settings";
   }

   return "null";
}
