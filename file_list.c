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
#include "compat/strcasestr.h"
#ifdef _MSC_VER
#include "msvc/msvc_compat.h"
#endif

struct item_file
{
   char *path;
   char *alt;
   unsigned type;
   size_t directory_ptr;
};

void file_list_push(file_list_t *list,
      const char *path, unsigned type, size_t directory_ptr)
{
   if (!list)
      return;

   if (list->size >= list->capacity)
   {
      list->capacity++;
      list->capacity *= 2;
      list->list = (struct item_file*)realloc(list->list, list->capacity * sizeof(struct item_file));
   }

   list->list[list->size].path = strdup(path);
   list->list[list->size].alt = NULL;
   list->list[list->size].type = type;
   list->list[list->size].directory_ptr = directory_ptr;
   list->size++;
}

void file_list_pop(file_list_t *list, size_t *directory_ptr)
{
   if (!(list->size == 0))
      free(list->list[--list->size].path);

   if (directory_ptr)
      *directory_ptr = list->list[list->size].directory_ptr;
}

void file_list_free(file_list_t *list)
{
   size_t i;
   for (i = 0; i < list->size; i++)
      free(list->list[i].path);
   free(list->list);
   free(list);
}

void file_list_clear(file_list_t *list)
{
   size_t i;
   for (i = 0; i < list->size; i++)
   {
      free(list->list[i].path);
      free(list->list[i].alt);
   }
   list->size = 0;
}

void file_list_set_alt_at_offset(file_list_t *list, size_t index,
      const char *alt)
{
   free(list->list[index].alt);
   list->list[index].alt = strdup(alt);
}

void file_list_get_alt_at_offset(const file_list_t *list, size_t index,
      const char **alt)
{
   if (alt)
      *alt = list->list[index].alt ? list->list[index].alt : list->list[index].path;
}

static int file_list_alt_cmp(const void *a_, const void *b_)
{
   const struct item_file *a = (const struct item_file*)a_;
   const struct item_file *b = (const struct item_file*)b_;
   const char *cmp_a = a->alt ? a->alt : a->path;
   const char *cmp_b = b->alt ? b->alt : b->path;
   return strcasecmp(cmp_a, cmp_b);
}

void file_list_sort_on_alt(file_list_t *list)
{
   qsort(list->list, list->size, sizeof(list->list[0]), file_list_alt_cmp);
}

void file_list_get_at_offset(const file_list_t *list, size_t index,
      const char **path, unsigned *file_type)
{
   if (path)
      *path = list->list[index].path;
   if (file_type)
      *file_type = list->list[index].type;
}

void file_list_get_last(const file_list_t *list,
      const char **path, unsigned *file_type)
{
   if (list->size)
      file_list_get_at_offset(list, list->size - 1, path, file_type);
}

bool file_list_search(const file_list_t *list, const char *needle, size_t *index)
{
   size_t i;
   const char *alt;
   bool ret = false;
   for (i = 0; i < list->size; i++)
   {
      file_list_get_alt_at_offset(list, i, &alt);
      if (!alt)
         continue;

      const char *str = strcasestr(alt, needle);
      if (str == alt) // Found match with first chars, best possible match.
      {
         *index = i;
         ret = true;
         break;
      }
      else if (str && !ret) // Found mid-string match, but try to find a match with first chars before we settle.
      {
         *index = i;
         ret = true;
      }
   }

   return ret;
}

