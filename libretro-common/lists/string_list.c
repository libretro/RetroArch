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

#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include <lists/string_list.h>
#include <compat/strl.h>
#include <compat/posix_string.h>

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
      memset(&new_data[list->cap], 0,
            sizeof(*new_data) * (cap - list->cap));

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
   list->elems = NULL;
   list->size  = 0;
   list->cap   = 0;
   return true;
}

struct string_list *string_list_new(void)
{
   struct string_list_elem *elems = NULL;
   struct string_list *list = (struct string_list*)
      malloc(sizeof(*list));
   if (!list)
      return NULL;

   list->cap  = 0;
   list->size = 0;
   list->elems = NULL;

   elems = (struct string_list_elem*)
      calloc(32, sizeof(*elems));
   if (!elems)
   {
      free(list);
      return NULL;
   }

   list->elems = elems;
   list->cap   = 32;

   return list;
}

bool string_list_initialize(struct string_list *list)
{
   struct string_list_elem *elems = NULL;
   if (!list)
      return false;

   elems = (struct string_list_elem*)
      calloc(32, sizeof(*elems));
   if (!elems)
   {
      string_list_deinitialize(list);
      return false;
   }

   list->elems = elems;
   list->size  = 0;
   list->cap   = 32;
   return true;
}

bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr)
{
   char *data_dup = NULL;

   if (      list->size >= list->cap
         && !string_list_capacity(list,
               (list->cap > 0) ? (list->cap * 2) : 32))
      return false;

   data_dup = strdup(elem);
   if (!data_dup)
      return false;

   list->elems[list->size].data = data_dup;
   list->elems[list->size].attr = attr;
   list->size++;

   return true;
}

bool string_list_append_n(struct string_list *list, const char *elem,
      size_t len, union string_list_elem_attr attr)
{
   char *data_dup = NULL;

   if (      list->size >= list->cap
         && !string_list_capacity(list,
               (list->cap > 0) ? (list->cap * 2) : 32))
      return false;

   data_dup = (char*)malloc(len + 1);
   if (!data_dup)
      return false;
   memcpy(data_dup, elem, len);
   data_dup[len] = '\0';

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

/**
 * Count delimited tokens in @str without modifying it.
 * Treats any character in @delim as a separator (same as strtok).
 * Returns the number of non-empty tokens.
 */
static size_t string_count_tokens(const char *str, const char *delim)
{
   size_t count   = 0;
   bool   in_tok  = false;
   const char *p  = str;

   for (; *p != '\0'; p++)
   {
      if (strchr(delim, (unsigned char)*p))
         in_tok = false;
      else if (!in_tok)
      {
         in_tok = true;
         count++;
      }
   }

   return count;
}

struct string_list *string_split(const char *str, const char *delim)
{
   const char *p = str;
   struct string_list *list = string_list_new();
   if (!list)
      return NULL;

   while (*p)
   {
      union string_list_elem_attr attr;
      const char *tok;

      while (*p && strchr(delim, *p))
         p++;
      if (!*p)
         break;

      tok = p;
      while (*p && !strchr(delim, *p))
         p++;

      attr.i = 0;
      if (!string_list_append_n(list, tok, p - tok, attr))
         goto error;
   }

   return list;

error:
   string_list_free(list);
   return NULL;
}

bool string_split_noalloc(struct string_list *list,
      const char *str, const char *delim)
{
   size_t _len;
   const char *end;
   const char *p   = str;

   if (!list || !str || !delim || !*delim)
      return false;

   /* Compute delimiter length once. */
   {
      const char *d = delim;
      while (*d)
         d++;
      _len = d - delim;
   }

   /* Pre-size to avoid repeated reallocs. */
   {
      size_t __len = string_count_tokens(str, delim);
      if (__len > list->cap)
      {
         if (!string_list_capacity(list, __len))
            return false;
      }
   }

   while (*p)
   {
      union string_list_elem_attr attr;
      size_t __len;

      attr.i = 0;
      end    = strstr(p, delim);

      if (end)
      {
         __len = end - p;
         if (__len > 0)
         {
            if (!string_list_append_n(list, p, __len, attr))
               return false;
         }
         p = end + _len;
      }
      else
      {
         const char *s = p;
         while (*s)
            s++;
         __len = s - p;
         if (__len > 0)
         {
            if (!string_list_append_n(list, p, __len, attr))
               return false;
         }
         break;
      }
   }

   return true;
}

int string_list_find_elem(const struct string_list *list, const char *elem)
{
   if (list && elem)
   {
      size_t i;
      for (i = 0; i < list->size; i++)
      {
         const unsigned char *p1 = (const unsigned char*)list->elems[i].data;
         const unsigned char *p2 = (const unsigned char*)elem;
         while ((*p1 | 32) == (*p2 | 32) || (*p1 == *p2))
         {
            if (*p1 == '\0')
               return (int)(i + 1);
            p1++;
            p2++;
         }
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
      size_t _len = strlcpy(prefixed, prefix, sizeof(prefixed));
      strlcpy(prefixed + _len, elem, sizeof(prefixed) - _len);
      for (i = 0; i < list->size; i++)
      {
         const char *data = list->elems[i].data;
         const char *a    = data;
         const char *b    = elem;
         while (tolower((unsigned char)*a) == tolower((unsigned char)*b))
         {
            if (*a == '\0')
               return true;
            a++;
            b++;
         }

         a = data;
         b = prefixed;
         while (tolower((unsigned char)*a) == tolower((unsigned char)*b))
         {
            if (*a == '\0')
               return true;
            a++;
            b++;
         }
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

   dest->elems = NULL;
   dest->size  = src->size;
   dest->cap   = (src->cap < dest->size) ? dest->size : src->cap;

   elems = (struct string_list_elem*)
      calloc(dest->cap, sizeof(struct string_list_elem));
   if (!elems)
   {
      free(dest);
      return NULL;
   }

   dest->elems = elems;

   for (i = 0; i < src->size; i++)
   {
      const char *_src = src->elems[i].data;
      size_t      slen = _src ? strlen(_src) : 0;

      dest->elems[i].data = NULL;
      dest->elems[i].attr = src->elems[i].attr;

      if (slen != 0)
      {
         char *ret = (char*)malloc(slen + 1);
         if (ret)
         {
            memcpy(ret, _src, slen + 1);  /* memcpy > strcpy: no NUL scan */
            dest->elems[i].data = ret;
         }
      }
   }

   return dest;
}
