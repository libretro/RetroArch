/* Copyright  (C) 2010-2017 The RetroArch team
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

bool file_list_reserve(file_list_t *list, size_t nitems)
{
   const size_t item_size = sizeof(struct item_file);
   struct item_file *new_data;

   if (nitems < list->capacity || nitems > (size_t)-1/item_size)
      return false;

   new_data = (struct item_file*)realloc(list->list, nitems * item_size);

   if (new_data)
   {
      memset(&new_data[list->capacity], 0, item_size * (nitems - list->capacity));

      list->list     = new_data;
      list->capacity = nitems;
   }

   return new_data != NULL;
}

static void file_list_add(file_list_t *list, unsigned idx,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx)
{
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
}

static bool file_list_expand_if_needed(file_list_t *list)
{
   if (list->size >= list->capacity)
      return file_list_reserve(list, list->capacity * 2 + 1);

   return true;
}

bool file_list_prepend(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx)
{
   unsigned i;

   if (!file_list_expand_if_needed(list))
      return false;

   for (i = (unsigned)list->size; i > 0; i--)
   {
      struct item_file *copy = (struct item_file*)
         calloc(1, sizeof(struct item_file));

      memcpy(copy, &list->list[i-1], sizeof(struct item_file));

      memcpy(&list->list[i-1], &list->list[i], sizeof(struct item_file));
      memcpy(&list->list[i],             copy, sizeof(struct item_file));

      free(copy);
   }

   file_list_add(list, 0, path, label, type,
         directory_ptr, entry_idx);

   return true;
}

bool file_list_append(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx)
{
   if (!file_list_expand_if_needed(list))
      return false;

   file_list_add(list, (unsigned)list->size, path, label, type,
         directory_ptr, entry_idx);

   return true;
}

size_t file_list_get_size(const file_list_t *list)
{
   if (!list)
      return 0;
   return list->size;
}

size_t file_list_get_directory_ptr(const file_list_t *list)
{
   size_t size = file_list_get_size(list);
   return list->list[size].directory_ptr;
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
   size_t i;

   if (!list)
      return;

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
   free(list);
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

void file_list_copy(const file_list_t *src, file_list_t *dst)
{
   struct item_file *item = NULL;

   if (!src || !dst)
      return;

   if (dst->list)
   {
      for (item = dst->list; item < &dst->list[dst->size]; ++item)
      {
         if (!item)
            continue;

         if (item->path)
            free(item->path);
         item->path = NULL;

         if (item->label)
            free(item->label);
         item->label = NULL;

         if (item->alt)
            free(item->alt);
         item->alt = NULL;
      }

      free(dst->list);
      dst->list = NULL;
   }

   dst->size     = 0;
   dst->capacity = 0;
   dst->list     = (struct item_file*)malloc(src->size * sizeof(struct item_file));

   if (!dst->list)
      return;

   dst->size     = dst->capacity = src->size;

   memcpy(dst->list, src->list, dst->size * sizeof(struct item_file));

   for (item = dst->list; item < &dst->list[dst->size]; ++item)
   {
      if (item->path)
         item->path  = strdup(item->path);

      if (item->label)
         item->label = strdup(item->label);

      if (item->alt)
         item->alt   = strdup(item->alt);
   }
}

void file_list_set_label_at_offset(file_list_t *list, size_t idx,
      const char *label)
{
   if (!list)
      return;

   if (list->list[idx].label)
      free(list->list[idx].label);
   list->list[idx].alt      = NULL;

   if (label)
      list->list[idx].label = strdup(label);
}

void file_list_get_label_at_offset(const file_list_t *list, size_t idx,
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
   list->list[idx].alt      = NULL;

   if (alt)
      list->list[idx].alt   = strdup(alt);
}

void file_list_get_alt_at_offset(const file_list_t *list, size_t idx,
      const char **alt)
{
   if (!list)
      return;

   if (alt)
      *alt = list->list[idx].alt ?
         list->list[idx].alt : list->list[idx].path;
}

static int file_list_alt_cmp(const void *a_, const void *b_)
{
   const struct item_file *a = (const struct item_file*)a_;
   const struct item_file *b = (const struct item_file*)b_;
   const char *cmp_a = a->alt ? a->alt : a->path;
   const char *cmp_b = b->alt ? b->alt : b->path;
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

void file_list_set_userdata(const file_list_t *list, size_t idx, void *ptr)
{
   if (!list || !ptr)
      return;
   list->list[idx].userdata = ptr;
}

void file_list_set_actiondata(const file_list_t *list, size_t idx, void *ptr)
{
   if (!list || !ptr)
      return;
   list->list[idx].actiondata = ptr;
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

void *file_list_get_last_actiondata(const file_list_t *list)
{
   if (!list)
      return NULL;
   return list->list[list->size - 1].actiondata;
}

void file_list_get_at_offset(const file_list_t *list, size_t idx,
      const char **path, const char **label, unsigned *file_type,
      size_t *entry_idx)
{
   if (!list)
      return;

   if (path)
      *path      = list->list[idx].path;
   if (label)
      *label     = list->list[idx].label;
   if (file_type)
      *file_type = list->list[idx].type;
   if (entry_idx)
      *entry_idx = list->list[idx].entry_idx;
}

void file_list_get_last(const file_list_t *list,
      const char **path, const char **label,
      unsigned *file_type, size_t *entry_idx)
{
   if (!list)
      return;

   if (list->size)
      file_list_get_at_offset(list, list->size - 1, path, label, file_type, entry_idx);
}

bool file_list_search(const file_list_t *list, const char *needle, size_t *idx)
{
   size_t i;
   const char *alt = NULL;
   bool ret        = false;

   if (!list)
      return false;

   for (i = 0; i < list->size; i++)
   {
      const char *str = NULL;

      file_list_get_alt_at_offset(list, i, &alt);
      if (!alt)
      {
         file_list_get_label_at_offset(list, i, &alt);
         if (!alt)
            continue;
      }

      str = (const char *)strcasestr(alt, needle);
      if (str == alt)
      {
         /* Found match with first chars, best possible match. */
         *idx = i;
         ret = true;
         break;
      }
      else if (str && !ret)
      {
         /* Found mid-string match, but try to find a match with
          * first characters before we settle. */
         *idx = i;
         ret = true;
      }
   }

   return ret;
}
