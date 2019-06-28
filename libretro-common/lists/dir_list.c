/* Copyright  (C) 2010-2018 The RetroArch team
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

/**
 * dir_list_sort:
 * @list      : pointer to the directory listing.
 * @dir_first : move the directories in the listing to the top?
 *
 * Sorts a directory listing.
 *
 **/
void dir_list_sort(struct string_list *list, bool dir_first)
{
   if (list)
      qsort(list->elems, list->size, sizeof(struct string_list_elem),
            dir_first ? qstrcmp_dir : qstrcmp_plain);
}

/**
 * dir_list_free:
 * @list : pointer to the directory listing
 *
 * Frees a directory listing.
 *
 **/
void dir_list_free(struct string_list *list)
{
   string_list_free(list);
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
 * Returns: -1 on error, 0 on success.
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

      if (!include_hidden && *name == '.')
         continue;
      if (!strcmp(name, ".") || !strcmp(name, ".."))
         continue;

      file_path[0] = '\0';
      fill_pathname_join(file_path, dir, name, sizeof(file_path));

      if (retro_dirent_is_dir(entry, NULL))
      {
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
 * Returns: true success, false in case of error.
 **/
bool dir_list_append(struct string_list *list,
      const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive)
{
   struct string_list *ext_list   = ext ? string_split(ext, "|") : NULL;
   bool ret                       = dir_list_read(dir, list, ext_list,
         include_dirs, include_hidden, include_compressed, recursive) != -1;

   string_list_free(ext_list);

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
 * Returns: pointer to a directory listing of type 'struct string_list *' on success,
 * NULL in case of error. Has to be freed manually.
 **/
struct string_list *dir_list_new(const char *dir,
      const char *ext, bool include_dirs,
      bool include_hidden, bool include_compressed,
      bool recursive)
{
   struct string_list *list       = NULL;

   if (!(list = string_list_new()))
      return NULL;

   if (!dir_list_append(list, dir, ext, include_dirs,
            include_hidden, include_compressed, recursive))
   {
      string_list_free(list);
      return NULL;
   }

   return list;
}
