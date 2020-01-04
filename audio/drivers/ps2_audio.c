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

#include "../../retroarch.h"

#define AUDIO_BUFFER 128 * 1024
#define AUDIO_CHANNELS 2
#define AUDIO_BITS 16

typedef struct ps2_audio
{
   /* TODO/FIXME - nonblock is not implemented */
   bool nonblock;
   bool running;

} ps2_audio_t;

static void audioConfigure(ps2_audio_t *ps2, unsigned rate)
{
   int err;
   struct audsrv_fmt_t format;

   format.bits     = AUDIO_BITS;
   format.freq     = rate;
   format.channels = AUDIO_CHANNELS;

   err             = audsrv_set_format(&format);

   if (err)
   {
      printf("set format returned %d\n", err);
      printf("audsrv returned error string: %s\n", audsrv_get_error_string());
   }

   audsrv_set_volume(MAX_VOLUME);
}

static void *ps2_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   ps2_audio_t *ps2 = (ps2_audio_t*)calloc(1, sizeof(ps2_audio_t));

   if (!ps2)
      return NULL;

   audioConfigure(ps2, rate);

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

static ssize_t ps2_audio_write(void *data, const void *buf, size_t size)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (!ps2->running)
      return -1;

   return audsrv_play_audio(buf, size);
}

static bool ps2_audio_alive(void *data)
{
   bool       alive = false;
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2)
      alive = ps2->running;

   return alive;
}

static bool ps2_audio_stop(void *data)
{
   bool        stop = true;
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2)
   {
      audsrv_stop_audio();
      ps2->running = false;
   }

   return stop;
}

static bool ps2_audio_start(void *data, bool is_shutdown)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   bool       start = true;

   if (ps2)
      ps2->running = true;

   return start;
}

static void ps2_audio_set_nonblock_state(void *data, bool toggle)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2)
      ps2->nonblock = toggle;
}

static bool ps2_audio_use_float(void *data)
{
   return false;
}

static size_t ps2_audio_write_avail(void *data)
{
   ps2_audio_t* ps2 = (ps2_audio_t*)data;

   if (ps2 && ps2->running)
      return AUDIO_BUFFER;

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
