/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Francisco Javier Trujillo Mata
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

#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <kernel.h>
#include <audsrv.h>

#include "../audio_driver.h"

typedef struct ps2_audio
{
   fifo_buffer_t* buffer;
   bool nonblocking;
   volatile bool running;
   int worker_thread;
   int lock;
   int cond_lock;

} ps2_audio_t;

static ps2_audio_t *backup_ps2;
static u8 audioThreadStack[4 * 1024] __attribute__ ((aligned(16)));

#define AUDIO_OUT_BUFFER 2 * 1024
#define AUDIO_BUFFER 64 * 1024
#define AUDIO_CHANNELS 2
#define AUDIO_BITS 16
#define AUDIO_PRIORITY 0x7F /* LOWER VALUE GRATHER PRIORITY*/

static void audioMainLoop(void *data)
{
   char out_tmp[AUDIO_OUT_BUFFER];
   ps2_audio_t* ps2 = backup_ps2;

   while (ps2->running)
   {
      size_t size;

      WaitSema(ps2->lock);
      size = MIN(fifo_read_avail(ps2->buffer), sizeof(out_tmp)); 
      fifo_read(ps2->buffer, out_tmp, size);
      iSignalSema(ps2->lock);
      iSignalSema(ps2->cond_lock);

      audsrv_wait_audio(size);
      audsrv_play_audio(out_tmp, size);
   }

   audsrv_stop_audio();
   ExitDeleteThread();
}

static void audioCreateThread(ps2_audio_t *ps2)
{
   int ret;
   ee_thread_t thread;

   thread.func=&audioMainLoop;
   thread.stack=audioThreadStack;
   thread.stack_size=sizeof(audioThreadStack);
   thread.gp_reg=&_gp;
   thread.initial_priority=AUDIO_PRIORITY;
   thread.attr=thread.option=0;

   /*Backup the PS2 content to be used in the thread */
   backup_ps2 = ps2;

   ps2->running = true;
   ps2->worker_thread = CreateThread(&thread);

   if (ps2->worker_thread >= 0) {
      ret = StartThread(ps2->worker_thread, NULL);
      if (ret < 0) {
         printf("sound_init: StartThread returned %d\n", ret);
      }
   } else {
      printf("CreateThread failed: %d\n", ps2->worker_thread);
   }
}

static void audioStopNDeleteThread(ps2_audio_t *ps2)
{
   ps2->running = false;
   if (ps2->worker_thread) {
      ps2->worker_thread = 0;
   }
}

static void audioConfigure(ps2_audio_t *ps2, unsigned rate)
{
   int err;
   struct audsrv_fmt_t format;

   format.bits = AUDIO_BITS;
   format.freq = rate;
   format.channels = AUDIO_CHANNELS;

   err = audsrv_set_format(&format);

   if (err){
      printf("set format returned %d\n", err);
      printf("audsrv returned error string: %s\n", audsrv_get_error_string());
   }

   audsrv_set_volume(MAX_VOLUME);
}

static void audioCreateSemas(ps2_audio_t *ps2)
{
   ee_sema_t lock_info;
   ee_sema_t cond_lock_info;

   lock_info.max_count = 1;
   lock_info.init_count = 1;
   lock_info.option = 0;
   ps2->lock = CreateSema(&lock_info);

   cond_lock_info.init_count = 1;
   cond_lock_info.max_count  = 1;
   cond_lock_info.option = 0;

   ps2->cond_lock = CreateSema(&cond_lock_info);
}

static void *ps2_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   ps2_audio_t *ps2 = (ps2_audio_t*)calloc(1, sizeof(ps2_audio_t));

   if (!ps2)
      return NULL;

   ps2->buffer = fifo_new(AUDIO_BUFFER);
   audioConfigure(ps2, rate);
   audioCreateSemas(ps2);
   audioCreateThread(ps2);

   return ps2;
}

static void ps2_audio_free(void *data)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if(!ps2)
      return;

   if(ps2->running){
      audioStopNDeleteThread(ps2);

      if (ps2->lock){
         iDeleteSema(ps2->lock);
         ps2->lock = 0;
      }

      if (ps2->cond_lock){
         iDeleteSema(ps2->cond_lock);
         ps2->cond_lock = 0;
      }

   }
   fifo_free(ps2->buffer);
   free(ps2);
}

static ssize_t ps2_audio_write(void *data, const void *buf, size_t size)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (!ps2->running)
      return -1;

   if (ps2->nonblocking){
      if (fifo_write_avail(ps2->buffer) < size)
         return 0;
   }

   while (fifo_write_avail(ps2->buffer) < size) {
      WaitSema(ps2->cond_lock);
   }

   WaitSema(ps2->lock);
   fifo_write(ps2->buffer, buf, size);
   iSignalSema(ps2->lock);

   return size;
}

static bool ps2_audio_alive(void *data)
{
   bool alive = false;

   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (ps2) {
      alive = ps2->running;
   }

   return alive;
}

static bool ps2_audio_stop(void *data)
{
   bool stop = true;
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2) {
      audioStopNDeleteThread(ps2);
      audsrv_stop_audio();
   }

   return stop;
}

static bool ps2_audio_start(void *data, bool is_shutdown)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   bool start = true;

   if (ps2) {
      if (!ps2->running && !ps2->worker_thread) {
         audioCreateThread(ps2);
      }
   }

   return start;
}

static void ps2_audio_set_nonblock_state(void *data, bool toggle)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2) {
      ps2->nonblocking = toggle;
   }
}

static bool ps2_audio_use_float(void *data)
{
   return false;
}

static size_t ps2_audio_write_avail(void *data)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2 && ps2->running) {
      size_t size;
      WaitSema(ps2->lock);
      size = AUDIO_BUFFER - fifo_read_avail(ps2->buffer); 
      iSignalSema(ps2->lock);
      return size;
   }

   return 0;
}

static size_t ps2_audio_buffer_size(void *data)
{
   return AUDIO_BUFFER;
}

audio_driver_t audio_ps2 = {
   ps2_audio_init,
   ps2_audio_write,
   ps2_audio_stop,
   ps2_audio_start,
   ps2_audio_alive,
   ps2_audio_set_nonblock_state,
   ps2_audio_free,
   ps2_audio_use_float,
   "ps2",
   NULL,
   NULL,
   ps2_audio_write_avail,
   ps2_audio_buffer_size
};
