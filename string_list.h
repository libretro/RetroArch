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

#ifndef __RARCH_STRING_LIST_H
#define __RARCH_STRING_LIST_H

#include "boolean.h"

#ifdef __cplusplus
extern "C" {
#endif

union string_list_elem_attr
{
   bool  b;
   int   i;
   void *p;
};

struct string_list_elem
{
   char *data;
   union string_list_elem_attr attr;
};

struct string_list
{
   struct string_list_elem *elems;
   size_t size;
   size_t cap;
};

bool string_list_find_elem(const struct string_list *list, const char *elem);

bool string_list_find_elem_prefix(const struct string_list *list,
      const char *prefix, const char *elem);

struct string_list *string_split(const char *str, const char *delim);

struct string_list *string_list_new(void);

bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr);

void string_list_free(struct string_list *list);

void string_list_join_concat(char *buffer, size_t size,
      const struct string_list *list, const char *sep);

void string_list_set(struct string_list *list, unsigned index,
      const char *str);

#ifdef __cplusplus
}
#endif

#endif
