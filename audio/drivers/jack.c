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

#define JACK_MAX_PORTS 8
#define FRAMES(x) (x / (sizeof(float) * jd->channels))

typedef struct jack
{
   jack_client_t *client;
   jack_port_t *ports[JACK_MAX_PORTS];
   unsigned channels;   /* ports, and samples a frame in the ring */
   uint32_t layout;     /* the frontend's mask, one port a position */
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
   /* Periods the process callback could not fill from the ring: the
    * frontend starving this driver, which is not the same event as
    * the server missing its own deadline - that is counted next to
    * it, from the XRUN callback, because the answer to each is
    * different and a ring that is already big enough is not the fix
    * for the second. */
   retro_atomic_size_t underruns;
   retro_atomic_size_t xruns;

   /* The device clock, from the cycle times the server keeps.
    *
    * consumed above counts the frames the server asked for, which is
    * device-paced and is what the sink estimate uses - but it says
    * nothing about when those frames went out.
    * jack_get_cycle_times() does: it returns the frame time at the
    * start of the cycle together with the microsecond time of the same
    * instant, which is the pair this needs, and it is meant to be
    * called from the process callback.
    *
    * current_frames is 32 bits and wraps in about a day at 48 kHz, so
    * the difference is taken between cycles and accumulated, which is
    * correct across the wrap as long as a cycle is shorter than the
    * wrap - which it is by nine orders of magnitude.
    *
    * A fit over every cycle, not two points: noise on a single anchor
    * divides by the window and reads as drift. Touched only by the
    * process callback; the result is published as one int, in ppm.
    * Nothing here feeds rate control. */
   jack_nframes_t     clk_last_frames;
   uint64_t           clk_frames_total;
   uint64_t           clk_anchor_usec;
   int                clk_have_anchor;
   double             clk_sx, clk_sy, clk_sxx, clk_sxy, clk_n;
   unsigned           clk_rate;
   retro_atomic_int_t clk_ppm;
   retro_atomic_int_t clk_valid;
   /* The server saying its rate or its period has changed. Both make
    * everything downstream wrong - what the resampler produces, what
    * the ring was sized for, how long a write waits - so both ask the
    * frontend to build this driver again. */
   retro_atomic_int_t reconfigure;
   /* Written by the server's threads, read by the frontend's. Was a
    * volatile bool, which orders nothing between threads. */
   retro_atomic_int_t shutdown;
   retro_atomic_int_t is_paused;
   bool nonblock;
} jack_t;

static size_t ja_read_deinterleaved(jack_t *jd, float *dst[JACK_MAX_PORTS], jack_nframes_t dst_offset,
      jack_ringbuffer_data_t buf, jack_nframes_t nframes)
{
   unsigned i;
   jack_nframes_t j, frames_avail;
   const float *src = (const float *)buf.buf;

   if (nframes <= 0)
      return 0;

   frames_avail = FRAMES(buf.len);
   nframes = nframes < frames_avail ? nframes : frames_avail;

   for (j = 0; j < nframes; j++)
      for (i = 0; i < jd->channels; i++)
         dst[i][dst_offset + j] = *src++;

   return nframes;
}

/* Frames the server has taken since the client started. JACK calls the
 * process callback once per period and says how long the period is, so
 * counting what it asks for counts device time; there is no queue to
 * subtract, unlike ALSA, because the callback is the device consuming
 * the audio rather than a queue being filled. */
/* The server clock, for the statistics overlay. */
static bool ja_device_clock_ppm(void *data, double *ppm)
{
   jack_t *jd = (jack_t*)data;
   if (!jd || !retro_atomic_load_acquire_int(&jd->clk_valid))
      return false;
   *ppm = (double)retro_atomic_load_acquire_int(&jd->clk_ppm);
   return true;
}

static size_t ja_frames_consumed(void *data)
{
   jack_t *jd = (jack_t*)data;
   if (!jd)
      return 0;
   return retro_atomic_load_acquire_size(&jd->consumed);
}

static int ja_process_cb(jack_nframes_t nframes, void *data)
{
   jack_t        *jd = (jack_t*)data;
   unsigned       i;
   float         *dst[JACK_MAX_PORTS];
   jack_ringbuffer_data_t buf[2];
   jack_nframes_t read = 0;

   if (nframes > 0)
   {
      retro_atomic_fetch_add_size(&jd->consumed, (size_t)nframes);

      /* The device clock - see the note on the fields. Two reads and a
       * handful of flops, on a call meant for this thread. */
      {
         jack_nframes_t cur_f = 0;
         jack_time_t    cur_u = 0, next_u = 0;
         float          per_u = 0.0f;

         if (     jd->clk_rate
               && jack_get_cycle_times(jd->client, &cur_f, &cur_u,
                     &next_u, &per_u) == 0)
         {
            if (!jd->clk_have_anchor)
            {
               jd->clk_last_frames  = cur_f;
               jd->clk_frames_total = 0;
               jd->clk_anchor_usec  = (uint64_t)cur_u;
               jd->clk_have_anchor  = 1;
               jd->clk_sx = jd->clk_sy = jd->clk_sxx = 0.0;
               jd->clk_sxy = jd->clk_n = 0.0;
            }
            else if ((uint64_t)cur_u > jd->clk_anchor_usec)
            {
               /* Wrap-safe in 32 bits, which is what the difference of
                * two jack_nframes_t is. */
               jack_nframes_t step = cur_f - jd->clk_last_frames;
               double x, y, d;

               jd->clk_last_frames   = cur_f;
               jd->clk_frames_total += (uint64_t)step;

               x = (double)((uint64_t)cur_u - jd->clk_anchor_usec)
                  / 1000000.0;
               y = (double)jd->clk_frames_total;

               jd->clk_sx  += x;
               jd->clk_sy  += y;
               jd->clk_sxx += x * x;
               jd->clk_sxy += x * y;
               jd->clk_n   += 1.0;

               d = jd->clk_n * jd->clk_sxx - jd->clk_sx * jd->clk_sx;
               if (x >= 1.0 && d > 0.0)
               {
                  double slope = (jd->clk_n * jd->clk_sxy
                        - jd->clk_sx * jd->clk_sy) / d;
                  double ppm   = (slope / (double)jd->clk_rate - 1.0)
                     * 1000000.0;
                  if (ppm > -100000.0 && ppm < 100000.0)
                  {
                     retro_atomic_store_release_int(&jd->clk_ppm, (int)ppm);
                     retro_atomic_store_release_int(&jd->clk_valid, 1);
                  }
               }
            }
            else
            {
               jd->clk_last_frames  = cur_f;
               jd->clk_frames_total = 0;
               jd->clk_anchor_usec  = (uint64_t)cur_u;
               jd->clk_sx = jd->clk_sy = jd->clk_sxx = 0.0;
               jd->clk_sxy = jd->clk_n = 0.0;
            }
         }
      }

      for (i = 0; i < jd->channels; i++)
         dst[i] = (float *)jack_port_get_buffer(jd->ports[i], nframes);

      /* Paused: silence out, and the ring left where it is. The
       * client stays live and connected - the graph, the scheduling
       * and the queued audio all survive a pause, and resuming is the
       * next period rather than a reconnection - but a pause that
       * went on reading would drain what is queued and then play
       * silence, which is not a pause. */
      if (retro_atomic_load_acquire_int(&jd->is_paused))
      {
         for (i = 0; i < jd->channels; i++)
            memset(dst[i], 0, (size_t)nframes * sizeof(float));
      }
      else
      {
         jack_ringbuffer_get_read_vector(jd->buffer, buf);

         for (i = 0; i < 2; i++)
            read += ja_read_deinterleaved(jd, dst, read, buf[i], nframes - read);

         jack_ringbuffer_read_advance(jd->buffer, read * sizeof(float) * jd->channels);

         if (read < nframes)
            retro_atomic_fetch_add_size(&jd->underruns, 1);

         for (; read < nframes; read++)
            for (i = 0; i < jd->channels; i++)
               dst[i][read] = 0.0f;
      }
   }
#ifdef HAVE_THREADS
   scond_signal(jd->cond);
#endif
   return 0;
}

/* The server's rate or its period changing makes everything
 * downstream wrong: what the resampler produces, what the ring was
 * sized for, how long a write waits, and what latency was reported.
 * A rate change is the worse of the two - 48000 against 44100 is
 * nearly nine percent, where rate control moves half of one - and
 * nothing slow can recover it. Neither is repaired from the server's
 * own thread; the frontend is asked to build this driver again, on
 * its own thread, against whatever the server now is. */
static int ja_sample_rate_cb(jack_nframes_t nframes, void *data)
{
   jack_t *jd = (jack_t*)data;
   if (jd)
   {
      retro_atomic_store_release_int(&jd->reconfigure, 1);
      retro_atomic_store_release_int(
            &audio_state_get_ptr()->reinit_request, 1);
   }
   return 0;
}

static int ja_buffer_size_cb(jack_nframes_t nframes, void *data)
{
   jack_t *jd = (jack_t*)data;
   if (jd)
   {
      retro_atomic_store_release_int(&jd->reconfigure, 1);
      retro_atomic_store_release_int(
            &audio_state_get_ptr()->reinit_request, 1);
   }
   return 0;
}

/* The server missing its own deadline, which is not this driver
 * starving it: counted apart from the ring's own underruns, because
 * a bigger ring is the answer to one and not the other. */
static int ja_xrun_cb(void *data)
{
   jack_t *jd = (jack_t*)data;
   if (jd)
      retro_atomic_fetch_add_size(&jd->xruns, 1);
   return 0;
}

static void ja_shutdown_cb(void *data)
{
   jack_t *jd = (jack_t*)data;

   if (!jd)
      return;

   retro_atomic_store_release_int(&jd->shutdown, 1);
#ifdef HAVE_THREADS
   scond_signal(jd->cond);
#endif
}

/* The two ports the setting may name, then the server's physical
 * inputs in order for the rest - which, past stereo, is where the
 * user's patchbay takes over. Returns the count filled; a server with
 * fewer physical inputs than the layout has positions leaves the
 * rest unconnected. */
static int ja_parse_ports(jack_t *jd, char **dest_ports, const char **jports)
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

   for (i = parsed; i < (int)jd->channels; i++)
   {
      int k;
      for (k = 0; k <= i && jports[k]; k++) ;
      if (k <= i)
         break;                 /* the server has no more physical inputs */
      dest_ports[i] = strdup(jports[i]);
   }

   return i;
}

static size_t ja_find_buffersize(jack_t *jd, int latency, unsigned out_rate)
{
   jack_latency_range_t range;
   int i, buffer_frames, min_buffer_frames;
   int jack_latency     = 0;
   int           frames = latency * out_rate / 1000;

   for (i = 0; i < (int)jd->channels; i++)
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

   /* The ring holds interleaved float, a sample a channel a frame.
    * Sized at one sample a frame it held half the frames asked for, and
    * buffer_size() reported that half, so the rate control's setpoint -
    * half of it again - sat at a quarter of the latency setting. */
   return buffer_frames * jd->channels * sizeof(jack_default_audio_sample_t);
}

static void *ja_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   int i;
   char *dest_ports[JACK_MAX_PORTS];
   const char **jports = NULL;
   /* one output port a position of the frontend's layout, named for
    * the position, in the mask's ascending order */
   static const char *port_names[16] = {
      "front-left", "front-right", "front-center", "lfe",
      "rear-left", "rear-right", "front-left-of-center", "front-right-of-center",
      "rear-center", "side-left", "side-right", NULL, NULL, NULL, NULL, NULL };
   uint32_t layout   = audio_driver_requested_layout();
   unsigned channels = audio_layout_channels(layout);
   size_t       bufsize = 0;
   int           parsed = 0;
   jack_t           *jd = (jack_t*)calloc(1, sizeof(jack_t));

   if (!jd)
      return NULL;

   retro_atomic_size_init(&jd->consumed, 0);
   retro_atomic_size_init(&jd->underruns, 0);
   retro_atomic_size_init(&jd->xruns, 0);
   retro_atomic_int_init(&jd->reconfigure, 0);
   retro_atomic_int_init(&jd->shutdown, 0);
   /* Not paused, which is what the plain bool this replaced was
    * zero-initialised to: a driver that came up paused would be
    * silent for any path that does not call start(), and that is a
    * behaviour change this has no reason to make. Set before the
    * client is activated, since the process callback reads it. */
   retro_atomic_int_init(&jd->is_paused, 0);

#ifdef HAVE_THREADS
   jd->cond      = scond_new();
   jd->cond_lock = slock_new();
#endif

   jd->client = jack_client_open("RetroArch", JackNullOption, NULL);
   if (!jd->client)
      goto error;

   *new_rate     = jack_get_sample_rate(jd->client);
   jd->clk_rate  = *new_rate;
   retro_atomic_int_init(&jd->clk_ppm, 0);
   retro_atomic_int_init(&jd->clk_valid, 0);

   jack_set_process_callback(jd->client, ja_process_cb, jd);
   /* Registered before activation, as JACK requires of a client that
    * depends on either - and this depends on both. */
   jack_set_sample_rate_callback(jd->client, ja_sample_rate_cb, jd);
   jack_set_buffer_size_callback(jd->client, ja_buffer_size_cb, jd);
   jack_set_xrun_callback(jd->client, ja_xrun_cb, jd);
   jack_on_shutdown(jd->client, ja_shutdown_cb, jd);

   if (channels < 2 || channels > JACK_MAX_PORTS)
   {
      layout   = AUDIO_LAYOUT_STEREO;
      channels = 2;
   }
   jd->layout   = layout;
   jd->channels = channels;
   if (channels == 2)
   {
      jd->ports[0] = jack_port_register(jd->client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
      jd->ports[1] = jack_port_register(jd->client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   }
   else
   {
      unsigned bit, n = 0;
      for (bit = 0; bit < 16 && n < channels; bit++)
         if (layout & (1u << bit))
            jd->ports[n++] = jack_port_register(jd->client, port_names[bit] ? port_names[bit] : "out",
                  JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
   }
   for (i = 0; i < (int)channels; i++)
      if (!jd->ports[i])
      {
         RARCH_ERR("[JACK] Failed to register ports.\n");
         goto error;
      }
   if (channels > 2)
      RARCH_LOG("[JACK] %u output ports for layout 0x%03x, named for their positions.\n", channels, layout);

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

   RARCH_LOG("[JACK] Internal buffer size: %d frames.\n", (int)(bufsize / (jd->channels * sizeof(jack_default_audio_sample_t))));

   jd->buffer = jack_ringbuffer_create(bufsize);
   /* Kept out of the pager's reach: the process callback reads this
    * ring on a realtime thread, where a page fault is a missed
    * deadline. A system that will not allow it says so and audio
    * carries on - a restrictive memlock limit is a configuration, not
    * a failure. */
   if (jd->buffer && jack_ringbuffer_mlock(jd->buffer) != 0)
      RARCH_WARN("[JACK] The ring could not be locked into memory; a page fault on the realtime thread will cost a period.\n");
   if (!jd->buffer)
   {
      RARCH_ERR("[JACK] Failed to create buffers.\n");
      goto error;
   }

   parsed = ja_parse_ports(jd, dest_ports, jports);

   if (jack_activate(jd->client) < 0)
   {
      RARCH_ERR("[JACK] Failed to activate Jack.\n");
      goto error;
   }

   /* The first two must connect, as always; a position past stereo
    * with nowhere to go is left for the patchbay, and said so. */
   for (i = 0; i < parsed; i++)
   {
      if (jack_connect(jd->client, jack_port_name(jd->ports[i]), dest_ports[i]))
      {
         if (i < 2)
         {
            RARCH_ERR("[JACK] Failed to connect to Jack port.\n");
            goto error;
         }
         RARCH_WARN("[JACK] Port %s did not connect to %s; connect it in the patchbay.\n",
               jack_port_name(jd->ports[i]), dest_ports[i]);
      }
   }
   if (parsed < (int)jd->channels)
      RARCH_WARN("[JACK] %u of %u ports left unconnected: the server has %d physical inputs. Connect them in the patchbay.\n",
            jd->channels - (unsigned)parsed, jd->channels, parsed);

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

      if (retro_atomic_load_acquire_int(&jd->shutdown))
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
      to_write = (to_write / (jd->channels * sizeof(float))) * (jd->channels * sizeof(float));

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
      retro_atomic_store_release_int(&jd->is_paused, 1);
   return true;
}

static bool ja_alive(void *data)
{
   jack_t *jd = (jack_t*)data;
   if (!jd)
      return false;
   /* A server that has gone is not alive, whatever the pause flag
    * says: the two were independent and only the pause was reported,
    * so a client whose server died read as playing. */
   if (retro_atomic_load_acquire_int(&jd->shutdown))
      return false;
   return !retro_atomic_load_acquire_int(&jd->is_paused);
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
   if (!jd)
      return false;
   /* Nothing to start on a client whose server has gone; saying so
    * lets the frontend take it up rather than play to nothing. */
   if (retro_atomic_load_acquire_int(&jd->shutdown))
      return false;
   retro_atomic_store_release_int(&jd->is_paused, 0);
   return true;
}

static void ja_free(void *data)
{
   jack_t *jd = (jack_t*)data;
   size_t  x;

   retro_atomic_store_release_int(&jd->shutdown, 1);

   /* What the server's clock was doing against the wall. Logged, not
    * acted on: until these have been read off a range of hardware they
    * are measurements, and a clock estimate that is wrong is worse
    * than one that is absent. With a device driving the graph this is
    * that device; with something else driving it, it is that. */
   if (retro_atomic_load_acquire_int(&jd->clk_valid))
      RARCH_LOG("[JACK] Server clock, fitted from the cycle times:"
            " %+d ppm against %u Hz.\n",
            retro_atomic_load_acquire_int(&jd->clk_ppm), jd->clk_rate);
   else
      RARCH_LOG("[JACK] Server clock: not enough usable cycle times to"
            " fit one.\n");

   /* The server's own missed deadlines, said once, here. Kept apart
    * from this driver's underruns because the answers differ: a
    * bigger ring here fixes the one and does nothing for the other. */
   if ((x = retro_atomic_load_acquire_size(&jd->xruns)))
      RARCH_LOG("[JACK] The server missed its deadline %u time%s this session; that is the graph, not this driver's buffer.\n",
            (unsigned)x, x == 1 ? "" : "s");

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
      if (retro_atomic_load_acquire_int(&jd->shutdown))
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

/* What this driver failed to supply, which is what the frontend's
 * statistics are about. The server's own missed deadlines are logged
 * at teardown instead: they are not this driver starving, and
 * reporting them here would read as if they were. */
static size_t ja_underruns(void *data)
{
   jack_t *jd = (jack_t*)data;
   return jd ? retro_atomic_load_acquire_size(&jd->underruns) : 0;
}

static uint32_t ja_layout(void *data)
{
   jack_t *jd = (jack_t*)data;
   return jd ? jd->layout : AUDIO_LAYOUT_STEREO;
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
   ja_frames_consumed,
   ja_underruns,
   ja_layout,
   NULL, /* frames_consumed_fallback */
   ja_device_clock_ppm
};
