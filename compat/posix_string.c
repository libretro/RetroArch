/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "posix_string.h"

#undef strcasecmp
#undef strdup
#undef isblank
#include <ctype.h>
#include <stdlib.h>
#include <stddef.h>
#include "strl.h"

#include <string.h>

int strcasecmp_ssnes__(const char *a, const char *b)
{
   while (*a && *b)
   {
      int a_ = tolower(*a);
      int b_ = tolower(*b);
      if (a_ != b_)
         return a_ - b_;

      a++;
      b++;
   }

   return tolower(*a) - tolower(*b);
}

char *strdup_ssnes__(const char *orig)
{
   size_t len = strlen(orig) + 1;
   char *ret = (char*)malloc(len);
   if (!ret)
      return NULL;

   strlcpy(ret, orig, len);
   return ret;
}

int isblank_ssnes__(int c)
{
   return (c == ' ') || (c == '\t');
}

