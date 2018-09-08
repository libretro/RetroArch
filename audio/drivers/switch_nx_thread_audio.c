/*  RetroArch - A frontend for libretro.
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

#include <switch.h>

#include <queues/fifo_queue.h>
#include "../audio_driver.h"
#include "../../verbosity.h"

#include "../../tasks/tasks_internal.h"

#define THREAD_STACK_SIZE (1024 * 8)

#define AUDIO_THREAD_CPU 2

#define CHANNELCOUNT 2
#define BYTESPERSAMPLE sizeof(uint16_t)
#define SAMPLE_SIZE (CHANNELCOUNT * BYTESPERSAMPLE)

#define AUDIO_BUFFER_COUNT 2

static inline void lockMutex(Mutex* mtx)
{
      mutexLock(mtx);
}

typedef struct
{
      fifo_buffer_t* fifo;
      Mutex fifoLock;
      CondVar cond;
      Mutex condLock;

      size_t fifoSize;

      volatile bool running;
      bool nonblocking;
      bool is_paused;

      AudioOutBuffer buffer[AUDIO_BUFFER_COUNT];
      Thread thread;

      unsigned latency;
      uint32_t sampleRate;
} switch_thread_audio_t;

static void mainLoop(void* data)
{
      switch_thread_audio_t* swa = (switch_thread_audio_t*)data;

      if (!swa)
            return;

      RARCH_LOG("[Audio]: start mainLoop cpu %u tid %u\n", svcGetCurrentProcessorNumber(), swa->thread.handle);

      for (int i = 0; i < AUDIO_BUFFER_COUNT; i++)
      {
            swa->buffer[i].next = NULL; // Unused
            swa->buffer[i].buffer_size = swa->fifoSize;
            swa->buffer[i].buffer = memalign(0x1000, swa->buffer[i].buffer_size);
            swa->buffer[i].data_size = swa->buffer[i].buffer_size;
            swa->buffer[i].data_offset = 0;

            memset(swa->buffer[i].buffer, 0, swa->buffer[i].buffer_size);
            audoutAppendAudioOutBuffer(&swa->buffer[i]);
      }

      AudioOutBuffer* released_out_buffer = NULL;
      u32 released_out_count;
      Result rc;

      while (swa->running)
      {
            if (!released_out_buffer)
            {
                  rc = audoutWaitPlayFinish(&released_out_buffer, &released_out_count, U64_MAX);
                  if (R_FAILED(rc))
                  {
                        swa->running = false;
                        RARCH_LOG("[Audio]: audoutGetReleasedAudioOutBuffer failed: %d\n", (int)rc);
                        break;
                  }
                  released_out_buffer->data_size = 0;
            }

            size_t bufAvail = released_out_buffer->buffer_size - released_out_buffer->data_size;

            lockMutex(&swa->fifoLock);

            size_t avail = fifo_read_avail(swa->fifo);
            size_t to_write = MIN(avail, bufAvail);
            if (to_write > 0)
                  fifo_read(swa->fifo, ((u8*)released_out_buffer->buffer) + released_out_buffer->data_size, to_write);

            mutexUnlock(&swa->fifoLock);
            condvarWakeAll(&swa->cond);

            released_out_buffer->data_size += to_write;
            if (released_out_buffer->data_size >= released_out_buffer->buffer_size / 2)
            {
                  rc = audoutAppendAudioOutBuffer(released_out_buffer);
                  if (R_FAILED(rc))
                  {
                        RARCH_LOG("[Audio]: audoutAppendAudioOutBuffer failed: %d\n", (int)rc);
                  }
                  released_out_buffer = NULL;
            }
            else
            {
                  svcSleepThread(16000000); // 16ms
            }
      }
}

static void *switch_thread_audio_init(const char *device, unsigned rate, unsigned latency, unsigned block_frames, unsigned *new_rate)
{
      (void)device;

      switch_thread_audio_t *swa = (switch_thread_audio_t *)calloc(1, sizeof(switch_thread_audio_t));

      if (!swa)
            return NULL;

      swa->running = true;
      swa->nonblocking = true;
      swa->is_paused = true;
      swa->latency = MAX(latency, 8);

      Result rc = audoutInitialize();
      if (R_FAILED(rc))
      {
            RARCH_LOG("[Audio]: audio init failed %d\n", (int)rc);
            return NULL;
      }

      rc = audoutStartAudioOut();
      if (R_FAILED(rc))
      {
            RARCH_LOG("[Audio]: audio start init failed: %d\n", (int)rc);
            return NULL;
      }

      swa->sampleRate = audoutGetSampleRate();
      *new_rate = swa->sampleRate;

      mutexInit(&swa->fifoLock);
      swa->fifoSize = (swa->sampleRate * SAMPLE_SIZE * swa->latency) / 1000;
      swa->fifo = fifo_new(swa->fifoSize);

      condvarInit(&swa->cond, &swa->condLock);

      RARCH_LOG("[Audio]: switch_thread_audio_init device %s requested rate %hu rate %hu latency %hu block_frames %hu fifoSize %lu\n",
                  device, rate, swa->sampleRate, swa->latency, block_frames, swa->fifoSize);

      u32 prio;
      svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
      rc = threadCreate(&swa->thread, &mainLoop, (void*)swa, THREAD_STACK_SIZE, prio + 1, AUDIO_THREAD_CPU);

      if (R_FAILED(rc))
      {
            RARCH_LOG("[Audio]: thread creation failed create %u\n", swa->thread.handle);
            swa->running = false;
            return NULL;
      }

      if (R_FAILED(threadStart(&swa->thread)))
      {
            RARCH_LOG("[Audio]: thread creation failed start %u\n", swa->thread.handle);
            threadClose(&swa->thread);
            swa->running = false;
            return NULL;
      }

      return swa;
}

static bool switch_thread_audio_start(void *data, bool is_shutdown)
{
      //RARCH_LOG("[Audio]: switch_thread_audio_start\n");
      switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

      if (!swa)
            return false;

      swa->is_paused = false;
      return true;
}

static bool switch_thread_audio_stop(void *data)
{
      //RARCH_LOG("[Audio]: switch_thread_audio_stop\n");
      switch_thread_audio_t* swa = (switch_thread_audio_t*)data;

      if (!swa)
            return false;

      swa->is_paused = true;
      return true;
}

static void switch_thread_audio_free(void *data)
{
      //RARCH_LOG("[Audio]: switch_thread_audio_free\n");
      switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

      if (!swa)
            return;

      if (swa->running)
      {
            swa->running = false;
            threadWaitForExit(&swa->thread);
            threadClose(&swa->thread);
      }

      audoutStopAudioOut();
      audoutExit();

      if (swa->fifo)
      {
            fifo_free(swa->fifo);
            swa->fifo = NULL;
      }

      for (int i = 0; i < AUDIO_BUFFER_COUNT; i++)
            free(swa->buffer[i].buffer);

      free(swa);
      swa = NULL;
}

static ssize_t switch_thread_audio_write(void *data, const void *buf, size_t size)
{
      switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

      if (!swa || !swa->running)
            return 0;

      size_t avail;
      size_t written;

      if (swa->nonblocking)
      {
            lockMutex(&swa->fifoLock);
            avail = fifo_write_avail(swa->fifo);
            written = MIN(avail, size);
            if (written > 0)
            {
                  fifo_write(swa->fifo, buf, written);
            }
            mutexUnlock(&swa->fifoLock);
      }
      else
      {
            written = 0;
            while (written < size && swa->running)
            {
                  lockMutex(&swa->fifoLock);
                  avail = fifo_write_avail(swa->fifo);
                  if (avail == 0)
                  {
                        mutexUnlock(&swa->fifoLock);
                        lockMutex(&swa->condLock);
                        if (swa->running)
                              condvarWait(&swa->cond);
                        mutexUnlock(&swa->condLock);
                  }
                  else
                  {
                        size_t write_amt = MIN(size - written, avail);
                        fifo_write(swa->fifo, (const char*)buf + written, write_amt);
                        mutexUnlock(&swa->fifoLock);
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
      //RARCH_LOG("[Audio]: switch_thread_audio_set_nonblock_state state %d\n", (int)state);
      switch_thread_audio_t *swa = (switch_thread_audio_t *)data;

      if (swa)
            swa->nonblocking = state;
}

static bool switch_thread_audio_use_float(void *data)
{
      (void)data;
      return false;
}

static size_t switch_thread_audio_write_avail(void *data)
{
      switch_thread_audio_t* swa = (switch_thread_audio_t*)data;

      lockMutex(&swa->fifoLock);
      size_t val = fifo_write_avail(swa->fifo);
      mutexUnlock(&swa->fifoLock);

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
      NULL, // device_list_new
      NULL, // device_list_free
      switch_thread_audio_write_avail,
      switch_thread_audio_buffer_size
};

/* vim: set ts=6 sw=6 sts=6: */
