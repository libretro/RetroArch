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

#include <retro_timers.h>

#include "../audio_driver.h"

#define AUDIO_BUFFER 128 * 1024
#define AUDIO_CHANNELS 2
#define AUDIO_BITS 16
#define AUDIO_FRAME_BYTES (AUDIO_CHANNELS * (AUDIO_BITS / 8))

/* audsrv_wait_audio() is the SDK's own wait and has no bound, so a
 * server that stops draining parks the caller for good. Polling what
 * audsrv reports free ends either way: the laps cap the case where it
 * keeps draining but never frees enough, and the case where it has
 * stopped. Never reached while audio plays - a period frees long
 * before this does. */
#define AUDIO_WAIT_LAPS 8
#define AUDIO_WAIT_US   4000

typedef struct ps2_audio
{
   /* Bytes handed to audsrv since the format was set. Less what its
    * ring still holds, that is what the SPU has played. */
   uint64_t bytes_written;
   /* What audsrv reported free before anything was queued, which is
    * its ring. write_avail() is measured against it. */
   size_t   ring_size;
   /* TODO/FIXME - nonblock is not implemented */
   bool nonblock;
   bool running;
} ps2_audio_t;

static void *ps2_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   int avail;
   struct audsrv_fmt_t format;
   ps2_audio_t *ps2 = (ps2_audio_t*)calloc(1, sizeof(ps2_audio_t));

   if (!ps2)
      return NULL;

   format.bits     = AUDIO_BITS;
   format.freq     = rate;
   format.channels = AUDIO_CHANNELS;

   audsrv_set_format(&format);
   audsrv_set_volume(MAX_VOLUME);

   /* Asked with nothing queued, so what is free is the whole ring. */
   avail           = audsrv_available();
   ps2->ring_size  = (avail > 0) ? (size_t)avail : (size_t)AUDIO_BUFFER;

   return ps2;
}

static void ps2_audio_free(void *data)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (!ps2)
      return;

   ps2->running = false;
   audsrv_stop_audio();
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

static size_t ps2_audio_wait_writable(void *data, size_t len)
{
   int laps         = AUDIO_WAIT_LAPS;
   ps2_audio_t *ps2 = (ps2_audio_t*)data;

   if (!ps2 || !ps2->running)
      return 0;
   /* Capped at half the ring, so the wait always has an end within a
    * draining server's reach. */
   if (len > ps2->ring_size / 2)
      len = ps2->ring_size / 2;

   for (;;)
   {
      size_t avail = ps2_audio_write_avail(ps2);
      if (avail >= len)
         return avail;
      if (--laps < 0)
         break;
      retro_sleep_us(AUDIO_WAIT_US);
   }
   return 0;
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
