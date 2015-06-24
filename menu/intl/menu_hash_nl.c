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
#include <string.h>
#include <rhash.h>

#include "../menu_hash.h"

const char *menu_hash_to_str_nl(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core Informatie";
      case MENU_LABEL_CORE_LIST:
         return "Laad Core";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Instellingen";
      case MENU_LABEL_VALUE_ONLINE_UPDATER:
         return "Online Updaten";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Systeem Informatie";
      default:
         break;
   }

   return "null";
}
