/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifdef __PSL1GHT__
#include <audio/audio.h>
#else
#include <cell/audio.h>
#endif

#include <string.h>
#include "../fifo_buffer.h"

#include "sdk_defines.h"

#ifdef __PSL1GHT__
#include <sys/event_queue.h>
#include <lv2/mutex.h>
#include <lv2/cond.h>
//forward decl. for audioAddData
extern int audioAddData(uint32_t portNum, float *data, uint32_t frames, float volume);
#else
#include <sys/event.h>
#include <sys/synchronization.h>
#endif

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

static void event_loop(uint64_t data)
{
   void * ptr_data = (void*)(uintptr_t)data;
   ps3_audio_t *aud = ptr_data;
   sys_event_queue_t id;
   sys_ipc_key_t key;
   sys_event_t event;

   pAudioCreateNotifyEventQueue(&id, &key);
   pAudioSetNotifyEventQueue(key);

   float out_tmp[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS] __attribute__((aligned(16)));

   while (!aud->quit_thread)
   {
      pSysEventQueueReceive(id, &event, SYS_NO_TIMEOUT);

      pLwMutexLock(&aud->lock, SYS_NO_TIMEOUT);
      if (fifo_read_avail(aud->buffer) >= sizeof(out_tmp))
         fifo_read(aud->buffer, out_tmp, sizeof(out_tmp));
      else
         memset(out_tmp, 0, sizeof(out_tmp));
      pLwMutexUnlock(&aud->lock);
      pLwCondSignal(&aud->cond);

      pAudioAddData(aud->audio_port, out_tmp, CELL_AUDIO_BLOCK_SAMPLES, 1.0);
   }

   pAudioRemoveNotifyEventQueue(key);
   sys_ppu_thread_exit(0);
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

   pAudioPortParam params;
   pAudioInit();
   params.numChannels = AUDIO_CHANNELS;
   params.numBlocks = AUDIO_BLOCKS;
#ifdef HAVE_HEADSET
   if(g_console.sound_mode == SOUND_MODE_HEADSET)
      params.param_attrib = CELL_AUDIO_PORTATTR_OUT_SECONDARY;
   else
#endif
      params.param_attrib = 0;

   if (pAudioPortOpen(&params, &data->audio_port) != CELL_OK)
   {
      pAudioQuit();
      free(data);
      return NULL;
   }

   data->buffer = fifo_new(CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS * AUDIO_BLOCKS * sizeof(float));

#ifdef __PSL1GHT__
   sys_lwmutex_attr_t lock_attr = {SYS_LWMUTEX_ATTR_PROTOCOL, SYS_LWMUTEX_ATTR_RECURSIVE, '\0'};
   sys_lwmutex_attr_t cond_lock_attr = {SYS_LWMUTEX_ATTR_PROTOCOL, SYS_LWMUTEX_ATTR_RECURSIVE, '\0'};
   sys_lwcond_attribute_t cond_attr = {'\0'};
#else
   sys_lwmutex_attribute_t lock_attr;
   sys_lwmutex_attribute_t cond_lock_attr;
   sys_lwcond_attribute_t cond_attr;
#endif

#ifndef __PSL1GHT__
   pLwMutexAttributeInitialize(lock_attr);
   pLwMutexAttributeInitialize(cond_lock_attr);
   sys_lwcond_attribute_initialize(cond_attr);
#endif

   pLwMutexCreate(&data->lock, &lock_attr);
   pLwMutexCreate(&data->cond_lock, &cond_lock_attr);
   pLwCondCreate(&data->cond, &data->cond_lock, &cond_attr);

   pAudioPortStart(data->audio_port);
   sys_ppu_thread_create(&data->thread, event_loop, (uint64_t)data, 1500, 0x1000, SYS_PPU_THREAD_CREATE_JOINABLE, (char*)"sound");
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
         pLwCondWait(&aud->cond, 0);
   }

   pLwMutexLock(&aud->lock, SYS_NO_TIMEOUT);
   fifo_write(aud->buffer, buf, size);
   pLwMutexUnlock(&aud->lock);
   return size;
}

static bool ps3_audio_stop(void *data)
{
   ps3_audio_t *aud = data;
   pAudioPortStop(aud->audio_port);
   return true;
}

static bool ps3_audio_start(void *data)
{
   ps3_audio_t *aud = data;
   pAudioPortStart(aud->audio_port);
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
   pAudioPortStart(aud->audio_port);
   sys_ppu_thread_join(aud->thread, &val);

   pAudioPortStop(aud->audio_port);
   pAudioPortClose(aud->audio_port);
   pAudioQuit();
   fifo_free(aud->buffer);

   pLwMutexDestroy(&aud->lock);
   pLwMutexDestroy(&aud->cond_lock);
   pLwCondDestroy(&aud->cond);

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

