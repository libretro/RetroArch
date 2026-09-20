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

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
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

   SceUID thread;

   int port;
   int rate;

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
#define AUDIO_BUFFER_SIZE (1u<<13u)
#define AUDIO_BUFFER_SIZE_MASK (AUDIO_BUFFER_SIZE-1)

/* What the writer may fill. One frame short of the ring, so that a
 * full ring and an empty one do not both read as write_pos ==
 * read_pos - which let the writer lap and overwrite the window the
 * device was reading. */
#define AUDIO_BUFFER_USABLE  (AUDIO_BUFFER_SIZE - 1u)

/* The silence sits past the ring. read_pos only ever advances by
 * AUDIO_OUT_COUNT, which divides AUDIO_BUFFER_SIZE, so no window
 * handed to the output syscall crosses into it. */
#define AUDIO_SILENCE_OFFSET AUDIO_BUFFER_SIZE
#define AUDIO_ARENA_COUNT    (AUDIO_BUFFER_SIZE + AUDIO_OUT_COUNT)

/* The period the output call holds while the device plays it.
 * It is buffering the driver controls, so buffer_size() counts
 * it with the ring. */
#define AUDIO_DEVICE_FRAMES  AUDIO_OUT_COUNT

/* Bound on any wait for the audio thread to consume: one wait, and how
 * many of them before the caller gets the pass back. The thread
 * consumes a period every period while the device runs. */
#define PSP2_AUDIO_WAIT_US   100000
#define PSP2_AUDIO_WAIT_LAPS 8

/* The only rate the main port opens at. */
#define PSP2_AUDIO_RATE 48000

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

      cond                = ((uint16_t)(write_pos - read_pos) & AUDIO_BUFFER_SIZE_MASK)
            < (AUDIO_OUT_COUNT * 2);

      if (!cond)
      {
         read_pos      += AUDIO_OUT_COUNT;
         read_pos      &= AUDIO_BUFFER_SIZE_MASK;
      }
      else
         retro_atomic_fetch_add_size(&psp->underruns, 1);

      sceAudioOutOutput(psp->port,
            psp->buffer_u32
            + (cond ? AUDIO_SILENCE_OFFSET : read_pos_2));

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

   /* Cache aligned, not necessary but helpful. */
   psp->buffer_u32    = (uint32_t*)calloc(AUDIO_ARENA_COUNT, sizeof(uint32_t));

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
   retro_atomic_int_init(&psp->running, 1);
   psp->worker_thread = sthread_create(psp2_audio_mainloop, psp);

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
   uint16_t sample_count = len / sizeof(uint32_t);

   if (!retro_atomic_load_acquire_int(&psp->running))
      return -1;

   /* The ring is counted in uint32_t frames (write_pos, read_pos,
    * AUDIO_BUFFER_SIZE); len is bytes.  Both room checks below used to
    * compare the frame count against len, i.e. demanded four times the
    * room actually needed: non-blocking writes were refused - the audio
    * dropped - with plenty of space free, and blocking ones waited for
    * space that rate control was not trying to free.  psp2_write_avail()
    * and psp2_wait_writable() already convert; compare frames to
    * frames here too. */
   if (psp->nonblock)
   {
      if (AUDIO_BUFFER_USABLE - ((uint16_t)(write_pos - (uint16_t)
               retro_atomic_load_acquire_int(&psp->read_pos))
               & AUDIO_BUFFER_SIZE_MASK) < sample_count)
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
      while (AUDIO_BUFFER_USABLE - ((uint16_t)(write_pos - (uint16_t)
         retro_atomic_load_acquire_int(&psp->read_pos))
         & AUDIO_BUFFER_SIZE_MASK) < sample_count)
      {
         int key;
         if (--laps < 0 || !retro_atomic_load_acquire_int(&psp->running))
            return 0;
         key = retro_eventcount_prepare_wait(&psp->park);
         if (   (AUDIO_BUFFER_USABLE - ((uint16_t)(write_pos - (uint16_t)
                  retro_atomic_load_acquire_int(&psp->read_pos))
                  & AUDIO_BUFFER_SIZE_MASK) >= sample_count)
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

   if ((write_pos + sample_count) > AUDIO_BUFFER_SIZE)
   {
      memcpy(psp->buffer_u32 + write_pos, s,
            (AUDIO_BUFFER_SIZE - write_pos) * sizeof(uint32_t));
      memcpy(psp->buffer_u32, (uint32_t*)s +
            (AUDIO_BUFFER_SIZE - write_pos),
            (write_pos + sample_count - AUDIO_BUFFER_SIZE) * sizeof(uint32_t));
   }
   else
      memcpy(psp->buffer_u32 + write_pos, s, len);

   write_pos      += sample_count;
   write_pos      &= AUDIO_BUFFER_SIZE_MASK;
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
         psp->worker_thread = sthread_create(psp2_audio_mainloop, psp);
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
   _len = AUDIO_BUFFER_USABLE - ((uint16_t)((uint16_t)
         retro_atomic_load_relaxed_int(&psp->write_pos) - (uint16_t)
         retro_atomic_load_acquire_int(&psp->read_pos))
         & AUDIO_BUFFER_SIZE_MASK);
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

   if (want > AUDIO_BUFFER_SIZE / 2)
      want = AUDIO_BUFFER_SIZE / 2;

   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&psp->running))
         return 0;
      avail = AUDIO_BUFFER_USABLE - ((uint16_t)((uint16_t)
            retro_atomic_load_relaxed_int(&psp->write_pos) - (uint16_t)
            retro_atomic_load_acquire_int(&psp->read_pos))
            & AUDIO_BUFFER_SIZE_MASK);
      if (avail >= want)
         break;
      /* Bounded per wait and overall: a thread that has stopped
       * consuming hands the pass back as no space coming from this
       * call. The room is re-checked inside the window. */
      if (--laps < 0)
         return 0;
      key = retro_eventcount_prepare_wait(&psp->park);
      if ((AUDIO_BUFFER_USABLE - ((uint16_t)((uint16_t)
               retro_atomic_load_relaxed_int(&psp->write_pos) - (uint16_t)
               retro_atomic_load_acquire_int(&psp->read_pos))
               & AUDIO_BUFFER_SIZE_MASK)) >= want
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
   /* In bytes: the ring plus the period the device holds, in
    * uint32_t frames of int16 stereo. */
   return (AUDIO_BUFFER_SIZE + AUDIO_DEVICE_FRAMES) * sizeof(uint32_t);
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
