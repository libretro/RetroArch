/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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
#include <stdlib.h>
#include "general.h"

#include <jack/jack.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define FRAMES(x) (x / (sizeof(int16_t) * 2))
#define SAMPLES(x) (x / sizeof(int16_t))

typedef struct jack
{
   jack_client_t *client;
   jack_port_t *ports[2];
   jack_ringbuffer_t *buffer[2];
   volatile bool shutdown;
   bool nonblock;

   pthread_cond_t cond;
   pthread_mutex_t cond_lock;
} jack_t;

static int process_cb(jack_nframes_t nframes, void *data)
{
   jack_t *jd = data;
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

   //static int underrun = 0;
   //if (min_avail < nframes)
   //{
   //   SSNES_LOG("JACK: Underrun count: %d\n", underrun++);
   //   fprintf(stderr, "required %d frames, got %d.\n", (int)nframes, (int)min_avail);
   //}

   for (int i = 0; i < 2; i++)
   {
      jack_default_audio_sample_t *out = jack_port_get_buffer(jd->ports[i], nframes);
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
   jack_t *jd = data;
   jd->shutdown = true;
   pthread_cond_signal(&jd->cond);
}

static inline void s16_to_float(jack_default_audio_sample_t * restrict out, const int16_t * restrict in, size_t samples)
{
   for (int i = 0; i < samples; i++)
      out[i] = (float)in[i] / 0x8000;
}

static void parse_ports(const char **dest_ports, const char **jports)
{
   int parsed = 0;

   const char *con = strtok(g_settings.audio.device, ",");
   if (con)
      dest_ports[parsed++] = con;
   con = strtok(NULL, ",");
   if (con)
      dest_ports[parsed++] = con;

   for (int i = parsed; i < 2; i++)
      dest_ports[i] = jports[i];
}

static void* __jack_init(const char* device, int rate, int latency)
{
   jack_t *jd = calloc(1, sizeof(jack_t));
   if ( jd == NULL )
      return NULL;

   const char **jports = NULL;

   jd->client = jack_client_open("SSNES", JackNullOption, NULL);
   if (jd->client == NULL)
      goto error;

   g_settings.audio.out_rate = jack_get_sample_rate(jd->client);

   jack_set_process_callback(jd->client, process_cb, jd);
   jack_on_shutdown(jd->client, shutdown_cb, jd);

   jd->ports[0] = jack_port_register(jd->client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   jd->ports[1] = jack_port_register(jd->client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   if (jd->ports[0] == NULL || jd->ports[1] == NULL)
   {
      SSNES_ERR("Failed to register ports.\n");
      goto error;
   }

   jack_nframes_t bufsize;
   jack_nframes_t jack_bufsize = jack_get_buffer_size(jd->client);
   
   bufsize = (latency * g_settings.audio.out_rate / 1000) > jack_bufsize * 2 ? (latency * g_settings.audio.out_rate / 1000) : jack_bufsize * 2;
   bufsize *= sizeof(jack_default_audio_sample_t);

   //fprintf(stderr, "jack buffer size: %d\n", (int)bufsize);
   for (int i = 0; i < 2; i++)
   {
      jd->buffer[i] = jack_ringbuffer_create(bufsize);
      if (jd->buffer[i] == NULL)
      {
         SSNES_ERR("Failed to create buffers.\n");
         goto error;
      }
   }

   const char *dest_ports[2];
   jports = jack_get_ports(jd->client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
   if (jports == NULL)
   {
      SSNES_ERR("Failed to get ports.\n");
      goto error;
   }

   parse_ports(dest_ports, jports);

   if (jack_activate(jd->client) < 0)
   {
      SSNES_ERR("Failed to activate Jack...\n");
      goto error;
   }

   for (int i = 0; i < 2; i++)
   {
      if (jack_connect(jd->client, jack_port_name(jd->ports[i]), dest_ports[i]))
      {
         SSNES_ERR("Failed to connect to Jack port.\n");
         goto error;
      }
   }

   pthread_cond_init(&jd->cond, NULL);
   pthread_mutex_init(&jd->cond_lock, NULL);


   jack_free(jports);
   return jd;

error:
   if (jports != NULL)
      jack_free(jports);
   return NULL;
}

static size_t write_buffer(jack_t *jd, const void *buf, size_t size)
{
   //fprintf(stderr, "write_buffer: size: %zu\n", size);
   // Convert our data to float, deinterleave and write.
   jack_default_audio_sample_t out_buffer[size / sizeof(int16_t)];
   jack_default_audio_sample_t out_deinterleaved_buffer[2][FRAMES(size)];
   s16_to_float(out_buffer, buf, SAMPLES(size));

   for (int i = 0; i < 2; i++)
      for (size_t j = 0; j < FRAMES(size); j++)
         out_deinterleaved_buffer[i][j] = out_buffer[j * 2 + i];

   for(;;)
   {
      if (jd->shutdown)
         return 0;

      size_t avail[2];
      avail[0] = jack_ringbuffer_write_space(jd->buffer[0]);
      avail[1] = jack_ringbuffer_write_space(jd->buffer[1]);
      size_t min_avail = avail[0] < avail[1] ? avail[0] : avail[1];

      if (jd->nonblock)
      {
         if (min_avail < FRAMES(size) * sizeof(jack_default_audio_sample_t))
            size = min_avail * 2 * sizeof(int16_t) / sizeof(jack_default_audio_sample_t);
         break;
      }

      else
      {
         //fprintf(stderr, "Write avail is: %d\n", (int)min_avail);
         if (min_avail >= FRAMES(size) * sizeof(jack_default_audio_sample_t))
            break;
      }

      pthread_mutex_lock(&jd->cond_lock);
      pthread_cond_wait(&jd->cond, &jd->cond_lock);
      pthread_mutex_unlock(&jd->cond_lock);
   }

   for (int i = 0; i < 2; i++)
      jack_ringbuffer_write(jd->buffer[i], (const char*)out_deinterleaved_buffer[i], FRAMES(size) * sizeof(jack_default_audio_sample_t));
   return size;
}

static ssize_t __jack_write(void* data, const void* buf, size_t size)
{
   jack_t *jd = data;

   return write_buffer(jd, buf, size);
}

static bool __jack_stop(void *data)
{
   (void)data;
   return true;
}

static void __jack_set_nonblock_state(void *data, bool state)
{
   jack_t *jd = data;
   jd->nonblock = state;
}

static bool __jack_start(void *data)
{
   (void)data;
   return true;
}

static void __jack_free(void *data)
{
   jack_t *jd = data;

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

const audio_driver_t audio_jack = {
   .init = __jack_init,
   .write = __jack_write,
   .stop = __jack_stop,
   .start = __jack_start,
   .set_nonblock_state = __jack_set_nonblock_state,
   .free = __jack_free,
   .ident = "jack"
};

   


   
   
