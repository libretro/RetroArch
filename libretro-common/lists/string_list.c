/* Copyright  (C) 2010-2020 The RetroArch team
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
#include <string.h>

#include <lists/string_list.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <string/stdstring.h>

static bool string_list_deinitialize_internal(struct string_list *list)
{
   if (!list)
      return false;

   if (list->elems)
   {
      unsigned i;
      for (i = 0; i < (unsigned)list->size; i++)
      {
         if (list->elems[i].data)
            free(list->elems[i].data);
         if (list->elems[i].userdata)
            free(list->elems[i].userdata);
         list->elems[i].data     = NULL;
         list->elems[i].userdata = NULL;
      }

      free(list->elems);
   }

   list->elems = NULL;

   return true;
}

bool string_list_capacity(struct string_list *list, size_t cap)
{
   struct string_list_elem *new_data = (struct string_list_elem*)
      realloc(list->elems, cap * sizeof(*new_data));

   if (!new_data)
      return false;

   if (cap > list->cap)
      memset(&new_data[list->cap], 0, sizeof(*new_data) * (cap - list->cap));

   list->elems = new_data;
   list->cap   = cap;
   return true;
}

void string_list_free(struct string_list *list)
{
   if (!list)
      return;

   string_list_deinitialize_internal(list);

   free(list);
}

bool string_list_deinitialize(struct string_list *list)
{
   if (!list)
      return false;
   if (!string_list_deinitialize_internal(list))
      return false;
   list->elems              = NULL;
   list->size               = 0;
   list->cap                = 0;
   return true;
}

struct string_list *string_list_new(void)
{
   struct string_list_elem *
      elems                 = NULL;
   struct string_list *list = (struct string_list*)
      malloc(sizeof(*list));
   if (!list)
      return NULL;

   list->cap                = 0;
   list->size               = 0;
   list->elems              = NULL;

   if (!(elems = (struct string_list_elem*)
      calloc(32, sizeof(*elems))))
   {
      string_list_free(list);
      return NULL;
   }

   list->elems              = elems;
   list->cap                = 32;

   return list;
}

bool string_list_initialize(struct string_list *list)
{
   struct string_list_elem *
      elems                 = NULL;
   if (!list)
      return false;
   if (!(elems = (struct string_list_elem*)
      calloc(32, sizeof(*elems))))
   {
      string_list_deinitialize(list);
      return false;
   }
   list->elems              = elems;
   list->size               = 0;
   list->cap                = 32;
   return true;
}

bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr)
{
   char *data_dup = NULL;

   /* Note: If 'list' is incorrectly initialised
    * (i.e. if struct is zero initialised and
    * string_list_initialize() is not called on
    * it) capacity will be zero. This will cause
    * a segfault. Handle this case by forcing the new
    * capacity to a fixed size of 32 */
   if (      list->size >= list->cap
         && !string_list_capacity(list,
               (list->cap > 0) ? (list->cap * 2) : 32))
      return false;

   if (!(data_dup = strdup(elem)))
      return false;

   list->elems[list->size].data = data_dup;
   list->elems[list->size].attr = attr;

   list->size++;
   return true;
}

void string_list_join_concat(char *s, size_t len,
      const struct string_list *list, const char *delim)
{
   size_t _len = strlen(s);

   /* If @s is already 'full', nothing
    * further can be added
    * > This condition will also be triggered
    *   if @s is not NULL-terminated,
    *   in which case any attempt to increment
    *   @s or decrement @len would lead to
    *   undefined behaviour */
   if (_len < len)
   {
      size_t i;
      for (i = 0; i < list->size; i++)
      {
         _len += strlcpy(s + _len, list->elems[i].data, len - _len);
         if ((i + 1) < list->size)
            _len += strlcpy(s + _len, delim, len - _len);
      }
   }
}

void string_list_join_concat_special(char *s, size_t len,
      const struct string_list *list, const char *delim)
{
   size_t i;
   size_t _len = strlen(s);
   for (i = 0; i < list->size; i++)
   {
      _len += strlcpy(s + _len, list->elems[i].data, len - _len);
      if ((i + 1) < list->size)
         _len += strlcpy(s + _len, delim, len - _len);
   }
}

struct string_list *string_split(const char *str, const char *delim)
{
   char *save      = NULL;
   char *copy      = NULL;
   const char *tmp = NULL;
   struct string_list *list = string_list_new();

   if (!list)
      return NULL;

   if (!(copy = strdup(str)))
      goto error;

   tmp = strtok_r(copy, delim, &save);
   while (tmp)
   {
      union string_list_elem_attr attr;

      attr.i = 0;

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

bool string_split_noalloc(struct string_list *list,
      const char *str, const char *delim)
{
   char *save      = NULL;
   char *copy      = NULL;
   const char *tmp = NULL;

   if (!list)
      return false;

   if (!(copy = strdup(str)))
      return false;

   tmp             = strtok_r(copy, delim, &save);
   while (tmp)
   {
      union string_list_elem_attr attr;

      attr.i = 0;

      if (!string_list_append(list, tmp, attr))
      {
         free(copy);
         return false;
      }

      tmp = strtok_r(NULL, delim, &save);
   }

   free(copy);
   return true;
}

int string_list_find_elem(const struct string_list *list, const char *elem)
{
   if (list)
   {
      size_t i;
      for (i = 0; i < list->size; i++)
      {
         if (string_is_equal_noncase(list->elems[i].data, elem))
            return (int)(i + 1);
      }
   }
   return 0;
}

bool string_list_find_elem_prefix(const struct string_list *list,
      const char *prefix, const char *elem)
{
   if (list)
   {
      size_t i;
      char prefixed[255];
      size_t _len  = strlcpy(prefixed, prefix, sizeof(prefixed));
      strlcpy(prefixed + _len, elem, sizeof(prefixed) - _len);
      for (i = 0; i < list->size; i++)
      {
         if (     string_is_equal_noncase(list->elems[i].data, elem)
               || string_is_equal_noncase(list->elems[i].data, prefixed))
            return true;
      }
   }
   return false;
}

struct string_list *string_list_clone(const struct string_list *src)
{
   size_t i;
   struct string_list_elem *elems = NULL;
   struct string_list *dest       = (struct string_list*)
      malloc(sizeof(struct string_list));

   if (!dest)
      return NULL;

   dest->elems               = NULL;
   dest->size                = src->size;
   if (src->cap < dest->size)
      dest->cap              = dest->size;
   else
      dest->cap              = src->cap;

   if (!(elems = (struct string_list_elem*)
      calloc(dest->cap, sizeof(struct string_list_elem))))
   {
      free(dest);
      return NULL;
   }

   dest->elems               = elems;

   for (i = 0; i < src->size; i++)
   {
      const char *_src    = src->elems[i].data;
      size_t      len     = _src ? strlen(_src) : 0;

      dest->elems[i].data = NULL;
      dest->elems[i].attr = src->elems[i].attr;

      if (len != 0)
      {
         char *ret        = (char*)malloc(len + 1);
         if (ret)
         {
            strcpy(ret, _src);
            dest->elems[i].data = ret;
         }
      }
   }

   return dest;
}
