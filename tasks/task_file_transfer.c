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
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#include <string/stdstring.h>

#ifdef HAVE_AUDIOMIXER
#include "task_audio_mixer.h"
#endif
#include "task_file_transfer.h"
#include "tasks_internal.h"

bool task_image_load_handler(retro_task_t *task);

static int task_file_transfer_iterate_transfer(nbio_handle_t *nbio)
{
   size_t i;

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
               struct nbio_t *handle = (struct nbio_t*)nbio_open(nbio->path, NBIO_READ);

               if (handle)
               {
                  nbio->handle       = handle;
                  nbio->status       = NBIO_STATUS_TRANSFER;

                  nbio_begin_read(handle);
                  return;
               }
               else
                  task_set_cancelled(task, true);
            }
            break;
         case NBIO_STATUS_TRANSFER_PARSE:
            if (!nbio || task_file_transfer_iterate_parse(nbio) == -1)
               task_set_cancelled(task, true);
            nbio->status = NBIO_STATUS_TRANSFER_FINISHED;
            break;
         case NBIO_STATUS_TRANSFER:
            if (!nbio || task_file_transfer_iterate_transfer(nbio) == -1)
               nbio->status = NBIO_STATUS_TRANSFER_PARSE;
            break;
         case NBIO_STATUS_TRANSFER_FINISHED:
            break;
      }

      switch (nbio->type)
      {
         case NBIO_TYPE_PNG:
         case NBIO_TYPE_JPEG:
         case NBIO_TYPE_TGA:
         case NBIO_TYPE_BMP:
            if (!task_image_load_handler(task))
               task_set_finished(task, true);
            break;
         case NBIO_TYPE_MP3:
         case NBIO_TYPE_FLAC:
         case NBIO_TYPE_OGG:
         case NBIO_TYPE_MOD:
         case NBIO_TYPE_WAV:
#ifdef HAVE_AUDIOMIXER
            if (!task_audio_mixer_load_handler(task))
               task_set_finished(task, true);
#endif
            break;
         case NBIO_TYPE_NONE:
         default:
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
