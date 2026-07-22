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
#include <formats/data_transfer.h>
#include <features/features_cpu.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_AUDIOMIXER
#include "task_audio_mixer.h"
#endif
#include "task_file_transfer.h"
#include "tasks_internal.h"

#ifdef RARCH_INTERNAL
#include "../gfx/video_display_server.h"
#endif

bool task_image_load_handler(retro_task_t *task);

/* Forward task progress to the platform's window/taskbar progress
 * indicator (e.g. ITaskbarList3 on Win32). Wire this as a task's
 * progress_cb to have the desktop reflect that task's progress.
 *
 * Lives in this always-built TU (rather than the network-gated
 * task_http.c where it originated) so non-network tasks -- core
 * backup, manual content scan, etc. -- can use it without pulling
 * in HAVE_NETWORKING. */
void task_window_progress_cb(retro_task_t *task)
{
#ifdef RARCH_INTERNAL
   if (task)
      video_display_server_set_window_progress(task->progress,
            ((task->flags & RETRO_TASK_FLG_FINISHED) > 0));
#endif
}

/* File-size threshold (bytes) below which the iterative transfer
 * loop runs to completion in a single tick rather than spreading
 * work across multiple frames.  Thumbnails, box art, and small
 * config files are typically well under this limit, so finishing
 * them in one tick eliminates several frames of latency.
 *
 * Raised from 256 KB to 1 MB: modern box-art PNGs at 512x720 can
 * exceed 400-600 KB, and loading them iteratively over several
 * frames is visibly laggy on menu scroll. 1 MB is still small
 * enough that a single blocking read completes in well under a
 * frame on every supported platform. */
#define NBIO_SMALL_FILE_THRESHOLD  (1024 * 1024)

/* Per-tick fill budget for the video data_transfer spine: comfortably
 * ahead of the still decoder's needs (it completes at 2-3% of the
 * file) without monopolising the tick. */
#define NBIO_XFER_TICK_BYTES       (1024 * 1024)

/* Full-file image types (PNG/JPEG/TGA/BMP) read under a time budget
 * instead: they need every byte before decoding, so there is nothing
 * to pace for - each tick reads as much as the storage delivers in a
 * few milliseconds, giving fast media the whole file in a tick or
 * two while slow media still never stalls a frame.  The video types
 * keep the byte budget: their stills complete on a small prefix and
 * a racing fill would just read past the point of use. */
#define NBIO_XFER_TICK_USEC        4000
#define NBIO_XFER_TIME_CHUNK       (256 * 1024)

const uint8_t *nbio_xfer_ptr(nbio_handle_t *nbio, size_t *len)
{
   return data_transfer_ptr(nbio->xfer, len);
}

bool nbio_xfer_progress(nbio_handle_t *nbio, size_t *done, size_t *total)
{
   size_t len = 0;
   if (!nbio->xfer)
      return false;
   data_transfer_ptr(nbio->xfer, &len);
   if (done)
      *done  = data_transfer_avail(nbio->xfer);
   if (total)
      *total = len;
   return !data_transfer_complete(nbio->xfer)
       && !data_transfer_failed(nbio->xfer)
       && !data_transfer_capped(nbio->xfer);
}

bool nbio_xfer_complete_ok(nbio_handle_t *nbio)
{
   return nbio->xfer && data_transfer_complete(nbio->xfer);
}

void nbio_xfer_close(nbio_handle_t *nbio)
{
   if (nbio->xfer)
      data_transfer_free(nbio->xfer);   /* cancels an in-flight read */
   nbio->xfer = NULL;
}

static int task_file_transfer_iterate_transfer(nbio_handle_t *nbio)
{
   if (nbio->is_finished)
      return 0;

   if (     nbio->type == NBIO_TYPE_WEBM
         || nbio->type == NBIO_TYPE_MP4
         || nbio->type == NBIO_TYPE_WEBP)
      data_transfer_iterate(nbio->xfer, NBIO_XFER_TICK_BYTES);
   else
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      do
      {
         data_transfer_iterate(nbio->xfer, NBIO_XFER_TIME_CHUNK);
         if (data_transfer_complete(nbio->xfer)
               || data_transfer_failed(nbio->xfer)
               || data_transfer_capped(nbio->xfer))
            break;
      } while (cpu_features_get_time_usec() - t0
            < NBIO_XFER_TICK_USEC);
   }
   if (data_transfer_complete(nbio->xfer)
         || data_transfer_failed(nbio->xfer)
         || data_transfer_capped(nbio->xfer))
      return -1;
   return 0;
}


static int task_file_transfer_iterate_parse(nbio_handle_t *nbio)
{
   if (nbio->cb)
   {
      /* Retrieve the actual data length so the callback receives
       * a meaningful value instead of the previous hard-coded 0. */
      size_t len = 0;
      nbio_xfer_ptr(nbio, &len);
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
               /* Every path load travels the data_transfer prefix
                * spine: filestream/VFS routing, 64-bit lengths, the
                * hardware guard behind avail, honest short reads.
                * (The nbio backends remain available through the
                * library's own data_transfer_open/adopt.) */
               if ((nbio->xfer = data_transfer_open_prefix(
                           nbio->path, 0)))
               {
                  size_t xlen = 0;
                  data_transfer_ptr(nbio->xfer, &xlen);
                  if (xlen <= NBIO_SMALL_FILE_THRESHOLD)
                  {
                     /* small file: finish in this tick */
                     data_transfer_iterate(nbio->xfer, 0);
                     if (!data_transfer_complete(nbio->xfer))
                     {
                        task_set_flags(task,
                              RETRO_TASK_FLG_CANCELLED, true);
                        break;
                     }
                     nbio->status = NBIO_STATUS_TRANSFER_PARSE;
                     task_set_progress(task, 100);
                     goto do_transfer_parse;
                  }
                  nbio->status = NBIO_STATUS_TRANSFER;
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
               if (!nbio_xfer_complete_ok(nbio))
               {
                  /* The read ended short of the file: fail the task
                   * rather than parse a buffer whose tail was never
                   * written. */
                  task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
                  break;
               }
               nbio->status = NBIO_STATUS_TRANSFER_PARSE;
               /* Fall through to parse immediately instead of
                * waiting for the next tick — saves one frame. */
               goto do_transfer_parse;
            }
            /* Report I/O progress so the UI can show a progress bar
             * for local file transfers, not just HTTP downloads. */
            {
               size_t done = 0, total = 0;
               nbio_xfer_progress(nbio, &done, &total);
               if (total > 0)
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
         case NBIO_TYPE_WEBP:
         case NBIO_TYPE_WEBM:
         case NBIO_TYPE_MP4:
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
