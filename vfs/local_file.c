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

#include "local_driver.h"
#include "vfs_file.h"
#include <streams/file_stream.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct local_file_data
{
   RFILE *fp;
};

unsigned int translate_mode(enum retro_vfs_open_mode mode)
{
   switch (mode)
   {
   case RETRO_RDONLY:
      return RFILE_MODE_READ;
   case RETRO_WRONLY:
      return RFILE_MODE_WRITE;
   case RETRO_RDWR:
      return RFILE_MODE_READ_WRITE;
   case RETRO_RDWR_TRUNC:
      return RFILE_MODE_READ_WRITE; // TODO
   default:
      break;
   }

   return RFILE_MODE_READ;
}

void *local_open_file(const char *path, enum retro_vfs_open_mode mode)
{
   if (!path)
      return NULL;

   char local_path[PATH_MAX];
   if (local_vfs_translate_path(path, local_path, sizeof(local_path)))
   {
      struct local_file_data *file = (struct local_file_data*)malloc(sizeof(*file));
      if (file)
      {
         file->fp = filestream_open(local_path, translate_mode(mode), 0);
         if (file->fp)
            return file;

         free(file);
      }
   }

   return NULL;
}

int64_t local_read_file(void* data, uint8_t *buffer, size_t buffer_size)
{
   struct local_file_data *file = (struct local_file_data*)data;
   if (file)
      return filestream_read(file->fp, buffer, buffer_size);

   return -1;
}

int64_t local_write_file(void* data, const uint8_t *buffer, size_t buffer_size)
{
   struct local_file_data *file = (struct local_file_data*)data;
   if (file)
      return filestream_write(file->fp, buffer, buffer_size);

   return -1;
}

int64_t local_seek_file(void* data, uint64_t position)
{
   struct local_file_data *file = (struct local_file_data*)data;
   if (file)
      return filestream_seek(file->fp, position, SEEK_SET);

   return -1;
}

int64_t local_get_file_position(void* data)
{
   struct local_file_data *file = (struct local_file_data*)data;
   if (file)
      return filestream_tell(file->fp);

   return -1;
}

int64_t local_get_file_size(void* data)
{
   int64_t length = -1;

   struct local_file_data *file = (struct local_file_data*)data;
   if (file)
   {
      int current_pos = filestream_tell(file->fp);
      if (current_pos >= 0)
      {
         filestream_seek(file->fp, 0, SEEK_END);
         length = filestream_tell(file->fp);
         filestream_seek(file->fp, current_pos, SEEK_SET);
      }
   }

   return length;
}

int64_t local_resize_file(void* data, uint64_t size)
{
   return -1; // TODO
}

void local_close_file(void* data)
{
   struct local_file_data *file = (struct local_file_data*)data;
   if (file)
   {
      filestream_close(file->fp);
      free(file);
   }
}

struct vfs_file_t *init_local_file(void)
{
   struct vfs_file_t *file = (struct vfs_file_t*)malloc(sizeof(struct vfs_file_t));
   if (file)
   {
      file->open = local_open_file;
      file->read = local_read_file;
      file->write = local_write_file;
      file->seek = local_seek_file;
      file->get_file_position = local_get_file_position;
      file->get_file_size = local_get_file_size;
      file->resize = local_resize_file;
      file->close = local_close_file;
      return file;
   }

   return NULL;
}
