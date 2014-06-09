/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef MENU_LIST_H__
#define MENU_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../../boolean.h"

struct item_file;
typedef struct file_list
{
   struct item_file *list;

   size_t capacity;
   size_t size;
} file_list_t;

void file_list_free(void *list);

void file_list_push(void *userdata, const char *path,
      unsigned type, size_t current_directory_ptr);
void file_list_pop(void *list, size_t *directory_ptr);
void file_list_clear(void *list);

void file_list_get_last(const void *list,
      const char **path, unsigned *type);

size_t file_list_get_size(const void *list);
size_t file_list_get_directory_ptr(const void *list);

void file_list_get_at_offset(const void *list, size_t index,
      const char **path, unsigned *type);

void file_list_set_alt_at_offset(void *list, size_t index,
      const char *alt);
void file_list_get_alt_at_offset(const void *list, size_t index,
      const char **alt);

void file_list_sort_on_alt(void *list);

bool file_list_search(const void *list, const char *needle, size_t *index);

#ifdef __cplusplus
}
#endif
#endif

