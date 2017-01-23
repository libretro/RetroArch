/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <string.h>
#include <errno.h>
#include <file/nbio.h>
#include <formats/image.h>
#include <compat/strl.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#include <rhash.h>

#include <string/stdstring.h>

#include "tasks_internal.h"
#include "../file_path_special.h"
#include "../verbosity.h"

static int task_file_transfer_iterate_transfer(nbio_handle_t *nbio)
{
   size_t i;

   if (!nbio)
      return -1;
   
   nbio->pos_increment = 5;

   if (nbio->is_finished)
      return 0;

   for (i = 0; i < nbio->pos_increment; i++)
   {
      if (nbio_iterate(nbio->handle))
         return -1;
   }

   return 0;
}

static int task_file_transfer_iterate_parse(nbio_handle_t *nbio)
{
   if (!nbio)
      return -1;

   if (nbio->cb)
   {
      int len = 0;
      if (nbio->cb(nbio, len) == -1)
         return -1;
   }

   return 0;
}

void task_file_load_handler(retro_task_t *task)
{
   nbio_handle_t         *nbio  = (nbio_handle_t*)task->state;

   if (nbio)
   {
      switch (nbio->status)
      {
         case NBIO_STATUS_INIT:
            if (nbio && !string_is_empty(nbio->path))
            {
               const char *fullpath  = nbio->path;
               struct nbio_t *handle = nbio_open(fullpath, NBIO_READ);

               if (handle)
               {
                  nbio->handle       = handle;
                  nbio->status       = NBIO_STATUS_TRANSFER;

                  if (strstr(fullpath, file_path_str(FILE_PATH_PNG_EXTENSION)))
                     nbio->image_type = IMAGE_TYPE_PNG;
                  else if (strstr(fullpath, file_path_str(FILE_PATH_JPEG_EXTENSION)) 
                        || strstr(fullpath, file_path_str(FILE_PATH_JPG_EXTENSION)))
                     nbio->image_type = IMAGE_TYPE_JPEG;
                  else if (strstr(fullpath, file_path_str(FILE_PATH_BMP_EXTENSION)))
                     nbio->image_type = IMAGE_TYPE_BMP;
                  else if (strstr(fullpath, file_path_str(FILE_PATH_TGA_EXTENSION)))
                     nbio->image_type = IMAGE_TYPE_TGA;

                  nbio_begin_read(handle);
                  return;
               }
               else
                  task_set_cancelled(task, true);
            }
            break;
         case NBIO_STATUS_TRANSFER_PARSE:
            if (task_file_transfer_iterate_parse(nbio) == -1)
               task_set_cancelled(task, true);
            nbio->status = NBIO_STATUS_TRANSFER_PARSE_FREE;
            break;
         case NBIO_STATUS_TRANSFER:
            if (task_file_transfer_iterate_transfer(nbio) == -1)
               nbio->status = NBIO_STATUS_TRANSFER_PARSE;
            break;
         case NBIO_STATUS_TRANSFER_PARSE_FREE:
         case NBIO_STATUS_POLL:
         default:
            break;
      }

      switch (nbio->image_type)
      {
         case IMAGE_TYPE_PNG:
         case IMAGE_TYPE_JPEG:
         case IMAGE_TYPE_TGA:
         case IMAGE_TYPE_BMP:
            if (!task_image_load_handler(task))
               task_set_finished(task, true);
            break;
         case 0:
            if (nbio->is_finished)
               task_set_finished(task, true);
            break;
      }
   }

   if (task_get_cancelled(task))
   {
      task_set_error(task, strdup("Task canceled."));
      task_set_finished(task, true);
   }
}
