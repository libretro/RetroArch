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

#include <stdio.h>
#include <stdint.h>
#include "string_list.h"
#include <string.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>
#include <compat/posix_string.h>

void string_list_free(struct string_list *list)
{
   size_t i;
   if (!list)
      return;

   for (i = 0; i < list->size; i++)
      free(list->elems[i].data);
   free(list->elems);
   free(list);
}

static bool string_list_capacity(struct string_list *list, size_t cap)
{
   struct string_list_elem *new_data = NULL;
   rarch_assert(cap > list->size);

   new_data = (struct string_list_elem*)
      realloc(list->elems, cap * sizeof(*new_data));

   if (!new_data)
      return false;

   list->elems = new_data;
   list->cap   = cap;
   return true;
}

struct string_list *string_list_new(void)
{
   struct string_list *list = (struct string_list*)
      calloc(1, sizeof(*list));

   if (!list)
      return NULL;

   if (!string_list_capacity(list, 32))
   {
      string_list_free(list);
      return NULL;
   }

   return list;
}

bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr)
{
   char *data_dup;
   if (list->size >= list->cap &&
         !string_list_capacity(list, list->cap * 2))
      return false;

   data_dup = strdup(elem);
   if (!data_dup)
      return false;

   list->elems[list->size].data = data_dup;
   list->elems[list->size].attr = attr;

   list->size++;
   return true;
}

void string_list_set(struct string_list *list,
      unsigned idx, const char *str)
{
   free(list->elems[idx].data);
   rarch_assert(list->elems[idx].data = strdup(str));
}

void string_list_join_concat(char *buffer, size_t size,
      const struct string_list *list, const char *sep)
{
   size_t i, len = strlen(buffer);
   rarch_assert(len < size);
   buffer += len;
   size -= len;

   for (i = 0; i < list->size; i++)
   {
      strlcat(buffer, list->elems[i].data, size);
      if ((i + 1) < list->size)
         strlcat(buffer, sep, size);
   }
}

struct string_list *string_split(const char *str, const char *delim)
{
   char *save      = NULL;
   char *copy      = NULL;
   const char *tmp = NULL;
   struct string_list *list = string_list_new();

   if (!list)
      goto error;

   copy = strdup(str);
   if (!copy)
      goto error;

   tmp = strtok_r(copy, delim, &save);
   while (tmp)
   {
      union string_list_elem_attr attr;
      memset(&attr, 0, sizeof(attr));

      if (!string_list_append(list, tmp, attr))
         goto error;

      tmp = strtok_r(NULL, delim, &save);
   }

   free(copy);
   return list;

error:
   string_list_free(list);
   free(copy);
   return NULL;
}

bool string_list_find_elem(const struct string_list *list, const char *elem)
{
   size_t i;
   if (!list)
      return false;

   for (i = 0; i < list->size; i++)
   {
      if (strcasecmp(list->elems[i].data, elem) == 0)
         return true;
   }

   return false;
}

bool string_list_find_elem_prefix(const struct string_list *list,
      const char *prefix, const char *elem)
{
   size_t i;
   char prefixed[PATH_MAX];

   if (!list)
      return false;

   snprintf(prefixed, sizeof(prefixed), "%s%s", prefix, elem);

   for (i = 0; i < list->size; i++)
   {
      if (strcasecmp(list->elems[i].data, elem) == 0 ||
            strcasecmp(list->elems[i].data, prefixed) == 0)
         return true;
   }

   return false;
}
