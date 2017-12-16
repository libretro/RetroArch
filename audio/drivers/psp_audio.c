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
#include <queues/fifo_queue.h>

#ifdef VITA
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/audioout.h>
#else
#include <pspkernel.h>
#include <pspaudio.h>
#endif

#include "../audio_driver.h"

typedef struct psp_audio
{
   bool nonblocking;

   int rate;

   size_t buffer_size;
   size_t period_size;

   volatile bool running;
   fifo_buffer_t *buffer;
   sthread_t *worker_thread;
   slock_t *fifo_lock;
   scond_t *cond;
   slock_t *cond_lock;

} psp_audio_t;

#define AUDIO_OUT_COUNT 512u
#define AUDIO_BUFFER_SIZE (1u<<13u)

static void audioMainLoop(void *data)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   uint8_t     *buf = (uint8_t *)calloc(1, psp->period_size);

#ifdef VITA
   int port         = sceAudioOutOpenPort(
         SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_OUT_COUNT,
         psp->rate, SCE_AUDIO_OUT_MODE_STEREO);
#else
   sceAudioSRCChReserve(AUDIO_OUT_COUNT, psp->rate, 2);
#endif

   while (psp->running)
   {
      size_t avail;
      size_t fifo_size;
      slock_lock(psp->fifo_lock);
      avail = fifo_read_avail(psp->buffer);
      fifo_size = MIN(psp->period_size, avail);
      fifo_read(psp->buffer, buf, fifo_size);
      scond_signal(psp->cond);
      slock_unlock(psp->fifo_lock);

      /* If underrun, fill rest with silence. */
      memset(buf + fifo_size, 0, psp->period_size - fifo_size);

#ifdef VITA
      sceAudioOutOutput(port, buf);
#else
      sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, buf);
#endif
   }

#ifdef VITA
   sceAudioOutReleasePort(port);
#else
   sceAudioSRCChRelease();
#endif

   slock_lock(psp->cond_lock);
   psp->running = false;
   scond_signal(psp->cond);
   slock_unlock(psp->cond_lock);
   free(buf);
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

   psp->buffer_size = AUDIO_BUFFER_SIZE * sizeof(uint32_t);
   psp->period_size = AUDIO_OUT_COUNT * sizeof(uint32_t);

   psp->fifo_lock = slock_new();
   psp->cond_lock = slock_new();
   psp->cond = scond_new();
   psp->buffer = fifo_new(psp->buffer_size);

   psp->rate        = rate;

   psp->nonblocking = false;
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
            slock_lock(psp->cond_lock);
            psp->running = false;
            slock_unlock(psp->cond_lock);
            sthread_join(psp->worker_thread);
      }
      if (psp->buffer)
            fifo_free(psp->buffer);
      if (psp->cond)
            scond_free(psp->cond);
      if (psp->fifo_lock)
            slock_free(psp->fifo_lock);
      if (psp->cond_lock)
            slock_free(psp->cond_lock);
   }

   free(psp);
}

static ssize_t psp_audio_write(void *data, const void *buf, size_t size)
{
   psp_audio_t* psp = (psp_audio_t*)data;
   if (!psp->running)
      return -1;

   if (psp->nonblocking)
   {
      size_t avail;
      size_t write_amt;

      slock_lock(psp->fifo_lock);
      avail           = fifo_write_avail(psp->buffer);
      write_amt       = MIN(avail, size);

      fifo_write(psp->buffer, buf, write_amt);
      slock_unlock(psp->fifo_lock);

      return write_amt;
   }
   else
   {
      size_t written = 0;
      while (written < size && psp->running)
      {
         size_t avail;
         slock_lock(psp->fifo_lock);
         avail = fifo_write_avail(psp->buffer);

         if (avail == 0)
         {
            slock_unlock(psp->fifo_lock);
            slock_lock(psp->cond_lock);
            if (psp->running)
               scond_wait(psp->cond, psp->cond_lock);
            slock_unlock(psp->cond_lock);
         }
         else
         {
            size_t write_amt = MIN(size - written, avail);
            fifo_write(psp->buffer,
                  (const char*)buf + written, write_amt);
            slock_unlock(psp->fifo_lock);
            written += write_amt;
         }
      }
      return written;
   }
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
      psp->nonblocking = toggle;
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
   val = fifo_write_avail(psp->buffer);
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
#ifdef VITA
   "vita",
#else
   "psp",
#endif
   NULL,
   NULL,
   psp_write_avail,
   psp_buffer_size
};
