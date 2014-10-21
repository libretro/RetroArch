/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "file_ops.h"
#include <file/file_path.h>
#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_miscellaneous.h>

#ifdef __HAIKU__
#include <kernel/image.h>
#endif

#if (defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)) || defined(__QNX__) || defined(PSP)
#include <unistd.h> /* stat() is defined here */
#endif

#if defined(__CELLOS_LV2__)

#ifndef S_ISDIR
#define S_ISDIR(x) (x & 0040000)
#endif

#endif

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

/* Dump to file. */
bool write_file(const char *path, const void *data, size_t size)
{
   bool ret = false;
   FILE *file = fopen(path, "wb");
   if (!file)
      return false;

   ret = fwrite(data, 1, size, file) == size;
   fclose(file);
   return ret;
}

bool write_empty_file(const char *path)
{
   FILE *file = fopen(path, "w");
   if (!file)
      return false;

   fclose(file);

   return true;
}

static long read_generic_file(const char *path, void **buf)
{
   long rc = 0, len = 0;
   void *rom_buf = NULL;

   FILE *file = fopen(path, "rb");

   if (!file)
      goto error;

   fseek(file, 0, SEEK_END);
   len = ftell(file);
   rewind(file);
   rom_buf = malloc(len + 1);
   if (!rom_buf)
   {
      RARCH_ERR("Couldn't allocate memory.\n");
      goto error;
   }

   if ((rc = fread(rom_buf, 1, len, file)) < len)
      RARCH_WARN("Didn't read whole file.\n");

   *buf = rom_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)rom_buf)[len] = '\0';
   fclose(file);
   return rc;

error:
   if (file)
      fclose(file);
   free(rom_buf);
   *buf = NULL;
   return -1;

}

/* Generic file loader. */
long read_file(const char *path, void **buf)
{
   /* Here we check, whether the file, we are about to read is
    * inside an archive, or not.
    *
    * We determine, whether a file is inside a compressed archive,
    * by checking for the # inside the URL.
    *
    * For example: fullpath: /home/user/game.7z/mygame.rom
    * carchive_path: /home/user/game.7z
    * */
#ifdef HAVE_COMPRESSION
   if (path_contains_compressed_file(path))
      return read_compressed_file(path,buf,0);
#endif
   return read_generic_file(path,buf);
}

/* Reads file content as one string. */
bool read_file_string(const char *path, char **buf)
{
   *buf = NULL;
   FILE *file = fopen(path, "r");
   long len = 0;
   char *ptr = NULL;

   if (!file)
      goto error;

   /* ftell with "r" can be troublesome ...
    * Haven't run into issues yet though. */
   fseek(file, 0, SEEK_END);

   /* Takes account of being able to read
    * in EOF and '\0' at end. */
   len = ftell(file) + 2;
   rewind(file);

   *buf = (char*)calloc(len, sizeof(char));
   if (!*buf)
      goto error;

   ptr = *buf;

   while (ptr && !feof(file))
   {
      size_t bufsize = (size_t)(((ptrdiff_t)*buf +
               (ptrdiff_t)len) - (ptrdiff_t)ptr);
      fgets(ptr, bufsize, file);

      ptr += strlen(ptr);
   }

   ptr = strchr(ptr, EOF);
   if (ptr)
      *ptr = '\0';

   fclose(file);
   return true;

error:
   if (file)
      fclose(file);
   if (*buf)
      free(*buf);
   return false;
}
