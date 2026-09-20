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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_atomic.h>

#include <libSceAudioOut.h>
#include <defines/ps4_defines.h>

#include "../audio_driver.h"

typedef struct ps4_audio
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
    * worker consumes in place - ps4->buffer_u32 + read_pos goes straight
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
} ps4_audio_t;

#define AUDIO_OUT_COUNT 512u
#define AUDIO_BUFFER_SIZE (1u<<13u)
#define AUDIO_BUFFER_SIZE_MASK (AUDIO_BUFFER_SIZE-1)

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
#define PS4_AUDIO_WAIT_US   100000
#define PS4_AUDIO_WAIT_LAPS 8

/* The only rate sceAudioOut opens at. */
#define PS4_AUDIO_RATE 48000

/* Return port used */
static int ps4_configure_audio(unsigned rate, unsigned *new_rate)
{
   /* Every other rate is refused, which leaves the session with no
    * audio at all; open at the one it takes and tell the frontend to
    * resample to it. */
   if (rate != PS4_AUDIO_RATE)
      *new_rate = PS4_AUDIO_RATE;
   return sceAudioOutOpen(0xff,
         SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0, AUDIO_OUT_COUNT,
         PS4_AUDIO_RATE, SCE_AUDIO_OUT_MODE_STEREO);
}

static void ps4_audio_mainloop(void *data)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;

   while (retro_atomic_load_acquire_int(&ps4->running))
   {
      bool cond           = false;
      uint16_t read_pos   = (uint16_t)
            retro_atomic_load_relaxed_int(&ps4->read_pos);
      uint16_t read_pos_2 = read_pos;
      uint16_t write_pos  = (uint16_t)
            retro_atomic_load_acquire_int(&ps4->write_pos);

      cond                = ((uint16_t)(write_pos - read_pos) & AUDIO_BUFFER_SIZE_MASK)
            < (AUDIO_OUT_COUNT * 2);

      if (!cond)
      {
         read_pos      += AUDIO_OUT_COUNT;
         read_pos      &= AUDIO_BUFFER_SIZE_MASK;
      }
      else
         retro_atomic_fetch_add_size(&ps4->underruns, 1);

      sceAudioOutOutput(ps4->port,
            ps4->buffer_u32
            + (cond ? AUDIO_SILENCE_OFFSET : read_pos_2));

      retro_atomic_fetch_add_size(&ps4->consumed, AUDIO_OUT_COUNT);

      /* Release only now: the call returns once the device has
       * taken the window, so the period is the writer's from
       * here and not before. */
      if (!cond)
         retro_atomic_store_release_int(&ps4->read_pos, read_pos);

      retro_eventcount_notify(&ps4->park);
   }

   return;
}

static void *ps4_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   int port;
   ps4_audio_t *ps4 = (ps4_audio_t*)calloc(1, sizeof(ps4_audio_t));

   if (!ps4)
      return NULL;

   /* The library is what opens the port, so it comes up first. */
   sceAudioOutInit();

   if ((port = ps4_configure_audio(rate, new_rate)) < 0)
   {
      free(ps4);
      return NULL;
   }

   /* Cache aligned, not necessary but helpful. */
   ps4->buffer_u32    = (uint32_t*)calloc(AUDIO_ARENA_COUNT, sizeof(uint32_t));

   retro_atomic_size_init(&ps4->consumed, 0);
   retro_atomic_size_init(&ps4->underruns, 0);
   retro_atomic_int_init(&ps4->read_pos, 0);
   retro_atomic_int_init(&ps4->write_pos, 0);
   ps4->port          = port;

   if (   !ps4->buffer_u32
       || !retro_eventcount_init(&ps4->park))
   {
      sceAudioOutClose(port);
      free(ps4->buffer_u32);
      free(ps4);
      return NULL;
   }

   ps4->nonblock      = false;
   retro_atomic_int_init(&ps4->running, 1);
   ps4->worker_thread = sthread_create(ps4_audio_mainloop, ps4);

   return ps4;
}

static void ps4_audio_free(void *data)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;
   if (!ps4)
      return;

   if (retro_atomic_load_acquire_int(&ps4->running))
   {
      if (ps4->worker_thread)
      {
         retro_atomic_store_release_int(&ps4->running, 0);
         sthread_join(ps4->worker_thread);
      }
   }
   retro_eventcount_free(&ps4->park);
   free(ps4->buffer_u32);
   ps4->worker_thread = NULL;

   sceAudioOutClose(ps4->port);

   free(ps4);

}

static ssize_t ps4_audio_write(void *data, const void *s, size_t len)
{
   ps4_audio_t* ps4      = (ps4_audio_t*)data;
   uint16_t write_pos    = (uint16_t)
         retro_atomic_load_relaxed_int(&ps4->write_pos);
   uint16_t sample_count = len / sizeof(uint32_t);

   if (!retro_atomic_load_acquire_int(&ps4->running))
      return -1;

   /* The ring is counted in uint32_t frames (write_pos, read_pos,
    * AUDIO_BUFFER_SIZE); len is bytes.  Both room checks below used to
    * compare the frame count against len, i.e. demanded four times the
    * room actually needed: non-blocking writes were refused - the audio
    * dropped - with plenty of space free, and blocking ones waited for
    * space that rate control was not trying to free.  ps4_write_avail()
    * and ps4_wait_writable() already convert; compare frames to
    * frames here too. */
   if (ps4->nonblock)
   {
      if (AUDIO_BUFFER_SIZE - ((uint16_t)(write_pos - (uint16_t)
               retro_atomic_load_acquire_int(&ps4->read_pos))
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
      int laps = PS4_AUDIO_WAIT_LAPS;
      while (AUDIO_BUFFER_SIZE - ((uint16_t)(write_pos - (uint16_t)
         retro_atomic_load_acquire_int(&ps4->read_pos))
         & AUDIO_BUFFER_SIZE_MASK) < sample_count)
      {
         int key;
         if (--laps < 0 || !retro_atomic_load_acquire_int(&ps4->running))
            return 0;
         key = retro_eventcount_prepare_wait(&ps4->park);
         if (   (AUDIO_BUFFER_SIZE - ((uint16_t)(write_pos - (uint16_t)
                  retro_atomic_load_acquire_int(&ps4->read_pos))
                  & AUDIO_BUFFER_SIZE_MASK) >= sample_count)
             || !retro_atomic_load_acquire_int(&ps4->running))
         {
            retro_eventcount_cancel_wait(&ps4->park);
            continue;
         }
         if (!retro_eventcount_commit_wait_timeout(&ps4->park, key,
                  PS4_AUDIO_WAIT_US))
            continue;
      }
   }

   if ((write_pos + sample_count) > AUDIO_BUFFER_SIZE)
   {
      memcpy(ps4->buffer_u32 + write_pos, s,
            (AUDIO_BUFFER_SIZE - write_pos) * sizeof(uint32_t));
      memcpy(ps4->buffer_u32, (uint32_t*)s +
            (AUDIO_BUFFER_SIZE - write_pos),
            (write_pos + sample_count - AUDIO_BUFFER_SIZE) * sizeof(uint32_t));
   }
   else
      memcpy(ps4->buffer_u32 + write_pos, s, len);

   write_pos      += sample_count;
   write_pos      &= AUDIO_BUFFER_SIZE_MASK;
   /* Release: the samples land before the index that publishes
    * them. */
   retro_atomic_store_release_int(&ps4->write_pos, write_pos);
   return len;
}

static bool ps4_audio_alive(void *data)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;
   if (!ps4)
      return false;
   return retro_atomic_load_acquire_int(&ps4->running) != 0;
}

static bool ps4_audio_stop(void *data)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;

   if (ps4)
   {
      retro_atomic_store_release_int(&ps4->running, 0);
      /* A writer parked on the ring must see the flag drop; the
       * worker may already be gone and notify nothing further. */
      retro_eventcount_notify(&ps4->park);

      if (ps4->worker_thread)
      {
         sthread_join(ps4->worker_thread);
         ps4->worker_thread = NULL;
      }
   }
   return true;
}

static bool ps4_audio_start(void *data, bool is_shutdown)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;

   if (ps4 && !retro_atomic_load_acquire_int(&ps4->running))
   {
      if (!ps4->worker_thread)
      {
         retro_atomic_store_release_int(&ps4->running, 1);
         ps4->worker_thread = sthread_create(ps4_audio_mainloop, ps4);
      }
   }

   return true;
}

static void ps4_audio_set_nonblock_state(void *data, bool toggle)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;
   if (ps4)
      ps4->nonblock = toggle;
}

static size_t ps4_write_avail(void *data)
{
   size_t _len;
   ps4_audio_t* ps4 = (ps4_audio_t*)data;

   if (!ps4 || !retro_atomic_load_acquire_int(&ps4->running))
      return 0;
   _len = AUDIO_BUFFER_SIZE - ((uint16_t)((uint16_t)
         retro_atomic_load_relaxed_int(&ps4->write_pos) - (uint16_t)
         retro_atomic_load_acquire_int(&ps4->read_pos))
         & AUDIO_BUFFER_SIZE_MASK);
   return _len * sizeof(uint32_t);
}

/* Sleep on the condition the output thread signals after every block
 * until the fifo has room for len, in the same units ps4_audio_write()
 * compares against, capped at half the fifo so the wait always ends.
 * Returns the free space as ps4_write_avail() reports it, or 0 when
 * the output thread is not running. */
static size_t ps4_wait_writable(void *data, size_t len)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;
   size_t avail;
   int laps         = PS4_AUDIO_WAIT_LAPS;
   /* len arrives in bytes; the ring is counted in uint32_t frames. */
   size_t want      = len / sizeof(uint32_t);

   if (want > AUDIO_BUFFER_SIZE / 2)
      want = AUDIO_BUFFER_SIZE / 2;

   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&ps4->running))
         return 0;
      avail = AUDIO_BUFFER_SIZE - ((uint16_t)((uint16_t)
            retro_atomic_load_relaxed_int(&ps4->write_pos) - (uint16_t)
            retro_atomic_load_acquire_int(&ps4->read_pos))
            & AUDIO_BUFFER_SIZE_MASK);
      if (avail >= want)
         break;
      /* Bounded per wait and overall: a thread that has stopped
       * consuming hands the pass back as no space coming from this
       * call. The room is re-checked inside the window. */
      if (--laps < 0)
         return 0;
      key = retro_eventcount_prepare_wait(&ps4->park);
      if ((AUDIO_BUFFER_SIZE - ((uint16_t)((uint16_t)
               retro_atomic_load_relaxed_int(&ps4->write_pos) - (uint16_t)
               retro_atomic_load_acquire_int(&ps4->read_pos))
               & AUDIO_BUFFER_SIZE_MASK)) >= want
            || !retro_atomic_load_acquire_int(&ps4->running))
      {
         retro_eventcount_cancel_wait(&ps4->park);
         continue;
      }
      if (!retro_eventcount_commit_wait_timeout(&ps4->park, key,
               PS4_AUDIO_WAIT_US))
         continue;
   }
   return avail * sizeof(uint32_t);
}

/* sceAudioOut is opened in SCE_AUDIO_OUT_MODE_STEREO, which is 16-bit
 * PCM; float output would need a different port mode. */
static bool ps4_audio_use_float(void *data) { return false; }
static size_t ps4_buffer_size(void *data)
{
   /* In bytes: the ring plus the period the device holds, in
    * uint32_t frames of int16 stereo. */
   return (AUDIO_BUFFER_SIZE + AUDIO_DEVICE_FRAMES) * sizeof(uint32_t);
}

/* Frames the device has taken since init: the output call returns
 * when it has taken the one before, so the worker counts a period
 * per completed call. */
static size_t ps4_frames_consumed(void *data)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;
   if (!ps4)
      return 0;
   return retro_atomic_load_acquire_size(&ps4->consumed);
}

/* Periods played as silence for want of audio. */
static size_t ps4_underruns(void *data)
{
   ps4_audio_t* ps4 = (ps4_audio_t*)data;
   if (!ps4)
      return 0;
   return retro_atomic_load_acquire_size(&ps4->underruns);
}

audio_driver_t audio_ps4 = {
   ps4_audio_init,
   ps4_audio_write,
   ps4_audio_stop,
   ps4_audio_start,
   ps4_audio_alive,
   ps4_audio_set_nonblock_state,
   ps4_audio_free,
   ps4_audio_use_float,
   "orbis",
   NULL,
   NULL,
   ps4_write_avail,
   ps4_buffer_size,
   NULL, /* write_raw */
   ps4_wait_writable,
   ps4_frames_consumed,
   ps4_underruns
};
