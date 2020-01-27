/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>

#include <boolean.h>
#include <rthreads/rthreads.h>

#include "../../retroarch.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define FRAMES(x) (x / (sizeof(float) * 2))

typedef struct jack
{
   jack_client_t *client;
   jack_port_t *ports[2];
   jack_ringbuffer_t *buffer[2];
   volatile bool shutdown;
   bool nonblock;
   bool is_paused;

#ifdef HAVE_THREADS
   scond_t *cond;
   slock_t *cond_lock;
#endif
   size_t buffer_size;
} jack_t;

static int process_cb(jack_nframes_t nframes, void *data)
{
   int i;
   jack_nframes_t avail[2], min_avail;
   jack_t *jd = (jack_t*)data;

   if (nframes <= 0)
   {
#ifdef HAVE_THREADS
      scond_signal(jd->cond);
#endif
      return 0;
   }

   avail[0]  = jack_ringbuffer_read_space(jd->buffer[0]);
   avail[1]  = jack_ringbuffer_read_space(jd->buffer[1]);
   min_avail = ((avail[0] < avail[1]) ? avail[0] : avail[1]) / sizeof(jack_default_audio_sample_t);

   if (min_avail > nframes)
      min_avail = nframes;

   for (i = 0; i < 2; i++)
   {
      jack_nframes_t f;
      jack_default_audio_sample_t *out = (jack_default_audio_sample_t*)jack_port_get_buffer(jd->ports[i], nframes);

      jack_ringbuffer_read(jd->buffer[i], (char*)out, min_avail * sizeof(jack_default_audio_sample_t));

      for (f = min_avail; f < nframes; f++)
         out[f] = 0.0f;
   }
#ifdef HAVE_THREADS
   scond_signal(jd->cond);
#endif
   return 0;
}

static void shutdown_cb(void *data)
{
   jack_t *jd = (jack_t*)data;

   if (!jd)
      return;

   jd->shutdown = true;
#ifdef HAVE_THREADS
   scond_signal(jd->cond);
#endif
}

static int parse_ports(char **dest_ports, const char **jports)
{
   int i;
   char           *save   = NULL;
   int           parsed   = 0;
   settings_t *settings   = config_get_ptr();
   char *audio_device_cpy = strdup(settings->arrays.audio_device);
   const char      *con   = strtok_r(audio_device_cpy, ",", &save);

   if (con)
      dest_ports[parsed++] = strdup(con);
   con = strtok_r(NULL, ",", &save);
   if (con)
      dest_ports[parsed++] = strdup(con);

   for (i = parsed; i < 2; i++)
      dest_ports[i] = strdup(jports[i]);

   free(audio_device_cpy);
   return 2;
}

static size_t find_buffersize(jack_t *jd, int latency, unsigned out_rate)
{
   jack_latency_range_t range;
   int i, buffer_frames, min_buffer_frames;
   int jack_latency     = 0;
   int           frames = latency * out_rate / 1000;

   for (i = 0; i < 2; i++)
   {
      jack_port_get_latency_range(jd->ports[i], JackPlaybackLatency, &range);
      if ((int)range.max > jack_latency)
         jack_latency = range.max;
   }

   RARCH_LOG("[JACK]: Jack latency is %d frames.\n", jack_latency);

   buffer_frames     = frames - jack_latency;
   min_buffer_frames = jack_get_buffer_size(jd->client) * 2;

   RARCH_LOG("[JACK]: Minimum buffer size is %d frames.\n", min_buffer_frames);

   if (buffer_frames < min_buffer_frames)
      buffer_frames = min_buffer_frames;

   return buffer_frames * sizeof(jack_default_audio_sample_t);
}

static void *ja_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   int i;
   char *dest_ports[2];
   const char **jports = NULL;
   size_t       bufsize = 0;
   int           parsed = 0;
   jack_t           *jd = (jack_t*)calloc(1, sizeof(jack_t));

   if (!jd)
      return NULL;

#ifdef HAVE_THREADS
   jd->cond      = scond_new();
   jd->cond_lock = slock_new();
#endif

   jd->client = jack_client_open("RetroArch", JackNullOption, NULL);
   if (!jd->client)
      goto error;

   *new_rate = jack_get_sample_rate(jd->client);

   jack_set_process_callback(jd->client, process_cb, jd);
   jack_on_shutdown(jd->client, shutdown_cb, jd);

   jd->ports[0] = jack_port_register(jd->client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   jd->ports[1] = jack_port_register(jd->client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   if (!jd->ports[0] || !jd->ports[1])
   {
      RARCH_ERR("[JACK]: Failed to register ports.\n");
      goto error;
   }

   jports = jack_get_ports(jd->client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
   if (!jports)
   {
      RARCH_ERR("[JACK]: Failed to get ports.\n");
      goto error;
   }

   bufsize         = find_buffersize(jd, latency, *new_rate);
   jd->buffer_size = bufsize;

   RARCH_LOG("[JACK]: Internal buffer size: %d frames.\n", (int)(bufsize / sizeof(jack_default_audio_sample_t)));
   for (i = 0; i < 2; i++)
   {
      jd->buffer[i] = jack_ringbuffer_create(bufsize);
      if (!jd->buffer[i])
      {
         RARCH_ERR("[JACK]: Failed to create buffers.\n");
         goto error;
      }
   }

   parsed = parse_ports(dest_ports, jports);

   if (jack_activate(jd->client) < 0)
   {
      RARCH_ERR("[JACK]: Failed to activate Jack...\n");
      goto error;
   }

   for (i = 0; i < 2; i++)
   {
      if (jack_connect(jd->client, jack_port_name(jd->ports[i]), dest_ports[i]))
      {
         RARCH_ERR("[JACK]: Failed to connect to Jack port.\n");
         goto error;
      }
   }

   for (i = 0; i < parsed; i++)
      free(dest_ports[i]);

   jack_free(jports);
   return jd;

error:
   for (i = 0; i < parsed; i++)
      free(dest_ports[i]);
   if (jports)
      jack_free(jports);
   free(jd);
   return NULL;
}

static size_t write_buffer(jack_t *jd, const float *buf, size_t size)
{
   int i;
   size_t j, written = 0;
   jack_default_audio_sample_t out_deinterleaved_buffer[2][AUDIO_CHUNK_SIZE_NONBLOCKING * AUDIO_MAX_RATIO];
   size_t frames = FRAMES(size);

   /* Avoid buffer overflow if a DSP plugin generated a huge number of frames. */
   if (frames > AUDIO_CHUNK_SIZE_NONBLOCKING * AUDIO_MAX_RATIO)
      frames = AUDIO_CHUNK_SIZE_NONBLOCKING * AUDIO_MAX_RATIO;

   for (i = 0; i < 2; i++)
      for (j = 0; j < frames; j++)
         out_deinterleaved_buffer[i][j] = buf[j * 2 + i];

   while (written < frames)
   {
      size_t avail[2], min_avail, write_frames;
      if (jd->shutdown)
         return 0;

      avail[0] = jack_ringbuffer_write_space(jd->buffer[0]);
      avail[1] = jack_ringbuffer_write_space(jd->buffer[1]);

      min_avail = avail[0] < avail[1] ? avail[0] : avail[1];
      min_avail /= sizeof(float);

      write_frames = frames - written > min_avail ? min_avail : frames - written;

      if (write_frames > 0)
      {
         for (i = 0; i < 2; i++)
         {
            jack_ringbuffer_write(jd->buffer[i], (const char*)&out_deinterleaved_buffer[i][written],
                  write_frames * sizeof(jack_default_audio_sample_t));
         }
         written += write_frames;
      }
#ifdef HAVE_THREADS
      else
      {
         slock_lock(jd->cond_lock);
         scond_wait(jd->cond, jd->cond_lock);
         slock_unlock(jd->cond_lock);
      }
#endif

      if (jd->nonblock)
         break;
   }

   return written * sizeof(float) * 2;
}

static ssize_t ja_write(void *data, const void *buf, size_t size)
{
   jack_t *jd = (jack_t*)data;

   return write_buffer(jd, (const float*)buf, size);
}

static bool ja_stop(void *data)
{
   jack_t *jd = (jack_t*)data;
   if (jd)
      jd->is_paused = true;
   return true;
}

static bool ja_alive(void *data)
{
   jack_t *jd = (jack_t*)data;
   if (!jd)
      return false;
   return !jd->is_paused;
}

static void ja_set_nonblock_state(void *data, bool state)
{
   jack_t *jd = (jack_t*)data;
   if (jd)
      jd->nonblock = state;
}

static bool ja_start(void *data, bool is_shutdown)
{
   jack_t *jd = (jack_t*)data;
   if (jd)
      jd->is_paused = false;
   return true;
}

static void ja_free(void *data)
{
   int i;
   jack_t *jd = (jack_t*)data;

   jd->shutdown = true;

   if (jd->client)
   {
      jack_deactivate(jd->client);
      jack_client_close(jd->client);
   }

   for (i = 0; i < 2; i++)
      if (jd->buffer[i])
         jack_ringbuffer_free(jd->buffer[i]);

#ifdef HAVE_THREADS
   if (jd->cond_lock)
      slock_free(jd->cond_lock);
   if (jd->cond)
      scond_free(jd->cond);
#endif
   free(jd);
}

static bool ja_use_float(void *data)
{
   (void)data;
   return true;
}

static size_t ja_write_avail(void *data)
{
   jack_t *jd = (jack_t*)data;
   return jack_ringbuffer_write_space(jd->buffer[0]);
}

static size_t ja_buffer_size(void *data)
{
   jack_t *jd = (jack_t*)data;
   return jd->buffer_size;
}

audio_driver_t audio_jack = {
   ja_init,
   ja_write,
   ja_stop,
   ja_start,
   ja_alive,
   ja_set_nonblock_state,
   ja_free,
   ja_use_float,
   "jack",
   NULL,
   NULL,
   ja_write_avail,
   ja_buffer_size,
};
