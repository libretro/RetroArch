/* Copyright  (C) 2010-2014 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (string_list.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <string/string_list.h>
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
