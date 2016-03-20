/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef _DIR_LIST_SPECIAL_H
#define _DIR_LIST_SPECIAL_H

#include <stdint.h>

#include <lists/string_list.h>

enum dir_list_type
{
   DIR_LIST_NONE = 0,
   DIR_LIST_CORES,
   DIR_LIST_CORE_INFO,
   DIR_LIST_DATABASES,
   DIR_LIST_COLLECTIONS,
   DIR_LIST_PLAIN,
   DIR_LIST_SHADERS
};

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter);

#endif
