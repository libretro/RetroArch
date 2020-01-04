/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - misson20000
 *  Copyright (C) 2018      - m4xw
 *  Copyright (C) 2018      - lifajucejo
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
#include <sys/unistd.h>

#ifdef HAVE_LIBNX
#include <switch.h>
#else
#include <libtransistor/nx.h>
#endif

#include <queues/fifo_queue.h>
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../../tasks/tasks_internal.h"

#include "switch_audio_compat.h"

static const size_t thread_stack_size = 1024 * 8;
static const int thread_preferred_cpu = 2;
static const int channel_count = 2;
static const size_t sample_size = sizeof(uint16_t);
static const size_t frame_size = channel_count * sample_size;

#define AUDIO_BUFFER_COUNT 2

typedef struct
{
   fifo_buffer_t* fifo;
   compat_mutex fifoLock;
   compat_condvar cond;
   compat_mutex condLock;

   size_t fifoSize;

   volatile bool running;
   bool nonblock;
   bool is_paused;

   compat_audio_out_buffer buffers[AUDIO_BUFFER_COUNT];
   compat_thread thread;

   unsigned latency;
   uint32_t sampleRate;

#ifndef HAVE_LIBNX
   audio_output_t output;
   handle_t event;
#endif
} switch_thread_audio_t;

static void mainLoop(void* data)
{
   Result rc;
   uint32_t released_out_count                  = 0;
   compat_audio_out_buffer *released_out_buffer = NULL;
   switch_thread_audio_t                  *swa  = (switch_thread_audio_t*)data;

   if (!swa)
      return;

   RARCH_LOG("[Audio]: start mainLoop cpu %u tid %u\n", svcGetCurrentProcessorNumber(), swa->thread.handle);

   while (swa->running)
   {
      size_t buf_avail, avail, to_write;

      if (!released_out_buffer)
      {
#ifdef HAVE_LIBNX
         rc = audoutWaitPlayFinish(&released_out_buffer, &released_out_count, U64_MAX);
#else
         uint32_t handle_idx = 0;
         svcWaitSynchronization(&handle_idx, &swa->event, 1, 33333333);
         svcResetSignal(swa->event);

         rc = audio_ipc_output_get_released_buffer(&swa->output, &released_out_count, &released_out_buffer);
#endif
         if (R_FAILED(rc))
         {
            swa->running = false;
            RARCH_LOG("[Audio]: audoutGetReleasedAudioOutBuffer failed: %d\n", (int)rc);
            break;
         }
         released_out_buffer->data_size = 0;
      }

      buf_avail = released_out_buffer->buffer_size - released_out_buffer->data_size;

      compat_mutex_lock(&swa->fifoLock);

      avail    = fifo_read_avail(swa->fifo);
      to_write = MIN(avail, buf_avail);
      if (to_write > 0)
      {
	      uint8_t *base;
#ifdef HAVE_LIBNX
	      base = (uint8_t*) released_out_buffer->buffer;
#else
	      base = (uint8_t*) released_out_buffer->sample_data;
#endif
         fifo_read(swa->fifo, base + released_out_buffer->data_size, to_write);
      }

      compat_mutex_unlock(&swa->fifoLock);
      compat_condvar_wake_all(&swa->cond);

      released_out_buffer->data_size += to_write;
      if (released_out_buffer->data_size >= released_out_buffer->buffer_size / 2)
      {
         rc = switch_audio_ipc_output_append_buffer(swa, released_out_buffer);
         if (R_FAILED(rc))
         {
            RARCH_LOG("[Audio]: audoutAppendAudioOutBuffer failed: %d\n", (int)rc);
         }
         released_out_buffer = NULL;
      }
      else
         svcSleepThread(16000000); /* 16ms */
   }
}

static void *switch_thread_audio_init(const char *device, unsigned rate, unsigned latency, unsigned block_frames, unsigned *new_rate)
{
   Result rc;
   unsigned i;
   uint32_t prio;
   char names[8][0x20];
   uint32_t num_names  = 0;
   switch_thread_audio_t *swa = (switch_thread_audio_t *)calloc(1, sizeof(*swa));

   if (!swa)
      return NULL;

   swa->running     = true;
   swa->nonblock    = true;
   swa->is_paused   = true;
   swa->latency     = MAX(latency, 8);

   rc               = switch_audio_ipc_init();
   if (R_FAILED(rc))
   {
      RARCH_LOG("[Audio]: audio init failed %d\n", (int)rc);
      free(swa);
      return NULL;
   }

#ifdef HAVE_LIBNX
   rc = audoutStartAudioOut();
   if (R_FAILED(rc))
   {
      RARCH_LOG("[Audio]: audio start init failed: %d\n", (int)rc);
      goto fail_audio_ipc;
   }

   swa->sampleRate = audoutGetSampleRate();
#else
   if (audio_ipc_list_outputs(&names[0], 8, &num_names) != RESULT_OK)
	   goto fail_audio_ipc;

   if (num_names != 1)
   {
	   RARCH_ERR("[Audio]: got back more than one AudioOut\n");
	   goto fail_audio_ipc;
   }

   if (audio_ipc_open_output(names[0], &swa->output) != RESULT_OK)
	   goto fail_audio_ipc;

   swa->sampleRate = swa->output.sample_rate;

   if (swa->output.num_channels != 2)
   {
      RARCH_ERR("expected %d channels, got %d\n", 2,
            swa->output.num_channels);
      goto fail_audio_output;
   }

   if (swa->output.sample_format != PCM_INT16)
   {
      RARCH_ERR("expected PCM_INT16, got %d\n", swa->output.sample_format);
      goto fail_audio_output;
   }

   if (audio_ipc_output_register_buffer_event(&swa->output, &swa->event) != 0)
      goto fail_audio_output;
#endif

   *new_rate     = swa->sampleRate;
   swa->fifoSize = (swa->sampleRate * sample_size * swa->latency) / 1000;

   for (i = 0; i < AUDIO_BUFFER_COUNT; i++)
   {
#ifdef HAVE_LIBNX
      swa->buffers[i].next        = NULL; /* Unused */
      swa->buffers[i].data_offset = 0;
      swa->buffers[i].buffer_size = swa->fifoSize;
      swa->buffers[i].data_size   = swa->buffers[i].buffer_size;
      swa->buffers[i].buffer      = memalign(0x1000, swa->buffers[i].buffer_size);

      if (!swa->buffers[i].buffer)
         goto fail;

      memset(swa->buffers[i].buffer, 0, swa->buffers[i].buffer_size);
#else
      swa->buffers[i].ptr         = &swa->buffers[i].sample_data;
      swa->buffers[i].unknown     = 0;
      swa->buffers[i].buffer_size = swa->fifoSize;
      swa->buffers[i].data_size   = swa->buffers[i].buffer_size;
      swa->buffers[i].sample_data = alloc_pages(swa->buffers[i].buffer_size, swa->buffers[i].buffer_size, NULL);

      if (!swa->buffers[i].sample_data)
	      goto fail_audio_output;

      memset(swa->buffers[i].sample_data, 0, swa->buffers[i].buffer_size);
#endif

      if (switch_audio_ipc_output_append_buffer(swa, &swa->buffers[i]) != 0)
         goto fail_audio_output;
   }

   compat_mutex_create(&swa->fifoLock);
   swa->fifo = fifo_new(swa->fifoSize);

   compat_condvar_create(&swa->cond);

   RARCH_LOG("[Audio]: switch_thread_audio_init device %s requested rate %hu rate %hu latency %hu block_frames %hu fifoSize %lu\n",
         device, rate, swa->sampleRate, swa->latency, block_frames, swa->fifoSize);

   svcGetThreadPriority(&prio, 0xffff8000);
   rc = compat_thread_create(&swa->thread, &mainLoop, (void*)swa, thread_stack_size, prio - 1, thread_preferred_cpu);

   if (R_FAILED(rc))
   {
      RARCH_LOG("[Audio]: thread creation failed create %u\n", swa->thread.handle);
      swa->running = false;
      return NULL;
   }

   if (R_FAILED(compat_thread_start(&swa->thread)))
   {
      RARCH_LOG("[Audio]: thread creation failed start %u\n", swa->thread.handle);
      compat_thread_close(&swa->thread);
      swa->running = false;
      return NULL;
   }

   return swa;

fail_audio_output:
#ifndef HAVE_LIBNX
   audio_ipc_output_close(&swa->output);
#endif
fail_audio_ipc:
   switch_audio_ipc_finalize();
fail:
   free(swa); // freeing a null ptr is valid
   return NULL;
}

static bool switch_thread_audio_start(void *data, bool is_shutdown)
{
   /* RARCH_LOG("[Audio]: switch_thread_audio_start\n"); */
   switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

   if (!swa)
      return false;

   swa->is_paused = false;
   return true;
}

static bool switch_thread_audio_stop(void *data)
{
   switch_thread_audio_t* swa = (switch_thread_audio_t*)data;

   if (!swa)
      return false;

   swa->is_paused = true;
   return true;
}

static void switch_thread_audio_free(void *data)
{
   unsigned i;
   switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

   if (!swa)
         return;

   if (swa->running)
   {
         swa->running = false;
         compat_thread_join(&swa->thread);
         compat_thread_close(&swa->thread);
   }

   switch_audio_ipc_output_stop(swa);
   switch_audio_ipc_finalize();

   if (swa->fifo)
   {
         fifo_free(swa->fifo);
         swa->fifo = NULL;
   }

   for (i = 0; i < ARRAY_SIZE(swa->buffers); i++)
   {
#ifdef HAVE_LIBNX
         free(swa->buffers[i].buffer);
#else
         free_pages(swa->buffers[i].sample_data);
#endif
   }

   free(swa);
   swa = NULL;
}

static ssize_t switch_thread_audio_write(void *data, const void *buf, size_t size)
{
   size_t avail, written;
   switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

   if (!swa || !swa->running)
         return 0;

   if (swa->nonblock)
   {
      compat_mutex_lock(&swa->fifoLock);
      avail = fifo_write_avail(swa->fifo);
      written = MIN(avail, size);
      if (written > 0)
         fifo_write(swa->fifo, buf, written);
      compat_mutex_unlock(&swa->fifoLock);
   }
   else
   {
      written = 0;
      while (written < size && swa->running)
      {
         compat_mutex_lock(&swa->fifoLock);
         avail = fifo_write_avail(swa->fifo);
         if (avail == 0)
         {
            compat_mutex_unlock(&swa->fifoLock);
            compat_mutex_lock(&swa->condLock);
            if (swa->running)
               compat_condvar_wait(&swa->cond, &swa->condLock);
            compat_mutex_unlock(&swa->condLock);
         }
         else
         {
            size_t write_amt = MIN(size - written, avail);
            fifo_write(swa->fifo, (const char*)buf + written, write_amt);
            compat_mutex_unlock(&swa->fifoLock);
            written += write_amt;
         }
      }
   }

   return written;
}

static bool switch_thread_audio_alive(void *data)
{
   switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

   if (!swa)
         return false;

   return !swa->is_paused;
}

static void switch_thread_audio_set_nonblock_state(void *data, bool state)
{
   switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

   if (swa)
      swa->nonblock = state;
}

static bool switch_thread_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t switch_thread_audio_write_avail(void *data)
{
   size_t val;
   switch_thread_audio_t* swa = (switch_thread_audio_t*)data;

   compat_mutex_lock(&swa->fifoLock);
   val = fifo_write_avail(swa->fifo);
   compat_mutex_unlock(&swa->fifoLock);

   return val;
}

size_t switch_thread_audio_buffer_size(void *data)
{
   switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

   if (!swa)
         return 0;

   return swa->fifoSize;
}

audio_driver_t audio_switch_thread = {
      switch_thread_audio_init,
      switch_thread_audio_write,
      switch_thread_audio_stop,
      switch_thread_audio_start,
      switch_thread_audio_alive,
      switch_thread_audio_set_nonblock_state,
      switch_thread_audio_free,
      switch_thread_audio_use_float,
      "switch_thread",
      NULL, /* device_list_new */
      NULL, /* device_list_free */
      switch_thread_audio_write_avail,
      switch_thread_audio_buffer_size
};

/* vim: set ts=3 sw=3 */
