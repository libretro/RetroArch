/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (string_list.h).
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

#ifndef __LIBRETRO_SDK_STRING_LIST_H
#define __LIBRETRO_SDK_STRING_LIST_H

#include <retro_common_api.h>

#include <boolean.h>
#include <stdlib.h>
#include <stddef.h>

RETRO_BEGIN_DECLS

union string_list_elem_attr
{
   bool  b;
   int   i;
   void *p;
};

struct string_list_elem
{
   char *data;
   void *userdata;
   union string_list_elem_attr attr;
};

struct string_list
{
   struct string_list_elem *elems;
   size_t size;
   size_t cap;
};

/**
 * string_list_find_elem:
 * @list             : pointer to string list
 * @elem             : element to find inside the string list.
 *
 * Searches for an element (@elem) inside the string list.
 *
 * @return Number of elements found, otherwise 0.
 */
int string_list_find_elem(const struct string_list *list, const char *elem);

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
      const char *prefix, const char *elem);

/**
 * string_split:
 * @str              : string to turn into a string list
 * @delim            : delimiter character to use for splitting the string.
 *
 * Creates a new string list based on string @str, delimited by @delim.
 *
 * Returns: new string list if successful, otherwise NULL.
 */
struct string_list *string_split(const char *str, const char *delim);

bool string_split_noalloc(struct string_list *list,
      const char *str, const char *delim);

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
struct string_list *string_separate(char *str, const char *delim);

bool string_separate_noalloc(struct string_list *list, 
      char *str, const char *delim);

bool string_list_deinitialize(struct string_list *list);

bool string_list_initialize(struct string_list *list);

/**
 * string_list_new:
 *
 * Creates a new string list. Has to be freed manually.
 *
 * @return New string list if successful, otherwise NULL.
 **/
struct string_list *string_list_new(void);

/**
 * string_list_append:
 * @list             : pointer to string list
 * @elem             : element to add to the string list
 * @attr             : attributes of new element.
 *
 * Appends a new element to the string list.

 * Hidden non-leaf function cost:
 * - Calls string_list_capacity()
 * - Calls strdup
 *
 * @return true if successful, otherwise false.
 **/
bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr);

/**
 * string_list_append_n:
 * @list             : pointer to string list
 * @elem             : element to add to the string list
 * @length           : read at most this many bytes from elem
 * @attr             : attributes of new element.
 *
 * Appends a new element to the string list.
 *
 * Hidden non-leaf function cost:
 * - Calls string_list_capacity()
 * - Calls malloc
 * - Calls strlcpy
 *
 * @return true if successful, otherwise false.
 **/
bool string_list_append_n(struct string_list *list, const char *elem,
      unsigned length, union string_list_elem_attr attr);

/**
 * string_list_free
 * @list             : pointer to string list object
 *
 * Frees a string list.
 **/
void string_list_free(struct string_list *list);

/**
 * string_list_join_concat:
 * @buffer           : buffer that @list will be joined to.
 * @size             : length of @buffer.
 * @list             : pointer to string list.
 * @delim            : delimiter character for @list.
 *
 * A string list will be joined/concatenated as a
 * string to @buffer, delimited by @delim.
 *
 * NOTE: @buffer must be NULL-terminated.
 *
 * Hidden non-leaf function cost:
 * - Calls strlen_size()
 * - Calls strlcat x times in a loop
 **/
void string_list_join_concat(char *buffer, size_t size,
      const struct string_list *list, const char *sep);

/**
 * string_list_set:
 * @list             : pointer to string list
 * @idx              : index of element in string list
 * @str              : value for the element.
 *
 * Set value of element inside string list.
 *
 * Hidden non-leaf function cost:
 * - Calls free
 * - Calls strdup
 **/
void string_list_set(struct string_list *list, unsigned idx,
      const char *str);

struct string_list *string_list_clone(const struct string_list *src);

RETRO_END_DECLS

#endif
