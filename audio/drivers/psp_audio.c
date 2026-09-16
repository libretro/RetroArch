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
#if defined(VITA) || defined(PSP)
#include <malloc.h>
#endif
#include <stdio.h>
#include <string.h>

#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_atomic.h>

#if defined(VITA)
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/audioout.h>
#elif defined(PSP)
#include <pspkernel.h>
#include <pspaudio.h>
#elif defined(ORBIS)
#include <libSceAudioOut.h>
#include <defines/ps4_defines.h>
#include <verbosity.h>
#endif

#include "../audio_driver.h"

typedef struct psp_audio
{
   uint32_t* buffer;
   uint32_t* zeroBuffer;

   sthread_t *worker_thread;
   /* The writer's park; the worker notifies after every period it
    * frees. Gated: the steady state, writer ahead and never waiting,
    * costs the worker nothing per period, where the old
    * signal-every-iteration took cond_lock each time. */
   retro_eventcount_t park;

   SceUID thread;

   int port;
   int rate;

   /* The ring's index pair, the SPSC discipline by hand because the
    * worker consumes in place - psp->buffer + read_pos goes straight
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
} psp_audio_t;

#define AUDIO_OUT_COUNT 512u
#define AUDIO_BUFFER_SIZE (1u<<13u)
#define AUDIO_BUFFER_SIZE_MASK (AUDIO_BUFFER_SIZE-1)

/* Bound on any wait for the audio thread to consume: one wait, and how
 * many of them before the caller gets the pass back. The thread
 * consumes a period every period while the device runs. */
#define PSP_AUDIO_WAIT_US   100000
#define PSP_AUDIO_WAIT_LAPS 8

/* Return port used */
static int psp_configure_audio(unsigned rate)
{
#if defined(VITA)
   return sceAudioOutOpenPort(
         SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_OUT_COUNT,
         rate, SCE_AUDIO_OUT_MODE_STEREO);
#elif defined(ORBIS)
   return sceAudioOutOpen(0xff,
         SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0, AUDIO_OUT_COUNT,
         rate, SCE_AUDIO_OUT_MODE_STEREO);
#else
   return sceAudioSRCChReserve(AUDIO_OUT_COUNT, rate, 2);
#endif
}

static void psp_audio_mainloop(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;

   while (retro_atomic_load_acquire_int(&psp->running))
   {
      bool cond           = false;
      uint16_t read_pos   = (uint16_t)
            retro_atomic_load_relaxed_int(&psp->read_pos);
#if defined(VITA) || defined(ORBIS)
      uint16_t read_pos_2 = read_pos;
#endif
      uint16_t write_pos  = (uint16_t)
            retro_atomic_load_acquire_int(&psp->write_pos);

      cond                = ((uint16_t)(write_pos - read_pos) & AUDIO_BUFFER_SIZE_MASK)
            < (AUDIO_OUT_COUNT * 2);

      if (!cond)
      {
         read_pos      += AUDIO_OUT_COUNT;
         read_pos      &= AUDIO_BUFFER_SIZE_MASK;
         /* Release: the period is free for the writer only after
          * this store; the syscall below reads the old window, which
          * the writer cannot touch until it sees the new index. */
         retro_atomic_store_release_int(&psp->read_pos, read_pos);
      }

      retro_eventcount_notify(&psp->park);

#if defined(VITA) || defined(ORBIS)
      sceAudioOutOutput(psp->port,
        cond ? (psp->zeroBuffer)
              : (psp->buffer + read_pos_2));
#else
      sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX,
              cond
            ? (psp->zeroBuffer)
            : (psp->buffer + read_pos));
#endif
   }

   return;
}

static void *psp_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   int port;
   psp_audio_t *psp = (psp_audio_t*)calloc(1, sizeof(psp_audio_t));

   if (!psp)
      return NULL;

   if ((port = psp_configure_audio(rate)) < 0)
   {
      free(psp);
      return NULL;
   }

#if defined(ORBIS)
   sceAudioOutInit();
#endif
   /* Cache aligned, not necessary but helpful. */
   psp->buffer        = (uint32_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(uint32_t));
   memset(psp->buffer, 0, AUDIO_BUFFER_SIZE * sizeof(uint32_t));

   psp->zeroBuffer    = (uint32_t*)malloc(AUDIO_OUT_COUNT   * sizeof(uint32_t));
   memset(psp->zeroBuffer, 0, AUDIO_OUT_COUNT * sizeof(uint32_t));

   retro_atomic_int_init(&psp->read_pos, 0);
   retro_atomic_int_init(&psp->write_pos, 0);
   psp->port          = port;

   if (!retro_eventcount_init(&psp->park))
   {
      free(psp->buffer);
      free(psp->zeroBuffer);
      free(psp);
      return NULL;
   }

   psp->nonblock      = false;
   retro_atomic_int_init(&psp->running, 1);
   psp->worker_thread = sthread_create(psp_audio_mainloop, psp);

   return psp;
}

static void psp_audio_free(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;
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
   free(psp->buffer);
   psp->worker_thread = NULL;
   free(psp->zeroBuffer);

#if defined(VITA)
      sceAudioOutReleasePort(psp->port);
#elif defined(ORBIS)
      sceAudioOutClose(psp->port);
#else
      sceAudioSRCChRelease();
#endif

   free(psp);

}

static ssize_t psp_audio_write(void *data, const void *s, size_t len)
{
   psp_audio_t* psp      = (psp_audio_t*)data;
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
    * space that rate control was not trying to free.  psp_write_avail()
    * and psp_wait_writable() already convert; compare frames to
    * frames here too. */
   if (psp->nonblock)
   {
      if (AUDIO_BUFFER_SIZE - ((uint16_t)(write_pos - (uint16_t)
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
      int laps = PSP_AUDIO_WAIT_LAPS;
      while (AUDIO_BUFFER_SIZE - ((uint16_t)(write_pos - (uint16_t)
         retro_atomic_load_acquire_int(&psp->read_pos))
         & AUDIO_BUFFER_SIZE_MASK) < sample_count)
      {
         int key;
         if (--laps < 0 || !retro_atomic_load_acquire_int(&psp->running))
            return 0;
         key = retro_eventcount_prepare_wait(&psp->park);
         if (   (AUDIO_BUFFER_SIZE - ((uint16_t)(write_pos - (uint16_t)
                  retro_atomic_load_acquire_int(&psp->read_pos))
                  & AUDIO_BUFFER_SIZE_MASK) >= sample_count)
             || !retro_atomic_load_acquire_int(&psp->running))
         {
            retro_eventcount_cancel_wait(&psp->park);
            continue;
         }
         if (!retro_eventcount_commit_wait_timeout(&psp->park, key,
                  PSP_AUDIO_WAIT_US))
            continue;
      }
   }

   if ((write_pos + sample_count) > AUDIO_BUFFER_SIZE)
   {
      memcpy(psp->buffer + write_pos, s,
            (AUDIO_BUFFER_SIZE - write_pos) * sizeof(uint32_t));
      memcpy(psp->buffer, (uint32_t*)s +
            (AUDIO_BUFFER_SIZE - write_pos),
            (write_pos + sample_count - AUDIO_BUFFER_SIZE) * sizeof(uint32_t));
   }
   else
      memcpy(psp->buffer + write_pos, s, len);

   write_pos      += sample_count;
   write_pos      &= AUDIO_BUFFER_SIZE_MASK;
   /* Release: the samples land before the index that publishes
    * them. */
   retro_atomic_store_release_int(&psp->write_pos, write_pos);
   return len;
}

static bool psp_audio_alive(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   if (!psp)
      return false;
   return retro_atomic_load_acquire_int(&psp->running) != 0;
}

static bool psp_audio_stop(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;

#if defined(ORBIS)
   return false;
#else
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
#endif
}

static bool psp_audio_start(void *data, bool is_shutdown)
{
   psp_audio_t* psp = (psp_audio_t*)data;

   if (psp && !retro_atomic_load_acquire_int(&psp->running))
   {
      if (!psp->worker_thread)
      {
         retro_atomic_store_release_int(&psp->running, 1);
         psp->worker_thread = sthread_create(psp_audio_mainloop, psp);
      }
   }

   return true;
}

static void psp_audio_set_nonblock_state(void *data, bool toggle)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   if (psp)
      psp->nonblock = toggle;
}

static size_t psp_write_avail(void *data)
{
   size_t _len;
   psp_audio_t* psp = (psp_audio_t*)data;

   if (!psp || !retro_atomic_load_acquire_int(&psp->running))
      return 0;
   _len = AUDIO_BUFFER_SIZE - ((uint16_t)((uint16_t)
         retro_atomic_load_relaxed_int(&psp->write_pos) - (uint16_t)
         retro_atomic_load_acquire_int(&psp->read_pos))
         & AUDIO_BUFFER_SIZE_MASK);
   return _len * sizeof(uint32_t);
}

/* Sleep on the condition the output thread signals after every block
 * until the fifo has room for len, in the same units psp_audio_write()
 * compares against, capped at half the fifo so the wait always ends.
 * Returns the free space as psp_write_avail() reports it, or 0 when
 * the output thread is not running. */
static size_t psp_wait_writable(void *data, size_t len)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   size_t avail;
   int laps         = PSP_AUDIO_WAIT_LAPS;
   /* len arrives in bytes; the ring is counted in uint32_t frames. */
   size_t want      = len / sizeof(uint32_t);

   if (want > AUDIO_BUFFER_SIZE / 2)
      want = AUDIO_BUFFER_SIZE / 2;

   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&psp->running))
         return 0;
      avail = AUDIO_BUFFER_SIZE - ((uint16_t)((uint16_t)
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
      if ((AUDIO_BUFFER_SIZE - ((uint16_t)((uint16_t)
               retro_atomic_load_relaxed_int(&psp->write_pos) - (uint16_t)
               retro_atomic_load_acquire_int(&psp->read_pos))
               & AUDIO_BUFFER_SIZE_MASK)) >= want
            || !retro_atomic_load_acquire_int(&psp->running))
      {
         retro_eventcount_cancel_wait(&psp->park);
         continue;
      }
      if (!retro_eventcount_commit_wait_timeout(&psp->park, key,
               PSP_AUDIO_WAIT_US))
         continue;
   }
   return avail * sizeof(uint32_t);
}

/* sceAudio takes 16-bit PCM only; there is no float output on the
 * PSP hardware or in the kernel API. */
static bool psp_audio_use_float(void *data) { return false; }
static size_t psp_buffer_size(void *data)
{
   /* In bytes: the ring holds AUDIO_BUFFER_SIZE uint32_t frames of int16
    * stereo. The comment beside this had the multiplication and left it
    * out. */
   return AUDIO_BUFFER_SIZE * sizeof(uint32_t);
}

audio_driver_t audio_psp = {
   psp_audio_init,
   psp_audio_write,
   psp_audio_stop,
   psp_audio_start,
   psp_audio_alive,
   psp_audio_set_nonblock_state,
   psp_audio_free,
   psp_audio_use_float,
#if defined(VITA)
   "vita",
#elif defined(ORBIS)
   "orbis",
#else
   "psp",
#endif
   NULL,
   NULL,
   psp_write_avail,
   psp_buffer_size,
   NULL, /* write_raw */
   psp_wait_writable
};
