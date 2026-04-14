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
#include <file/nbio.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_AUDIOMIXER
#include "task_audio_mixer.h"
#endif
#include "task_file_transfer.h"
#include "tasks_internal.h"

bool task_image_load_handler(retro_task_t *task);

/* Default number of nbio_iterate() calls per handler tick.
 * Callers may override by setting nbio->pos_increment to a
 * non-zero value before queuing the task.  A value of 0 (the
 * default from calloc / explicit init) selects this default. */
#define NBIO_DEFAULT_POS_INCREMENT 5

/* File-size threshold (bytes) below which the iterative transfer
 * loop runs to completion in a single tick rather than spreading
 * work across multiple frames.  Thumbnails and small config files
 * are typically well under this limit, so finishing them in one
 * tick eliminates several frames of latency. */
#define NBIO_SMALL_FILE_THRESHOLD  (256 * 1024)

static int task_file_transfer_iterate_transfer(nbio_handle_t *nbio)
{
   size_t i;
   unsigned iters;

   if (nbio->is_finished)
      return 0;

   /* Use caller-provided iteration count if set, otherwise default.
    * Unlike the old code this does NOT overwrite pos_increment, so
    * callers that tune it (e.g. for audio streaming) keep their value. */
   iters = nbio->pos_increment ? nbio->pos_increment
                               : NBIO_DEFAULT_POS_INCREMENT;

   for (i = 0; i < iters; i++)
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
      /* Retrieve the actual data length so the callback receives
       * a meaningful value instead of the previous hard-coded 0. */
      size_t len = 0;
      nbio_get_ptr(nbio->handle, &len);
      if (nbio->cb(nbio, len) == -1)
         return -1;
   }

   return 0;
}

void task_file_load_handler(retro_task_t *task)
{
   uint8_t flg;
   nbio_handle_t         *nbio  = (nbio_handle_t*)task->state;
   if (nbio)
   {
      switch (nbio->status)
      {
         case NBIO_STATUS_INIT:
            if (nbio->path)
            {
               struct nbio_t *handle = (struct nbio_t*)nbio_open(nbio->path, NBIO_READ);
               if (handle)
               {
                  size_t _len = 0;
                  nbio->handle       = handle;

                  /* Fast path: try load_entire to skip the iterate loop.
                   * For mmap this returns instantly (zero-copy), for AIO
                   * it does a single blocking wait. If the data is ready
                   * immediately, jump straight to TRANSFER_PARSE and
                   * skip the multi-tick TRANSFER state entirely. */
                  if (nbio_load_entire(handle, &_len))
                  {
                     /* Fall through: run parse in the same tick instead
                      * of returning and waiting for the next frame.
                      * This saves one full frame of latency for files
                      * that complete via the fast path. */
                     nbio->status    = NBIO_STATUS_TRANSFER_PARSE;
                     goto do_transfer_parse;
                  }

                  /* Fallback: backend needs iterative I/O (stdio).
                   * For small files, attempt to finish all iterations
                   * in this same tick to avoid multi-frame overhead. */
                  nbio->status       = NBIO_STATUS_TRANSFER;
                  nbio_begin_read(handle);

                  {
                     size_t file_len = 0;
                     nbio_get_ptr(handle, &file_len);
                     if (file_len > 0
                           && file_len <= NBIO_SMALL_FILE_THRESHOLD)
                     {
                        /* Small file: iterate until done in one tick */
                        while (!nbio_iterate(handle));
                        nbio->status = NBIO_STATUS_TRANSFER_PARSE;
                        task_set_progress(task, 100);
                        goto do_transfer_parse;
                     }
                  }
                  return;
               }
               task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
            }
            break;
do_transfer_parse:
         case NBIO_STATUS_TRANSFER_PARSE:
            if (task_file_transfer_iterate_parse(nbio) == -1)
            {
               task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
               break;
            }
            nbio->status = NBIO_STATUS_TRANSFER_FINISHED;
            break;
         case NBIO_STATUS_TRANSFER:
            if (task_file_transfer_iterate_transfer(nbio) == -1)
            {
               nbio->status = NBIO_STATUS_TRANSFER_PARSE;
               /* Fall through to parse immediately instead of
                * waiting for the next tick — saves one frame. */
               goto do_transfer_parse;
            }
            /* Report I/O progress so the UI can show a progress bar
             * for local file transfers, not just HTTP downloads. */
            {
               size_t done = 0, total = 0;
               if (nbio_get_progress(nbio->handle, &done, &total)
                     && total > 0)
               {
                  if (done < (((size_t)-1) / 100))
                     task_set_progress(task, (int8_t)(done * 100 / total));
                  else
                     task_set_progress(task,
                           (int8_t)MIN((signed)done / (total / 100), 100));
               }
            }
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
               task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
            break;
         case NBIO_TYPE_MP3:
         case NBIO_TYPE_FLAC:
         case NBIO_TYPE_OGG:
         case NBIO_TYPE_MOD:
         case NBIO_TYPE_WAV:
#ifdef HAVE_AUDIOMIXER
            if (!task_audio_mixer_load_handler(task))
               task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
#endif
            break;
         case NBIO_TYPE_NONE:
         default:
            if (nbio->is_finished)
               task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
            break;
      }
   }
   flg = task_get_flags(task);
   if ((flg & RETRO_TASK_FLG_CANCELLED) > 0)
   {
      task_set_error(task, strldup("Task canceled.", sizeof("Task canceled.")));
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   }
}
