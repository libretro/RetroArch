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
#include <file/file_extract.h>
#endif

#ifdef HAVE_7ZIP
#include "decompress/7zip_support.h"
#endif

#ifdef HAVE_ZLIB
#include "decompress/zip_support.h"
#endif

#ifdef __HAIKU__
#include <kernel/image.h>
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
bool write_file(const char *path, const void *data, ssize_t size)
{
   ssize_t ret   = 0;
   FILE *file   = fopen(path, "wb");
   if (!file)
      return false;

   ret = fwrite(data, 1, size, file);
   fclose(file);
   return (ret == size);
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
static int read_generic_file(const char *path, void **buf, ssize_t *len)
{
   long ret                 = 0;
   ssize_t content_buf_size = 0;
   void *content_buf        = NULL;
   FILE *file               = fopen(path, "rb");

   if (!file)
      goto error;

   if (fseek(file, 0, SEEK_END) != 0)
      goto error;

   content_buf_size = ftell(file);
   if (content_buf_size < 0)
      goto error;

   rewind(file);

   content_buf = malloc(content_buf_size + 1);

   if (!content_buf)
      goto error;

   if ((ret = fread(content_buf, 1, content_buf_size, file)) < content_buf_size)
      RARCH_WARN("Didn't read whole file.\n");

   *buf    = content_buf;

   /* Allow for easy reading of strings to be safe.
    * Will only work with sane character formatting (Unix). */
   ((char*)content_buf)[content_buf_size] = '\0';

   if (fclose(file) != 0)
      RARCH_WARN("Failed to close file stream.\n");

   if (len)
      *len = ret;

   return 1;

error:
   if (file)
      fclose(file);
   if (content_buf)
      free(content_buf);
   if (len)
      *len = -1;
   *buf = NULL;
   return 0;
}

#ifdef HAVE_COMPRESSION
/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
int read_compressed_file(const char * path, void **buf,
      const char* optional_filename, ssize_t *length)
{
   const char* file_ext               = NULL;
   char *archive_found                = NULL;
   char archive_path[PATH_MAX_LENGTH] = {0};

   if (optional_filename)
   {
      /* Safety check.  * If optional_filename and optional_filename 
       * exists, we simply return 0,
       * hoping that optional_filename is the 
       * same as requested.
       */
      if(path_file_exists(optional_filename))
      {
         *length = 0;
         return 1;
      }
   }

   /* We split carchive path and relative path: */
   strlcpy(archive_path, path, sizeof(archive_path));

   archive_found = (char*)strchr(archive_path,'#');

   rarch_assert(archive_found != NULL);

   /* We assure that there is something after the '#' symbol. */
   if (strlen(archive_found) <= 1)
   {
      /*
       * This error condition happens for example, when
       * path = /path/to/file.7z, or
       * path = /path/to/file.7z#
       */
      RARCH_ERR("Could not extract image path and carchive path from "
            "path: %s.\n", path);
      *length = 0;
      return 0;
   }

   /* We split the string in two, by putting a \0, where the hash was: */
   *archive_found  = '\0';
   archive_found  += 1;
   file_ext        = path_get_extension(archive_path);

#ifdef HAVE_7ZIP
   if (strcasecmp(file_ext,"7z") == 0)
   {
      *length = read_7zip_file(archive_path,archive_found,buf,optional_filename);
      if (*length != -1)
         return 1;
   }
#endif
#ifdef HAVE_ZLIB
   if (strcasecmp(file_ext,"zip") == 0)
   {
      *length = read_zip_file(archive_path,archive_found,buf,optional_filename);
      if (*length != -1)
         return 1;
   }
#endif
   return 0;
}
#endif

/**
 * read_file:
 * @path             : path to file.
 * @buf              : buffer to allocate and read the contents of the
 *                     file into. Needs to be freed manually.
 * @length           : Number of items read, -1 on error.
 *
 * Read the contents of a file into @buf. Will call read_compressed_file
 * if path contains a compressed file, otherwise will call read_generic_file.
 *
 * Returns: 1 if file read, 0 on error.
 */
int read_file(const char *path, void **buf, ssize_t *length)
{
#ifdef HAVE_COMPRESSION
   if (path_contains_compressed_file(path))
   {
      if (read_compressed_file(path, buf, NULL, length))
         return 1;
   }
#endif
   return read_generic_file(path, buf, length);
}

struct string_list *compressed_file_list_new(const char *path,
      const char* ext)
{
#ifdef HAVE_COMPRESSION
   const char* file_ext = path_get_extension(path);
#ifdef HAVE_7ZIP
   if (strcasecmp(file_ext,"7z") == 0)
      return compressed_7zip_file_list_new(path,ext);
#endif
#ifdef HAVE_ZLIB
   if (strcasecmp(file_ext,"zip") == 0)
      return zlib_get_file_list(path, ext);
#endif

#endif
   return NULL;
}
