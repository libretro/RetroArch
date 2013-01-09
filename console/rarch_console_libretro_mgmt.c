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
#include "../boolean.h"
#include "../file.h"
#include "rarch_console.h"

#include "rarch_console_libretro_mgmt.h"

bool rarch_manage_libretro_extension_supported(const char *filename)
{
   bool ext_supported = false;
   struct string_list *ext_list = NULL;
   const char *file_ext = path_get_extension(filename);
   const char *ext = rarch_console_get_rom_ext();

   if (ext)
      ext_list = string_split(ext, "|");

   if (ext_list && string_list_find_elem(ext_list, file_ext))
      ext_supported = true; 

   return ext_supported;
}
