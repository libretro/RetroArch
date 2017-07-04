/* Copyright  (C) 2010-2017 The RetroArch team
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
 * parse_dir_entry:
 * @name               : name of the directory listing entry.
 * @file_path          : file path of the directory listing entry.
 * @is_dir             : is the directory listing a directory?
 * @include_dirs       : include directories as part of the finished directory listing?
 * @include_compressed : Include compressed files, even if not part of ext_list.
 * @list               : pointer to directory listing.
 * @ext_list           : pointer to allowed file extensions listing.
 * @file_ext           : file extension of the directory listing entry.
 *
 * Parses a directory listing.
 *
 * Returns: zero on success, -1 on error, 1 if we should
 * continue to the next entry in the directory listing.
 **/
static int parse_dir_entry(const char *name, char *file_path,
      bool is_dir, bool include_dirs, bool include_compressed,
      struct string_list *list, struct string_list *ext_list,
      const char *file_ext)
{
   union string_list_elem_attr attr;
   bool is_compressed_file = false;
   bool supported_by_core  = false;

   attr.i                  = RARCH_FILETYPE_UNSET;

   if (!is_dir)
   {
      is_compressed_file = path_is_compressed_file(file_path);
      if (string_list_find_elem_prefix(ext_list, ".", file_ext))
         supported_by_core = true;
   }

   if (!include_dirs && is_dir)
      return 1;

   if (string_is_equal(name, ".") || string_is_equal(name, ".."))
      return 1;

   if (!is_dir && ext_list &&
           ((!is_compressed_file && !supported_by_core) ||
            (!supported_by_core && !include_compressed)))
      return 1;

   if (is_dir)
      attr.i = RARCH_DIRECTORY;
   if (is_compressed_file)
      attr.i = RARCH_COMPRESSED_ARCHIVE;
   /* The order of these ifs is important.
    * If the file format is explicitly supported by the libretro-core, we
    * need to immediately load it and not designate it as a compressed file.
    *
    * Example: .zip could be supported as a image by the core and as a
    * compressed_file. In that case, we have to interpret it as a image.
    *
    * */
   if (supported_by_core)
      attr.i = RARCH_PLAIN_FILE;

   if (!string_list_append(list, file_path, attr))
      return -1;

   return 0;
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
   struct RDIR *entry = retro_opendir(dir);

   if (!entry || retro_dirent_error(entry))
      goto error;

   retro_dirent_include_hidden(entry, include_hidden);

   while (retro_readdir(entry))
   {
      char file_path[PATH_MAX_LENGTH];
      bool is_dir                     = false;
      int ret                         = 0;
      const char *name                = retro_dirent_get_name(entry);
      const char *file_ext            = path_get_extension(name);

      file_path[0] = '\0';

      fill_pathname_join(file_path, dir, name, sizeof(file_path));
      is_dir = retro_dirent_is_dir(entry, file_path);

      if (!include_hidden)
      {
         if (*name == '.')
            continue;
      }

      if(is_dir && recursive)
      {
         if(strstr(name, ".") || strstr(name, ".."))
            continue;

         dir_list_read(file_path, list, ext_list, include_dirs,
               include_hidden, include_compressed, recursive);
      }

      ret    = parse_dir_entry(name, file_path, is_dir,
            include_dirs, include_compressed, list, ext_list, file_ext);

      if (ret == -1)
         goto error;

      if (ret == 1)
         continue;
   }

   retro_closedir(entry);

   return 0;

error:
   if (entry)
      retro_closedir(entry);
   return -1;
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
   struct string_list *ext_list   = NULL;
   struct string_list *list       = NULL;

   if (!(list = string_list_new()))
      return NULL;

   if (ext)
      ext_list = string_split(ext, "|");

   if(dir_list_read(dir, list, ext_list, include_dirs,
            include_hidden, include_compressed, recursive) == -1)
   {
      string_list_free(list);
      string_list_free(ext_list);
      return NULL;
   }

   string_list_free(ext_list);
   return list;
}

