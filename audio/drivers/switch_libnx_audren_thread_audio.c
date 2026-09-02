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

#include "../audio_driver.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

/* Bound on a wait for the render thread to take from the fifo: one
 * wait, in nanoseconds (condvarWaitTimeout's unit), and how many before
 * the caller gets the pass back. */
#define LIBNX_AUDREN_WAIT_NS   100000000ULL
#define LIBNX_AUDREN_WAIT_LAPS 8


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
   size_t fifo_size;
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
         available = aud->paused ? 0 : FIFO_READ_AVAIL(aud->fifo);
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
   int32_t thread_priority;

   RARCH_LOG("[Audren] Using libnx_audren_thread driver.\n");

   aud = (libnx_audren_thread_t*)calloc(1, sizeof(libnx_audren_thread_t));

   if (!aud)
   {
      RARCH_ERR("[Audren] struct alloc failed.\n");
      goto fail;
   }

   real_latency     = MAX(latency, 5);
   RARCH_LOG("[Audren] real_latency is %u.\n", real_latency);

   aud->running     = true;
   aud->paused      = false;
   aud->nonblock    = !block_frames;
   /* Two stages: the fifo the writer fills, which is what rate control
    * measures and holds half full, and the wave buffers the render
    * thread keeps full behind it. The fifo holds the setting, in bytes
    * of int16 stereo; the wave buffers together take what is left of
    * the setting after half the fifo - half of it - floored at 16 ms,
    * or the setting when lower, so the two add up to the setting. It
    * used to be the setting in frames used as bytes for both: a
    * quarter of the setting in the fifo, five quarters queued behind
    * it, and the fifo alone reported. */
   {
      size_t frame_bytes  = num_channels * sizeof(int16_t);
      size_t setting      = ((size_t)real_latency * sample_rate / 1000) * frame_bytes;
      size_t floor_bytes  = ((size_t)16 * sample_rate / 1000) * frame_bytes;
      size_t device_total = setting / 2;
      if (floor_bytes > setting)
         floor_bytes      = setting;
      if (device_total < floor_bytes)
         device_total     = floor_bytes;
      aud->fifo_size      = setting;
      aud->buffer_size    = device_total / BUFFER_COUNT;
      aud->buffer_size   -= aud->buffer_size % frame_bytes;
      if (aud->buffer_size < 64 * frame_bytes)
         aud->buffer_size = 64 * frame_bytes;
   }
   aud->samples     = (aud->buffer_size / num_channels / sizeof(int16_t));
   RARCH_LOG("[Audren] %u ms as a %u-frame fifo (measured) in front of %u wave buffers of %u frames (%u ms).\n",
         real_latency, (unsigned)(aud->fifo_size / (num_channels * sizeof(int16_t))),
         (unsigned)BUFFER_COUNT, (unsigned)aud->samples,
         (unsigned)(aud->samples * BUFFER_COUNT * 1000 / sample_rate));

   mempool_size     = (aud->buffer_size * BUFFER_COUNT +
         (AUDREN_MEMPOOL_ALIGNMENT-1)) &~ (AUDREN_MEMPOOL_ALIGNMENT-1);
   aud->mempool     = memalign(AUDREN_MEMPOOL_ALIGNMENT, mempool_size);

   if (!aud->mempool)
   {
      RARCH_ERR("[Audren] mempool alloc failed.\n");
      goto fail;
   }

   rc = audrenInitialize(&audio_renderer_config);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audren] audrenInitialize: %x.\n", rc);
      goto fail;
   }

   rc = audrvCreate(&aud->drv, &audio_renderer_config, num_channels);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audren] audrvCreate: %x.\n", rc);
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
      RARCH_ERR("[Audren] audrenStartAudioRenderer: %x.\n", rc);
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

   aud->fifo = fifo_new(aud->fifo_size);
   if (!aud->fifo)
   {
      RARCH_ERR("[Audren] fifo alloc failed.\n");
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
      RARCH_ERR("[Audren] threadCreate: %x.\n", rc);
      goto fail_drv;
   }

   rc = threadStart(&aud->thread);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audren] threadStart: %x.\n", rc);
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

/* Sleep on the condition the mixer thread wakes after every buffer it
 * consumes until at least len bytes fit in the fifo, capped at half of
 * it so the wait always ends. Returns the free space then, or 0 when
 * the thread is not running or the driver is paused. */
static size_t libnx_audren_thread_audio_wait_writable(void *data, size_t len)
{
   size_t available;
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;
   int laps                   = LIBNX_AUDREN_WAIT_LAPS;

   if (!aud)
      return 0;
   if (len > aud->fifo_size / 2)
      len = aud->fifo_size / 2;

   for (;;)
   {
      if (!aud->running || aud->paused)
         return 0;
      mutexLock(&aud->fifo_lock);
      available = FIFO_WRITE_AVAIL(aud->fifo);
      mutexUnlock(&aud->fifo_lock);
      if (available >= len)
         return available;
      /* Timed and capped: a renderer that has stopped hands the pass
       * back as no space coming from this call. */
      mutexLock(&aud->fifo_condlock);
      condvarWaitTimeout(&aud->fifo_condvar, &aud->fifo_condlock,
            LIBNX_AUDREN_WAIT_NS);
      mutexUnlock(&aud->fifo_condlock);
      if (--laps < 0)
         return 0;
   }
}

static size_t libnx_audren_thread_audio_buffer_size(void *data)
{
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud)
      return 0;

   /* The fifo: the stage the writer fills and write_avail() measures.
    * The wave buffers behind it are the render thread's, kept full, and
    * add on top. */
   return aud->fifo_size;
}

static ssize_t libnx_audren_thread_audio_write(void *data,
      const void *s, size_t len)
{
   size_t available, _len;
   libnx_audren_thread_t *aud = (libnx_audren_thread_t*)data;

   if (!aud || !aud->running)
      return -1;

   if (aud->paused)
      return 0;

   if (aud->nonblock)
   {
      mutexLock(&aud->fifo_lock);
      available = FIFO_WRITE_AVAIL(aud->fifo);
      _len      = MIN(available, len);
      if (_len > 0)
         fifo_write(aud->fifo, s, _len);
      mutexUnlock(&aud->fifo_lock);
   }
   else
   {
      int laps = LIBNX_AUDREN_WAIT_LAPS;

      _len = 0;
      while (_len < len && aud->running)
      {
         mutexLock(&aud->fifo_lock);
         available = FIFO_WRITE_AVAIL(aud->fifo);
         if (available)
         {
            size_t written_tmp = MIN(len - _len, available);
            fifo_write(aud->fifo, (const char*)s + _len, written_tmp);
            mutexUnlock(&aud->fifo_lock);
            _len += written_tmp;
         }
         else
         {
            /* The render thread signals each time it takes from the
             * fifo. Paused it takes nothing, and a renderer that has
             * stopped takes nothing either; the wait is timed and
             * capped, and the write returns what went. */
            mutexUnlock(&aud->fifo_lock);
            if (aud->paused)
               break;
            mutexLock(&aud->fifo_condlock);
            condvarWaitTimeout(&aud->fifo_condvar, &aud->fifo_condlock,
                  LIBNX_AUDREN_WAIT_NS);
            mutexUnlock(&aud->fifo_condlock);
            if (--laps < 0)
               break;
         }
      }
   }

   return _len;
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
   available = FIFO_WRITE_AVAIL(aud->fifo);
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
   NULL, /* write_raw */
   libnx_audren_thread_audio_wait_writable
};
