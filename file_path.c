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
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "file.h"
#include "general.h"
#include <stdlib.h>
#include "boolean.h"
#include <string.h>
#include <time.h>
#include "compat/strl.h"

#ifdef __CELLOS_LV2__
#include <cell/cell_fs.h>
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
char **dir_list_new(const char *dir, const char *ext)
{
   size_t cur_ptr = 0;
   size_t cur_size = 32;
   char **dir_list = NULL;

#ifdef _WIN32
   WIN32_FIND_DATA ffd;
   HANDLE hFind = INVALID_HANDLE_VALUE;

   char path_buf[PATH_MAX];

   if (strlcpy(path_buf, dir, sizeof(path_buf)) >= sizeof(path_buf))
      goto error;
#ifdef _XBOX
   if (strlcat(path_buf, "*", sizeof(path_buf)) >= sizeof(path_buf))
#else
   if (strlcat(path_buf, "/*", sizeof(path_buf)) >= sizeof(path_buf))
#endif
      goto error;

   if (ext)
   {
      if (strlcat(path_buf, ext, sizeof(path_buf)) >= sizeof(path_buf))
         goto error;
   }

   hFind = FindFirstFile(path_buf, &ffd);
   if (hFind == INVALID_HANDLE_VALUE)
      goto error;
#else
   DIR *directory = NULL;
   const struct dirent *entry = NULL;

   directory = opendir(dir);
   if (!directory)
      goto error;
#endif

   dir_list = (char**)calloc(cur_size, sizeof(char*));
   if (!dir_list)
      goto error;

#ifdef _WIN32 // Hard to read? Blame non-POSIX heathens!
   do
#else
   while ((entry = readdir(directory)))
#endif
   {
      // Not a perfect search of course, but hopefully good enough in practice.
#ifdef _WIN32
      if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
         continue;
      if (ext && !strstr(ffd.cFileName, ext))
         continue;
#else
      if (ext && !strstr(entry->d_name, ext))
         continue;
#endif

      dir_list[cur_ptr] = (char*)malloc(PATH_MAX);
      if (!dir_list[cur_ptr])
         goto error;

      strlcpy(dir_list[cur_ptr], dir, PATH_MAX);
#ifndef _XBOX
      strlcat(dir_list[cur_ptr], "/", PATH_MAX);
#endif
#ifdef _WIN32
      strlcat(dir_list[cur_ptr], ffd.cFileName, PATH_MAX);
#else
      strlcat(dir_list[cur_ptr], entry->d_name, PATH_MAX);
#endif

      cur_ptr++;
      if (cur_ptr + 1 == cur_size) // Need to reserve for NULL.
      {
         cur_size *= 2;
         dir_list = (char**)realloc(dir_list, cur_size * sizeof(char*));
         if (!dir_list)
            goto error;

         // Make sure it's all NULL'd out since we cannot rely on realloc to do this.
         memset(dir_list + cur_ptr, 0, (cur_size - cur_ptr) * sizeof(char*));
      }
   }
#if defined(_WIN32)
   while (FindNextFile(hFind, &ffd) != 0);
#endif

#ifdef _WIN32
   FindClose(hFind);
#else
   closedir(directory);
#endif
   return dir_list;

error:
   RARCH_ERR("Failed to open directory: \"%s\"\n", dir);
#ifdef _WIN32
   if (hFind != INVALID_HANDLE_VALUE)
      FindClose(hFind);
#else
   if (directory)
      closedir(directory);
#endif
   dir_list_free(dir_list);
   return NULL;
}

void dir_list_free(char **dir_list)
{
   if (!dir_list)
      return;

   char **orig = dir_list;
   while (*dir_list)
      free(*dir_list++);
   free(orig);
}

bool path_is_directory(const char *path)
{
#ifdef _WIN32
   DWORD ret = GetFileAttributes(path);
   return (ret & FILE_ATTRIBUTE_DIRECTORY) && (ret != INVALID_FILE_ATTRIBUTES);
#elif defined(__CELLOS_LV2__)
   CellFsStat buf;
   if (cellFsStat(path, &buf) < 0)
      return false;

   return buf.st_mode & CELL_FS_S_IFDIR;
#elif defined(XENON)
   // Dummy
   (void)path;
   return false;
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

