/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifdef HAVE_COMPRESSION
#include "file_extract.h"
#endif

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

/**
 * write_file:
 * @path             : path to file.
 * @data             : contents to write to the file.
 * @size             : size of the contents.
 *
 * Writes data to a file.
 *
 * Returns: true (1) on success, false (0) otherwise.
 */
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

/**
 * read_generic_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 *
 * Read the contents of a file into @buf.
 *
 * Returns: number of items read, -1 on error.
 */
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
   if (rom_buf)
      free(rom_buf);
   *buf = NULL;
   return -1;
}

#ifdef HAVE_COMPRESSION
/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
long read_compressed_file(const char * path, void **buf,
      const char* optional_filename)
{
   const char* file_ext;
   char archive_path[PATH_MAX_LENGTH], *archive_found = NULL;

   /* Safety check.
    * If optional_filename and optional_filename exists, we simply return 0,
    * hoping that optional_filename is the same as requested.
   */
   if (optional_filename)
      if(path_file_exists(optional_filename))
         return 0;

   //We split carchive path and relative path:
   strlcpy(archive_path,path,sizeof(archive_path));
   archive_found = (char*)strchr(archive_path,'#');
   rarch_assert(archive_found != NULL);

   //We assure that there is something after the '#' symbol
   if (strlen(archive_found) <= 1)
   {
      /*
       * This error condition happens for example, when
       * path = /path/to/file.7z, or
       * path = /path/to/file.7z#
       */
      RARCH_ERR("Could not extract image path and carchive path from "
            "path: %s.\n", path);
      return -1;
   }

   /* We split the string in two, by putting a \0, where the hash was: */
   *archive_found = '\0';
   archive_found+=1;

   file_ext = path_get_extension(archive_path);
#ifdef HAVE_7ZIP
   if (strcasecmp(file_ext,"7z") == 0)
      return read_7zip_file(archive_path,archive_found,buf,optional_filename);
#endif
#ifdef HAVE_ZLIB
   if (strcasecmp(file_ext,"zip") == 0)
      return read_zip_file(archive_path,archive_found,buf,optional_filename);
#endif
   return -1;
}
#endif

/**
 * read_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 *
 * Read the contents of a file into @buf. Will call read_compressed_file
 * if path contains a compressed file, otherwise will call read_generic_file.
 *
 * Returns: number of items read, -1 on error.
 */
long read_file(const char *path, void **buf)
{
#ifdef HAVE_COMPRESSION
   /* Here we check, whether the file, we are about to read is
    * inside an archive, or not.
    *
    * We determine, whether a file is inside a compressed archive,
    * by checking for the # inside the URL.
    *
    * For example: fullpath: /home/user/game.7z/mygame.rom
    * carchive_path: /home/user/game.7z
    * */
   if (path_contains_compressed_file(path))
      return read_compressed_file(path,buf,0);
#endif
   return read_generic_file(path,buf);
}

/**
 * read_file_string:
 * @path             : path to file to be read from.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 *
 * Reads file content as one string.
 *
 * Returns: true (1) on success, false (0) otherwise.
 */
bool read_file_string(const char *path, char **buf)
{
   long len = 0;
   char *ptr = NULL;
   FILE *file = fopen(path, "r");

   *buf = NULL;

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
