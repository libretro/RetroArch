/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
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
#include <stdlib.h>
#include <stdbool.h>
#include "../general.h"

#include <xenon_sound/sound.h>

#define SOUND_FREQUENCY 48000
#define MAX_BUFFER 2048

typedef struct
{
   uint32_t buffer[2048];
   bool nonblock;
} xenon_audio_t;

static void *xenon360_audio_init(const char *device, unsigned rate, unsigned latency)
{
   static bool inited = false;
   if (!inited)
   {
      xenon_sound_init();
      inited = true;
   }

   g_settings.audio.out_rate = SOUND_FREQUENCY;
   return calloc(1, sizeof(xenon_audio_t));
}

static inline uint32_t bswap_32(uint32_t val)
{
   return (val >> 24) | (val << 24) | ((val >> 8) & 0xff00) | ((val << 8) & 0xff0000);
}

static ssize_t xenon360_audio_write(void *data, const void *buf, size_t size)
{
   xenon_audio_t *xa = data;

   size_t written = 0;

   const uint32_t *in_buf = buf;
   for (size_t i = 0; i < (size >> 2); i++)
      xa->buffer[i] = bswap_32(in_buf[i]);

   if (!xa->nonblock)
   {
      while (xenon_sound_get_unplayed() >= MAX_BUFFER)
      {
         // libxenon doesn't have proper synchronization primitives for this :(
         udelay(50);
      }

      xenon_sound_submit(xa->buffer, size);
      written = size;
   }
   else
   {
      if (xenon_sound_get_unplayed() < MAX_BUFFER)
      {
         xenon_sound_submit(xa->buffer, size);
         written = size;
      }
   }

   return written;
}

static bool xenon360_audio_stop(void *data)
{
   (void)data;
   return true;
}

static void xenon360_audio_set_nonblock_state(void *data, bool state)
{
   xenon_audio_t *xa = data;
   xa->nonblock = state;
}

static bool xenon360_audio_start(void *data)
{
   (void)data;
   return true;
}

static void xenon360_audio_free(void *data)
{
   if (data)
      free(data);
}

const audio_driver_t audio_xenon360 = {
   .init = xenon360_audio_init,
   .write = xenon360_audio_write,
   .stop = xenon360_audio_stop,
   .start = xenon360_audio_start,
   .set_nonblock_state = xenon360_audio_set_nonblock_state,
   .free = xenon360_audio_free,
   .ident = "xenon360"
};

