/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_atomic.h>

#include <psp2/audioout.h>

#include "../audio_driver.h"

typedef struct psp2_audio
{
   /* The ring, followed by one period of silence for the worker
    * to output when the ring runs short. */
   uint32_t* buffer_u32;

   sthread_t *worker_thread;
   /* The writer's park; the worker notifies after every period it
    * frees. Gated: the steady state, writer ahead and never waiting,
    * costs the worker nothing per period, where the old
    * signal-every-iteration took cond_lock each time. */
   retro_eventcount_t park;

   /* For the sink rate estimate and the statistics overlay.
    * Only the worker writes either. */
   retro_atomic_size_t consumed;
   retro_atomic_size_t underruns;

   int port;
   int rate;

   /* Frames the ring holds. Set before the worker starts, and
    * fixed for the life of the driver. */
   unsigned ring;

   /* The ring's index pair, the SPSC discipline by hand because the
    * worker consumes in place - psp->buffer_u32 + read_pos goes straight
    * into the output syscall, and a retro_spsc would add a bounce
    * copy per period. Producer publishes write_pos with release
    * after the samples land; consumer publishes read_pos with
    * release after the syscall returns; each side reads the other's
    * index with acquire and its own relaxed. fifo_lock guarded
    * nothing else, and is gone. */
   retro_atomic_int_t read_pos;
   retro_atomic_int_t write_pos;

   retro_atomic_int_t running;
   bool nonblock;
} psp2_audio_t;

#define AUDIO_OUT_COUNT 512u

/* The ring is what the latency setting asks for, so it is a whole
 * number of periods rather than a power of two and the wrap is a
 * compare. Five periods is the floor, which is the default latency
 * setting: the ring has to hold a delivery beside the period the
 * worker is playing, and a delivery is a video frame of audio - 1600
 * frames for 30 Hz content at 48 kHz. Four starves 14% of periods on
 * that content through the threaded pipeline and 93% inline, five
 * 0.3% and 4%. Anything at or above the default is unaffected. */
#define AUDIO_RING_MIN  (AUDIO_OUT_COUNT * 5u)
#define AUDIO_RING_MAX  (AUDIO_OUT_COUNT * 64u)

/* The period the output call holds while the device plays it. It is
 * buffering the driver controls, so buffer_size() counts it with the
 * ring, and the silence the worker hands over sits one period past
 * the ring's end. */
#define AUDIO_DEVICE_FRAMES  AUDIO_OUT_COUNT

/* Frames held, and frames the writer may still place - one short of
 * the ring, so a full ring and an empty one do not both read as
 * write_pos == read_pos. */
#define RING_HELD(w, r, n) \
   ((unsigned)((w) >= (r) ? (w) - (r) : (n) - (r) + (w)))
#define RING_FREE(w, r, n) ((n) - 1u - RING_HELD((w), (r), (n)))

/* Bound on any wait for the audio thread to consume: one wait, and how
 * many of them before the caller gets the pass back. The thread
 * consumes a period every period while the device runs. */
#define PSP2_AUDIO_WAIT_US   100000
#define PSP2_AUDIO_WAIT_LAPS 8

/* The only rate the main port opens at. */
#define PSP2_AUDIO_RATE 48000

/* The ring the latency setting asks for: that many milliseconds at
 * the rate the device opened at, less the period the device holds,
 * in whole periods so no window the worker hands over crosses the
 * ring's end. */
static unsigned psp2_ring_frames(unsigned rate, unsigned latency)
{
   unsigned frames = rate * latency / 1000u;
   frames = (frames > AUDIO_DEVICE_FRAMES)
      ? frames - AUDIO_DEVICE_FRAMES : 0u;
   frames = ((frames + AUDIO_OUT_COUNT - 1u) / AUDIO_OUT_COUNT)
      * AUDIO_OUT_COUNT;
   if (frames < AUDIO_RING_MIN)
      return AUDIO_RING_MIN;
   if (frames > AUDIO_RING_MAX)
      return AUDIO_RING_MAX;
   return frames;
}

/* Return port used */
static int psp2_configure_audio(unsigned rate, unsigned *new_rate)
{
   /* The main port refuses every other rate, which leaves the session
    * with no audio at all; open at the one it takes and tell the
    * frontend to resample to it. */
   if (rate != PSP2_AUDIO_RATE)
      *new_rate = PSP2_AUDIO_RATE;
   return sceAudioOutOpenPort(
         SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_OUT_COUNT,
         PSP2_AUDIO_RATE, SCE_AUDIO_OUT_MODE_STEREO);
}

static void psp2_audio_mainloop(void *data)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;

   while (retro_atomic_load_acquire_int(&psp->running))
   {
      bool cond           = false;
      uint16_t read_pos   = (uint16_t)
            retro_atomic_load_relaxed_int(&psp->read_pos);
      uint16_t read_pos_2 = read_pos;
      uint16_t write_pos  = (uint16_t)
            retro_atomic_load_acquire_int(&psp->write_pos);

      /* A period in hand is a period to play. Holding one back
       * as a reserve only hands the device silence while the
       * ring has audio in it. */
      cond                = RING_HELD(write_pos, read_pos, psp->ring)
            < AUDIO_OUT_COUNT;

      if (!cond)
      {
         read_pos      += AUDIO_OUT_COUNT;
         if (read_pos  >= psp->ring)
            read_pos    = 0;
      }
      else
         retro_atomic_fetch_add_size(&psp->underruns, 1);

      sceAudioOutOutput(psp->port,
            psp->buffer_u32
            + (cond ? psp->ring : read_pos_2));

      retro_atomic_fetch_add_size(&psp->consumed, AUDIO_OUT_COUNT);

      /* Release only now: the call returns once the device has
       * taken the window, so the period is the writer's from
       * here and not before. */
      if (!cond)
         retro_atomic_store_release_int(&psp->read_pos, read_pos);

      retro_eventcount_notify(&psp->park);
   }

   return;
}

static void *psp2_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   int port;
   psp2_audio_t *psp = (psp2_audio_t*)calloc(1, sizeof(psp2_audio_t));

   if (!psp)
      return NULL;

   if ((port = psp2_configure_audio(rate, new_rate)) < 0)
   {
      free(psp);
      return NULL;
   }

   psp->rate          = *new_rate ? *new_rate : rate;
   psp->ring          = psp2_ring_frames(psp->rate, latency);

   /* Cache aligned, not necessary but helpful. */
   psp->buffer_u32    = (uint32_t*)calloc(psp->ring + AUDIO_DEVICE_FRAMES,
         sizeof(uint32_t));

   retro_atomic_size_init(&psp->consumed, 0);
   retro_atomic_size_init(&psp->underruns, 0);
   retro_atomic_int_init(&psp->read_pos, 0);
   retro_atomic_int_init(&psp->write_pos, 0);
   psp->port          = port;

   if (   !psp->buffer_u32
       || !retro_eventcount_init(&psp->park))
   {
      sceAudioOutReleasePort(port);
      free(psp->buffer_u32);
      free(psp);
      return NULL;
   }

   psp->nonblock      = false;
   /* running has to be set before the worker starts, so a worker that
    * never starts leaves a driver reporting itself alive with nothing
    * consuming the ring: writes wait out their laps and return zero,
    * and teardown acts on a state that was never true. */
   retro_atomic_int_init(&psp->running, 1);
   if (!(psp->worker_thread = sthread_create(psp2_audio_mainloop, psp)))
   {
      retro_eventcount_free(&psp->park);
      sceAudioOutReleasePort(port);
      free(psp->buffer_u32);
      free(psp);
      return NULL;
   }

   return psp;
}

static void psp2_audio_free(void *data)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;
   if (!psp)
      return;

   if (retro_atomic_load_acquire_int(&psp->running))
   {
      if (psp->worker_thread)
      {
         retro_atomic_store_release_int(&psp->running, 0);
         sthread_join(psp->worker_thread);
      }
   }
   retro_eventcount_free(&psp->park);
   free(psp->buffer_u32);
   psp->worker_thread = NULL;

   sceAudioOutReleasePort(psp->port);

   free(psp);

}

static ssize_t psp2_audio_write(void *data, const void *s, size_t len)
{
   psp2_audio_t* psp     = (psp2_audio_t*)data;
   uint16_t write_pos    = (uint16_t)
         retro_atomic_load_relaxed_int(&psp->write_pos);
   /* Frames, and wide enough for them: narrowed to uint16_t, a write of
    * more than 65535 frames wrapped to a small count that the room check
    * then waved through, and the copy below took len bytes for it. The
    * ring holds one frame short of its length, so no wait can free room
    * for more than that at once - take what fits and report the short
    * write rather than spending the laps on room that will not come. A
    * len that is not whole frames carries the frames it has. */
   size_t   sample_count = len / sizeof(uint32_t);

   if (!retro_atomic_load_acquire_int(&psp->running))
      return -1;
   if (!sample_count)
      return 0;
   if (sample_count > (size_t)(psp->ring - 1u))
      sample_count = psp->ring - 1u;
   len = sample_count * sizeof(uint32_t);

   /* The ring is counted in uint32_t frames (write_pos, read_pos,
    * ring); len is bytes.  Both room checks below used to
    * compare the frame count against len, i.e. demanded four times the
    * room actually needed: non-blocking writes were refused - the audio
    * dropped - with plenty of space free, and blocking ones waited for
    * space that rate control was not trying to free.  psp2_write_avail()
    * and psp2_wait_writable() already convert; compare frames to
    * frames here too. */
   if (psp->nonblock)
   {
      if (RING_FREE(write_pos, (uint16_t)
               retro_atomic_load_acquire_int(&psp->read_pos),
               psp->ring) < sample_count)
         return 0;
   }

   {
      /* The audio thread notifies every period it consumes. One that
       * has stopped consuming - the device suspended, the thread gone
       * - notifies nothing; each wait is bounded, the loop is
       * lap-bounded, and the write then returns having written
       * nothing rather than holding the caller. Room and liveness
       * are re-checked inside the eventcount's window, so a period
       * freed between the check and the park costs nothing. */
      int laps = PSP2_AUDIO_WAIT_LAPS;
      while (RING_FREE(write_pos, (uint16_t)
         retro_atomic_load_acquire_int(&psp->read_pos),
         psp->ring) < sample_count)
      {
         int key;
         if (--laps < 0 || !retro_atomic_load_acquire_int(&psp->running))
            return 0;
         key = retro_eventcount_prepare_wait(&psp->park);
         if (   (RING_FREE(write_pos, (uint16_t)
                  retro_atomic_load_acquire_int(&psp->read_pos),
                  psp->ring) >= sample_count)
             || !retro_atomic_load_acquire_int(&psp->running))
         {
            retro_eventcount_cancel_wait(&psp->park);
            continue;
         }
         if (!retro_eventcount_commit_wait_timeout(&psp->park, key,
                  PSP2_AUDIO_WAIT_US))
            continue;
      }
   }

   if ((write_pos + sample_count) > psp->ring)
   {
      memcpy(psp->buffer_u32 + write_pos, s,
            (psp->ring - write_pos) * sizeof(uint32_t));
      memcpy(psp->buffer_u32, (uint32_t*)s +
            (psp->ring - write_pos),
            (write_pos + sample_count - psp->ring) * sizeof(uint32_t));
   }
   else
      memcpy(psp->buffer_u32 + write_pos, s, len);

   write_pos       = (uint16_t)(write_pos + sample_count);
   if (write_pos  >= psp->ring)
      write_pos   -= psp->ring;
   /* Release: the samples land before the index that publishes
    * them. */
   retro_atomic_store_release_int(&psp->write_pos, write_pos);
   return len;
}

static bool psp2_audio_alive(void *data)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;
   if (!psp)
      return false;
   return retro_atomic_load_acquire_int(&psp->running) != 0;
}

static bool psp2_audio_stop(void *data)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;

   if (psp)
   {
      retro_atomic_store_release_int(&psp->running, 0);
      /* A writer parked on the ring must see the flag drop; the
       * worker may already be gone and notify nothing further. */
      retro_eventcount_notify(&psp->park);

      if (psp->worker_thread)
      {
         sthread_join(psp->worker_thread);
         psp->worker_thread = NULL;
      }
   }
   return true;
}

static bool psp2_audio_start(void *data, bool is_shutdown)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;

   if (psp && !retro_atomic_load_acquire_int(&psp->running))
   {
      if (!psp->worker_thread)
      {
         retro_atomic_store_release_int(&psp->running, 1);
         if (!(psp->worker_thread = sthread_create(psp2_audio_mainloop, psp)))
         {
            retro_atomic_store_release_int(&psp->running, 0);
            return false;
         }
      }
   }

   return true;
}

static void psp2_audio_set_nonblock_state(void *data, bool toggle)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;
   if (psp)
      psp->nonblock = toggle;
}

static size_t psp2_write_avail(void *data)
{
   size_t _len;
   psp2_audio_t* psp = (psp2_audio_t*)data;

   if (!psp || !retro_atomic_load_acquire_int(&psp->running))
      return 0;
   _len = RING_FREE((uint16_t)
         retro_atomic_load_relaxed_int(&psp->write_pos), (uint16_t)
         retro_atomic_load_acquire_int(&psp->read_pos), psp->ring);
   return _len * sizeof(uint32_t);
}

/* Sleep on the condition the output thread signals after every block
 * until the fifo has room for len, in the same units psp2_audio_write()
 * compares against, capped at half the fifo so the wait always ends.
 * Returns the free space as psp2_write_avail() reports it, or 0 when
 * the output thread is not running. */
static size_t psp2_wait_writable(void *data, size_t len)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;
   size_t avail;
   int laps         = PSP2_AUDIO_WAIT_LAPS;
   /* len arrives in bytes; the ring is counted in uint32_t frames. */
   size_t want      = len / sizeof(uint32_t);

   if (want > psp->ring / 2)
      want = psp->ring / 2;

   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&psp->running))
         return 0;
      avail = RING_FREE((uint16_t)
            retro_atomic_load_relaxed_int(&psp->write_pos), (uint16_t)
            retro_atomic_load_acquire_int(&psp->read_pos), psp->ring);
      if (avail >= want)
         break;
      /* Bounded per wait and overall: a thread that has stopped
       * consuming hands the pass back as no space coming from this
       * call. The room is re-checked inside the window. */
      if (--laps < 0)
         return 0;
      key = retro_eventcount_prepare_wait(&psp->park);
      if (RING_FREE((uint16_t)
               retro_atomic_load_relaxed_int(&psp->write_pos), (uint16_t)
               retro_atomic_load_acquire_int(&psp->read_pos),
               psp->ring) >= want
            || !retro_atomic_load_acquire_int(&psp->running))
      {
         retro_eventcount_cancel_wait(&psp->park);
         continue;
      }
      if (!retro_eventcount_commit_wait_timeout(&psp->park, key,
               PSP2_AUDIO_WAIT_US))
         continue;
   }
   return avail * sizeof(uint32_t);
}

/* sceAudioOut takes 16-bit PCM only; there is no float output on the
 * Vita hardware or in the kernel API. */
static bool psp2_audio_use_float(void *data) { return false; }
static size_t psp2_buffer_size(void *data)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;
   if (!psp)
      return 0;
   /* In bytes: the ring plus the period the device holds, in
    * uint32_t frames of int16 stereo. */
   return (psp->ring + AUDIO_DEVICE_FRAMES) * sizeof(uint32_t);
}

/* Frames the device has taken since init: the output call returns
 * when it has taken the one before, so the worker counts a period
 * per completed call. */
static size_t psp2_frames_consumed(void *data)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;
   if (!psp)
      return 0;
   return retro_atomic_load_acquire_size(&psp->consumed);
}

/* Periods played as silence for want of audio. */
static size_t psp2_underruns(void *data)
{
   psp2_audio_t* psp = (psp2_audio_t*)data;
   if (!psp)
      return 0;
   return retro_atomic_load_acquire_size(&psp->underruns);
}

audio_driver_t audio_psp2 = {
   psp2_audio_init,
   psp2_audio_write,
   psp2_audio_stop,
   psp2_audio_start,
   psp2_audio_alive,
   psp2_audio_set_nonblock_state,
   psp2_audio_free,
   psp2_audio_use_float,
   "vita",
   NULL,
   NULL,
   psp2_write_avail,
   psp2_buffer_size,
   NULL, /* write_raw */
   psp2_wait_writable,
   psp2_frames_consumed,
   psp2_underruns
};
