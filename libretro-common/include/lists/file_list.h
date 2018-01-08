/* Copyright  (C) 2010-2017 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (file_list.h).
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

#ifndef __LIBRETRO_SDK_FILE_LIST_H__
#define __LIBRETRO_SDK_FILE_LIST_H__

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#include <stddef.h>
#include <stdlib.h>

#include <boolean.h>

struct item_file
{
   char *path;
   char *label;
   char *alt;
   unsigned type;
   size_t directory_ptr;
   size_t entry_idx;
   void *userdata;
   void *actiondata;
};

typedef struct file_list
{
   struct item_file *list;

   size_t capacity;
   size_t size;
} file_list_t;


void *file_list_get_userdata_at_offset(const file_list_t *list,
      size_t index);

void *file_list_get_actiondata_at_offset(const file_list_t *list,
      size_t index);

/**
 * @brief frees the list
 *
 * NOTE: This function will also free() the entries actiondata
 * and userdata fields if they are non-null. If you store complex
 * or non-contiguous data there, make sure you free it's fields
 * before calling this function or you might get a memory leak.
 *
 * @param list
 */
void file_list_free(file_list_t *list);

/**
 * @brief makes the list big enough to contain at least nitems
 *
 * This function will not change the capacity if nitems is smaller
 * than the current capacity.
 *
 * @param list
 * @param nitems
 * @return whether or not the operation succeeded
 */
bool file_list_reserve(file_list_t *list, size_t nitems);

bool file_list_append(file_list_t *userdata, const char *path,
      const char *label, unsigned type, size_t current_directory_ptr,
      size_t entry_index);

bool file_list_prepend(file_list_t *list,
      const char *path, const char *label,
      unsigned type, size_t directory_ptr,
      size_t entry_idx);

void file_list_pop(file_list_t *list, size_t *directory_ptr);

void file_list_clear(file_list_t *list);

void file_list_copy(const file_list_t *src, file_list_t *dst);

void file_list_get_last(const file_list_t *list,
      const char **path, const char **label,
      unsigned *type, size_t *entry_idx);

void *file_list_get_last_actiondata(const file_list_t *list);

size_t file_list_get_size(const file_list_t *list);

size_t file_list_get_directory_ptr(const file_list_t *list);

void file_list_get_at_offset(const file_list_t *list, size_t index,
      const char **path, const char **label,
      unsigned *type, size_t *entry_idx);

void file_list_free_userdata(const file_list_t *list, size_t index);

void file_list_free_actiondata(const file_list_t *list, size_t idx);

void file_list_set_label_at_offset(file_list_t *list, size_t index,
      const char *label);

void file_list_get_label_at_offset(const file_list_t *list, size_t index,
      const char **label);

void file_list_set_alt_at_offset(file_list_t *list, size_t index,
      const char *alt);

void file_list_set_userdata(const file_list_t *list, size_t idx, void *ptr);

void file_list_set_actiondata(const file_list_t *list, size_t idx, void *ptr);

void file_list_get_alt_at_offset(const file_list_t *list, size_t index,
      const char **alt);

void file_list_sort_on_alt(file_list_t *list);

void file_list_sort_on_type(file_list_t *list);

bool file_list_search(const file_list_t *list, const char *needle,
      size_t *index);

RETRO_END_DECLS

#endif
