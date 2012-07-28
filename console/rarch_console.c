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
#include <string.h>
#include "../boolean.h"
#include "../compat/strl.h"
#include "../libretro.h"
#include "../compat/strl.h"
#include "../file.h"
#include "../general.h"

#include "rarch_console.h"

default_paths_t default_paths;

void rarch_convert_char_to_wchar(wchar_t *buf, const char * str, size_t size)
{
   mbstowcs(buf, str, size / sizeof(wchar_t));
}

const char * rarch_convert_wchar_to_const_char(const wchar_t * wstr)
{
   static char str[256];
   wcstombs(str, wstr, sizeof(str));
   return str;
}

void rarch_extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}
