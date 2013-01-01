/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "../general.h"

#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>
#include <pthread.h>
#include <stdint.h>
#include "../boolean.h"
#include <string.h>
#include <assert.h>

#define FRAMES(x) (x / (sizeof(float) * 2))

typedef struct jack
{
   jack_client_t *client;
   jack_port_t *ports[2];
   jack_ringbuffer_t *buffer[2];
   volatile bool shutdown;
   bool nonblock;

   pthread_cond_t cond;
   pthread_mutex_t cond_lock;
   size_t buffer_size;
} jack_t;

static int process_cb(jack_nframes_t nframes, void *data)
{
   jack_t *jd = (jack_t*)data;
   if (nframes <= 0)
   {
      pthread_cond_signal(&jd->cond);
      return 0;
   }

   jack_nframes_t avail[2];
   avail[0] = jack_ringbuffer_read_space(jd->buffer[0]);
   avail[1] = jack_ringbuffer_read_space(jd->buffer[1]);
   jack_nframes_t min_avail = ((avail[0] < avail[1]) ? avail[0] : avail[1]) / sizeof(jack_default_audio_sample_t);

   if (min_avail > nframes)
      min_avail = nframes;

   for (int i = 0; i < 2; i++)
   {
      jack_default_audio_sample_t *out = (jack_default_audio_sample_t*)jack_port_get_buffer(jd->ports[i], nframes);
      assert(out);
      jack_ringbuffer_read(jd->buffer[i], (char*)out, min_avail * sizeof(jack_default_audio_sample_t));

      for (jack_nframes_t f = min_avail; f < nframes; f++)
      {
         out[f] = 0.0f;
      }
   }
   pthread_cond_signal(&jd->cond);
   return 0;
}

static void shutdown_cb(void *data)
{
   jack_t *jd = (jack_t*)data;
   jd->shutdown = true;
   pthread_cond_signal(&jd->cond);
}

static int parse_ports(char **dest_ports, const char **jports)
{
   int parsed = 0;

   char *save;
   const char *con = strtok_r(g_settings.audio.device, ",", &save);
   if (con)
      dest_ports[parsed++] = strdup(con);
   con = strtok_r(NULL, ",", &save);
   if (con)
      dest_ports[parsed++] = strdup(con);

   for (int i = parsed; i < 2; i++)
      dest_ports[i] = strdup(jports[i]);

   return 2;
}

static size_t find_buffersize(jack_t *jd, int latency)
{
   int frames = latency * g_settings.audio.out_rate / 1000;

   jack_latency_range_t range;
   int jack_latency = 0;
   for (int i = 0; i < 2; i++)
   {
      jack_port_get_latency_range(jd->ports[i], JackPlaybackLatency, &range);
      if ((int)range.max > jack_latency)
         jack_latency = range.max;
   }

   RARCH_LOG("JACK: Jack latency is %d frames.\n", jack_latency);

   int buffer_frames = frames - jack_latency;
   int min_buffer_frames = jack_get_buffer_size(jd->client) * 2;
   RARCH_LOG("JACK: Minimum buffer size is %d frames.\n", min_buffer_frames);

   if (buffer_frames < min_buffer_frames)
      buffer_frames = min_buffer_frames;

   return buffer_frames * sizeof(jack_default_audio_sample_t);
}

static void *ja_init(const char *device, unsigned rate, unsigned latency)
{
   jack_t *jd = (jack_t*)calloc(1, sizeof(jack_t));
   if (!jd)
      return NULL;

   pthread_cond_init(&jd->cond, NULL);
   pthread_mutex_init(&jd->cond_lock, NULL);
   
   const char **jports = NULL;
   char *dest_ports[2];
   size_t bufsize = 0;
   int parsed = 0;

   jd->client = jack_client_open("RetroArch", JackNullOption, NULL);
   if (jd->client == NULL)
      goto error;

   g_settings.audio.out_rate = jack_get_sample_rate(jd->client);

   jack_set_process_callback(jd->client, process_cb, jd);
   jack_on_shutdown(jd->client, shutdown_cb, jd);

   jd->ports[0] = jack_port_register(jd->client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   jd->ports[1] = jack_port_register(jd->client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   if (jd->ports[0] == NULL || jd->ports[1] == NULL)
   {
      RARCH_ERR("Failed to register ports.\n");
      goto error;
   }
   
   jports = jack_get_ports(jd->client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
   if (jports == NULL)
   {
      RARCH_ERR("Failed to get ports.\n");
      goto error;
   }

   bufsize = find_buffersize(jd, latency);
   jd->buffer_size = bufsize;

   RARCH_LOG("JACK: Internal buffer size: %d frames.\n", (int)(bufsize / sizeof(jack_default_audio_sample_t)));
   for (int i = 0; i < 2; i++)
   {
      jd->buffer[i] = jack_ringbuffer_create(bufsize);
      if (jd->buffer[i] == NULL)
      {
         RARCH_ERR("Failed to create buffers.\n");
         goto error;
      }
   }

   parsed = parse_ports(dest_ports, jports);

   if (jack_activate(jd->client) < 0)
   {
      RARCH_ERR("Failed to activate Jack...\n");
      goto error;
   }

   for (int i = 0; i < 2; i++)
   {
      if (jack_connect(jd->client, jack_port_name(jd->ports[i]), dest_ports[i]))
      {
         RARCH_ERR("Failed to connect to Jack port.\n");
         goto error;
      }
   }

   for (int i = 0; i < parsed; i++)
      free(dest_ports[i]);
  
   jack_free(jports);
   return jd;

error:
   if (jports != NULL)
      jack_free(jports);
   return NULL;
}

static size_t write_buffer(jack_t *jd, const float *buf, size_t size)
{
   jack_default_audio_sample_t out_deinterleaved_buffer[2][AUDIO_CHUNK_SIZE_NONBLOCKING];

   for (int i = 0; i < 2; i++)
      for (size_t j = 0; j < FRAMES(size); j++)
         out_deinterleaved_buffer[i][j] = buf[j * 2 + i];

   size_t frames = FRAMES(size);

   size_t written = 0;
   while (written < frames)
   {
      if (jd->shutdown)
         return 0;

      size_t avail[2] = {
         jack_ringbuffer_write_space(jd->buffer[0]),
         jack_ringbuffer_write_space(jd->buffer[1]),
      };

      size_t min_avail = avail[0] < avail[1] ? avail[0] : avail[1];
      min_avail /= sizeof(float);

      size_t write_frames = frames - written > min_avail ? min_avail : frames - written;

      if (write_frames > 0)
      {
         for (int i = 0; i < 2; i++)
         {
            jack_ringbuffer_write(jd->buffer[i], (const char*)&out_deinterleaved_buffer[i][written],
                  write_frames * sizeof(jack_default_audio_sample_t));
         }
         written += write_frames;
      }
      else
      {
         pthread_mutex_lock(&jd->cond_lock);
         pthread_cond_wait(&jd->cond, &jd->cond_lock);
         pthread_mutex_unlock(&jd->cond_lock);
      }

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
   (void)data;
   return true;
}

static void ja_set_nonblock_state(void *data, bool state)
{
   jack_t *jd = (jack_t*)data;
   jd->nonblock = state;
}

static bool ja_start(void *data)
{
   (void)data;
   return true;
}

static void ja_free(void *data)
{
   jack_t *jd = (jack_t*)data;

   jd->shutdown = true;

   if (jd->client != NULL)
   {
      jack_deactivate(jd->client);
      jack_client_close(jd->client);
   }

   for (int i = 0; i < 2; i++)
      if (jd->buffer[i] != NULL)
         jack_ringbuffer_free(jd->buffer[i]);

   pthread_mutex_destroy(&jd->cond_lock);
   pthread_cond_destroy(&jd->cond);
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

const audio_driver_t audio_jack = {
   ja_init,
   ja_write,
   ja_stop,
   ja_start,
   ja_set_nonblock_state,
   ja_free,
   ja_use_float,
   "jack",
   ja_write_avail,
   ja_buffer_size,
};

