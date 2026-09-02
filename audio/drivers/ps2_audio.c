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

#include "../audio_driver.h"

#define AUDIO_BUFFER 128 * 1024
#define AUDIO_CHANNELS 2
#define AUDIO_BITS 16

typedef struct ps2_audio
{
   /* TODO/FIXME - nonblock is not implemented */
   bool nonblock;
   bool running;
} ps2_audio_t;

static void *ps2_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   struct audsrv_fmt_t format;
   ps2_audio_t *ps2 = (ps2_audio_t*)calloc(1, sizeof(ps2_audio_t));

   if (!ps2)
      return NULL;

   format.bits     = AUDIO_BITS;
   format.freq     = rate;
   format.channels = AUDIO_CHANNELS;

   audsrv_set_format(&format);
   audsrv_set_volume(MAX_VOLUME);

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
   ps2_audio_t* ps2 = (ps2_audio_t*)data;
   if (!ps2->running)
      return -1;
   return audsrv_play_audio(s, len);
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

/* No write_avail(): the driver hands audio to audsrv and has nothing
 * to measure its fill with. It used to report the whole buffer as free
 * whenever it was running, which the rate control read as an empty
 * device every frame and answered with its full upward correction,
 * permanently - audio a half percent fast, and sharp, for as long as
 * rate control was on. With no write_avail() the frontend disables
 * rate control for this driver and says so, which is the truth. */

static bool ps2_audio_use_float(void *data) { return false; }
static size_t ps2_audio_buffer_size(void *data) { return AUDIO_BUFFER; }

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
   NULL, /* write_avail */
   ps2_audio_buffer_size,
   NULL /* write_raw */
};
