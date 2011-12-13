/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../driver.h"
#include <stdlib.h>
#include "../general.h"

#include <xenon_sound/sound.h>

#define SOUND_FREQUENCY 48000
#define SOUND_SAMPLES_SIZE 2048

typedef struct
{
   bool nonblock;
} xenon360_audio_t;

static void *xenon360_init(const char *device, unsigned rate, unsigned latency)
{
   xenon_sound_init();
}

static ssize_t xenon360_write(void *data, const void *buf, size_t size)
{
   xenon360_audio_t *xa = data;
   #if 0
   if (xa->nonblock)
   {
      size_t avail = xaudio2_write_avail(xa->xa);
      if (avail == 0)
         return 0;
      if (avail < size)
         size = avail;
   }
   #endif

   xenon_sound_submit(buf, size);
   return 0;
}

static bool xenon360_stop(void *data)
{
   (void)data;
   return true;
}

static void xenon360_set_nonblock_state(void *data, bool state)
{
   xenon360_audio_t *xa = data;
   xa->nonblock = state;
}

static bool xenon360_start(void *data)
{
   (void)data;
   return true;
}

static bool xenon360_use_float(void *data)
{
   (void)data;
   return true;
}

static void xenon360_free(void *data)
{
   xenon360_audio_t *xa = data;
   if (xa)
      free(xa);
}

const audio_driver_t audio_xa = {
   .init = xenon360_init,
   .write = xenon360_write,
   .stop = xenon360_stop,
   .start = xenon360_start,
   .set_nonblock_state = xenon360_set_nonblock_state,
   .use_float = xenon360_use_float,
   .free = xenon360_free,
   .ident = "xenon360"
};
   
