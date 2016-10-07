/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "retro_driver.h"
#include "vfs_file.h"

#include <stdlib.h>

struct retro_file_data
{
   /* File opened at the translated path */
   void* vfs_file;
};

void *retro_open_file(const char *path, enum retro_vfs_open_mode mode)
{
   char translated_path[PATH_MAX];

   if (retro_vfs_translate_path(path, translated_path, sizeof(translated_path)))
   {
      struct retro_file_data *retro_file = (struct retro_file_data*)malloc(sizeof(*retro_file));
      if (retro_file)
      {
         retro_file->vfs_file = vfs_open_file(translated_path, mode);
         if (retro_file->vfs_file)
            return retro_file;

         free(retro_file);
      }
   }

   return NULL;
}

int64_t retro_read_file(void* data, uint8_t *buffer, size_t buffer_size)
{
   struct retro_file_data *retro_file = (struct retro_file_data*)data;

   if (retro_file)
      return vfs_read_file(retro_file->vfs_file, buffer, buffer_size);

   return -1;
}

int64_t retro_write_file(void* data, const uint8_t *buffer, size_t buffer_size)
{
   struct retro_file_data *retro_file = (struct retro_file_data*)data;

   if (retro_file)
      return vfs_write_file(retro_file->vfs_file, buffer, buffer_size);

   return -1;
}

int64_t retro_seek_file(void* data, uint64_t position)
{
   struct retro_file_data *retro_file = (struct retro_file_data*)data;

   if (retro_file)
      return vfs_seek_file(retro_file->vfs_file, position);

   return -1;
}

int64_t retro_get_file_position(void* data)
{
   struct retro_file_data *retro_file = (struct retro_file_data*)data;

   if (retro_file)
      return vfs_get_file_position(retro_file->vfs_file);

   return -1;
}

int64_t retro_get_file_size(void* data)
{
   struct retro_file_data *retro_file = (struct retro_file_data*)data;

   if (retro_file)
      return vfs_get_file_size(retro_file->vfs_file);

   return -1;
}

int64_t retro_resize_file(void* data, uint64_t size)
{
   struct retro_file_data *retro_file = (struct retro_file_data*)data;

   if (retro_file)
      return vfs_resize_file(retro_file->vfs_file, size);

   return -1;
}

void retro_close_file(void* data)
{
   struct retro_file_data *retro_file = (struct retro_file_data*)data;

   if (retro_file)
   {
      vfs_close_file(retro_file->vfs_file);
      free(retro_file);
   }
}

struct vfs_file_t *init_retro_file(void)
{
   struct vfs_file_t *file = (struct vfs_file_t*)malloc(sizeof(struct vfs_file_t));
   if (file)
   {
      file->open = retro_open_file;
      file->read = retro_read_file;
      file->write = retro_write_file;
      file->seek = retro_seek_file;
      file->get_file_position = retro_get_file_position;
      file->get_file_size = retro_get_file_size;
      file->resize = retro_resize_file;
      file->close = retro_close_file;
      return file;
   }

   return NULL;
}
