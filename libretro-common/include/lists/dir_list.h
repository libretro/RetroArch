/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dir_list.h).
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

#ifndef __LIBRETRO_SDK_DIR_LIST_H
#define __LIBRETRO_SDK_DIR_LIST_H

#include <retro_common_api.h>
#include <boolean.h>

#include <lists/string_list.h>

RETRO_BEGIN_DECLS

/**
 * dir_list_append:
 * @list               : existing list to append to.
 * @dir                : directory path.
 * @ext                : allowed extensions of file directory entries to include.
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : Only include files which match ext. Do not try to match compressed files, etc.
 * @recursive          : list directory contents recursively
 *
 * Create a directory listing, appending to an existing list
 *
 * @return Returns true on success, otherwise false.
 **/
bool dir_list_append(struct string_list *list, const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed, bool recursive);

/**
 * dir_list_new:
 * @dir                : directory path.
 * @ext                : allowed extensions of file directory entries to include.
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : include compressed files, even when not part of ext.
 * @recursive          : list directory contents recursively
 *
 * Create a directory listing.
 *
 * @return pointer to a directory listing of type 'struct string_list *' on success,
 * NULL in case of error. Has to be freed manually.
 **/
struct string_list *dir_list_new(const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed, bool recursive);

/**
 * dir_list_initialize:
 *
 * NOTE: @list must zero initialised before
 * calling this function, otherwise UB.
 **/
bool dir_list_initialize(struct string_list *list,
      const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive);

/**
 * dir_list_sort:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing.
 **/
void dir_list_sort(struct string_list *list, bool dir_first);

/**
 * dir_list_sort_ignore_ext:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing. File extensions are ignored.
 **/
void dir_list_sort_ignore_ext(struct string_list *list, bool dir_first);

/**
 * dir_list_free:
 * @list : pointer to the directory listing
 *
 * Frees a directory listing.
 **/
void dir_list_free(struct string_list *list);

bool dir_list_deinitialize(struct string_list *list);

RETRO_END_DECLS

#endif
