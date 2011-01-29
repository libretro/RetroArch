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


#include "driver.h"
#include "general.h"
#include <pulse/pulseaudio.h>
#include <stdbool.h>
#include <string.h>

#include <stdio.h>

typedef struct
{
   pa_mainloop *mainloop;
   pa_context *context;
   pa_stream *stream;
   bool nonblock;
} pa_t;

static void __pulse_free(void *data)
{
   pa_t *pa = data;
   if (pa)
   {
      if (pa->stream)
      {
         pa_stream_disconnect(pa->stream);
         pa_stream_unref(pa->stream);
      }

      if (pa->context)
      {
         pa_context_disconnect(pa->context);
         pa_context_unref(pa->context);
      }

      if (pa->mainloop)
         pa_mainloop_free(pa->mainloop);

      free(pa);
   }
}

static inline uint8_t is_little_endian(void)
{
   union
   {
      uint16_t x;
      uint8_t y[2];
   } u;

   u.x = 1;
   return u.y[0];
}

static void* __pulse_init(const char* device, int rate, int latency)
{
   pa_t *pa = calloc(1, sizeof(*pa));
   if (!pa)
      goto error;

   pa->mainloop = pa_mainloop_new();
   if (!pa->mainloop)
      goto error;

   pa->context = pa_context_new(pa_mainloop_get_api(pa->mainloop), "SSNES");
   if (!pa->context)
      goto error;
   if (pa_context_connect(pa->context, device, PA_CONTEXT_NOFLAGS, NULL) < 0)
      goto error;

   pa_context_state_t cstate;
   do
   {
      pa_mainloop_iterate(pa->mainloop, 1, NULL);
      cstate = pa_context_get_state(pa->context);
      if (!PA_CONTEXT_IS_GOOD(cstate)) goto error;
   } while (cstate != PA_CONTEXT_READY);

   pa_sample_spec spec = {
      .format = is_little_endian() ? PA_SAMPLE_FLOAT32LE : PA_SAMPLE_FLOAT32BE,
      .channels = 2,
      .rate = rate
   };

   pa->stream = pa_stream_new(pa->context, "audio", &spec, NULL);
   if (!pa->stream)
      goto error;

   pa_buffer_attr buffer_attr = {
      .maxlength = -1,
      .tlength = pa_usec_to_bytes(latency * PA_USEC_PER_MSEC, &spec),
      .prebuf = -1,
      .minreq = -1,
      .fragsize = -1
   };

   if (pa_stream_connect_playback(pa->stream, NULL, &buffer_attr, PA_STREAM_ADJUST_LATENCY, NULL, NULL) < 0)
      goto error;

   pa_stream_state_t sstate;
   do 
   {
      pa_mainloop_iterate(pa->mainloop, 1, NULL);
      sstate = pa_stream_get_state(pa->stream);
      if(!PA_STREAM_IS_GOOD(sstate)) goto error;
   } while(sstate != PA_STREAM_READY);

   return pa;

error:
   __pulse_free(pa); 
   return NULL;
}

static ssize_t __pulse_write(void* data, const void* buf, size_t size)
{
   pa_t *pa = data;

   unsigned length = pa_stream_writable_size(pa->stream);
   while (length < size)
   {
      pa_mainloop_iterate(pa->mainloop, 1, NULL);

      length = pa_stream_writable_size(pa->stream);

      if (pa->nonblock)
         break;
   }

   size_t write_size = length < size ? length : size;

   pa_stream_write(pa->stream, buf, write_size, NULL, 0LL, PA_SEEK_RELATIVE);
   return write_size;
}

static bool __pulse_stop(void *data)
{
   (void)data;
   return true;
}

static bool __pulse_start(void *data)
{
   (void)data;
   return true;
}

static void __pulse_set_nonblock_state(void *data, bool state)
{
   pa_t *pa = data;
   pa->nonblock = state;
}

static bool __pulse_use_float(void *data)
{
   (void)data;
   return true;
}

const audio_driver_t audio_pulse = {
   .init = __pulse_init,
   .write = __pulse_write,
   .stop = __pulse_stop,
   .start = __pulse_start,
   .set_nonblock_state = __pulse_set_nonblock_state,
   .use_float = __pulse_use_float,
   .free = __pulse_free,
   .ident = "pulse"
};

   


   
   
