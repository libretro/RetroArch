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

#include <retro_inline.h>

#include "../../retroarch.h"

#define SOUND_FREQUENCY 48000
#define MAX_BUFFER 2048

typedef struct
{
   uint32_t buffer[2048];
   bool nonblock;
   bool is_paused;
} xenon_audio_t;

static void *xenon360_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   static bool inited = false;

   if (!inited)
   {
      xenon_sound_init();
      inited = true;
   }

   *new_rate = SOUND_FREQUENCY;

   return calloc(1, sizeof(xenon_audio_t));
}

static INLINE uint32_t bswap_32(uint32_t val)
{
   return (val >> 24) | (val << 24) |
      ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000);
}

static ssize_t xenon360_audio_write(void *data, const void *buf, size_t size)
{
   size_t written = 0, i;
   const uint32_t *in_buf = buf;
   xenon_audio_t *xa = data;

   for (i = 0; i < (size >> 2); i++)
      xa->buffer[i] = bswap_32(in_buf[i]);

   if (xa->nonblock)
   {
      if (xenon_sound_get_unplayed() < MAX_BUFFER)
      {
         xenon_sound_submit(xa->buffer, size);
         written = size;
      }
   }
   else
   {
      while (xenon_sound_get_unplayed() >= MAX_BUFFER)
      {
         /* libxenon doesn't have proper
          * synchronization primitives for this... */
         udelay(50);
      }

      xenon_sound_submit(xa->buffer, size);
      written = size;
   }

   return written;
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

static bool xenon360_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t xenon360_write_avail(void *data)
{
   (void)data;
   return 0;
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
   xenon360_write_avail,
   NULL
};
