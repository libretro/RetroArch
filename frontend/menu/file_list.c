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

#include <stdlib.h>
#include <string.h>
#include "file_list.h"

struct rgui_file
{
   char *path;
   char *alt;
   unsigned type;
   size_t directory_ptr;
};

void rgui_list_push(void *userdata,
      const char *path, unsigned type, size_t directory_ptr)
{
   rgui_list_t *list = (rgui_list_t*)userdata;

   if (!list)
      return;

   if (list->size >= list->capacity)
   {
      list->capacity++;
      list->capacity *= 2;
      list->list = (struct rgui_file*)realloc(list->list, list->capacity * sizeof(struct rgui_file));
   }

   list->list[list->size].path = strdup(path);
   list->list[list->size].alt = NULL;
   list->list[list->size].type = type;
   list->list[list->size].directory_ptr = directory_ptr;
   list->size++;
}

void rgui_list_pop(rgui_list_t *list, size_t *directory_ptr)
{
   if (!(list->size == 0))
      free(list->list[--list->size].path);

   if (directory_ptr)
      *directory_ptr = list->list[list->size].directory_ptr;
}

void rgui_list_free(rgui_list_t *list)
{
   for (size_t i = 0; i < list->size; i++)
      free(list->list[i].path);
   free(list->list);
   free(list);
}

void rgui_list_clear(rgui_list_t *list)
{
   for (size_t i = 0; i < list->size; i++)
   {
      free(list->list[i].path);
      free(list->list[i].alt);
   }
   list->size = 0;
}

void rgui_list_set_alt_at_offset(rgui_list_t *list, size_t index,
      const char *alt)
{
   free(list->list[index].alt);
   list->list[index].alt = strdup(alt);
}

void rgui_list_get_alt_at_offset(const rgui_list_t *list, size_t index,
      const char **alt)
{
   if (alt)
      *alt = list->list[index].alt ? list->list[index].alt : list->list[index].path;
}

static int rgui_list_alt_cmp(const void *a_, const void *b_)
{
   const struct rgui_file *a = (const struct rgui_file*)a_;
   const struct rgui_file *b = (const struct rgui_file*)b_;
   const char *cmp_a = a->alt ? a->alt : a->path;
   const char *cmp_b = b->alt ? b->alt : b->path;
   return strcasecmp(cmp_a, cmp_b);
}

void rgui_list_sort_on_alt(rgui_list_t *list)
{
   qsort(list->list, list->size, sizeof(list->list[0]), rgui_list_alt_cmp);
}

void rgui_list_get_at_offset(const rgui_list_t *list, size_t index,
      const char **path, unsigned *file_type)
{
   if (path)
      *path = list->list[index].path;
   if (file_type)
      *file_type = list->list[index].type;
}

void rgui_list_get_last(const rgui_list_t *list,
      const char **path, unsigned *file_type)
{
   if (list->size)
      rgui_list_get_at_offset(list, list->size - 1, path, file_type);
}
