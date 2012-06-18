/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "file.h"
#include "general.h"
#include <stdlib.h>
#include "boolean.h"
#include <string.h>
#include <time.h>
#include "compat/strl.h"
#include "compat/posix_string.h"

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>

#define S_ISDIR(x) (x & CELL_FS_S_IFDIR)
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#ifdef _MSC_VER
#define setmode _setmode
#endif
#elif defined(_XBOX)
#include <xtl.h>
#define setmode _setmode
#define INVALID_FILE_ATTRIBUTES -1
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

// Yep, this is C alright ;)
struct string_list
{
   char **data;
   size_t size;
   size_t cap;
};

static bool string_list_capacity(struct string_list *list, size_t cap)
{
   rarch_assert(cap > list->size);

   char **new_data = (char**)realloc(list->data, cap * sizeof(char*));
   if (!new_data)
      return false;

   list->data = new_data;
   list->cap  = cap;
   return true;
}

static bool string_list_init(struct string_list *list)
{
   memset(list, 0, sizeof(*list));
   return string_list_capacity(list, 32);
}

static bool string_list_append(struct string_list *list, const char *elem)
{
   if (list->size + 1 >= list->cap && !string_list_capacity(list, list->cap * 2))
      return false;

   if (!(list->data[list->size] = strdup(elem)))
      return false;

   list->size++;
   return true;
}

static char **string_list_finalize(struct string_list *list)
{
   rarch_assert(list->cap > list->size);

   list->data[list->size] = NULL;
   return list->data;
}

static void string_list_cleanup(struct string_list *list)
{
   for (size_t i = 0; i < list->size; i++)
      free(list->data[i]);
   free(list->data);
   memset(list, 0, sizeof(*list));
}

static void string_list_free(char **list)
{
   if (!list)
      return;

   char **orig = list;
   while (*list)
      free(*list++);
   free(orig);
}

static char **string_split(const char *str, const char *delim)
{
   char *copy      = NULL;
   const char *tmp = NULL;
   struct string_list list;

   if (!string_list_init(&list))
      goto error;

   copy = strdup(str);
   if (!copy)
      return NULL;

   tmp = strtok(copy, delim);
   while (tmp)
   {
      if (!string_list_append(&list, tmp))
         goto error;

      tmp = strtok(NULL, delim);
   }

   free(copy);
   return string_list_finalize(&list);

error:
   string_list_cleanup(&list);
   free(copy);
   return NULL;
}

static bool string_list_find_elem(char * const *list, const char *elem)
{
   if (!list)
      return false;

   for (; *list; list++)
      if (strcmp(*list, elem) == 0)
         return true;

   return false;
}

static const char *path_get_extension(const char *path)
{
   const char *ext = strrchr(path, '.');
   if (ext)
      return ext + 1;
   else
      return "";
}

size_t dir_list_size(char * const *dir_list)
{
   if (!dir_list)
      return 0;

   size_t size = 0;
   while (*dir_list++)
      size++;

   return size;
}

static int qstrcmp(const void *a, const void *b)
{
   return strcasecmp(*(char * const*)a, *(char * const*)b);
}

void dir_list_sort(char **dir_list)
{
   if (!dir_list)
      return;

   qsort(dir_list, dir_list_size(dir_list), sizeof(char*), qstrcmp);
}

#ifdef _WIN32 // Because the API is just fucked up ...
char **dir_list_new(const char *dir, const char *ext, bool include_dirs)
{
   struct string_list list;
   if (!string_list_init(&list))
      return NULL;

   HANDLE hFind = INVALID_HANDLE_VALUE;
   WIN32_FIND_DATA ffd;

   char path_buf[PATH_MAX];
   snprintf(path_buf, sizeof(path_buf), "%s\\*", dir);

   char **ext_list = NULL;
   if (ext)
      ext_list = string_split(ext, "|");

   hFind = FindFirstFile(path_buf, &ffd);
   if (hFind == INVALID_HANDLE_VALUE)
      goto error;

   do
   {
      const char *name     = ffd.cFileName;
      const char *file_ext = path_get_extension(name);

      if (!include_dirs && path_is_directory(name))
         continue;

#ifndef _XBOX
      if (!path_is_directory(name) && ext_list && !string_list_find_elem(ext_list, file_ext))
         continue;
#endif

      char file_path[PATH_MAX];
      snprintf(file_path, sizeof(file_path), "%s\\%s", dir, name);

      if (!string_list_append(&list, file_path))
         goto error;
   }
   while (FindNextFile(hFind, &ffd) != 0);

   FindClose(hFind);
   string_list_free(ext_list);
   return string_list_finalize(&list);

error:
   RARCH_ERR("Failed to open directory: \"%s\"\n", dir);
   if (hFind != INVALID_HANDLE_VALUE)
      FindClose(hFind);
   
   string_list_cleanup(&list);
   string_list_free(ext_list);
   return NULL;
}
#else
char **dir_list_new(const char *dir, const char *ext, bool include_dirs)
{
   struct string_list list;
   if (!string_list_init(&list))
      return NULL;

   DIR *directory = NULL;
   const struct dirent *entry = NULL;

   char **ext_list = NULL;
   if (ext)
      ext_list = string_split(ext, "|");

   directory = opendir(dir);
   if (!directory)
      goto error;

   while ((entry = readdir(directory)))
   {
      const char *name     = entry->d_name;
      const char *file_ext = path_get_extension(name);

      if (!include_dirs && path_is_directory(name))
         continue;

#ifndef __CELLOS_LV2__
      if (!path_is_directory(name) && ext_list && !string_list_find_elem(ext_list, file_ext))
         continue;
#endif

      char file_path[PATH_MAX];
      snprintf(file_path, sizeof(file_path), "%s/%s", dir, name);

      if (!string_list_append(&list, file_path))
         goto error;
   }

   closedir(directory);

   string_list_free(ext_list);
   return string_list_finalize(&list);

error:
   RARCH_ERR("Failed to open directory: \"%s\"\n", dir);

   if (directory)
      closedir(directory);

   string_list_cleanup(&list);
   string_list_free(ext_list);
   return NULL;
}
#endif

void dir_list_free(char **dir_list)
{
   string_list_free(dir_list);
}

bool path_is_directory(const char *path)
{
#ifdef _WIN32
   DWORD ret = GetFileAttributes(path);
   return (ret & FILE_ATTRIBUTE_DIRECTORY) && (ret != INVALID_FILE_ATTRIBUTES);
#else
   struct stat buf;
   if (stat(path, &buf) < 0)
      return false;

   return S_ISDIR(buf.st_mode);
#endif
}

bool path_file_exists(const char *path)
{
   FILE *dummy = fopen(path, "rb");
   if (dummy)
   {
      fclose(dummy);
      return true;
   }
   return false;
}

void fill_pathname(char *out_path, const char *in_path, const char *replace, size_t size)
{
   char tmp_path[PATH_MAX];

   rarch_assert(strlcpy(tmp_path, in_path, sizeof(tmp_path)) < sizeof(tmp_path));
   char *tok = strrchr(tmp_path, '.');
   if (tok)
      *tok = '\0';

   rarch_assert(strlcpy(out_path, tmp_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
}

void fill_pathname_noext(char *out_path, const char *in_path, const char *replace, size_t size)
{
   rarch_assert(strlcpy(out_path, in_path, size) < size);
   rarch_assert(strlcat(out_path, replace, size) < size);
}

void fill_pathname_dir(char *in_dir, const char *in_basename, const char *replace, size_t size)
{
   rarch_assert(strlcat(in_dir, "/", size) < size);

   const char *base = strrchr(in_basename, '/');
   if (!base)
      base = strrchr(in_basename, '\\');

   if (base)
      base++;
   else
      base = in_basename;

   rarch_assert(strlcat(in_dir, base, size) < size);
   rarch_assert(strlcat(in_dir, replace, size) < size);
}

void fill_pathname_base(char *out_dir, const char *in_path, size_t size)
{
   const char *ptr = strrchr(in_path, '/');
   if (!ptr)
      ptr = strrchr(in_path, '\\');

   if (ptr)
      ptr++;
   else
      ptr = in_path;

   rarch_assert(strlcpy(out_dir, ptr, size) < size);
}

