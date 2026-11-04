/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

#include <stdlib.h>
#include <string.h>

#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>

#include "menu_content_downloads.h"

char *menu_content_downloads_load(const char *dir)
{
   char path[PATH_MAX_LENGTH];
   void *buf   = NULL;
   int64_t len = 0;

   if (!dir || !*dir)
      return NULL;
   fill_pathname_join_special(path, dir, MENU_CONTENT_DOWNLOADS_RECORD,
         sizeof(path));
   if (!path_is_valid(path) || !filestream_read_file(path, &buf, &len))
      return NULL;
   /* filestream_read_file() NUL-terminates what it returns. */
   return (char*)buf;
}

/* True when @name is one whole line of @record. */
static bool content_downloads_has(const char *record, const char *name)
{
   size_t len = strlen(name);
   const char *p;

   if (!record || !len)
      return false;
   for (p = record; (p = strstr(p, name)); p++)
      if (     (p == record || p[-1] == '\n')
            && (p[len] == '\n' || p[len] == '\0'))
         return true;
   return false;
}

bool menu_content_downloads_present(const char *dir, const char *record,
      const char *name)
{
   char path[PATH_MAX_LENGTH];

   if (!dir || !*dir || !name || !*name)
      return false;
   if (content_downloads_has(record, name))
      return true;
   fill_pathname_join_special(path, dir, name, sizeof(path));
   return path_is_valid(path);
}

bool menu_content_downloads_add(const char *dir, const char *name)
{
   char path[PATH_MAX_LENGTH];
   RFILE *file;
   char *record;
   bool ret = false;

   /* One name per line, so a name may not carry a line break. */
   if (     !dir || !*dir || !name || !*name
         || strchr(name, '\n') || strchr(name, '\r'))
      return false;

   record = menu_content_downloads_load(dir);
   if (content_downloads_has(record, name))
   {
      free(record);
      return true;
   }
   free(record);

   fill_pathname_join_special(path, dir, MENU_CONTENT_DOWNLOADS_RECORD,
         sizeof(path));
   /* UPDATE_EXISTING cannot create the file, so the first name opens
    * it plain. */
   if ((file = filestream_open(path,
               path_is_valid(path)
             ? RETRO_VFS_FILE_ACCESS_WRITE
             | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING
             : RETRO_VFS_FILE_ACCESS_WRITE,
               RETRO_VFS_FILE_ACCESS_HINT_NONE)))
   {
      size_t len = strlen(name);
      filestream_seek(file, 0, RETRO_VFS_SEEK_POSITION_END);
      ret =    filestream_write(file, name, len) == (int64_t)len
            && filestream_write(file, "\n", 1)   == 1;
      ret = filestream_close(file) == 0 && ret;
   }
   return ret;
}
