/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_list.c).
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
#include <stdlib.h>
#include <string.h>

#include <retro_common.h>
#include <lists/file_list.h>
#include <string/stdstring.h>
#include <compat/strcasestr.h>

static bool file_list_deinitialize_internal(file_list_t *list)
{
   size_t i;
   for (i = 0; i < list->size; i++)
   {
      file_list_free_userdata(list, i);
      file_list_free_actiondata(list, i);

      if (list->list[i].path)
         free(list->list[i].path);
      list->list[i].path = NULL;

      if (list->list[i].label)
         free(list->list[i].label);
      list->list[i].label = NULL;

      if (list->list[i].alt)
         free(list->list[i].alt);
      list->list[i].alt = NULL;
   }
   if (list->list)
      free(list->list);
   list->list = NULL;
   return true;
}

bool file_list_reserve(file_list_t *list, size_t nitems)
{
   const size_t item_size = sizeof(struct item_file);
   struct item_file *new_data;

   if (nitems < list->capacity || nitems > (size_t)-1/item_size)
      return false;

   if (!(new_data = (struct item_file*)realloc(list->list, nitems * item_size)))
      return false;

   memset(&new_data[list->capacity], 0, item_size * (nitems - list->capacity));

   list->list     = new_data;
   list->capacity = nitems;

   return true;
}

bool file_list_insert(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx,
      size_t idx)
{
   int i;

   /* Expand file list if needed */
   if (list->size >= list->capacity)
      if (!file_list_reserve(list, list->capacity * 2 + 1))
         return false;

   for (i = (unsigned)list->size; i > (int)idx; i--)
   {
      struct item_file *copy = (struct item_file*)
         malloc(sizeof(struct item_file));

      if (copy)
      {
         copy->path             = NULL;
         copy->label            = NULL;
         copy->alt              = NULL;
         copy->type             = 0;
         copy->directory_ptr    = 0;
         copy->entry_idx        = 0;
         copy->userdata         = NULL;
         copy->actiondata       = NULL;

         memcpy(copy, &list->list[i-1], sizeof(struct item_file));

         memcpy(&list->list[i-1], &list->list[i], sizeof(struct item_file));
         memcpy(&list->list[i],             copy, sizeof(struct item_file));

         free(copy);
      }
   }

   list->list[idx].path          = NULL;
   list->list[idx].label         = NULL;
   list->list[idx].alt           = NULL;
   list->list[idx].type          = type;
   list->list[idx].directory_ptr = directory_ptr;
   list->list[idx].entry_idx     = entry_idx;
   list->list[idx].userdata      = NULL;
   list->list[idx].actiondata    = NULL;

   if (label)
      list->list[idx].label      = strdup(label);
   if (path)
      list->list[idx].path       = strdup(path);

   list->size++;

   return true;
}

bool file_list_append(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx)
{
   unsigned idx = (unsigned)list->size;
   /* Expand file list if needed */
   if (idx >= list->capacity)
      if (!file_list_reserve(list, list->capacity * 2 + 1))
         return false;

   list->list[idx].path          = NULL;
   list->list[idx].label         = NULL;
   list->list[idx].alt           = NULL;
   list->list[idx].type          = type;
   list->list[idx].directory_ptr = directory_ptr;
   list->list[idx].entry_idx     = entry_idx;
   list->list[idx].userdata      = NULL;
   list->list[idx].actiondata    = NULL;

   if (label)
      list->list[idx].label      = strdup(label);
   if (path)
      list->list[idx].path       = strdup(path);

   list->size++;

   return true;
}

void file_list_pop(file_list_t *list, size_t *directory_ptr)
{
   if (!list)
      return;

   if (list->size != 0)
   {
      --list->size;
      if (list->list[list->size].path)
         free(list->list[list->size].path);
      list->list[list->size].path = NULL;

      if (list->list[list->size].label)
         free(list->list[list->size].label);
      list->list[list->size].label = NULL;
   }

   if (directory_ptr)
      *directory_ptr = list->list[list->size].directory_ptr;
}

void file_list_free(file_list_t *list)
{
   if (!list)
      return;
   file_list_deinitialize_internal(list);
   free(list);
}

bool file_list_deinitialize(file_list_t *list)
{
   if (!list)
      return false;
   if (!file_list_deinitialize_internal(list))
      return false;
   list->capacity = 0;
   list->size     = 0;
   return true;
}

void file_list_clear(file_list_t *list)
{
   size_t i;

   if (!list)
      return;

   for (i = 0; i < list->size; i++)
   {
      if (list->list[i].path)
         free(list->list[i].path);
      list->list[i].path = NULL;

      if (list->list[i].label)
         free(list->list[i].label);
      list->list[i].label = NULL;

      if (list->list[i].alt)
         free(list->list[i].alt);
      list->list[i].alt = NULL;
   }

   list->size = 0;
}

static void file_list_get_label_at_offset(const file_list_t *list, size_t idx,
      const char **label)
{
   if (!label || !list)
      return;

   *label = list->list[idx].path;
   if (list->list[idx].label)
      *label = list->list[idx].label;
}

void file_list_set_alt_at_offset(file_list_t *list, size_t idx,
      const char *alt)
{
   if (!list || !alt)
      return;
   if (list->list[idx].alt)
      free(list->list[idx].alt);
   list->list[idx].alt   = strdup(alt);
}

static int file_list_alt_cmp(const void *a_, const void *b_)
{
   const struct item_file *a = (const struct item_file*)a_;
   const struct item_file *b = (const struct item_file*)b_;
   const char *cmp_a         = a->alt ? a->alt : a->path;
   const char *cmp_b         = b->alt ? b->alt : b->path;
   return strcasecmp(cmp_a, cmp_b);
}

static int file_list_type_cmp(const void *a_, const void *b_)
{
   const struct item_file *a = (const struct item_file*)a_;
   const struct item_file *b = (const struct item_file*)b_;
   if (a->type < b->type)
      return -1;
   if (a->type == b->type)
      return 0;

   return 1;
}

void file_list_sort_on_alt(file_list_t *list)
{
   qsort(list->list, list->size, sizeof(list->list[0]), file_list_alt_cmp);
}

void file_list_sort_on_type(file_list_t *list)
{
   qsort(list->list, list->size, sizeof(list->list[0]), file_list_type_cmp);
}

void *file_list_get_userdata_at_offset(const file_list_t *list, size_t idx)
{
   if (!list)
      return NULL;
   return list->list[idx].userdata;
}

void *file_list_get_actiondata_at_offset(const file_list_t *list, size_t idx)
{
   if (!list)
      return NULL;
   return list->list[idx].actiondata;
}

void file_list_free_actiondata(const file_list_t *list, size_t idx)
{
   if (!list)
      return;
   if (list->list[idx].actiondata)
       free(list->list[idx].actiondata);
   list->list[idx].actiondata = NULL;
}

void file_list_free_userdata(const file_list_t *list, size_t idx)
{
   if (!list)
      return;
   if (list->list[idx].userdata)
       free(list->list[idx].userdata);
   list->list[idx].userdata = NULL;
}

bool file_list_search(const file_list_t *list, const char *needle, size_t *idx)
{
   size_t i;
   bool ret        = false;

   if (!list)
      return false;

   for (i = 0; i < list->size; i++)
   {
      const char *str = NULL;
      const char *alt = list->list[i].alt
            ? list->list[i].alt
            : list->list[i].path;

      if (!alt)
      {
         file_list_get_label_at_offset(list, i, &alt);
         if (!alt)
            continue;
      }

      if ((str = (const char *)strcasestr(alt, needle)) == alt)
      {
         /* Found match with first chars, best possible match. */
         *idx = i;
         ret  = true;
         break;
      }
      else if (str && !ret)
      {
         /* Found mid-string match, but try to find a match with
          * first characters before we settle. */
         *idx = i;
         ret  = true;
      }
   }

   return ret;
}
