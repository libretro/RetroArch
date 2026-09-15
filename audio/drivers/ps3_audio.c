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

#include <retro_spsc.h>

#include <defines/ps3_defines.h>

#include "../audio_driver.h"

#define AUDIO_BLOCKS 8
#define AUDIO_CHANNELS 2

/* Bound on any wait for the audio thread to consume, in microseconds
 * (sysLwCondWait's unit; 0 there means no timeout at all), and how many
 * of them before the caller gets the pass back. */
#define PS3_AUDIO_WAIT_US   100000
#define PS3_AUDIO_WAIT_LAPS 8

typedef struct
{
   /* Single producer (the frontend in ps3_audio_write), single
    * consumer (the output thread in ps3_event_loop): a lock-free
    * retro_spsc ring, so the writer no longer takes a mutex the
    * output thread holds across its pull, as it did with the fifo.
    * retro_spsc rounds capacity up to a power of two; ring_size is
    * the size asked for and the producer never fills past it.
    * cond_lock/cond remain for the writer's bounded waits. */
   retro_spsc_t ring;
   size_t ring_size;
   bool ring_init;
   sys_ppu_thread_t thread;
   sys_lwmutex_t cond_lock;
   sys_lwcond_t cond;
   uint32_t audio_port;
   bool nonblock;
   bool started;
   volatile bool quit_thread;
} ps3_audio_t;


#ifdef __PSL1GHT__
static void ps3_event_loop(void *data)
#else
static void ps3_event_loop(uint64_t data)
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

      if (retro_spsc_read_avail(&aud->ring) >= sizeof(out_tmp))
         retro_spsc_read(&aud->ring, out_tmp, sizeof(out_tmp));
      else
         memset(out_tmp, 0, sizeof(out_tmp));
      sysLwCondSignal(&aud->cond);

      audioAddData(aud->audio_port, out_tmp,
            AUDIO_BLOCK_SAMPLES, 1.0);
   }

   audioRemoveNotifyEventQueue(key);
   sysThreadExit(0);
}

static void *ps3_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   audioPortParam params;
   ps3_audio_t *data                 = NULL;
#ifdef __PSL1GHT__
   sys_lwmutex_attr_t cond_lock_attr =
   {SYS_LWMUTEX_ATTR_PROTOCOL, SYS_LWMUTEX_ATTR_RECURSIVE, "\0"};
   sys_lwcond_attr_t cond_attr       = {"\0"};
#else
   sys_lwmutex_attr_t cond_lock_attr;
   sys_lwcond_attr_t cond_attr;

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

   data->ring_size = AUDIO_BLOCK_SAMPLES *
         AUDIO_CHANNELS * AUDIO_BLOCKS * sizeof(float);
   data->ring_init = retro_spsc_init(&data->ring, data->ring_size);
   if (!data->ring_init)
   {
      audioPortClose(data->audio_port);
      audioQuit();
      free(data);
      return NULL;
   }

   sysLwMutexCreate(&data->cond_lock, &cond_lock_attr);
   sysLwCondCreate(&data->cond, &data->cond_lock, &cond_attr);

   audioPortStart(data->audio_port);
   data->started = true;
   sysThreadCreate(&data->thread, ps3_event_loop,
#ifdef __PSL1GHT__
   data,
#else
   (uint64_t)data,
#endif
   1500, 0x1000, SYS_THREAD_CREATE_JOINABLE, (char*)"sound");

   return data;
}

static size_t ps3_audio_write_avail(void *data);

/* Sleep on the condition the output thread signals after each block it
 * takes.  sysLwCondWait() requires the lwcond's mutex - cond_lock, the
 * one it was created with - to be held by the caller; called without
 * it, it fails at once (EPERM) instead of sleeping, so the bounded
 * waits below became a few microseconds of spinning and then gave up,
 * returning 0 and dropping the audio, whenever the fifo was full. */
static void ps3_audio_wait_block(ps3_audio_t *aud)
{
   sysLwMutexLock(&aud->cond_lock, PS3_SYS_NO_TIMEOUT);
   sysLwCondWait(&aud->cond, PS3_AUDIO_WAIT_US);
   sysLwMutexUnlock(&aud->cond_lock);
}

static ssize_t ps3_audio_write(void *data, const void *s, size_t len)
{
   ps3_audio_t *aud = data;

   if (aud->nonblock)
   {
      if (ps3_audio_write_avail(aud) < len)
         return 0;
   }

   {
      /* The audio thread signals each time it consumes a block. One
       * that has stopped - the port closed, the thread quitting -
       * signals nothing; the wait is timed and capped, and the write
       * then returns having written nothing rather than holding the
       * caller. */
      int laps = PS3_AUDIO_WAIT_LAPS;
      while (ps3_audio_write_avail(aud) < len)
      {
         if (!aud->started || aud->quit_thread)
            return 0;
         ps3_audio_wait_block(aud);
         if (--laps < 0)
            return 0;
      }
   }

   retro_spsc_write(&aud->ring, s, len);
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
   if (aud->ring_init)
      retro_spsc_free(&aud->ring);

   sysLwMutexDestroy(&aud->cond_lock);
   sysLwCondDestroy(&aud->cond);

   free(data);
}

static bool ps3_audio_use_float(void *data) { return true; }

/* Room against the size asked for, not the rounded-up capacity.
 * Producer-side query. */
static size_t ps3_audio_write_avail(void *data)
{
   ps3_audio_t *aud = data;
   size_t room   = retro_spsc_write_avail(&aud->ring);
   size_t excess = aud->ring.capacity - aud->ring_size;
   return room > excess ? room - excess : 0;
}

static size_t ps3_audio_buffer_size(void *data)
{
   ps3_audio_t *aud = data;
   return aud->ring_size;
}

/* Sleep on the condition the output thread signals after every block
 * it takes from the fifo, until at least len bytes fit, len capped at
 * half the fifo so the wait always ends. Returns the free space then,
 * or 0 once the port has been stopped. */
static size_t ps3_audio_wait_writable(void *data, size_t len)
{
   ps3_audio_t *aud = data;
   size_t avail;
   int laps         = PS3_AUDIO_WAIT_LAPS;

   if (len > aud->ring_size / 2)
      len = aud->ring_size / 2;

   for (;;)
   {
      if (!aud->started || aud->quit_thread)
         return 0;
      avail = ps3_audio_write_avail(aud);
      if (avail >= len)
         return avail;
      /* Timed and capped, as in the write: no room after this many
       * waits is a thread that has stopped consuming, and the pass is
       * handed back as no space coming from this call. */
      ps3_audio_wait_block(aud);
      if (--laps < 0)
         return 0;
   }
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
   ps3_audio_buffer_size,
   NULL, /* write_raw */
   ps3_audio_wait_writable
};
