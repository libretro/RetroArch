/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2020 - Justin Weiss
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

#include <3ds.h>
#include <string.h>
#include <malloc.h>
#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>

#include "../audio_driver.h"
#include "../../ctr/ctr_debug.h"

typedef struct
{
   fifo_buffer_t* fifo;
   size_t fifo_size;

   slock_t* fifo_lock;
   scond_t* fifo_avail;
   scond_t* fifo_done;
   sthread_t* thread;

   volatile bool running;

   bool nonblocking;
   bool playing;
   retro_time_t frame_time;
   int channel;
   ndspWaveBuf dsp_buf;

   uint32_t pos;
} ctr_dsp_thread_audio_t;

// PCM16 stereo
#define DSP_BYTES_TO_SAMPLES(bytes) (bytes / (2 * sizeof(uint16_t)))
#define DSP_SAMPLES_TO_BYTES(samples) (samples * 2 * sizeof(uint16_t))

static void ctr_dsp_audio_loop(void* data)
{
   uint32_t pos, buf_pos;
   ctr_dsp_thread_audio_t *ctr  = (ctr_dsp_thread_audio_t*)data;

   if (!ctr)
      return;

   while (1)
   {
      size_t buf_avail, avail, to_write;
      slock_lock(ctr->fifo_lock);

      do {
         avail = FIFO_READ_AVAIL(ctr->fifo);

         if (!avail) {
            scond_wait(ctr->fifo_avail, ctr->fifo_lock);
         }
      } while (!avail && ctr->running);

      slock_unlock(ctr->fifo_lock);

      if (!ctr->running)
         break;

      pos = ctr->pos;
      buf_pos = DSP_SAMPLES_TO_BYTES(ndspChnGetSamplePos(ctr->channel));

      buf_avail = buf_pos >= pos ? buf_pos - pos : ctr->fifo_size - pos;
      to_write = MIN(avail, buf_avail);

      slock_lock(ctr->fifo_lock);

      if (to_write > 0) {
         fifo_read(ctr->fifo, ctr->dsp_buf.data_pcm8 + pos, to_write);
         DSP_FlushDataCache(ctr->dsp_buf.data_pcm8 + pos, to_write);
         scond_signal(ctr->fifo_done);
      }

      slock_unlock(ctr->fifo_lock);

      if (buf_pos == pos) {
         svcSleepThread(100000);
      }

      ctr->pos = (pos + to_write) % ctr->fifo_size;
   }
}

static void ctr_dsp_thread_audio_free(void *data);

static void *ctr_dsp_thread_audio_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   ctr_dsp_thread_audio_t *ctr = NULL;

   (void)device;
   (void)rate;

   if (ndspInit() < 0)
      return NULL;

   ctr = (ctr_dsp_thread_audio_t*)calloc(1, sizeof(ctr_dsp_thread_audio_t));

   if (!ctr)
      return NULL;

   *new_rate    = 32728;

   ctr->running = true;
   ctr->channel = 0;

   ndspSetOutputMode(NDSP_OUTPUT_STEREO);
   ndspSetClippingMode(NDSP_CLIP_SOFT); /* ?? */
   ndspSetOutputCount(2);
   ndspChnReset(ctr->channel);
   ndspChnSetFormat(ctr->channel, NDSP_FORMAT_STEREO_PCM16);
   ndspChnSetInterp(ctr->channel, NDSP_INTERP_NONE);
   ndspChnSetRate(ctr->channel, 32728.0f);
   ndspChnWaveBufClear(ctr->channel);

   ctr->fifo_size = DSP_SAMPLES_TO_BYTES((*new_rate * MAX(latency, 8)) / 1000);

   ctr->dsp_buf.data_pcm16 = linearAlloc(ctr->fifo_size);
   memset(ctr->dsp_buf.data_pcm16, 0, ctr->fifo_size);
   DSP_FlushDataCache(ctr->dsp_buf.data_pcm16, ctr->fifo_size);

   ctr->dsp_buf.looping = true;
   ctr->dsp_buf.nsamples = DSP_BYTES_TO_SAMPLES(ctr->fifo_size);
   ndspChnWaveBufAdd(ctr->channel, &ctr->dsp_buf);

   ctr->fifo = fifo_new(ctr->fifo_size);

   if (!(ctr->fifo_lock = slock_new()) ||
       !(ctr->fifo_avail = scond_new()) ||
       !(ctr->fifo_done = scond_new()) ||
       !(ctr->thread = sthread_create(ctr_dsp_audio_loop, ctr)))
   {
      RARCH_LOG("[Audio]: thread creation failed.\n");
      ctr->running = false;
      ctr_dsp_thread_audio_free(ctr);
      return NULL;
   }

   ctr->pos = 0;
   ctr->playing = true;

   ctr->frame_time = (retro_time_t)roundf(1000000 * 4481134.0 / (*new_rate * 8192.0));

   ndspSetMasterVol(1.0);

   return ctr;
}

static void ctr_dsp_thread_audio_free(void *data)
{
   ctr_dsp_thread_audio_t* ctr = (ctr_dsp_thread_audio_t*)data;
   if (!ctr)
      return;

   if (ctr->running)
   {
      ctr->running = false;
      scond_signal(ctr->fifo_avail);
   }

   if (ctr->thread)
      sthread_join(ctr->thread);

   scond_free(ctr->fifo_avail);
   scond_free(ctr->fifo_done);
   slock_free(ctr->fifo_lock);

   if (ctr->fifo)
   {
      fifo_free(ctr->fifo);
      ctr->fifo = NULL;
   }

   ndspChnWaveBufClear(ctr->channel);

   linearFree(ctr->dsp_buf.data_pcm16);

   free(ctr);
   ndspExit();
   ctr = NULL;
}

static ssize_t ctr_dsp_thread_audio_write(void *data, const void *buf, size_t size)
{
   size_t avail, written;
   ctr_dsp_thread_audio_t * ctr = (ctr_dsp_thread_audio_t*)data;

   if (!ctr || !ctr->running)
      return 0;

   if (ctr->nonblocking)
   {
      slock_lock(ctr->fifo_lock);
      avail = FIFO_WRITE_AVAIL(ctr->fifo);
      written = MIN(avail, size);
      if (written > 0)
      {
         fifo_write(ctr->fifo, buf, written);
         scond_signal(ctr->fifo_avail);
      }
      slock_unlock(ctr->fifo_lock);
   }
   else
   {
      written = 0;
      while (written < size && ctr->running)
      {
         slock_lock(ctr->fifo_lock);
         avail = FIFO_WRITE_AVAIL(ctr->fifo);
         if (avail == 0)
         {
            if (ctr->running)
            {
               /* Wait a maximum of one frame, skip the write if the thread is still busy */
               if (!scond_wait_timeout(ctr->fifo_done, ctr->fifo_lock, ctr->frame_time)) {
                  slock_unlock(ctr->fifo_lock);
                  break;
               }
            }
            slock_unlock(ctr->fifo_lock);
         }
         else
         {
            size_t write_amt = MIN(size - written, avail);
            fifo_write(ctr->fifo, (const char*)buf + written, write_amt);
            scond_signal(ctr->fifo_avail);
            slock_unlock(ctr->fifo_lock);
            written += write_amt;
         }
      }
   }

   return written;
}

static bool ctr_dsp_thread_audio_stop(void *data)
{
   ctr_dsp_thread_audio_t* ctr = (ctr_dsp_thread_audio_t*)data;

   if (!ctr)
     return false;

   ndspSetMasterVol(0.0);
   ctr->playing = false;

   return true;
}

static bool ctr_dsp_thread_audio_alive(void *data)
{
   ctr_dsp_thread_audio_t* ctr = (ctr_dsp_thread_audio_t*)data;

   if (!ctr)
     return false;

   return ctr->playing;
}

static bool ctr_dsp_thread_audio_start(void *data, bool is_shutdown)
{
   ctr_dsp_thread_audio_t* ctr = (ctr_dsp_thread_audio_t*)data;

   if (!ctr)
     return false;

   /* Prevents restarting audio when the menu
    * is toggled off on shutdown */
   if (is_shutdown)
      return true;

   ndspSetMasterVol(1.0);
   ctr->playing = true;

   return true;
}

static void ctr_dsp_thread_audio_set_nonblock_state(void *data, bool state)
{
   ctr_dsp_thread_audio_t* ctr = (ctr_dsp_thread_audio_t*)data;
   if (ctr)
      ctr->nonblocking = state;
}

static bool ctr_dsp_thread_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t ctr_dsp_thread_audio_write_avail(void *data)
{
   size_t val;
   ctr_dsp_thread_audio_t* ctr = (ctr_dsp_thread_audio_t*)data;

   slock_lock(ctr->fifo_lock);
   val = FIFO_WRITE_AVAIL(ctr->fifo);
   slock_unlock(ctr->fifo_lock);

   return val;
}

static size_t ctr_dsp_thread_audio_buffer_size(void *data)
{
   ctr_dsp_thread_audio_t* ctr = (ctr_dsp_thread_audio_t*)data;
   return ctr->fifo_size;
}

audio_driver_t audio_ctr_dsp_thread = {
   ctr_dsp_thread_audio_init,
   ctr_dsp_thread_audio_write,
   ctr_dsp_thread_audio_stop,
   ctr_dsp_thread_audio_start,
   ctr_dsp_thread_audio_alive,
   ctr_dsp_thread_audio_set_nonblock_state,
   ctr_dsp_thread_audio_free,
   ctr_dsp_thread_audio_use_float,
   "dsp_thread",
   NULL,
   NULL,
   ctr_dsp_thread_audio_write_avail,
   ctr_dsp_thread_audio_buffer_size
};
