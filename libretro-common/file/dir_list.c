/* Copyright  (C) 2010-2015 The RetroArch team
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

#include <file/dir_list.h>
#include <file/file_path.h>
#include <compat/strl.h>
#include <compat/posix_string.h>

#if defined(_WIN32)
#ifdef _MSC_VER
#define setmode _setmode
#endif
#ifdef _XBOX
#include <xtl.h>
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <windows.h>
#endif
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

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

#ifndef _WIN32
/**
 *
 * dirent_is_directory:
 * @path         : path to the directory entry.
 * @entry        : pointer to the directory entry.
 *
 * Is the directory listing entry a directory?
 *
 * Returns: true if directory listing entry is
 * a directory, false if not.
 */

static bool dirent_is_directory(const char *path,
      const struct dirent *entry)
{
#if defined(PSP)
   return (entry->d_stat.st_attr & FIO_SO_IFDIR) == FIO_SO_IFDIR;
#elif defined(DT_DIR)
   if (entry->d_type == DT_DIR)
      return true;
   else if (entry->d_type == DT_UNKNOWN /* This can happen on certain file systems. */
         || entry->d_type == DT_LNK)
      return path_is_directory(path);
   return false;
#else /* dirent struct doesn't have d_type, do it the slow way ... */
   return path_is_directory(path);
#endif
}
#endif

/**
 * parse_dir_entry:
 * @name         : name of the directory listing entry.
 * @file_path    : file path of the directory listing entry.
 * @is_dir       : is the directory listing a directory?
 * @include_dirs : include directories as part of the finished directory listing?
 * @list         : pointer to directory listing.
 * @ext_list     : pointer to allowed file extensions listing.
 * @file_ext     : file extension of the directory listing entry.
 *
 * Parses a directory listing.
 *
 * Returns: zero on success, -1 on error, 1 if we should
 * continue to the next entry in the directory listing.
 **/
static int parse_dir_entry(const char *name, char *file_path,
      bool is_dir, bool include_dirs,
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

   if (!strcmp(name, ".") || !strcmp(name, ".."))
      return 1;

   if (!is_compressed_file && !is_dir && ext_list && !supported_by_core)
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
 * dir_list_new:
 * @dir          : directory path.
 * @ext          : allowed extensions of file directory entries to include.
 * @include_dirs : include directories as part of the finished directory listing?
 *
 * Create a directory listing.
 *
 * Returns: pointer to a directory listing of type 'struct string_list *' on success,
 * NULL in case of error. Has to be freed manually.
 **/
struct string_list *dir_list_new(const char *dir,
      const char *ext, bool include_dirs)
{
#ifdef _WIN32
   WIN32_FIND_DATA ffd;
   HANDLE hFind = INVALID_HANDLE_VALUE;
#else
   DIR *directory = NULL;
   const struct dirent *entry = NULL;
#endif
   char path_buf[PATH_MAX_LENGTH] = {0};
   struct string_list *ext_list   = NULL;
   struct string_list *list       = NULL;

   (void)path_buf;

   if (!(list = string_list_new()))
      return NULL;

   if (ext)
      ext_list = string_split(ext, "|");

#ifdef _WIN32
   snprintf(path_buf, sizeof(path_buf), "%s\\*", dir);

   hFind = FindFirstFile(path_buf, &ffd);
   if (hFind == INVALID_HANDLE_VALUE)
      goto error;

   do
   {
      int ret                         = 0;
      char file_path[PATH_MAX_LENGTH] = {0};
      const char *name                = ffd.cFileName;
      const char *file_ext            = path_get_extension(name);
      bool is_dir                     = ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

      fill_pathname_join(file_path, dir, name, sizeof(file_path));

      ret = parse_dir_entry(name, file_path, is_dir,
            include_dirs, list, ext_list, file_ext);

      if (ret == -1)
         goto error;

      if (ret == 1)
         continue;
   }while (FindNextFile(hFind, &ffd) != 0);

   FindClose(hFind);
   string_list_free(ext_list);
   return list;

error:
   if (hFind != INVALID_HANDLE_VALUE)
      FindClose(hFind);
#else
   directory = opendir(dir);
   if (!directory)
      goto error;

   while ((entry = readdir(directory)))
   {
      int ret                         = 0;
      char file_path[PATH_MAX_LENGTH] = {0};
      const char *name                = entry->d_name;
      const char *file_ext            = path_get_extension(name);
      bool is_dir                     = false;

      fill_pathname_join(file_path, dir, name, sizeof(file_path));

      is_dir = dirent_is_directory(file_path, entry);

      ret = parse_dir_entry(name, file_path, is_dir,
            include_dirs, list, ext_list, file_ext);

      if (ret == -1)
         goto error;

      if (ret == 1)
         continue;
   }

   closedir(directory);

   string_list_free(ext_list);
   return list;

error:

   if (directory)
      closedir(directory);

#endif
   string_list_free(list);
   string_list_free(ext_list);
   return NULL;
}
