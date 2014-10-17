/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef _MENU_LIST_H
#define _MENU_LIST_H

#include <stddef.h>
#include "../../file_list.h"

#ifdef __cplusplus
extern "C" {
#endif

void menu_list_free(file_list_t *list);

void menu_list_pop(file_list_t *list, size_t *directory_ptr);

void menu_list_pop_stack(file_list_t *list);

void menu_list_clear(file_list_t *list);

void menu_list_push(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr);

void menu_list_push_stack(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr);

#ifdef __cplusplus
}
#endif

#endif
