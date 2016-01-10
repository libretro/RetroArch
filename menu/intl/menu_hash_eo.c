/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../menu_hash.h"

const char *menu_hash_to_str_eo(uint32_t hash)
{
   switch (hash)
   {
      case 0:
      default:
         break;
   }

   return "null";
}

int menu_hash_get_help_eo(uint32_t hash, char *s, size_t len)
{
   int ret = 0;

   switch (hash)
   {
      case 0:
      default:
         ret = -1;
         break;
   }

   return ret;
}
