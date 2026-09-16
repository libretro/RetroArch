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
#include <retro_inline.h>
#include <lists/file_list.h>
#include <compat/strcasestr.h>

/* Empty strings are handed to file_list_append() constantly -- a
 * directory listing labels every one of its entries "" -- and each one
 * used to cost a malloc() for a single NUL byte, which the allocator
 * rounds up to a whole chunk anyway.  They all share this instead.
 *
 * The entry still holds a valid empty string rather than NULL, so every
 * reader is unaffected: the alternative, storing NULL, changes what
 * file_list_get_label_at_offset() returns and would have to be audited
 * against every strlen() of an entry field in the menu drivers.
 *
 * Nothing writes to an entry's path, label or alt in place -- they are
 * replaced wholesale by file_list_set_label_at_offset() and
 * file_list_set_alt_at_offset() -- so one shared buffer is safe to hand
 * to every entry of every list. */
static char file_list_empty_str[1] = "";

static char *file_list_strdup(const char *s)
{
   if (!s)
      return NULL;
   if (!*s)
      return file_list_empty_str;
   return strdup(s);
}

static void file_list_strfree(char *s)
{
   if (s && s != file_list_empty_str)
      free(s);
}

static bool file_list_deinitialize_internal(file_list_t *list)
{
   size_t i;
   for (i = 0; i < list->size; i++)
   {
      file_list_free_userdata(list, i);
      file_list_free_actiondata(list, i);

      file_list_strfree(list->list[i].path);
      list->list[i].path  = NULL;

      file_list_strfree(list->list[i].label);
      list->list[i].label = NULL;

      file_list_strfree(list->list[i].alt);
      list->list[i].alt   = NULL;
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

/* Helper function to initialize item_file structure */
static INLINE void init_item_file(struct item_file *item,
    const char *path, const char *label, unsigned type,
    size_t directory_ptr, size_t entry_idx)
{
    /* NULL-gate both strdup calls: strdup(NULL) is undefined
     * behaviour (glibc crashes).  The sibling file_list_append
     * uses the same gating pattern.  Callers have been seen to
     * pass NULL path here via menu_entries_prepend when
     * msg_hash_to_str returns NULL for an enum that no active
     * language handler recognises. */
    item->path          = file_list_strdup(path);
    item->label         = file_list_strdup(label);
    item->alt           = NULL;
    item->type          = type;
    item->directory_ptr = directory_ptr;
    item->entry_idx     = entry_idx;
    item->userdata      = NULL;
    item->actiondata    = NULL;
}

bool file_list_insert(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx,
      size_t idx)
{
   /* Expand file list if needed */
   if (list->size >= list->capacity)
   {
      size_t new_capacity = list->capacity > 0 ? list->capacity * 2 : 1;
      if (!file_list_reserve(list, new_capacity))
         return false;
   }

   /* Shift elements to the right using memmove */
   if (idx < list->size)
      memmove(&list->list[idx + 1], &list->list[idx],
            (list->size - idx) * sizeof(struct item_file));

   init_item_file(&list->list[idx], path, label, type, directory_ptr, entry_idx);
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

   list->list[idx].label         = file_list_strdup(label);
   list->list[idx].path          = file_list_strdup(path);

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

      /* Every allocation the entry owns goes back here, matching
       * file_list_deinitialize_internal. The two helpers clear the
       * slot as they go, so a caller that has already released
       * userdata or actiondata itself - menu_list_pop_stack does,
       * through the driver's list_free hook - passes through them. */
      file_list_free_userdata  (list, list->size);
      file_list_free_actiondata(list, list->size);

      file_list_strfree(list->list[list->size].path);
      list->list[list->size].path  = NULL;

      file_list_strfree(list->list[list->size].label);
      list->list[list->size].label = NULL;

      file_list_strfree(list->list[list->size].alt);
      list->list[list->size].alt   = NULL;
   }

   /* A list that never had an entry has no backing array to read a
    * directory_ptr out of. */
   if (directory_ptr && list->list)
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
      file_list_strfree(list->list[i].path);
      list->list[i].path  = NULL;

      file_list_strfree(list->list[i].label);
      list->list[i].label = NULL;

      file_list_strfree(list->list[i].alt);
      list->list[i].alt   = NULL;
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

/* Releases an entry's label and clears the slot.
 *
 * The label may be the shared empty string rather than an allocation of
 * its own, so it cannot be handed to free(). Callers outside this file
 * that want to relabel an entry -- the menu drivers do, on the stack
 * top -- have to come through here or through
 * file_list_set_label_at_offset() rather than free()ing the pointer
 * themselves. */
void file_list_free_label(file_list_t *list, size_t idx)
{
   if (!list || idx >= list->size)
      return;
   file_list_strfree(list->list[idx].label);
   list->list[idx].label = NULL;
}

void file_list_set_label_at_offset(file_list_t *list, size_t idx,
      const char *label)
{
   if (!list || !label)
      return;
   file_list_strfree(list->list[idx].label);
   list->list[idx].label = file_list_strdup(label);
}

void file_list_set_alt_at_offset(file_list_t *list, size_t idx,
      const char *alt)
{
   if (!list || !alt)
      return;
   file_list_strfree(list->list[idx].alt);
   list->list[idx].alt   = file_list_strdup(alt);
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
   if (!list || idx >= list->size)
      return NULL;
   return list->list[idx].userdata;
}

void *file_list_get_actiondata_at_offset(const file_list_t *list, size_t idx)
{
   if (!list || idx >= list->size)
      return NULL;
   return list->list[idx].actiondata;
}

void file_list_free_actiondata(const file_list_t *list, size_t idx)
{
   if (!list)
      return;
   if (list->list[idx].actiondata)
   {
      if (list->actiondata_free)
         list->actiondata_free(list->list[idx].actiondata);
      else
         free(list->list[idx].actiondata);
   }
   list->list[idx].actiondata = NULL;
}

void file_list_free_userdata(const file_list_t *list, size_t idx)
{
   if (!list)
      return;
   if (list->list[idx].userdata)
   {
      if (list->userdata_free)
         list->userdata_free(list->list[idx].userdata);
      else
         free(list->list[idx].userdata);
   }
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

      if ((str = (const char *)compat_strcasestr(alt, needle)) == alt)
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
