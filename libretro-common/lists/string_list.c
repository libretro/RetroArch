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

/**
 * string_list_capacity:
 * @list             : pointer to string list
 * @cap              : new capacity for string list.
 *
 * Change maximum capacity of string list's size.
 *
 * @return true if successful, otherwise false.
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
 * string_list_free
 * @list             : pointer to string list object
 *
 * Frees a string list.
 **/
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

/**
 * string_list_new:
 *
 * Creates a new string list. Has to be freed manually.
 *
 * @return New string list if successful, otherwise NULL.
 **/
struct string_list *string_list_new(void)
{
   struct string_list_elem *
      elems                 = NULL;
   struct string_list *list = (struct string_list*)
      malloc(sizeof(*list));
   if (!list)
      return NULL;

   if (!(elems = (struct string_list_elem*)
      calloc(32, sizeof(*elems))))
   {
      string_list_free(list);
      return NULL;
   }

   list->elems              = elems;
   list->size               = 0;
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

/**
 * string_list_append:
 * @list             : pointer to string list
 * @elem             : element to add to the string list
 * @attr             : attributes of new element.
 *
 * Appends a new element to the string list.
 *
 * @return true if successful, otherwise false.
 **/
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
   if (list->size >= list->cap &&
         !string_list_capacity(list,
               (list->cap > 0) ? (list->cap * 2) : 32))
      return false;

   if (!(data_dup = strdup(elem)))
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
 * @return true if successful, otherwise false.
 **/
bool string_list_append_n(struct string_list *list, const char *elem,
      unsigned length, union string_list_elem_attr attr)
{
   char *data_dup = NULL;

   if (list->size >= list->cap &&
         !string_list_capacity(list, list->cap * 2))
      return false;

   if (!(data_dup = (char*)malloc(length + 1)))
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
 * @s                : buffer that @list will be joined to.
 * @len              : length of @s.
 * @list             : pointer to string list.
 * @delim            : delimiter character for @list.
 *
 * A string list will be joined/concatenated as a
 * string to @buffer, delimited by @delim.
 **/
void string_list_join_concat(char *s, size_t len,
      const struct string_list *list, const char *delim)
{
   size_t i;
   size_t _len = strlen_size(s, len);

   /* If @s is already 'full', nothing
    * further can be added
    * > This condition will also be triggered
    *   if @s is not NULL-terminated,
    *   in which case any attempt to increment
    *   @s or decrement @len would lead to
    *   undefined behaviour */
   if (_len >= len)
      return;

   s      += _len;
   len    -= _len;

   for (i = 0; i < list->size; i++)
   {
      size_t __len = strlcat(s, list->elems[i].data, len);
      if ((i + 1) < list->size)
         strlcpy(s + __len, delim, len - __len);
   }
}

/**
 * string_list_join_concat:
 * @s                : buffer that @list will be joined to.
 * @len              : length of @s.
 * @list             : pointer to string list.
 * @delim            : delimiter character for @list.
 *
 * Specialized version of string_list_join_concat
 * without the bounds check.
 *
 * A string list will be joined/concatenated as a
 * string to @s, delimited by @delim.
 *
 * TODO/FIXME - eliminate the strlcat
 **/
void string_list_join_concat_special(char *s, size_t len,
      const struct string_list *list, const char *delim)
{
   size_t i;

   for (i = 0; i < list->size; i++)
   {
      size_t __len = strlcat(s, list->elems[i].data, len);
      if ((i + 1) < list->size)
         strlcpy(s + __len, delim, len - __len);
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

/**
 * string_separate:
 * @str              : string to turn into a string list
 * @delim            : delimiter character to use for separating the string.
 *
 * Creates a new string list based on string @str, delimited by @delim.
 * Includes empty strings - i.e. two adjacent delimiters will resolve
 * to a string list element of "".
 *
 * @return New string list if successful, otherwise NULL.
 **/
struct string_list *string_separate(char *str, const char *delim)
{
   char *token              = NULL;
   char **str_ptr           = NULL;
   struct string_list *list = NULL;

   /* Sanity check */
   if (!str || string_is_empty(delim))
      return NULL;
   if (!(list = string_list_new()))
	   return NULL;

   str_ptr = &str;
   token   = string_tokenize(str_ptr, delim);

   while (token)
   {
      union string_list_elem_attr attr;

      attr.i = 0;

      if (!string_list_append(list, token, attr))
      {
         free(token);
         string_list_free(list);
         return NULL;
      }

      free(token);
      token = string_tokenize(str_ptr, delim);
   }

   return list;
}

bool string_separate_noalloc(
      struct string_list *list,
      char *str, const char *delim)
{
   char *token              = NULL;
   char **str_ptr           = NULL;

   /* Sanity check */
   if (!str || string_is_empty(delim) || !list)
      return false;

   str_ptr = &str;
   token   = string_tokenize(str_ptr, delim);

   while (token)
   {
      union string_list_elem_attr attr;

      attr.i = 0;

      if (!string_list_append(list, token, attr))
      {
         free(token);
         return false;
      }

      free(token);
      token = string_tokenize(str_ptr, delim);
   }

   return true;
}

/**
 * string_list_find_elem:
 *
 * @param list
 * Pointer to string list
 * @param elem
 * Element to find inside the string list.
 *
 * Searches for an element (@elem) inside the string list.
 *
 * @return Number of elements found, otherwise 0.
 */
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

/**
 * string_list_find_elem_prefix:
 *
 * @param list
 * Pointer to string list
 * @param prefix
 * Prefix to append to @elem
 * @param elem
 * Element to find inside the string list.
 *
 * Searches for an element (@elem) inside the string list. Will
 * also search for the same element prefixed by @prefix.
 *
 * @return true if element could be found, otherwise false.
 */
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
      const char *_src       = src->elems[i].data;
      size_t      len        = _src ? strlen(_src) : 0;

      dest->elems[i].data    = NULL;
      dest->elems[i].attr    = src->elems[i].attr;

      if (len != 0)
      {
         char *result        = (char*)malloc(len + 1);
         if (result)
         {
            strcpy(result, _src);
            dest->elems[i].data = result;
         }
      }
   }

   return dest;
}
