/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *

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

#include "list.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct sgui_file
{
   char *path;
   sgui_file_type_t type;
};

struct sgui_list
{
   struct sgui_file *list;

   size_t capacity;
   size_t ptr;
};

sgui_list_t *sgui_list_new(void)
{
   return (sgui_list_t*)calloc(1, sizeof(sgui_list_t));
}

bool sgui_list_empty(const sgui_list_t *list)
{
   return list->ptr == 0;
}

void sgui_list_push(sgui_list_t *list, const char *path, sgui_file_type_t type)
{
   if (list->ptr >= list->capacity)
   {
      list->capacity++;
      list->capacity *= 2;
      list->list = (struct sgui_file*)realloc(list->list, list->capacity * sizeof(struct sgui_file));
   }

   list->list[list->ptr].path = strdup(path);
   list->list[list->ptr].type = type;
   list->ptr++;
}

void sgui_list_pop(sgui_list_t *list)
{
   if (!sgui_list_empty(list))
      free(list->list[--list->ptr].path);
}

void sgui_list_free(sgui_list_t *list)
{
   for (size_t i = 0; i < list->ptr; i++)
      free(list->list[i].path);
   free(list->list);
   free(list);
}

void sgui_list_clear(sgui_list_t *list)
{
   for (size_t i = 0; i < list->ptr; i++)
      free(list->list[i].path);
   list->ptr = 0;
}

void sgui_list_back(const sgui_list_t *list,
      const char **path, sgui_file_type_t *file_type)
{
   if (sgui_list_size(list) > 0)
      sgui_list_at(list, sgui_list_size(list) - 1, path, file_type);
}

size_t sgui_list_size(const sgui_list_t *list)
{
   return list->ptr;
}

void sgui_list_at(const sgui_list_t *list, size_t index,
      const char **path, sgui_file_type_t *file_type)
{
   if (path)
      *path = list->list[index].path;
   if (file_type)
      *file_type = list->list[index].type;
}

static int list_comp(const void *a_, const void *b_)
{
   const struct sgui_file *a = (const struct sgui_file*)a_;
   const struct sgui_file *b = (const struct sgui_file*)b_;

   if (a->type != b->type)
      return a->type == SGUI_FILE_DIRECTORY ? -1 : 1;

   return strcmp(a->path, b->path);
}

void sgui_list_sort(sgui_list_t *list)
{
   qsort(list->list, list->ptr, sizeof(struct sgui_file), list_comp);
}

