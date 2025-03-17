/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>

#include <queues/fifo_queue.h>

#include <defines/ps3_defines.h>

#include "../audio_driver.h"

#define AUDIO_BLOCKS 8
#define AUDIO_CHANNELS 2

typedef struct
{
   fifo_buffer_t *buffer;
   sys_ppu_thread_t thread;
   sys_lwmutex_t lock;
   sys_lwmutex_t cond_lock;
   sys_lwcond_t cond;
   uint32_t audio_port;
   bool nonblock;
   bool started;
   volatile bool quit_thread;
} ps3_audio_t;


#ifdef __PSL1GHT__
static void event_loop(void *data)
#else
static void event_loop(uint64_t data)
#endif
{
   float out_tmp[AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS]
      __attribute__((aligned(16)));
   sys_event_queue_t id;
   sys_ipc_key_t key;
   sys_event_t event;
   ps3_audio_t *aud = (ps3_audio_t*)(uintptr_t)data;

   audioCreateNotifyEventQueue(&id, &key);
   audioSetNotifyEventQueue(key);

   while (!aud->quit_thread)
   {
      sysEventQueueReceive(id, &event, PS3_SYS_NO_TIMEOUT);

      sysLwMutexLock(&aud->lock, PS3_SYS_NO_TIMEOUT);
      if (FIFO_READ_AVAIL(aud->buffer) >= sizeof(out_tmp))
         fifo_read(aud->buffer, out_tmp, sizeof(out_tmp));
      else
         memset(out_tmp, 0, sizeof(out_tmp));
      sysLwMutexUnlock(&aud->lock);
      sysLwCondSignal(&aud->cond);

      audioAddData(aud->audio_port, out_tmp,
            AUDIO_BLOCK_SAMPLES, 1.0);
   }

   audioRemoveNotifyEventQueue(key);
   sysThreadExit(0);
}

static void *ps3_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   audioPortParam params;
   ps3_audio_t *data                 = NULL;
#ifdef __PSL1GHT__
   sys_lwmutex_attr_t lock_attr      =
   {SYS_LWMUTEX_ATTR_PROTOCOL, SYS_LWMUTEX_ATTR_RECURSIVE, "\0"};
   sys_lwmutex_attr_t cond_lock_attr =
   {SYS_LWMUTEX_ATTR_PROTOCOL, SYS_LWMUTEX_ATTR_RECURSIVE, "\0"};
   sys_lwcond_attr_t cond_attr       = {"\0"};
#else
   sys_lwmutex_attr_t lock_attr;
   sys_lwmutex_attr_t cond_lock_attr;
   sys_lwcond_attr_t cond_attr;

   sys_lwmutex_attribute_initialize(lock_attr);
   sys_lwmutex_attribute_initialize(cond_lock_attr);
   sys_lwcond_attribute_initialize(cond_attr);
#endif

   data                              = calloc(1, sizeof(*data));
   if (!data)
      return NULL;

   audioInit();

   params.numChannels                = AUDIO_CHANNELS;
   params.numBlocks                  = AUDIO_BLOCKS;
   params.param_attrib               = 0;
#if 0
#ifdef HAVE_HEADSET
   if (global->console.sound.mode == SOUND_MODE_HEADSET)
      params.param_attrib            = CELL_AUDIO_PORTATTR_OUT_SECONDARY;
#endif
#endif

   if (audioPortOpen(&params, &data->audio_port) != CELL_OK)
   {
      audioQuit();
      free(data);
      return NULL;
   }

   data->buffer = fifo_new(AUDIO_BLOCK_SAMPLES *
         AUDIO_CHANNELS * AUDIO_BLOCKS * sizeof(float));

   sysLwMutexCreate(&data->lock, &lock_attr);
   sysLwMutexCreate(&data->cond_lock, &cond_lock_attr);
   sysLwCondCreate(&data->cond, &data->cond_lock, &cond_attr);

   audioPortStart(data->audio_port);
   data->started = true;
   sysThreadCreate(&data->thread, event_loop,
#ifdef __PSL1GHT__
   data,
#else
   (uint64_t)data,
#endif
   1500, 0x1000, SYS_THREAD_CREATE_JOINABLE, (char*)"sound");

   return data;
}

static ssize_t ps3_audio_write(void *data, const void *s, size_t len)
{
   ps3_audio_t *aud = data;

   if (aud->nonblock)
   {
      if (FIFO_WRITE_AVAIL(aud->buffer) < len)
         return 0;
   }

   while (FIFO_WRITE_AVAIL(aud->buffer) < len)
      sysLwCondWait(&aud->cond, 0);

   sysLwMutexLock(&aud->lock, PS3_SYS_NO_TIMEOUT);
   fifo_write(aud->buffer, s, len);
   sysLwMutexUnlock(&aud->lock);
   return len;
}

static bool ps3_audio_stop(void *data)
{
   ps3_audio_t *aud = data;
   if (aud->started)
   {
      audioPortStop(aud->audio_port);
      aud->started = false;
   }
   return true;
}

static bool ps3_audio_start(void *data, bool is_shutdown)
{
   ps3_audio_t *aud = data;
   if (!aud->started)
   {
      audioPortStart(aud->audio_port);
      aud->started = true;
   }
   return true;
}

static bool ps3_audio_alive(void *data)
{
   ps3_audio_t *aud = data;
   if (!aud)
      return false;
   return aud->started;
}

static void ps3_audio_set_nonblock_state(void *data, bool toggle)
{
   ps3_audio_t *aud = data;
   if (aud)
      aud->nonblock = toggle;
}

static void ps3_audio_free(void *data)
{
   uint64_t val;
   ps3_audio_t *aud = data;

   aud->quit_thread = true;
   ps3_audio_start(aud, false);
   sysThreadJoin(aud->thread, &val);

   ps3_audio_stop(aud);
   audioPortClose(aud->audio_port);
   audioQuit();
   fifo_free(aud->buffer);

   sysLwMutexDestroy(&aud->lock);
   sysLwMutexDestroy(&aud->cond_lock);
   sysLwCondDestroy(&aud->cond);

   free(data);
}

static bool ps3_audio_use_float(void *data) { return true; }

static size_t ps3_audio_write_avail(void *data)
{
   /* TODO/FIXME - implement? */
   return 0;
}

audio_driver_t audio_ps3 = {
   ps3_audio_init,
   ps3_audio_write,
   ps3_audio_stop,
   ps3_audio_start,
   ps3_audio_alive,
   ps3_audio_set_nonblock_state,
   ps3_audio_free,
   ps3_audio_use_float,
   "ps3",
   NULL,
   NULL,
   ps3_audio_write_avail,
   NULL
};
