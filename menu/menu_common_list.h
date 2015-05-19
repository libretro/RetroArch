/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef MENU_COMMON_LIST_H__
#define MENU_COMMON_LIST_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void menu_common_list_clear(void *data);

void menu_common_list_set_selection(void *data);

void menu_common_list_insert(void *data,
      const char *path, const char *label,
      unsigned type, size_t idx);

void menu_common_list_delete(void *data, size_t idx,
      size_t list_size);

#ifdef __cplusplus
}
#endif

#endif
