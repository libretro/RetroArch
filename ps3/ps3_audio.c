/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "../driver.h"
#include "../general.h"
#include <stdlib.h>

#include <string.h>
#include "../fifo_buffer.h"

#include "sdk_defines.h"

#define AUDIO_BLOCKS 8 // 8 or 16. Guess what we choose? :)
#define AUDIO_CHANNELS 2 // All hail glorious stereo!

typedef struct
{
   uint32_t audio_port;
   bool nonblocking;
   volatile bool quit_thread;
   fifo_buffer_t *buffer;

   sys_ppu_thread_t thread;
   sys_lwmutex_t lock;
   sys_lwmutex_t cond_lock;
   sys_lwcond_t cond;
} ps3_audio_t;

#ifdef __PSL1GHT__
static void event_loop(void *data)
#else
static void event_loop(uint64_t data)
#endif
{
#ifdef __PSL1GHT__
   ps3_audio_t *aud = data;
#else
   void * ptr_data = (void*)(uintptr_t)data;
   ps3_audio_t *aud = ptr_data;
#endif
   sys_event_queue_t id;
   sys_ipc_key_t key;
   sys_event_t event;

   cellAudioCreateNotifyEventQueue(&id, &key);
   cellAudioSetNotifyEventQueue(key);

   float out_tmp[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS] __attribute__((aligned(16)));

   while (!aud->quit_thread)
   {
      sys_event_queue_receive(id, &event, SYS_NO_TIMEOUT);

      sys_lwmutex_lock(&aud->lock, SYS_NO_TIMEOUT);
      if (fifo_read_avail(aud->buffer) >= sizeof(out_tmp))
         fifo_read(aud->buffer, out_tmp, sizeof(out_tmp));
      else
         memset(out_tmp, 0, sizeof(out_tmp));
      sys_lwmutex_unlock(&aud->lock);
      sys_lwcond_signal(&aud->cond);

      cellAudioAddData(aud->audio_port, out_tmp, CELL_AUDIO_BLOCK_SAMPLES, 1.0);
   }

   cellAudioRemoveNotifyEventQueue(key);
   sys_ppu_thread_exit(0);
}

static void *ps3_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)latency;
   (void)device;
   (void)rate;

   ps3_audio_t *data = calloc(1, sizeof(*data));
   if (!data)
      return NULL;

   CellAudioPortParam params;
   cellAudioInit();
   params.numChannels = AUDIO_CHANNELS;
   params.numBlocks = AUDIO_BLOCKS;
#ifdef HAVE_HEADSET
   if(g_extern.console.sound.mode == SOUND_MODE_HEADSET)
      params.param_attrib = CELL_AUDIO_PORTATTR_OUT_SECONDARY;
   else
#endif
      params.param_attrib = 0;

   if (cellAudioPortOpen(&params, &data->audio_port) != CELL_OK)
   {
      cellAudioQuit();
      free(data);
      return NULL;
   }

   data->buffer = fifo_new(CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS * AUDIO_BLOCKS * sizeof(float));

#ifdef __PSL1GHT__
   sys_lwmutex_attr_t lock_attr = {SYS_LWMUTEX_ATTR_PROTOCOL, SYS_LWMUTEX_ATTR_RECURSIVE, "\0"};
   sys_lwmutex_attr_t cond_lock_attr = {SYS_LWMUTEX_ATTR_PROTOCOL, SYS_LWMUTEX_ATTR_RECURSIVE, "\0"};
   sys_lwcond_attribute_t cond_attr = {"\0"};
#else
   sys_lwmutex_attribute_t lock_attr;
   sys_lwmutex_attribute_t cond_lock_attr;
   sys_lwcond_attribute_t cond_attr;

   sys_lwmutex_attribute_initialize(lock_attr);
   sys_lwmutex_attribute_initialize(cond_lock_attr);
   sys_lwcond_attribute_initialize(cond_attr);
#endif

   sys_lwmutex_create(&data->lock, &lock_attr);
   sys_lwmutex_create(&data->cond_lock, &cond_lock_attr);
   sys_lwcond_create(&data->cond, &data->cond_lock, &cond_attr);

   cellAudioPortStart(data->audio_port);
   sys_ppu_thread_create(&data->thread, event_loop,
#ifdef __PSL1GHT__
   data,
#else
   (uint64_t)data,
#endif
   1500, 0x1000, SYS_PPU_THREAD_CREATE_JOINABLE, (char*)"sound");

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
         sys_lwcond_wait(&aud->cond, 0);
   }

   sys_lwmutex_lock(&aud->lock, SYS_NO_TIMEOUT);
   fifo_write(aud->buffer, buf, size);
   sys_lwmutex_unlock(&aud->lock);
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
   return true;
}

static void ps3_audio_set_nonblock_state(void *data, bool state)
{
   ps3_audio_t *aud = data;
   aud->nonblocking = state;
}

static void ps3_audio_free(void *data)
{
   uint64_t val;
   ps3_audio_t *aud = data;

   aud->quit_thread = true;
   cellAudioPortStart(aud->audio_port);
   sys_ppu_thread_join(aud->thread, &val);

   cellAudioPortStop(aud->audio_port);
   cellAudioPortClose(aud->audio_port);
   cellAudioQuit();
   fifo_free(aud->buffer);

   sys_lwmutex_destroy(&aud->lock);
   sys_lwmutex_destroy(&aud->cond_lock);
   sys_lwcond_destroy(&aud->cond);

   free(data);
}

static bool ps3_audio_use_float(void *data)
{
   (void)data;
   return true;
}

const audio_driver_t audio_ps3 = {
   ps3_audio_init,
   ps3_audio_write,
   ps3_audio_stop,
   ps3_audio_start,
   ps3_audio_set_nonblock_state,
   ps3_audio_use_float,
   ps3_audio_free,
   "ps3"
};

