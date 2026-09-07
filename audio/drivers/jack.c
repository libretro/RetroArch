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

#include <retro_atomic.h>

#include <jack/jack.h>
#include <lists/string_list.h>
#include <jack/types.h>
#include <jack/ringbuffer.h>

#include <boolean.h>
#include <rthreads/rthreads.h>

#include "../../configuration.h"
#include "../audio_driver.h"
#include "../../verbosity.h"

#define FRAMES(x) (x / (sizeof(float) * 2))

typedef struct jack
{
   jack_client_t *client;
   jack_port_t *ports[2];
   jack_ringbuffer_t *buffer;
#ifdef HAVE_THREADS
   scond_t *cond;
   slock_t *cond_lock;
#endif
   size_t buffer_size;
#ifdef HAVE_THREADS
   /* Bound for the blocking wait in ja_write, one process period.
    * See the note there. */
   int64_t wait_us;
#endif
   /* Frames the server has asked for since the client started, for the
    * sink rate estimate. ja_process_cb() is called once per period with
    * the period's length, so this counts the device's own clock -
    * silence during an underrun included, because the period elapsed
    * either way. Written only by the process callback, read by the
    * frontend through ja_frames_consumed(). */
   retro_atomic_size_t consumed;
   volatile bool shutdown;
   bool nonblock;
   bool is_paused;
} jack_t;

static size_t ja_read_deinterleaved(float *dst[2], jack_nframes_t dst_offset,
      jack_ringbuffer_data_t buf, jack_nframes_t nframes)
{
   int i;
   jack_nframes_t j, frames_avail;
   const float *src = (const float *)buf.buf;

   if (nframes <= 0)
      return 0;

   frames_avail = FRAMES(buf.len);
   nframes = nframes < frames_avail ? nframes : frames_avail;

   for (j = 0; j < nframes; j++)
      for (i = 0; i < 2; i++)
         dst[i][dst_offset + j] = *src++;

   return nframes;
}

/* Frames the server has taken since the client started. JACK calls the
 * process callback once per period and says how long the period is, so
 * counting what it asks for counts device time; there is no queue to
 * subtract, unlike ALSA, because the callback is the device consuming
 * the audio rather than a queue being filled. */
static size_t ja_frames_consumed(void *data)
{
   jack_t *jd = (jack_t*)data;
   if (!jd)
      return 0;
   return retro_atomic_load_acquire_size(&jd->consumed);
}

static int ja_process_cb(jack_nframes_t nframes, void *data)
{
   jack_t *jd = (jack_t*)data;

   if (nframes > 0)
   {
      retro_atomic_fetch_add_size(&jd->consumed, (size_t)nframes);
      int i;
      float *dst[2];
      jack_ringbuffer_data_t buf[2];
      jack_nframes_t read = 0;
      for (i = 0; i < 2; i++)
         dst[i] = (float *)jack_port_get_buffer(jd->ports[i], nframes);

      jack_ringbuffer_get_read_vector(jd->buffer, buf);

      for (i = 0; i < 2; i++)
         read += ja_read_deinterleaved(dst, read, buf[i], nframes - read);

      jack_ringbuffer_read_advance(jd->buffer, read * sizeof(float) * 2);

      for (; read < nframes; read++)
         for (i = 0; i < 2; i++)
            dst[i][read] = 0.0f;
   }
#ifdef HAVE_THREADS
   scond_signal(jd->cond);
#endif
   return 0;
}

static void ja_shutdown_cb(void *data)
{
   jack_t *jd = (jack_t*)data;

   if (!jd)
      return;

   jd->shutdown = true;
#ifdef HAVE_THREADS
   scond_signal(jd->cond);
#endif
}

static int ja_parse_ports(char **dest_ports, const char **jports)
{
   int i;
   int parsed               = 0;
   settings_t *settings     = config_get_ptr();
   const char *audio_device = settings->arrays.audio_device;
   const char *comma        = strchr(audio_device, ',');

   if (comma && comma != audio_device)
   {
      dest_ports[parsed++] = strndup(audio_device, comma - audio_device);
      if (*(comma + 1))
         dest_ports[parsed++] = strdup(comma + 1);
   }
   else if (*audio_device)
      dest_ports[parsed++] = strdup(audio_device);

   for (i = parsed; i < 2; i++)
      dest_ports[i] = strdup(jports[i]);

   return 2;
}

static size_t ja_find_buffersize(jack_t *jd, int latency, unsigned out_rate)
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

   RARCH_LOG("[JACK] Jack latency is %d frames.\n", jack_latency);
   /* The graph's own stage behind the ring, for the statistics
    * overlay: what the port reports for playback, at the JACK rate the
    * driver runs at. */
   audio_driver_set_device_latency((size_t)jack_latency);

   buffer_frames     = frames - jack_latency;
   min_buffer_frames = jack_get_buffer_size(jd->client) * 2;

   RARCH_LOG("[JACK] Minimum buffer size is %d frames.\n", min_buffer_frames);

   if (buffer_frames < min_buffer_frames)
      buffer_frames = min_buffer_frames;

   /* The ring holds interleaved stereo float, two samples a frame.
    * Sized at one sample a frame it held half the frames asked for, and
    * buffer_size() reported that half, so the rate control's setpoint -
    * half of it again - sat at a quarter of the latency setting. */
   return buffer_frames * 2 * sizeof(jack_default_audio_sample_t);
}

static void *ja_init(const char *device,
      unsigned rate, unsigned latency,
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

   retro_atomic_size_init(&jd->consumed, 0);

#ifdef HAVE_THREADS
   jd->cond      = scond_new();
   jd->cond_lock = slock_new();
#endif

   jd->client = jack_client_open("RetroArch", JackNullOption, NULL);
   if (!jd->client)
      goto error;

   *new_rate = jack_get_sample_rate(jd->client);

   jack_set_process_callback(jd->client, ja_process_cb, jd);
   jack_on_shutdown(jd->client, ja_shutdown_cb, jd);

   jd->ports[0] = jack_port_register(jd->client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   jd->ports[1] = jack_port_register(jd->client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   if (!jd->ports[0] || !jd->ports[1])
   {
      RARCH_ERR("[JACK] Failed to register ports.\n");
      goto error;
   }

   jports = jack_get_ports(jd->client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
   if (!jports)
   {
      RARCH_ERR("[JACK] Failed to get ports.\n");
      goto error;
   }

   bufsize         = ja_find_buffersize(jd, latency, *new_rate);
   jd->buffer_size = bufsize;

#ifdef HAVE_THREADS
   /* One process-callback period, floored so a tiny JACK buffer does
    * not turn the bounded wait into a spin. */
   jd->wait_us     = *new_rate
      ? (int64_t)jack_get_buffer_size(jd->client) * 1000000 / *new_rate
      : 1000;
   if (jd->wait_us < 1000)
      jd->wait_us  = 1000;
#endif

   RARCH_LOG("[JACK] Internal buffer size: %d frames.\n", (int)(bufsize / (2 * sizeof(jack_default_audio_sample_t))));

   jd->buffer = jack_ringbuffer_create(bufsize);
   if (!jd->buffer)
   {
      RARCH_ERR("[JACK] Failed to create buffers.\n");
      goto error;
   }

   parsed = ja_parse_ports(dest_ports, jports);

   if (jack_activate(jd->client) < 0)
   {
      RARCH_ERR("[JACK] Failed to activate Jack.\n");
      goto error;
   }

   for (i = 0; i < 2; i++)
   {
      if (jack_connect(jd->client, jack_port_name(jd->ports[i]), dest_ports[i]))
      {
         RARCH_ERR("[JACK] Failed to connect to Jack port.\n");
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
   /* The client, ring, condition and lock are made before the server
    * is asked for anything; a server that is not there leaves all four
    * to release. */
   if (jd->client)
      jack_client_close(jd->client);
   if (jd->buffer)
      jack_ringbuffer_free(jd->buffer);
   if (jd->cond)
      scond_free(jd->cond);
   if (jd->cond_lock)
      slock_free(jd->cond_lock);
   free(jd);
   return NULL;
}

/* How many period-long waits a blocked write or wait_writable() may
 * take before it gives up on the server making room. The process
 * callback frees a period every period on a running graph; a graph
 * that has stopped running it - the client deactivated, the server
 * frozen - never does, and shutdown_cb is not called for that. */
#define JACK_WAIT_LAPS 8

static ssize_t ja_write(void *data, const void *buf_, size_t len)
{
   size_t _len = 0;
   jack_t      *jd = (jack_t*)data;
   const char *buf = (const char *)buf_;
   int laps        = JACK_WAIT_LAPS;

   while (len > 0)
   {
      size_t avail, to_write;

      if (jd->shutdown)
         return 0;

      avail = jack_ringbuffer_write_space(jd->buffer);

      to_write = (len < avail) ? len : avail;
      /* Quantise to whole stereo frames, not single floats.  JACK's
       * ringbuffer holds size-1 bytes, so with frame-aligned usage the
       * free space is never frame-aligned: rounding to sizeof(float)
       * let the first ring-full write land on a half frame, after which
       * every sample was L/R-swapped (with a one-sample interchannel
       * skew) for the rest of the session.  The process callback reads
       * in whole frames, so the writer must feed whole frames. */
      to_write = (to_write / (2 * sizeof(float))) * (2 * sizeof(float));

      if (to_write > 0)
      {
         jack_ringbuffer_write(jd->buffer, buf, to_write);
         buf     += to_write;
         len     -= to_write;
         _len    += to_write;
      }
      else if (!jd->nonblock)
      {
#ifdef HAVE_THREADS
         /* Timed, not indefinite.  The predicate is the ringbuffer's
          * write space, which is lock-free - the consumer is JACK's
          * real-time process callback and must not take a lock - so
          * cond_lock cannot also guard it, and a signal raised between
          * the write_space test above and this wait reaches no waiter.
          *
          * That matters most for the one signal that is never
          * repeated.  ja_process_cb signals every period, but once the
          * JACK server goes away it is never called again;
          * ja_shutdown_cb then signals exactly once, without
          * cond_lock.  Losing that single wakeup to the window left
          * this thread parked forever, because the jd->shutdown test
          * that would have released it sits at the top of a loop the
          * thread can no longer reach.  A timed wait puts it back
          * inside the loop, where both shutdown and write space are
          * rechecked. */
         slock_lock(jd->cond_lock);
         scond_wait_timeout(jd->cond, jd->cond_lock, jd->wait_us);
         slock_unlock(jd->cond_lock);
         /* Bounded overall as well as per wait: a graph that never
          * makes room ends the write with what went. */
         if (--laps < 0)
            break;
#else
         break;
#endif
         continue;
      }
      else
         break;
   }

   return _len;
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

   if (jd->buffer)
      jack_ringbuffer_free(jd->buffer);

#ifdef HAVE_THREADS
   if (jd->cond_lock)
      slock_free(jd->cond_lock);
   if (jd->cond)
      scond_free(jd->cond);
#endif
   free(jd);
}

static bool ja_use_float(void *data) { return true; }

static size_t ja_write_avail(void *data)
{
   jack_t *jd = (jack_t*)data;
   return jack_ringbuffer_write_space(jd->buffer);
}

static size_t ja_buffer_size(void *data)
{
   jack_t *jd = (jack_t*)data;
   return jd->buffer_size;
}

/* Sleep on the condition the process callback signals after every
 * cycle until at least len bytes fit in the ring, capped at half of it
 * so the wait always ends. Returns the free space then, or 0 once the
 * server has shut the client down. */
static size_t ja_wait_writable(void *data, size_t len)
{
   jack_t *jd = (jack_t*)data;
   size_t avail;

   int laps = JACK_WAIT_LAPS;

   if (len > jd->buffer_size / 2)
      len = jd->buffer_size / 2;

   for (;;)
   {
      if (jd->shutdown)
         return 0;
      avail = jack_ringbuffer_write_space(jd->buffer);
      if (avail >= len)
         return avail;
#ifdef HAVE_THREADS
      slock_lock(jd->cond_lock);
      scond_wait_timeout(jd->cond, jd->cond_lock, jd->wait_us);
      slock_unlock(jd->cond_lock);
      /* No room after this many periods: the graph is not running the
       * process callback, and the pass is handed back as no space
       * coming from this call rather than waited on further. */
      if (--laps < 0)
         return 0;
#else
      return 0;
#endif
   }
}

/* The device string is "left_port,right_port": physical input ports the
 * stream connects to. List every physical input port from a throwaway
 * client, without starting a server that is not already running. */
static void *ja_device_list_new(void *data)
{
   int i;
   jack_status_t status;
   union string_list_elem_attr attr;
   jack_client_t *client   = NULL;
   const char   **ports    = NULL;
   struct string_list *sl  = string_list_new();

   (void)data;
   attr.i = 0;
   if (!sl)
      return NULL;

   client = jack_client_open("RetroArch-enum", JackNoStartServer, &status);
   if (!client)
   {
      string_list_free(sl);
      return NULL;
   }

   ports = jack_get_ports(client, NULL, NULL, JackPortIsPhysical | JackPortIsInput);
   if (ports)
   {
      for (i = 0; ports[i]; i++)
         string_list_append(sl, ports[i], attr);
      jack_free(ports);
   }

   jack_client_close(client);
   return sl;
}

static void ja_device_list_free(void *data, void *array_list_data)
{
   struct string_list *sl = (struct string_list*)array_list_data;
   (void)data;
   if (sl)
      string_list_free(sl);
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
   ja_device_list_new,
   ja_device_list_free,
   ja_write_avail,
   ja_buffer_size,
   NULL, /* write_raw */
   ja_wait_writable,
   ja_frames_consumed
};
