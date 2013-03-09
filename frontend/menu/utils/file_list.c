/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "file_list.h"
#include "../rgui.h"

struct rgui_file
{
   char *path;
   unsigned type;
   size_t directory_ptr;
};

struct rgui_list
{
   struct rgui_file *list;

   size_t capacity;
   size_t ptr;
};

rgui_list_t *rgui_list_new(void)
{
   return (rgui_list_t*)calloc(1, sizeof(rgui_list_t));
}

bool rgui_list_empty(const rgui_list_t *list)
{
   return list->ptr == 0;
}

void rgui_list_push(rgui_list_t *list,
      const char *path, unsigned type, size_t directory_ptr)
{
   if (list->ptr >= list->capacity)
   {
      list->capacity++;
      list->capacity *= 2;
      list->list = (struct rgui_file*)realloc(list->list, list->capacity * sizeof(struct rgui_file));
   }

   list->list[list->ptr].path = strdup(path);
   list->list[list->ptr].type = type;
   list->list[list->ptr].directory_ptr = directory_ptr;
   list->ptr++;
}

void rgui_list_pop(rgui_list_t *list)
{
   if (!rgui_list_empty(list))
      free(list->list[--list->ptr].path);
}

void rgui_list_free(rgui_list_t *list)
{
   for (size_t i = 0; i < list->ptr; i++)
      free(list->list[i].path);
   free(list->list);
   free(list);
}

void rgui_list_clear(rgui_list_t *list)
{
   for (size_t i = 0; i < list->ptr; i++)
      free(list->list[i].path);
   list->ptr = 0;
}

void rgui_list_back(const rgui_list_t *list,
      const char **path, unsigned *file_type, size_t *directory_ptr)
{
   if (rgui_list_size(list) > 0)
      rgui_list_at(list, rgui_list_size(list) - 1, path, file_type, directory_ptr);
}

size_t rgui_list_size(const rgui_list_t *list)
{
   return list->ptr;
}

void rgui_list_at(const rgui_list_t *list, size_t index,
      const char **path, unsigned *file_type, size_t *directory_ptr)
{
   if (path)
      *path = list->list[index].path;
   if (file_type)
      *file_type = list->list[index].type;
   if (directory_ptr)
      *directory_ptr = list->list[index].directory_ptr;
}

static int list_comp(const void *a_, const void *b_)
{
   const struct rgui_file *a = (const struct rgui_file*)a_;
   const struct rgui_file *b = (const struct rgui_file*)b_;

   if (a->type != b->type)
      return a->type == RGUI_FILE_DIRECTORY ? -1 : 1;

   return strcasecmp(a->path, b->path);
}

void rgui_list_sort(rgui_list_t *list)
{
   qsort(list->list, list->ptr, sizeof(struct rgui_file), list_comp);
}

