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

/* Types that need the whole file before they finish read under a time
 * budget instead of a byte one: there is no prefix to stop at, so
 * each tick reads as much as the storage delivers in a few
 * milliseconds, giving fast media the file in a tick or two while
 * slow media still never stalls a frame.
 *
 * The byte budget above stays with the types whose decode completes
 * on a prefix - the video stills, and WEBP, which does not decode
 * against a growing buffer the way PNG and JPEG do but starts once
 * rwebp_still_ready() says its still chunk is wholly resident.  For
 * those a racing fill just reads past the point of use.  (PNG and
 * JPEG are avail-aware and decode against the partial buffer, but
 * they still need every byte to finish, so the time budget is theirs
 * too - task_image.c's is_prefix grouping answers a different
 * question from this one and the two are meant to differ.)
 *
 * The time budget's value itself lives with the window in
 * task_nbio_slice.c (NBIO_XFER_TICK_USEC): the slice open/close
 * calls below hand out exactly that allowance. */

/* The shared per-frame I/O window itself (state + the three
 * task_nbio_slice_* entry points) lives in task_nbio_slice.c: it is
 * shared with other budgeted handlers (the manual content scanner's
 * BEGIN spine), and a TU of its own keeps its dependencies to the
 * clock and the task queue, so tests can link the real window
 * without dragging the image/nbio spine in with it.  The design
 * rationale - why a shared window rather than a per-task slice, why
 * the floor - is documented there. */

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
      data_transfer_free(nbio->xfer);
   nbio->xfer = NULL;
}

static int task_file_transfer_iterate_transfer(nbio_handle_t *nbio)
{
   if (nbio->is_finished)
      return 0;

   {
      nbio_budget_t b;
      /* Both budgets go to the fill and whichever comes first stops
       * it: the prefix types keep their byte cap so they do not race
       * past the point of use, and every type is bounded by the
       * tick's remaining time. */
      size_t bytes = (     nbio->type == NBIO_TYPE_WEBM
                        || nbio->type == NBIO_TYPE_MP4
                        || nbio->type == NBIO_TYPE_WEBP)
         ? (size_t)NBIO_XFER_TICK_BYTES : 0;

      task_nbio_slice_open(&b);
      data_transfer_iterate_while(nbio->xfer, bytes,
            task_nbio_slice_within_budget, &b);
      task_nbio_slice_close(&b);
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
                *
                * No commit_cap, for any type - including the prefix
                * ones.  There was briefly a 256 MiB cap here for
                * WEBM/MP4/WEBP, reasoned from "a still decodes at a
                * few per cent of its file, so a fill still running
                * at 256 MiB is pathology".  The premise is false: a
                * non-faststart MP4 keeps its moov index at EOF, so
                * the demuxer cannot even open - let alone decode a
                * still - until the WHOLE file is resident.  That is
                * not a malformed file; it is what most cameras and
                * phones write.  Every such file past the cap filled
                * to exactly the cap, settled capped(), and the
                * handler cancelled: no still, no thumbnail, and no
                * animation either, since the upload callback bails
                * before its animation block when the task delivers
                * nothing.  4K content was hit first purely by size.
                *
                * No constant fixes that - the legitimate worst-case
                * need is the file length - and the per-tick pacing
                * already bounds what any single frame can cost, so
                * the cap protected nothing the budgets did not. */
               /* The open is real I/O - a filestream_open, a size
                * query, and on the pool-miss path fresh-page faults -
                * and it ran outside the window: neither budgeted nor
                * charged, so a queue of cold opens could silently
                * blow the very slice the fills respect.  It cannot
                * be interrupted mid-call, but it can be charged, so
                * the fills that follow in the same period get that
                * much less. */
               {
                  nbio_budget_t b;
                  task_nbio_slice_open(&b);
                  nbio->xfer = data_transfer_open_prefix(nbio->path, 0);
                  task_nbio_slice_close(&b);
               }
               if (nbio->xfer)
               {
                  nbio->status = NBIO_STATUS_TRANSFER;
                  /* Fall through into the fill instead of returning
                   * from a tick that read nothing - the same frame
                   * the two jumps to do_transfer_parse save, at the
                   * other end of the transfer.
                   *
                   * A file under NBIO_SMALL_FILE_THRESHOLD used to
                   * get an unbudgeted iterate() here, finishing in
                   * this tick however long that took.  That was the
                   * one path with no bound at all: forty 512 KiB
                   * loads cost 10.72 ms in a frame with the cache
                   * warm and nothing whatever on cold storage.  It
                   * takes the shared window like everything else
                   * now, and still finishes in one tick whenever the
                   * window has the room. */
                  goto do_transfer;
               }
            }
            /* Outside the path check, not inside it.  A NULL path
             * used to fall out of INIT having set no flag and
             * changed no status, so the next tick re-entered INIT
             * and did the same thing forever: a task that never
             * finishes, never cancels, and never leaves the running
             * queue - a permanent per-frame call under the regular
             * task queue and a hot spin on the worker thread under
             * the threaded one.
             *
             * Every producer currently guards its strdup, so this is
             * closing the hole rather than fixing a live bug.  A
             * state machine with no terminal for one of its inputs
             * is the kind of thing that only stays unreachable until
             * someone adds a caller. */
            task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
            break;
do_transfer_parse:
         case NBIO_STATUS_TRANSFER_PARSE:
            /* Reaching parse means the whole file is in, so report it
             * once here where both entries pass.  Only the small-file
             * branch used to set 100, and the multi-tick path never
             * did: it jumps straight from the completing fill to
             * parse, skipping the progress update below, so a large
             * local load's bar stopped at whatever the last partial
             * tick had reported - 0 for a two-tick load - and then
             * vanished. */
            task_set_progress(task, 100);
            if (task_file_transfer_iterate_parse(nbio) == -1)
            {
               task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
               break;
            }
            nbio->status = NBIO_STATUS_TRANSFER_FINISHED;
            break;
do_transfer:
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
               /* Scale both sides down until the multiply cannot
                * overflow, rather than switching to a signed division
                * for the large case.  done can exceed INT_MAX - every
                * file over 2 GB, and on a 32-bit target every file
                * over ~43 MiB took that branch - and converting an
                * out-of-range size_t to int is implementation-defined:
                * on the usual two's-complement targets it lands
                * negative and the percentage ran backwards.  The
                * divisor there could also reach zero for a total under
                * 100, which only the same unreachable magnitudes kept
                * safe.
                *
                * done <= total always, so testing total is enough.
                * The loop never runs on a 64-bit size_t, and where it
                * does it costs a bit of precision on a value that is
                * about to be squashed into 0-100 anyway. */
               while (total > (((size_t)-1) / 100))
               {
                  done  >>= 1;
                  total >>= 1;
               }
               if (total > 0)
                  task_set_progress(task, (int8_t)(done * 100 / total));
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
         /* M4A and OPUS were missing from this list while being set
          * by both mixer push paths, so they fell to default: the
          * file was read, the task reported finished, and
          * task_audio_mixer_load_handler - which is what builds the
          * task's data and donates the transfer to the mixer - never
          * ran.  The sound loaded and then silently did not play. */
         case NBIO_TYPE_M4A:
         case NBIO_TYPE_OPUS:
#ifdef HAVE_AUDIOMIXER
            if (!task_audio_mixer_load_handler(task))
               task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
#endif
            break;
         case NBIO_TYPE_NONE:
         default:
            /* is_finished is the parse callback's signal that it is
             * done with the buffer, and it is an undocumented
             * obligation: a callback that returns 0 without setting
             * it left the task parked in TRANSFER_FINISHED with
             * nothing in either switch able to move it, which is the
             * same never-terminating task the INIT path had.
             *
             * Reaching TRANSFER_FINISHED already means the callback
             * returned success, so there is nothing left to do for a
             * type with no decode handler of its own.  Keep the
             * is_finished test as well: a callback may set it before
             * the status advances. */
            if (     nbio->is_finished
                  || nbio->status == NBIO_STATUS_TRANSFER_FINISHED)
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
