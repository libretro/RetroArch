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

#ifndef RGUI_LIST_H__
#define RGUI_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "boolean.h"

struct item_file;
typedef struct file_list
{
   struct item_file *list;

   size_t capacity;
   size_t size;
} file_list_t;

void file_list_free(file_list_t *list);

void file_list_push(file_list_t *userdata, const char *path,
      unsigned type, size_t current_directory_ptr);
void file_list_pop(file_list_t *list, size_t *directory_ptr);
void file_list_clear(file_list_t *list);

void file_list_get_last(const file_list_t *list,
      const char **path, unsigned *type);

void file_list_get_at_offset(const file_list_t *list, size_t index,
      const char **path, unsigned *type);

void file_list_set_alt_at_offset(file_list_t *list, size_t index,
      const char *alt);
void file_list_get_alt_at_offset(const file_list_t *list, size_t index,
      const char **alt);

void file_list_sort_on_alt(file_list_t *list);

bool file_list_search(const file_list_t *list, const char *needle, size_t *index);

#ifdef __cplusplus
}
#endif
#endif

