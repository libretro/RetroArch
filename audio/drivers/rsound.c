/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdlib.h>

#include <boolean.h>

#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>

#include "../audio_driver.h"
#include "rsound.h"

typedef struct rsd
{
   rsound_t *rd;

   fifo_buffer_t *buffer;
   slock_t *cond_lock;
   scond_t *cond;

   /* Bound for the blocking wait in rs_write, one callback period.
    * See the note there. */
   int64_t wait_us;
   /* The fifo's capacity in bytes: what buffer_size() reports. */
   size_t fifo_size;

   bool nonblock;
   bool is_paused;
   volatile bool has_error;
} rsd_t;

static ssize_t rsound_audio_cb(void *data, size_t bytes, void *userdata)
{
   rsd_t *rsd        = (rsd_t*)userdata;
   size_t avail      = FIFO_READ_AVAIL(rsd->buffer);
   size_t write_size = bytes > avail ? avail : bytes;
   fifo_read(rsd->buffer, data, write_size);
   scond_signal(rsd->cond);

   return write_size;
}

static void rsound_err_cb(void *userdata)
{
   rsd_t *rsd = (rsd_t*)userdata;
   rsd->has_error = true;
   scond_signal(rsd->cond);
}

static void *rs_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   int channels, format;
   rsound_t *rd  = NULL;
   rsd_t *rsd    = (rsd_t*)calloc(1, sizeof(rsd_t));
   if (!rsd)
      return NULL;

   if (rsd_init(&rd) < 0)
      goto error;

   rsd->cond_lock = slock_new();
   rsd->cond      = scond_new();

   channels       = 2;
   format         = RSD_S16_NE;

   /* Two stages: the fifo here, which the writer fills and rate control
    * measures and holds half full, and librsound's own buffer behind
    * it, which its callback pulls from the fifo to keep full. The fifo
    * holds the latency setting, in bytes of int16 stereo, and is what
    * is reported; librsound is asked for what is left of the setting
    * after half the fifo - half of it, floored at 16 ms or the setting
    * - so the two add up to the setting. The fifo was a fixed 4 KiB,
    * 21 ms at 48 kHz, whatever the setting, and librsound was asked
    * for the whole setting on top of it. */
   {
      unsigned server_latency = latency / 2;
      unsigned floor_ms       = latency < 16 ? latency : 16;
      if (server_latency < floor_ms)
         server_latency       = floor_ms;
      rsd->fifo_size          = ((size_t)rate * latency / 1000)
            * channels * sizeof(int16_t);
      if (rsd->fifo_size < 1024 * 4)
         rsd->fifo_size       = 1024 * 4;
      rsd->buffer             = fifo_new(rsd->fifo_size);
      rsd_set_param(rd, RSD_CHANNELS, &channels);
      rsd_set_param(rd, RSD_SAMPLERATE, &rate);
      rsd_set_param(rd, RSD_LATENCY, &server_latency);
      RARCH_LOG("[RSound] %u ms setting: a %u-byte fifo (%u ms, rate control holds it about half full) in front of a %u ms server buffer; about %u ms from write to the server.\n",
            latency, (unsigned)rsd->fifo_size,
            (unsigned)(rsd->fifo_size / (channels * sizeof(int16_t)) * 1000 / rate),
            server_latency,
            (unsigned)(rsd->fifo_size / (channels * sizeof(int16_t)) * 1000 / rate / 2 + server_latency));
   }

   if (device)
      rsd_set_param(rd, RSD_HOST, (void*)device);

   rsd_set_param(rd, RSD_FORMAT, &format);

   rsd_set_callback(rd, rsound_audio_cb, rsound_err_cb, 256, rsd);

   /* The callback asks for 256 bytes at a time; at 16-bit stereo that
    * is 64 frames.  Floored so a high sample rate does not turn the
    * bounded wait into a spin. */
   rsd->wait_us   = rate
      ? (int64_t)(256 / 4) * 1000000 / rate
      : 1000;
   if (rsd->wait_us < 1000)
      rsd->wait_us = 1000;

   if (rsd_start(rd) < 0)
   {
      free(rsd);
      goto error;
   }

   rsd->rd = rd;
   return rsd;

error:
   rsd_free(rd);
   return NULL;
}

/* How many period-long waits a blocked write or wait_writable() may
 * take before giving up on the server making room. rsound_err_cb
 * covers a server that fails; this covers one that merely stops. */
#define RSOUND_WAIT_LAPS 8

static ssize_t rs_write(void *data, const void *buf, size_t len)
{
   size_t _len;
   rsd_t *rsd = (rsd_t*)data;

   if (rsd->has_error)
      return -1;

   if (rsd->nonblock)
   {
      size_t avail;

      rsd_callback_lock(rsd->rd);

      avail  = FIFO_WRITE_AVAIL(rsd->buffer);
      _len   = avail > len ? len : avail;

      fifo_write(rsd->buffer, buf, _len);
      rsd_callback_unlock(rsd->rd);
   }
   else
   {
      int laps = RSOUND_WAIT_LAPS;

      _len = 0;
      while (_len < len && !rsd->has_error)
      {
         size_t avail;
         rsd_callback_lock(rsd->rd);

         avail = FIFO_WRITE_AVAIL(rsd->buffer);

         if (avail == 0)
         {
            rsd_callback_unlock(rsd->rd);
            if (!rsd->has_error)
            {
               /* Timed, not indefinite.  The predicate is guarded by
                * librsound's callback lock, not cond_lock, and neither
                * rsound_audio_cb nor rsound_err_cb holds cond_lock
                * when it signals - so a signal raised between the
                * has_error test above and this wait reaches no waiter.
                *
                * rsound_err_cb is the case that matters: librsound
                * calls it and immediately returns from its worker
                * thread, at every one of its error exits.  It is
                * therefore the last signal that will ever be raised,
                * and losing it to the window left this thread parked
                * with nothing alive to wake it.  A timed wait returns
                * to the enclosing loop, which rechecks has_error. */
               slock_lock(rsd->cond_lock);
               scond_wait_timeout(rsd->cond, rsd->cond_lock,
                     rsd->wait_us);
               slock_unlock(rsd->cond_lock);
               /* And bounded overall: a server that stops draining
                * without erroring ends the write with what went. */
               if (--laps < 0)
                  break;
            }
         }
         else
         {
            size_t write_amt = len - _len > avail ? avail : len - _len;
            fifo_write(rsd->buffer, (const char*)buf + _len, write_amt);
            rsd_callback_unlock(rsd->rd);
            _len += write_amt;
         }
      }
      return _len;
   }
   return 0;
}

static bool rs_stop(void *data)
{
   rsd_t *rsd = (rsd_t*)data;
   rsd_stop(rsd->rd);
   rsd->is_paused = true;

   return true;
}

static void rs_set_nonblock_state(void *data, bool state)
{
   rsd_t *rsd    = (rsd_t*)data;
   rsd->nonblock = state;
}

static bool rs_alive(void *data)
{
   rsd_t *rsd = (rsd_t*)data;
   if (rsd)
      return !rsd->is_paused;
   return false;
}

static bool rs_start(void *data, bool is_shutdown)
{
   rsd_t *rsd = (rsd_t*)data;
   if (rsd_start(rsd->rd) < 0)
      return false;
   rsd->is_paused = false;

   return true;
}

static void rs_free(void *data)
{
   rsd_t *rsd = (rsd_t*)data;

   rsd_stop(rsd->rd);
   rsd_free(rsd->rd);

   fifo_free(rsd->buffer);
   slock_free(rsd->cond_lock);
   scond_free(rsd->cond);

   free(rsd);
}

static size_t rs_write_avail(void *data)
{
   size_t val;
   rsd_t *rsd = (rsd_t*)data;

   if (rsd->has_error)
      return 0;
   rsd_callback_lock(rsd->rd);
   val = FIFO_WRITE_AVAIL(rsd->buffer);
   rsd_callback_unlock(rsd->rd);
   return val;
}

/* TODO/FIXME - implement? */
/* The fifo: the stage the writer fills and write_avail() measures.
 * librsound's buffer behind it is kept full by its callback and adds
 * on top. */
static size_t rs_buffer_size(void *data)
{
   rsd_t *rsd = (rsd_t*)data;
   return rsd ? rsd->fifo_size : 0;
}

/* Sleep on the condition librsound's audio callback signals after every
 * pull until at least len bytes fit in the fifo, capped at half the
 * reported buffer so the wait always ends. Timed, for the same reason
 * rs_write()'s wait is: the error callback's signal can be lost. Returns
 * the free space then, or 0 once librsound has reported an error. */
static size_t rs_wait_writable(void *data, size_t len)
{
   rsd_t *rsd = (rsd_t*)data;
   size_t avail;
   int laps = RSOUND_WAIT_LAPS;

   if (len > rs_buffer_size(data) / 2)
      len = rs_buffer_size(data) / 2;

   for (;;)
   {
      if (rsd->has_error)
         return 0;
      rsd_callback_lock(rsd->rd);
      avail = FIFO_WRITE_AVAIL(rsd->buffer);
      rsd_callback_unlock(rsd->rd);
      if (avail >= len)
         return avail;
      slock_lock(rsd->cond_lock);
      scond_wait_timeout(rsd->cond, rsd->cond_lock, rsd->wait_us);
      slock_unlock(rsd->cond_lock);
      /* No room after this many waits: the server is not draining and
       * has not said so; the pass is handed back rather than waited
       * on further. */
      if (--laps < 0)
         return 0;
   }
}
static bool rs_use_float(void *data) { return false; }

audio_driver_t audio_rsound = {
   rs_init,
   rs_write,
   rs_stop,
   rs_start,
   rs_alive,
   rs_set_nonblock_state,
   rs_free,
   rs_use_float,
   "rsound",
   NULL,
   NULL,
   rs_write_avail,
   rs_buffer_size,
   NULL, /* write_raw */
   rs_wait_writable
};
