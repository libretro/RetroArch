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
#include <boolean.h>

#include <xenon_sound/sound.h>
#include <time/time.h>

#include <retro_inline.h>

#include "../audio_driver.h"

#define SOUND_FREQUENCY 48000
#define FRAME_BYTES     4      /* int16 stereo */
/* libxenon's ring and one of its 32 descriptors, in bytes. The queue is
 * kept a descriptor short of the ring: xenon_sound_submit() copies in
 * without looking at what is still unplayed. */
#define XENON360_RING   65536
#define XENON360_BLOCK  2048
/* Frames converted per pass; a longer write goes in several. */
#define XENON360_STAGE_FRAMES 2048

typedef struct
{
   /* Bytes the driver lets queue: the latency setting. */
   size_t capacity;
   uint32_t buffer[XENON360_STAGE_FRAMES];
   bool nonblock;
   bool is_paused;
} xenon_audio_t;

static void *xenon360_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   static bool inited = false;
   xenon_audio_t *xa;

   if (!inited)
   {
      xenon_sound_init();
      inited = true;
   }

   *new_rate = SOUND_FREQUENCY;

   if (!(xa = (xenon_audio_t*)calloc(1, sizeof(*xa))))
      return NULL;

   xa->capacity  = (size_t)SOUND_FREQUENCY * latency / 1000 * FRAME_BYTES;
   if (xa->capacity < XENON360_BLOCK)
      xa->capacity = XENON360_BLOCK;
   if (xa->capacity > XENON360_RING - XENON360_BLOCK)
      xa->capacity = XENON360_RING - XENON360_BLOCK;

   return xa;
}

/* Bytes the queue takes now: the capacity less what is unplayed, in
 * whole frames. */
static size_t xenon360_audio_room(const xenon_audio_t *xa)
{
   int unplayed = xenon_sound_get_unplayed();
   if (unplayed < 0)
      unplayed = 0;
   if ((size_t)unplayed >= xa->capacity)
      return 0;
   return (xa->capacity - (size_t)unplayed) & ~(size_t)(FRAME_BYTES - 1);
}

/* Full 32-bit byte reversal of a packed stereo frame.  On this
 * big-endian host that is a per-sample 16-bit byteswap AND an L/R
 * channel swap fused together - i.e. the hardware is being fed
 * little-endian samples in R,L order.  Whether the channel swap is a
 * hardware requirement or a long-standing accident is unverifiable
 * without a devkit; the swap has shipped this way since the driver
 * was introduced, so it is documented rather than changed. */
static INLINE uint32_t xenon360_bswap_32(uint32_t val)
{
   return (val >> 24) | (val << 24) |
      ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000);
}

/* How many 50 us delays a blocking write waits for room before giving
 * up on it (about a second). */
#define XENON360_AUDIO_WAIT_LAPS 20000

/* Converts and submits in passes of at most the staging buffer, each no
 * more than the room; non-blocking, that is exactly what write_avail()
 * reported. */
static ssize_t xenon360_audio_write(void *data, const void *s, size_t len)
{
   size_t written         = 0;
   int laps               = XENON360_AUDIO_WAIT_LAPS;
   const uint32_t *in_buf = (const uint32_t*)s;
   xenon_audio_t *xa      = (xenon_audio_t*)data;

   len &= ~(size_t)(FRAME_BYTES - 1);

   while (written < len)
   {
      size_t i, n;
      size_t room = xenon360_audio_room(xa);

      if (!room)
      {
         /* Capped: a sound queue that stops draining never makes
          * room, and the write then returns what it took. libxenon
          * has no event to wait on. */
         if (xa->nonblock || --laps < 0)
            break;
         udelay(50);
         continue;
      }

      n = len - written;
      if (n > room)
         n = room;
      if (n > sizeof(xa->buffer))
         n = sizeof(xa->buffer);

      for (i = 0; i < n / FRAME_BYTES; i++)
         xa->buffer[i] = xenon360_bswap_32(in_buf[written / FRAME_BYTES + i]);
      xenon_sound_submit(xa->buffer, (int)n);
      written += n;
   }

   return (ssize_t)written;
}

static bool xenon360_audio_stop(void *data)
{
   xenon_audio_t *xa = data;
   xa->is_paused = true;
   return true;
}

static bool xenon360_audio_alive(void *data)
{
   xenon_audio_t *xa = data;
   if (!xa)
      return false;
   return !xa->is_paused;
}

static void xenon360_audio_set_nonblock_state(void *data, bool state)
{
   xenon_audio_t *xa = data;
   if (xa)
      xa->nonblock = state;
}

static bool xenon360_audio_start(void *data, bool is_shutdown)
{
   xenon_audio_t *xa = data;
   xa->is_paused = false;
   return true;
}

static void xenon360_audio_free(void *data)
{
   if (data)
      free(data);
}

/* libxenon's sound API submits 16-bit big-endian PCM to the hardware;
 * the driver byteswaps for it. There is no float path. */
static bool xenon360_use_float(void *data) { return false; }

static size_t xenon360_audio_write_avail(void *data)
{
   xenon_audio_t *xa = (xenon_audio_t*)data;
   return xa ? xenon360_audio_room(xa) : 0;
}

static size_t xenon360_audio_buffer_size(void *data)
{
   xenon_audio_t *xa = (xenon_audio_t*)data;
   return xa ? xa->capacity : 0;
}

audio_driver_t audio_xenon360 = {
   xenon360_audio_init,
   xenon360_audio_write,
   xenon360_audio_stop,
   xenon360_audio_start,
   xenon360_audio_alive,
   xenon360_audio_set_nonblock_state,
   xenon360_audio_free,
   xenon360_use_float,
   "xenon360",
   NULL,
   NULL,
   xenon360_audio_write_avail,
   xenon360_audio_buffer_size,
   NULL  /* write_raw */
};
