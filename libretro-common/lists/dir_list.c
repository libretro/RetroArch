/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dir_list.c).
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

#include <stdlib.h>

#if defined(_WIN32) && defined(_XBOX)
#include <xtl.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <file/file_path.h>

#include <compat/strl.h>
#include <retro_dirent.h>

#include <string/stdstring.h>
#include <retro_miscellaneous.h>

static int qstrcmp_plain(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;

   return strcasecmp(a->data, b->data);
}

static int qstrcmp_plain_noext(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;

   const char *ext_a = path_get_extension(a->data);
   size_t l_a = string_is_empty(ext_a) ? strlen(a->data) : (size_t)(ext_a - a->data - 1);
   const char *ext_b = path_get_extension(b->data);
   size_t l_b = string_is_empty(ext_b) ? strlen(b->data) : (size_t)(ext_b - b->data - 1);

   int rv = strncasecmp(a->data, b->data, MIN(l_a, l_b));
   if (rv == 0 && l_a != l_b)
       return (int)(l_a - l_b);
   return rv;
}

static int qstrcmp_dir(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;
   int a_type = a->attr.i;
   int b_type = b->attr.i;

   /* Sort directories before files. */
   if (a_type != b_type)
      return b_type - a_type;
   return strcasecmp(a->data, b->data);
}

static int qstrcmp_dir_noext(const void *a_, const void *b_)
{
   const struct string_list_elem *a = (const struct string_list_elem*)a_;
   const struct string_list_elem *b = (const struct string_list_elem*)b_;
   int a_type = a->attr.i;
   int b_type = b->attr.i;

   /* Sort directories before files. */
   if (a_type != b_type)
      return b_type - a_type;
   return qstrcmp_plain_noext(a, b);
}

/**
 * dir_list_sort:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing.
 **/
void dir_list_sort(struct string_list *list, bool dir_first)
{
   if (list)
      qsort(list->elems, list->size, sizeof(struct string_list_elem),
            dir_first ? qstrcmp_dir : qstrcmp_plain);
}

/**
 * dir_list_sort_ignore_ext:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing. File extensions are ignored.
 **/
void dir_list_sort_ignore_ext(struct string_list *list, bool dir_first)
{
   if (list)
      qsort(list->elems, list->size, sizeof(struct string_list_elem),
            dir_first ? qstrcmp_dir_noext : qstrcmp_plain_noext);
}

/**
 * dir_list_free:
 * @list : pointer to the directory listing
 *
 * Frees a directory listing.
 **/
void dir_list_free(struct string_list *list)
{
   string_list_free(list);
}

bool dir_list_deinitialize(struct string_list *list)
{
   if (!list)
      return false;
   return string_list_deinitialize(list);
}

/**
 * dir_list_read:
 * @dir                : directory path.
 * @list               : the string list to add files to
 * @ext_list           : the string list of extensions to include
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : Only include files which match ext. Do not try to match compressed files, etc.
 * @recursive          : list directory contents recursively
 *
 * Add files within a directory to an existing string list
 *
 * @return -1 on error, 0 on success.
 **/
static int dir_list_read(const char *dir,
      struct string_list *list, struct string_list *ext_list,
      bool include_dirs, bool include_hidden,
      bool include_compressed, bool recursive)
{
   struct RDIR *entry = retro_opendir_include_hidden(dir, include_hidden);

   if (!entry || retro_dirent_error(entry))
      goto error;

   while (retro_readdir(entry))
   {
      union string_list_elem_attr attr;
      char file_path[PATH_MAX_LENGTH];
      const char *name                = retro_dirent_get_name(entry);

      if (name[0] == '.' || name[0] == '$')
      {
         /* Do not include hidden files and directories */
         if (!include_hidden)
            continue;

         /* char-wise comparisons to avoid string comparison */

         /* Do not include current dir */
         if (name[1] == '\0')
            continue;
         /* Do not include parent dir */
         if (name[1] == '.' && name[2] == '\0')
            continue;
      }

      fill_pathname_join_special(file_path, dir, name, sizeof(file_path));

      if (retro_dirent_is_dir(entry, NULL))
      {
         /* Exclude this frequent hidden dir on platforms which can not handle hidden attribute */
         if (!include_hidden && strcmp(name, "System Volume Information") == 0)
            continue;

#if defined(IOS) || defined(OSX)
         if (string_ends_with(name, ".framework"))
         {
            attr.i = RARCH_PLAIN_FILE;
            if (!string_list_append(list, file_path, attr))
               goto error;
            continue;
         }
#endif
         if (recursive)
            dir_list_read(file_path, list, ext_list, include_dirs,
                  include_hidden, include_compressed, recursive);

         if (!include_dirs)
            continue;
         attr.i = RARCH_DIRECTORY;
      }
      else
      {
         const char *file_ext    = path_get_extension(name);

         attr.i                  = RARCH_FILETYPE_UNSET;

         /*
          * If the file format is explicitly supported by the libretro-core, we
          * need to immediately load it and not designate it as a compressed file.
          *
          * Example: .zip could be supported as a image by the core and as a
          * compressed_file. In that case, we have to interpret it as a image.
          *
          * */
         if (string_list_find_elem_prefix(ext_list, ".", file_ext))
            attr.i            = RARCH_PLAIN_FILE;
         else
         {
            bool is_compressed_file;
            if ((is_compressed_file = path_is_compressed_file(file_path)))
               attr.i               = RARCH_COMPRESSED_ARCHIVE;

            if (ext_list &&
                  (!is_compressed_file || !include_compressed))
               continue;
         }
      }

      if (!string_list_append(list, file_path, attr))
         goto error;
   }

   retro_closedir(entry);

   return 0;

error:
   if (entry)
      retro_closedir(entry);
   return -1;
}

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
bool dir_list_append(struct string_list *list,
      const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive)
{
   bool ret                         = false;
   struct string_list ext_list      = {0};
   struct string_list *ext_list_ptr = NULL;

   if (ext)
   {
      string_list_initialize(&ext_list);
      string_split_noalloc(&ext_list, ext, "|");
      ext_list_ptr                  = &ext_list;
   }
   ret                            = dir_list_read(dir, list, ext_list_ptr,
         include_dirs, include_hidden, include_compressed, recursive) != -1;
   string_list_deinitialize(&ext_list);
   return ret;
}

/**
 * dir_list_new:
 * @dir                : directory path.
 * @ext                : allowed extensions of file directory entries to include.
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_hidden     : include hidden files and directories as part of the finished directory listing?
 * @include_compressed : Only include files which match ext. Do not try to match compressed files, etc.
 * @recursive          : list directory contents recursively
 *
 * Create a directory listing.
 *
 * @return pointer to a directory listing of type 'struct string_list *' on success,
 * NULL in case of error. Has to be freed manually.
 **/
struct string_list *dir_list_new(const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive)
{
   struct string_list *list       = string_list_new();

   if (!list)
      return NULL;

   if (!dir_list_append(list, dir, ext, include_dirs,
            include_hidden, include_compressed, recursive))
   {
      string_list_free(list);
      return NULL;
   }

   return list;
}

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
      bool recursive)
{
   if (list && string_list_initialize(list))
      return dir_list_append(list, dir, ext, include_dirs,
            include_hidden, include_compressed, recursive);
   return false;
}
