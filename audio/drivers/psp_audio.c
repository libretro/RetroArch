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
#include <queues/fifo_queue.h>

#if defined(VITA)
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/audioout.h>
#elif defined(PSP)
#include <pspkernel.h>
#include <pspaudio.h>
#elif defined(ORBIS)
#include <audioout.h>
#define SCE_AUDIO_OUT_PORT_TYPE_MAIN   0
#define SCE_AUDIO_OUT_MODE_STEREO      1
#define SceUID uint32_t
#endif

#include "../../retroarch.h"

typedef struct psp_audio
{
   bool nonblock;

   uint32_t* buffer;
   uint32_t* zeroBuffer;

   SceUID thread;
   int rate;

   volatile bool running;
   volatile uint16_t read_pos;
   volatile uint16_t write_pos;

   sthread_t *worker_thread;
   slock_t *fifo_lock;
   scond_t *cond;
   slock_t *cond_lock;

} psp_audio_t;

#define AUDIO_OUT_COUNT 512u
#define AUDIO_BUFFER_SIZE (1u<<13u)
#define AUDIO_BUFFER_SIZE_MASK (AUDIO_BUFFER_SIZE-1)

static void audioMainLoop(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;

#if defined(VITA)
   int port         = sceAudioOutOpenPort(
         SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_OUT_COUNT,
         psp->rate, SCE_AUDIO_OUT_MODE_STEREO);
#elif defined(ORBIS)
   int port         = sceAudioOutOpen(0xff,
         SCE_AUDIO_OUT_PORT_TYPE_MAIN, 0, AUDIO_OUT_COUNT,
         psp->rate, SCE_AUDIO_OUT_MODE_STEREO);
#else
   sceAudioSRCChReserve(AUDIO_OUT_COUNT, psp->rate, 2);
#endif

   while (psp->running)
   {
      bool cond           = false;
      uint16_t read_pos   = psp->read_pos;
      uint16_t read_pos_2 = psp->read_pos;

      slock_lock(psp->fifo_lock);

      cond                = ((uint16_t)(psp->write_pos - read_pos) & AUDIO_BUFFER_SIZE_MASK)
            < (AUDIO_OUT_COUNT * 2);

      if (!cond)
      {
         read_pos      += AUDIO_OUT_COUNT;
         read_pos      &= AUDIO_BUFFER_SIZE_MASK;
         psp->read_pos  = read_pos;
      }

      slock_unlock(psp->fifo_lock);
      slock_lock(psp->cond_lock);
      scond_signal(psp->cond);
      slock_unlock(psp->cond_lock);

#if defined(VITA) || defined(ORBIS)
      sceAudioOutOutput(port,
        cond ? (psp->zeroBuffer)
              : (psp->buffer + read_pos_2));
#else
      sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, cond ? (psp->zeroBuffer)
            : (psp->buffer + read_pos));
#endif
   }

#if defined(VITA)
   sceAudioOutReleasePort(port);
#elif defined(ORBIS)
   sceAudioOutClose(port);
#else
   sceAudioSRCChRelease();
#endif

   return;
}

static void *psp_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   psp_audio_t *psp = (psp_audio_t*)calloc(1, sizeof(psp_audio_t));

   if (!psp)
      return NULL;

   (void)device;
   (void)latency;

#ifdef ORBIS
   psp->buffer      = (uint32_t*)
      malloc(AUDIO_BUFFER_SIZE * sizeof(uint32_t));
#else
   /* Cache aligned, not necessary but helpful. */
   psp->buffer      = (uint32_t*)
      memalign(64, AUDIO_BUFFER_SIZE * sizeof(uint32_t));
#endif
   memset(psp->buffer, 0, AUDIO_BUFFER_SIZE * sizeof(uint32_t));

#ifdef ORBIS
   psp->zeroBuffer      = (uint32_t*)
      malloc(AUDIO_OUT_COUNT * sizeof(uint32_t));
#else
   psp->zeroBuffer  = (uint32_t*)
      memalign(64, AUDIO_OUT_COUNT   * sizeof(uint32_t));
#endif
   memset(psp->zeroBuffer, 0, AUDIO_OUT_COUNT * sizeof(uint32_t));

   psp->read_pos    = 0;
   psp->write_pos   = 0;
   psp->rate        = rate;

   psp->fifo_lock = slock_new();
   psp->cond_lock = slock_new();
   psp->cond = scond_new();

   psp->nonblock    = false;
   psp->running     = true;
   psp->worker_thread = sthread_create(audioMainLoop, psp);

   return psp;
}

static void psp_audio_free(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   if(!psp)
      return;

   if(psp->running){
      if (psp->worker_thread)
      {
            psp->running = false;
            sthread_join(psp->worker_thread);
      }

      if (psp->cond)
            scond_free(psp->cond);
      if (psp->fifo_lock)
            slock_free(psp->fifo_lock);
      if (psp->cond_lock)
            slock_free(psp->cond_lock);
   }
   free(psp->buffer);
   psp->worker_thread = NULL;
   free(psp->zeroBuffer);
   free(psp);
}

static ssize_t psp_audio_write(void *data, const void *buf, size_t size)
{
   psp_audio_t* psp     = (psp_audio_t*)data;
   uint16_t write_pos   = psp->write_pos;
   uint16_t sampleCount = size / sizeof(uint32_t);

   if (!psp->running)
      return -1;

   if (psp->nonblock)
   {
      if (AUDIO_BUFFER_SIZE - ((uint16_t)
               (psp->write_pos - psp->read_pos) & AUDIO_BUFFER_SIZE_MASK) < size)
         return 0;
   }

   slock_lock(psp->cond_lock);
   while (AUDIO_BUFFER_SIZE - ((uint16_t)
      (psp->write_pos - psp->read_pos) & AUDIO_BUFFER_SIZE_MASK) < size)
      scond_wait(psp->cond, psp->cond_lock);
   slock_unlock(psp->cond_lock);

   slock_lock(psp->fifo_lock);
   if((write_pos + sampleCount) > AUDIO_BUFFER_SIZE)
   {
      memcpy(psp->buffer + write_pos, buf,
            (AUDIO_BUFFER_SIZE - write_pos) * sizeof(uint32_t));
      memcpy(psp->buffer, (uint32_t*) buf +
            (AUDIO_BUFFER_SIZE - write_pos),
            (write_pos + sampleCount - AUDIO_BUFFER_SIZE) * sizeof(uint32_t));
   }
   else
      memcpy(psp->buffer + write_pos, buf, size);

   write_pos      += sampleCount;
   write_pos      &= AUDIO_BUFFER_SIZE_MASK;
   psp->write_pos  = write_pos;

   slock_unlock(psp->fifo_lock);
   return size;

}

static bool psp_audio_alive(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   if (!psp)
      return false;
   return psp->running;
}

static bool psp_audio_stop(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;

   if (psp){
      psp->running = false;

      if (!psp->worker_thread)
      return true;

      sthread_join(psp->worker_thread);
      psp->worker_thread = NULL;
   }
   return true;
}

static bool psp_audio_start(void *data, bool is_shutdown)
{
   psp_audio_t* psp = (psp_audio_t*)data;

   if(psp && psp->running)
      return true;

   if (!psp->worker_thread)
   {
      psp->running = true;
      psp->worker_thread = sthread_create(audioMainLoop, psp);
   }

   return true;
}

static void psp_audio_set_nonblock_state(void *data, bool toggle)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   if (psp)
      psp->nonblock = toggle;
}

static bool psp_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t psp_write_avail(void *data)
{
   size_t val;
   psp_audio_t* psp = (psp_audio_t*)data;

   if (!psp||!psp->running)
      return 0;
   slock_lock(psp->fifo_lock);
   val = AUDIO_BUFFER_SIZE - ((uint16_t)
         (psp->write_pos - psp->read_pos) & AUDIO_BUFFER_SIZE_MASK);
   slock_unlock(psp->fifo_lock);
   return val;
}

static size_t psp_buffer_size(void *data)
{
   /* TODO */
   return AUDIO_BUFFER_SIZE /** sizeof(uint32_t)*/;
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
   psp_buffer_size
};
