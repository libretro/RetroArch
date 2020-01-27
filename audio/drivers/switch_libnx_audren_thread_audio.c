/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019      - p-sam
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

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include <switch.h>

#include <queues/fifo_queue.h>
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#define BUFFER_COUNT 5

static const int sample_rate           = 48000;
static const int num_channels          = 2;
static const uint8_t sink_channels[]   = { 0, 1 };
static const size_t thread_stack_size  = 1024 * 8;
static const int thread_preferred_cpu  = 2;

static const AudioRendererConfig audio_renderer_config =
{
   .output_rate     = AudioRendererOutputRate_48kHz,
   .num_voices      = 24,
   .num_effects     = 0,
   .num_sinks       = 1,
   .num_mix_objs    = 1,
   .num_mix_buffers = 2,
};

typedef struct
{
   AudioDriver drv;
   void* mempool;
   AudioDriverWaveBuf wavebufs[BUFFER_COUNT];
   size_t buffer_size;
   size_t samples;
   bool nonblock;

   fifo_buffer_t* fifo;
   Mutex fifo_lock;
   CondVar fifo_condvar;
   Mutex fifo_condlock;
   Thread thread;

   volatile bool running;
   volatile bool paused;
} libnx_audren_thread_t;

static void thread_job(void* data)
{
   unsigned i;
   libnx_audren_thread_t *aud          = (libnx_audren_thread_t*)data;
   size_t available                    = 0;
   size_t current_size                 = 0;
   size_t written_tmp                  = 0;
   AudioDriverWaveBuf* current_wavebuf = NULL;
   void* current_pool_ptr              = NULL;
   void* dstbuf                        = NULL;

   if (!aud)
      return;

   while (aud->running)
   {
      if (!current_wavebuf)
      {
         for (i = 0; i < BUFFER_COUNT; i++)
         {
            if (aud->wavebufs[i].state == AudioDriverWaveBufState_Free
               || aud->wavebufs[i].state == AudioDriverWaveBufState_Done)
            {
               current_wavebuf = &aud->wavebufs[i];
               current_pool_ptr = aud->mempool + (i * aud->buffer_size);
               current_size = 0;
               break;
            }
         }
      }

      if (current_wavebuf)
      {
         mutexLock(&aud->fifo_lock);
         available = aud->paused ? 0 : fifo_read_avail(aud->fifo);
         written_tmp = MIN(available, aud->buffer_size - current_size);
         dstbuf = current_pool_ptr + current_size;
         if (written_tmp > 0)
            fifo_read(aud->fifo, dstbuf, written_tmp);
         mutexUnlock(&aud->fifo_lock);

         if (written_tmp > 0)
         {
            condvarWakeAll(&aud->fifo_condvar);

            current_size += written_tmp;
            armDCacheFlush(dstbuf, written_tmp);
         }

         if (current_size == aud->buffer_size)
         {
            audrvVoiceAddWaveBuf(&aud->drv, 0, current_wavebuf);

            audrvUpdate(&aud->drv);
            if (!audrvVoiceIsPlaying(&aud->drv, 0))
            {
               audrvVoiceStart(&aud->drv, 0);
            }

            current_wavebuf = NULL;
         }

         svcSleepThread(1000UL);
      }
      else
      {
         audrvUpdate(&aud->drv);
         audrenWaitFrame();
      }
   }
}

static void *libnx_audren_thread_audio_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   unsigned i, j;
   libnx_audren_thread_t *aud;
   Result rc;
   int mpid;
   size_t mempool_size;
   unsigned real_latency;
   uint32_t thread_priority;

   RARCH_LOG("[Audio]: Using libnx_audren_thread driver\n");

   aud = (libnx_audren_thread_t*)calloc(1, sizeof(libnx_audren_thread_t));

   if (!aud)
   {
      RARCH_ERR("[Audio]: struct alloc failed\n");
      goto fail;
   }

   real_latency     = MAX(latency, 5);
   RARCH_LOG("[Audio]: real_latency is %u\n", real_latency);

   aud->running     = true;
   aud->paused      = false;
   aud->nonblock    = !block_frames;
   aud->buffer_size = (real_latency * sample_rate / 1000);
   aud->samples     = (aud->buffer_size / num_channels / sizeof(int16_t));

   mempool_size     = (aud->buffer_size * BUFFER_COUNT + 
         (AUDREN_MEMPOOL_ALIGNMENT-1)) &~ (AUDREN_MEMPOOL_ALIGNMENT-1);
   aud->mempool     = memalign(AUDREN_MEMPOOL_ALIGNMENT, mempool_size);

   if (!aud->mempool)
   {
      RARCH_ERR("[Audio]: mempool alloc failed\n");
      goto fail;
   }

   rc = audrenInitialize(&audio_renderer_config);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audio]: audrenInitialize: %x\n", rc);
      goto fail;
   }

   rc = audrvCreate(&aud->drv, &audio_renderer_config, num_channels);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audio]: audrvCreate: %x\n", rc);
      goto fail_init;
   }

   for (i = 0; i < BUFFER_COUNT; i++)
   {
      aud->wavebufs[i].data_raw            = aud->mempool;
      aud->wavebufs[i].size                = mempool_size;
      aud->wavebufs[i].start_sample_offset = i * aud->samples;
      aud->wavebufs[i].end_sample_offset   = aud->wavebufs[i].start_sample_offset + aud->samples;
   }

   mpid = audrvMemPoolAdd(&aud->drv, aud->mempool, mempool_size);
   audrvMemPoolAttach(&aud->drv, mpid);

   audrvDeviceSinkAdd(&aud->drv, AUDREN_DEFAULT_DEVICE_NAME,
         num_channels, sink_channels);

   rc = audrenStartAudioRenderer();
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audio]: audrenStartAudioRenderer: %x\n", rc);
   }

   audrvVoiceInit(&aud->drv, 0, num_channels, PcmFormat_Int16, sample_rate);
   audrvVoiceSetDestinationMix(&aud->drv, 0, AUDREN_FINAL_MIX_ID);
   for (i = 0; i < num_channels; i++)
   {
      for (j = 0; j < num_channels; j++)
      {
         audrvVoiceSetMixFactor(&aud->drv, 0, i == j ? 1.0f : 0.0f, i, j);
      }
   }

   aud->fifo = fifo_new(aud->buffer_size);
   if (!aud->fifo)
   {
      RARCH_ERR("[Audio]: fifo alloc failed\n");
      goto fail_drv;
   }

   mutexInit(&aud->fifo_lock);
   condvarInit(&aud->fifo_condvar);
   mutexInit(&aud->fifo_condlock);

   svcGetThreadPriority(&thread_priority, CUR_THREAD_HANDLE);
   rc = threadCreate(&aud->thread, &thread_job,
         (void*)aud, NULL, thread_stack_size,
         thread_priority - 1, thread_preferred_cpu);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audio]: threadCreate: %x\n", rc);
      goto fail_drv;
   }

   rc = threadStart(&aud->thread);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audio]: threadStart: %x\n", rc);
      threadClose(&aud->thread);
      goto fail_drv;
   }

   *new_rate = sample_rate;

   return aud;

fail_drv:
   audrvClose(&aud->drv);

fail_init:
   audrenExit();

fail:
   if (aud)
   {
      if (aud->mempool)
         free(aud->mempool);

      free(aud);
   }

   return NULL;
}

static size_t libnx_audren_thread_audio_buffer_size(void *data)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud)
      return 0;

   return aud->buffer_size;
}

static ssize_t libnx_audren_thread_audio_write(void *data,
      const void *buf, size_t size)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;
   size_t available, written, written_tmp;

   if (!aud || !aud->running)
      return -1;

   if (aud->paused)
      return 0;

   if (aud->nonblock)
   {
      mutexLock(&aud->fifo_lock);
      available = fifo_write_avail(aud->fifo);
      written = MIN(available, size);
      if (written > 0)
         fifo_write(aud->fifo, buf, written);
      mutexUnlock(&aud->fifo_lock);
   }
   else
   {
      written = 0;
      while (written < size && aud->running)
      {
         mutexLock(&aud->fifo_lock);
         available = fifo_write_avail(aud->fifo);
         if (available)
         {
            written_tmp = MIN(size - written, available);
            fifo_write(aud->fifo, (const char*)buf + written, written_tmp);
            mutexUnlock(&aud->fifo_lock);
            written += written_tmp;
         }
         else
         {
            mutexUnlock(&aud->fifo_lock);
            mutexLock(&aud->fifo_condlock);
            condvarWait(&aud->fifo_condvar, &aud->fifo_condlock);
            mutexUnlock(&aud->fifo_condlock);
         }
      }
   }

   return written;
}

static bool libnx_audren_thread_audio_stop(void *data)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud)
      return false;

   aud->paused = true;

   return true;
}

static bool libnx_audren_thread_audio_start(void *data, bool is_shutdown)
{
   (void)is_shutdown;
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud)
      return false;

   aud->paused = false;

   return true;
}

static bool libnx_audren_thread_audio_alive(void *data)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud)
      return false;

   return true;
}

static void libnx_audren_thread_audio_free(void *data)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud)
      return;

   aud->running = false;
   mutexUnlock(&aud->fifo_lock);
   threadWaitForExit(&aud->thread);
   threadClose(&aud->thread);
   audrvVoiceStop(&aud->drv, 0);
   audrvClose(&aud->drv);
   audrenExit();

   if (aud->mempool)
   {
      free(aud->mempool);
   }

   if (aud->fifo)
   {
      fifo_clear(aud->fifo);
      fifo_free(aud->fifo);
   }

   free(aud);
}

static bool libnx_audren_thread_audio_use_float(void *data)
{
   (void)data;
   return false; /* force S16 */
}

static size_t libnx_audren_thread_audio_write_avail(void *data)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;
   size_t available;

   if (!aud)
      return 0;

   mutexLock(&aud->fifo_lock);
   available = fifo_write_avail(aud->fifo);
   mutexUnlock(&aud->fifo_lock);

   return available;
}

static void libnx_audren_thread_audio_set_nonblock_state(void *data, bool state)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud)
      return;

   aud->nonblock = state;
}

audio_driver_t audio_switch_libnx_audren_thread = {
   libnx_audren_thread_audio_init,
   libnx_audren_thread_audio_write,
   libnx_audren_thread_audio_stop,
   libnx_audren_thread_audio_start,
   libnx_audren_thread_audio_alive,
   libnx_audren_thread_audio_set_nonblock_state,
   libnx_audren_thread_audio_free,
   libnx_audren_thread_audio_use_float,
   "switch_audren_thread",
   NULL, /* device_list_new */
   NULL, /* device_list_free */
   libnx_audren_thread_audio_write_avail,
   libnx_audren_thread_audio_buffer_size,
};
