/* Copyright  (C) 2010-2018 The RetroArch team
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

/**
 * string_list_free
 * @list             : pointer to string list object
 *
 * Frees a string list.
 */
void string_list_free(struct string_list *list)
{
   size_t i;
   if (!list)
      return;

   if (list->elems)
   {
      for (i = 0; i < list->size; i++)
      {
         if (list->elems[i].data)
            free(list->elems[i].data);
         list->elems[i].data = NULL;
      }

      free(list->elems);
   }

   list->elems = NULL;
   free(list);
}

/**
 * string_list_capacity:
 * @list             : pointer to string list
 * @cap              : new capacity for string list.
 *
 * Change maximum capacity of string list's size.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool string_list_capacity(struct string_list *list, size_t cap)
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

/**
 * string_list_new:
 *
 * Creates a new string list. Has to be freed manually.
 *
 * Returns: new string list if successful, otherwise NULL.
 */
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

/**
 * string_list_append:
 * @list             : pointer to string list
 * @elem             : element to add to the string list
 * @attr             : attributes of new element.
 *
 * Appends a new element to the string list.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr)
{
   char *data_dup = NULL;

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

/**
 * string_list_append_n:
 * @list             : pointer to string list
 * @elem             : element to add to the string list
 * @length           : read at most this many bytes from elem
 * @attr             : attributes of new element.
 *
 * Appends a new element to the string list.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool string_list_append_n(struct string_list *list, const char *elem,
      unsigned length, union string_list_elem_attr attr)
{
   char *data_dup = NULL;

   if (list->size >= list->cap &&
         !string_list_capacity(list, list->cap * 2))
      return false;

   data_dup = (char*)malloc(length + 1);

   if (!data_dup)
      return false;

   strlcpy(data_dup, elem, length + 1);

   list->elems[list->size].data = data_dup;
   list->elems[list->size].attr = attr;

   list->size++;
   return true;
}

/**
 * string_list_set:
 * @list             : pointer to string list
 * @idx              : index of element in string list
 * @str              : value for the element.
 *
 * Set value of element inside string list.
 **/
void string_list_set(struct string_list *list,
      unsigned idx, const char *str)
{
   free(list->elems[idx].data);
   list->elems[idx].data = strdup(str);
}

/**
 * string_list_join_concat:
 * @buffer           : buffer that @list will be joined to.
 * @size             : length of @buffer.
 * @list             : pointer to string list.
 * @delim            : delimiter character for @list.
 *
 * A string list will be joined/concatenated as a
 * string to @buffer, delimited by @delim.
 */
void string_list_join_concat(char *buffer, size_t size,
      const struct string_list *list, const char *delim)
{
   size_t i, len = strlen(buffer);

   buffer += len;
   size   -= len;

   for (i = 0; i < list->size; i++)
   {
      strlcat(buffer, list->elems[i].data, size);
      if ((i + 1) < list->size)
         strlcat(buffer, delim, size);
   }
}

/**
 * string_split:
 * @str              : string to turn into a string list
 * @delim            : delimiter character to use for splitting the string.
 *
 * Creates a new string list based on string @str, delimited by @delim.
 *
 * Returns: new string list if successful, otherwise NULL.
 */
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

/**
 * string_list_find_elem:
 * @list             : pointer to string list
 * @elem             : element to find inside the string list.
 *
 * Searches for an element (@elem) inside the string list.
 *
 * Returns: true (1) if element could be found, otherwise false (0).
 */
int string_list_find_elem(const struct string_list *list, const char *elem)
{
   size_t i;

   if (!list)
      return false;

   for (i = 0; i < list->size; i++)
   {
      if (string_is_equal_noncase(list->elems[i].data, elem))
         return (int)(i + 1);
   }

   return false;
}

/**
 * string_list_find_elem_prefix:
 * @list             : pointer to string list
 * @prefix           : prefix to append to @elem
 * @elem             : element to find inside the string list.
 *
 * Searches for an element (@elem) inside the string list. Will
 * also search for the same element prefixed by @prefix.
 *
 * Returns: true (1) if element could be found, otherwise false (0).
 */
bool string_list_find_elem_prefix(const struct string_list *list,
      const char *prefix, const char *elem)
{
   size_t i;
   char prefixed[255];

   if (!list)
      return false;

   prefixed[0] = '\0';

   strlcpy(prefixed, prefix, sizeof(prefixed));
   strlcat(prefixed, elem,   sizeof(prefixed));

   for (i = 0; i < list->size; i++)
   {
      if (string_is_equal_noncase(list->elems[i].data, elem) ||
            string_is_equal_noncase(list->elems[i].data, prefixed))
         return true;
   }

   return false;
}

struct string_list *string_list_clone(
      const struct string_list *src)
{
   unsigned i;
   struct string_list_elem *elems = NULL;
   struct string_list *dest       = (struct string_list*)
      calloc(1, sizeof(struct string_list));

   if (!dest)
      return NULL;

   dest->size      = src->size;
   dest->cap       = src->cap;
   if (dest->cap < dest->size)
      dest->cap    = dest->size;

   elems           = (struct string_list_elem*)
      calloc(dest->cap, sizeof(struct string_list_elem));

   if (!elems)
   {
      free(dest);
      return NULL;
   }

   dest->elems            = elems;

   for (i = 0; i < src->size; i++)
   {
      const char *_src    = src->elems[i].data;
      size_t      len     = _src ? strlen(_src) : 0;

      dest->elems[i].data = NULL;
      dest->elems[i].attr = src->elems[i].attr;

      if (len != 0)
      {
         char *result        = (char*)malloc(len + 1);
         strcpy(result, _src);
         dest->elems[i].data = result;
      }
   }

   return dest;
}
