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

#include "file_path.h"
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

#ifdef HAVE_7ZIP
#include "decompress/7zip_support.h"
#endif
#ifdef HAVE_ZLIB
#include "decompress/zip_support.h"
#endif

/* Generic compressed file loader.
 * Extracts to buf, unless optional_filename != 0
 * Then extracts to optional_filename and leaves buf alone.
 */
#ifdef HAVE_COMPRESSION
long read_compressed_file(const char * path, void **buf,
      const char* optional_filename)
{
   /* Safety check.
    * If optional_filename and optional_filename exists, we simply return 0,
    * hoping that optional_filename is the same as requested.
   */
   if (optional_filename)
      if(path_file_exists(optional_filename))
         return 0;

   //We split carchive path and relative path:
   char archive_path[PATH_MAX];
   strlcpy(archive_path,path,sizeof(archive_path));
   char* archive_found = strchr(archive_path,'#');
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

   //We split the string in two, by putting a \0, where the hash was:
   *archive_found = '\0';
   archive_found+=1;


   const char* file_ext = path_get_extension(archive_path);
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
      return compressed_zip_file_list_new(path,ext);
#endif

#endif
   return NULL;
}
