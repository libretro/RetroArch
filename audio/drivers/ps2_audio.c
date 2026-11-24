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
#include <string.h>

#include <kernel.h>
#include <audsrv.h>

#include <retro_atomic.h>

#include "../audio_driver.h"

#define AUDIO_BUFFER 128 * 1024
#define AUDIO_CHANNELS 2
#define AUDIO_BITS 16
#define AUDIO_FRAME_BYTES (AUDIO_CHANNELS * (AUDIO_BITS / 8))

/* What audsrv calls back on: the IOP sends the fill-buffer callback
 * after each block the SPU takes while at least this much of its ring
 * is free, so a waiter wakes once a block for as long as there is room
 * to look at. One SPU block of stereo 16-bit. */
#define AUDIO_ROOM_THRESHOLD (512 * AUDIO_FRAME_BYTES)

/* audsrv_wait_audio() is the SDK's own wait and has no bound, and
 * WaitSema() has none either, so the writable wait is bounded by a
 * kernel alarm posting the same semaphore: H-SYNC ticks, about 64 us
 * each, so roughly 32 ms. Never reached while audio plays - a block
 * frees long before it. */
#define AUDIO_WAIT_HSYNC 500

typedef struct ps2_audio
{
   /* Bytes handed to audsrv since the format was set. Less what its
    * ring still holds, that is what the SPU has played. */
   uint64_t bytes_written;
   /* What audsrv reported free before anything was queued, which is
    * its ring. write_avail() is measured against it. */
   size_t   ring_size;
   /* Posted by audsrv's fill-buffer callback, and by the writable
    * wait's alarm */
   s32  room_sema;
   /* TODO/FIXME - nonblock is not implemented */
   bool nonblock;
   bool running;
} ps2_audio_t;

/* Set by the writable wait's alarm; one driver at a time. A plain word
 * store on the EE, safe from the alarm's interrupt context. */
static retro_atomic_int_t ps2_audio_alarm_rang;

/* On audsrv's RPC server thread, an ordinary EE thread: the semaphore
 * alone, so a call still on its way after the driver is gone touches
 * nothing it freed */
static int ps2_audio_room_cb(void *arg)
{
   SignalSema((s32)(intptr_t)arg);
   return 0;
}

/* In the alarm's interrupt context */
static void ps2_audio_alarm(s32 alarm_id, u16 time, void *arg)
{
   (void)alarm_id;
   (void)time;
   retro_atomic_store_release_int(&ps2_audio_alarm_rang, 1);
   iSignalSema((s32)(intptr_t)arg);
   ExitHandler();
}

static void *ps2_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   int avail;
   ee_sema_t sema;
   struct audsrv_fmt_t format;
   ps2_audio_t *ps2 = (ps2_audio_t*)calloc(1, sizeof(ps2_audio_t));

   if (!ps2)
      return NULL;

   sema.init_count = 0;
   sema.max_count  = 1;
   sema.option     = 0;
   if ((ps2->room_sema = CreateSema(&sema)) < 0)
   {
      free(ps2);
      return NULL;
   }

   format.bits     = AUDIO_BITS;
   format.freq     = rate;
   format.channels = AUDIO_CHANNELS;

   audsrv_set_format(&format);
   audsrv_set_volume(MAX_VOLUME);

   /* Asked with nothing queued, so what is free is the whole ring. */
   avail           = audsrv_available();
   ps2->ring_size  = (avail > 0) ? (size_t)avail : (size_t)AUDIO_BUFFER;

   audsrv_on_fillbuf(AUDIO_ROOM_THRESHOLD, ps2_audio_room_cb,
         (void*)(intptr_t)ps2->room_sema);

   return ps2;
}

static void ps2_audio_free(void *data)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (!ps2)
      return;

   ps2->running = false;
   audsrv_stop_audio();
   /* A threshold of zero calls back no more */
   audsrv_on_fillbuf(0, NULL, NULL);
   DeleteSema(ps2->room_sema);
   free(ps2);
}

static ssize_t ps2_audio_write(void *data, const void *s, size_t len)
{
   int ret;
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (!ps2->running)
      return -1;
   ret = audsrv_play_audio(s, len);
   if (ret > 0)
      ps2->bytes_written += (uint64_t)ret;
   return ret;
}

static bool ps2_audio_alive(void *data)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (ps2)
      return ps2->running;
   return false;
}

static bool ps2_audio_stop(void *data)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (ps2)
   {
      audsrv_stop_audio();
      ps2->running = false;
   }
   return true;
}

static bool ps2_audio_start(void *data, bool is_shutdown)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (ps2)
      ps2->running = true;
   return true;
}

static void ps2_audio_set_nonblock_state(void *data, bool toggle)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2)
      ps2->nonblock = toggle;
}

/* What audsrv says is free in its ring: a measurement, not the whole
 * buffer reported as free every frame, which is what this used to do
 * and what rate control answered with its full upward correction for
 * as long as it was on. */
static size_t ps2_audio_write_avail(void *data)
{
   int avail;
   ps2_audio_t *ps2 = (ps2_audio_t*)data;

   if (!ps2 || !ps2->running)
      return 0;
   if ((avail = audsrv_available()) <= 0)
      return 0;
   if ((size_t)avail > ps2->ring_size)
      return ps2->ring_size;
   return (size_t)avail;
}

/* Frames the SPU has played: what was handed over, less what audsrv
 * still holds for it. */
static size_t ps2_audio_frames_consumed(void *data)
{
   int queued;
   uint64_t played;
   ps2_audio_t *ps2 = (ps2_audio_t*)data;

   if (!ps2)
      return 0;
   if ((queued = audsrv_queued()) < 0)
      queued = 0;
   played = ((uint64_t)queued >= ps2->bytes_written)
      ? 0 : ps2->bytes_written - (uint64_t)queued;
   return (size_t)(played / AUDIO_FRAME_BYTES);
}

/* Until len bytes fit, on the semaphore audsrv's fill-buffer callback
 * posts once a block while there is room, bounded by one alarm for the
 * whole wait. Returns the free space then, or 0 at the bound. */
static size_t ps2_audio_wait_writable(void *data, size_t len)
{
   s32 alarm;
   size_t avail;
   ps2_audio_t *ps2 = (ps2_audio_t*)data;

   if (!ps2 || !ps2->running)
      return 0;
   /* Capped at half the ring, so the wait always has an end within a
    * draining server's reach. */
   if (len > ps2->ring_size / 2)
      len = ps2->ring_size / 2;

   if ((avail = ps2_audio_write_avail(ps2)) >= len)
      return avail;

   /* Posts from before this wait say nothing about now */
   retro_atomic_store_release_int(&ps2_audio_alarm_rang, 0);
   while (PollSema(ps2->room_sema) >= 0) { }
   if ((alarm = SetAlarm(AUDIO_WAIT_HSYNC, ps2_audio_alarm,
               (void*)(intptr_t)ps2->room_sema)) < 0)
      return 0;
   for (;;)
   {
      if ((avail = ps2_audio_write_avail(ps2)) >= len)
         break;
      if (     retro_atomic_load_acquire_int(&ps2_audio_alarm_rang)
            || !ps2->running)
      {
         avail = 0;
         break;
      }
      WaitSema(ps2->room_sema);
   }
   /* With interrupts off the alarm cannot ring between the look and
    * the release, so an alarm id the kernel has since reused is never
    * the one released */
   {
      int masked = DIntr();
      if (!retro_atomic_load_acquire_int(&ps2_audio_alarm_rang))
         iReleaseAlarm(alarm);
      if (masked)
         EIntr();
   }
   return avail;
}

static bool ps2_audio_use_float(void *data) { return false; }

static size_t ps2_audio_buffer_size(void *data)
{
   ps2_audio_t *ps2 = (ps2_audio_t*)data;
   return ps2 ? ps2->ring_size : (size_t)AUDIO_BUFFER;
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
   ps2_audio_buffer_size,
   NULL, /* write_raw */
   ps2_audio_wait_writable,
   ps2_audio_frames_consumed
};
