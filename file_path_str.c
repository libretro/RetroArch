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

#include "file_path_special.h"

const char *file_path_str(enum file_path_enum enum_idx)
{
   switch (enum_idx)
   {
      case FILE_PATH_CONTENT_HISTORY:
         return "content_history.lpl";
      case FILE_PATH_MAIN_CONFIG:
         return "retroarch.cfg";
      case FILE_PATH_UNKNOWN:
      default:
         break;
   }

   return "null";
}
