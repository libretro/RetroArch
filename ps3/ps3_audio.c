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
#include "../general.h"
#include <stdlib.h>
#include <cell/audio.h>
#include <sys/timer.h>
#include <string.h>
#include <pthread.h>
#include "../fifo_buffer.h"
#include <sys/event.h>

#define AUDIO_BLOCKS 8 // 8 or 16. Guess what we choose? :)
#define AUDIO_CHANNELS 2 // All hail glorious stereo!

typedef struct
{
   uint32_t audio_port;
   bool nonblocking;
   volatile bool quit_thread;
   fifo_buffer_t *buffer;

   pthread_t thread;
   pthread_mutex_t lock;
   pthread_mutex_t cond_lock;
   pthread_cond_t cond;
} ps3_audio_t;

static void *event_loop(void *data)
{
   ps3_audio_t *aud = data;
   sys_event_queue_t id;
   sys_ipc_key_t key;
   sys_event_t event;

   cellAudioCreateNotifyEventQueue(&id, &key);
   cellAudioSetNotifyEventQueue(key);

   float out_tmp[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS] __attribute__((aligned(16)));

   while (!aud->quit_thread)
   {
      sys_event_queue_receive(id, &event, SYS_NO_TIMEOUT);

      pthread_mutex_lock(&aud->lock);
      if (fifo_read_avail(aud->buffer) >= sizeof(out_tmp))
         fifo_read(aud->buffer, out_tmp, sizeof(out_tmp));
      else
         memset(out_tmp, 0, sizeof(out_tmp));
      pthread_mutex_unlock(&aud->lock);

      cellAudioAddData(aud->audio_port, out_tmp, CELL_AUDIO_BLOCK_SAMPLES, 1.0);
      pthread_cond_signal(&aud->cond);
   }

   cellAudioRemoveNotifyEventQueue(key);
   pthread_exit(NULL);
   return NULL;
}

static void *ps3_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)latency;
   (void)device;
   (void)rate; // Always use 48kHz.
   g_settings.audio.out_rate = 48000.0;

   ps3_audio_t *data = calloc(1, sizeof(*data));
   if (!data)
      return NULL;

   CellAudioPortParam params;
   cellAudioInit();
   params.nChannel = AUDIO_CHANNELS;
   params.nBlock = AUDIO_BLOCKS;
   params.attr = 0;

   if (cellAudioPortOpen(&params, &data->audio_port) != CELL_OK)
   {
      cellAudioQuit();
      free(data);
      return NULL;
   }

   data->buffer = fifo_new(CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS * AUDIO_BLOCKS * sizeof(float));

   pthread_mutex_init(&data->lock, NULL);
   pthread_mutex_init(&data->cond_lock, NULL);
   pthread_cond_init(&data->cond, NULL);

   cellAudioPortStart(data->audio_port);
   pthread_create(&data->thread, NULL, event_loop, data);
   return data;
}

static ssize_t ps3_audio_write(void *data, const void *buf, size_t size)
{
   ps3_audio_t *aud = data;

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

static bool ps3_audio_stop(void *data)
{
   ps3_audio_t *aud = data;
   cellAudioPortStop(aud->audio_port);
   return true;
}

static bool ps3_audio_start(void *data)
{
   ps3_audio_t *aud = data;
   cellAudioPortStart(aud->audio_port);
   return false;
}

static void ps3_audio_set_nonblock_state(void *data, bool state)
{
   ps3_audio_t *aud = data;
   aud->nonblocking = state;
}

static void ps3_audio_free(void *data)
{
   ps3_audio_t *aud = data;

   aud->quit_thread = true;
   cellAudioPortStart(aud->audio_port);
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

static bool ps3_audio_use_float(void *data)
{
   (void)data;
   return true;
}

const audio_driver_t audio_ps3 = {
   .init = ps3_audio_init,
   .write = ps3_audio_write,
   .stop = ps3_audio_stop,
   .start = ps3_audio_start,
   .set_nonblock_state = ps3_audio_set_nonblock_state,
   .use_float = ps3_audio_use_float,
   .free = ps3_audio_free,
   .ident = "ps3"
};

