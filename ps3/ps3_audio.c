/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../driver.h"
#include <stdlib.h>
#include <cell/audio.h>
#include <sys/timer.h>
#include <string.h>
#include <pthread.h>
#include "buffer.h"
#include "resampler.h"
#include <sys/event.h>

#define AUDIO_BLOCKS 8 // 8 or 16. Guess what we choose? :)
#define AUDIO_CHANNELS 2 // All hail glorious stereo!
#define AUDIO_OUT_RATE (48000.0)

typedef struct
{
   float tmp_data[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS];
   uint32_t audio_port;
   bool nonblocking;
   volatile bool quit_thread;
   fifo_buffer_t *buffer;
   uint64_t input_rate;

   pthread_t thread;
   pthread_mutex_t lock;
   pthread_mutex_t cond_lock;
   pthread_cond_t cond;
} ps3_audio_t;

static size_t drain_fifo(void *cb_data, float **data)
{
   ps3_audio_t *aud = cb_data;

   int16_t tmp[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS];

   if (fifo_read_avail(aud->buffer) >= sizeof(tmp))
   {
      pthread_mutex_lock(&aud->lock);
      fifo_read(aud->buffer, tmp, sizeof(tmp));
      pthread_mutex_unlock(&aud->lock);
      resampler_s16_to_float(aud->tmp_data, tmp, CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS);
   }
   else
   {
      memset(aud->tmp_data, 0, sizeof(aud->tmp_data));
   }
   *data = aud->tmp_data;
   return CELL_AUDIO_BLOCK_SAMPLES;
}

static void *event_loop(void *data)
{
   ps3_audio_t *aud = data;
   sys_event_queue_t id;
   sys_ipc_key_t key;
   sys_event_t event;

   cellAudioCreateNotifyEventQueue(&id, &key);
   cellAudioSetNotifyEventQueue(key);

   resampler_t *resampler = resampler_new(drain_fifo, AUDIO_OUT_RATE/aud->input_rate, 2, data);

   float out_tmp[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS] __attribute__((aligned(16)));

   while (!aud->quit_thread)
   {
      sys_event_queue_receive(id, &event, SYS_NO_TIMEOUT);
      resampler_cb_read(resampler, CELL_AUDIO_BLOCK_SAMPLES, out_tmp);
      cellAudioAddData(aud->audio_port, out_tmp, CELL_AUDIO_BLOCK_SAMPLES, 1.0);
      pthread_cond_signal(&aud->cond);
   }

   cellAudioRemoveNotifyEventQueue(key);
   resampler_free(resampler);
   pthread_exit(NULL);
   return NULL;
}

static void* __ps3_init(const char* device, unsigned rate, unsigned latency)
{
   (void)latency;
   (void)device;

   ps3_audio_t *data = calloc(1, sizeof(*data));
   if (data == NULL)
      return NULL;

   CellAudioPortParam params;

   cellAudioInit();

   params.nChannel = AUDIO_CHANNELS;
   params.nBlock = AUDIO_BLOCKS;
   params.attr = 0;

   if (cellAudioPortOpen(&params, &data->audio_port) != CELL_OK)
   {
      cellAudioQuit();
      return NULL;
   }

   // Create a small fifo buffer. :)
   data->buffer = fifo_new(CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS * AUDIO_BLOCKS * sizeof(int16_t));
   data->input_rate = rate;

   pthread_mutex_init(&data->lock, NULL);
   pthread_mutex_init(&data->cond_lock, NULL);
   pthread_cond_init(&data->cond, NULL);

   cellAudioPortStart(data->audio_port);
   pthread_create(&data->thread, NULL, event_loop, data);
   return data;
}

// Should make some noise at least. :)
static ssize_t __ps3_write(void* data, const void* buf, size_t size) // Recieve exactly 1024 bytes at a time.
{
   ps3_audio_t *aud = data;

   // We will continuously write slightly more data than we should per second, and rely on blocking mechanisms to ensure we don't write too much. 
   if (aud->nonblocking)
   {
      if (fifo_write_avail(aud->buffer) < size)
         return 0;
   }
   else
   {
      while (fifo_write_avail(aud->buffer) < size)
      {
         pthread_mutex_lock(&aud->cond_lock);
         pthread_cond_wait(&aud->cond, &aud->lock);
         pthread_mutex_unlock(&aud->cond_lock);
      }
   }

   pthread_mutex_lock(&aud->lock);
   fifo_write(aud->buffer, buf, size);
   pthread_mutex_unlock(&aud->lock);
   return size;
}

static bool __ps3_stop(void *data)
{
   //ps3_audio_t *aud = data;
   //cellAudioPortStop(aud->audio_port);
   return true;
}

static bool __ps3_start(void *data)
{
   //ps3_audio_t *aud = data;
   //cellAudioPortStart(aud->audio_port);
   return false;
}

static void __ps3_set_nonblock_state(void *data, bool state)
{
   ps3_audio_t *aud = data;
   aud->nonblocking = state;
}

static void __ps3_free(void *data)
{
   ps3_audio_t *aud = data;

   aud->quit_thread = true;
   pthread_join(aud->thread, NULL);

   cellAudioPortStop(aud->audio_port);
   cellAudioPortClose(aud->audio_port);
   cellAudioQuit();
   fifo_free(aud->buffer);

   pthread_mutex_destroy(&aud->lock);
   pthread_mutex_destroy(&aud->cond_lock);
   pthread_cond_destroy(&aud->cond);

   free(data);
}

const audio_driver_t audio_ps3 = {
   .init = __ps3_init,
   .write = __ps3_write,
   .stop = __ps3_stop,
   .start = __ps3_start,
   .set_nonblock_state = __ps3_set_nonblock_state,
   .free = __ps3_free
};
