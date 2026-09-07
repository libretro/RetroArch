/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <string.h>

#include <lists/string_list.h>

#include <pulse/pulseaudio.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

typedef struct
{
   pa_threaded_mainloop *mainloop;
   pa_context *context;
   pa_stream *stream;
   size_t buffer_size;
   /* The server's request granularity; wait_writable() only needs
    * this much room to make progress, since the write itself fills in
    * the rest as it frees. */
   size_t minreq;
   struct string_list *devicelist;
   bool nonblock;
   bool success;
   bool is_paused;
   bool is_ready;
   /* Set by the timeout event pulse_wait_ms() arms. */
   bool timed_out;
   /* Frames handed to the server since the stream opened, for the sink
    * rate estimate; the device's own count is this less whatever is
    * still queued. Written and read on the frontend's thread, under
    * the mainloop lock where the writes happen. */
   uint64_t frames_written;
   unsigned rate;
} pa_t;

/* Bounds on the waits below. A server that is answering signals in
 * milliseconds; these are for one that is not - stopped consuming, or
 * gone without the state callback having fired yet. */
#define PULSE_CONNECT_WAIT_MS 3000
#define PULSE_OP_WAIT_MS      1000
#define PULSE_WRITE_WAIT_MS   1000

static void pulse_timeout_cb(pa_mainloop_api *a, pa_time_event *e,
      const struct timeval *tv, void *data)
{
   pa_t *pa = (pa_t*)data;
   (void)a; (void)e; (void)tv;
   pa->timed_out = true;
   pa_threaded_mainloop_signal(pa->mainloop, 0);
}

/* libpulse offers no timed wait, so one is made from a one-shot time
 * event on the mainloop that signals the waiter. Wakes on any signal,
 * as pa_threaded_mainloop_wait() does - callers re-check what they
 * were waiting for - or on the deadline, which returns false. Caller
 * holds the mainloop lock. */
static bool pulse_wait_ms(pa_t *pa, unsigned ms)
{
   pa_time_event *ev;

   pa->timed_out = false;
   ev = pa_context_rttime_new(pa->context,
         pa_rtclock_now() + (pa_usec_t)ms * PA_USEC_PER_MSEC,
         pulse_timeout_cb, pa);
   pa_threaded_mainloop_wait(pa->mainloop);
   if (ev)
      pa_threaded_mainloop_get_api(pa->mainloop)->time_free(ev);
   return !pa->timed_out;
}

static void pulse_free(void *data)
{
   pa_t *pa = (pa_t*)data;

   if (!pa)
      return;

   if (pa->mainloop)
      pa_threaded_mainloop_stop(pa->mainloop);

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
      pa_threaded_mainloop_free(pa->mainloop);

   if (pa->devicelist)
      string_list_free(pa->devicelist);

   free(pa);
}

static void pulse_stream_success_cb(pa_stream *s, int success, void *data)
{
   pa_t *pa = (pa_t*)data;
   (void)s;
   pa->success = success;
   pa_threaded_mainloop_signal(pa->mainloop, 0);
}

static void pulse_context_state_cb(pa_context *c, void *data)
{
   pa_t *pa = (pa_t*)data;

   switch (pa_context_get_state(c))
   {
      case PA_CONTEXT_READY:
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      case PA_CONTEXT_FAILED:
         RARCH_ERR("[PulseAudio] Connection failed.\n");
         pa->is_ready = false;
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      case PA_CONTEXT_TERMINATED:
         pa->is_ready = false;
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      default:
         break;
   }
}

static void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *data)
{
   union string_list_elem_attr attr;
   pa_t *pa = (pa_t*)data;
   attr.i = 0;

   if (!pa || !pa->devicelist)
      return;

   /* If EOL is set to a positive number,
    * you're at the end of the list */
   if (eol > 0)
      return;

   RARCH_DBG("[PulseAudio] Sink detected: %s.\n", l->name);
   string_list_append(pa->devicelist, l->name, attr);
}

static void pulse_stream_state_cb(pa_stream *s, void *data)
{
   pa_t *pa = (pa_t*)data;

   switch (pa_stream_get_state(s))
   {
      case PA_STREAM_READY:
         pa->is_ready = true;
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      case PA_STREAM_UNCONNECTED:
      case PA_STREAM_FAILED:
      case PA_STREAM_TERMINATED:
         pa->is_ready = false;
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      default:
         break;
   }
}

static void pulse_stream_request_cb(pa_stream *s, size_t len, void *data)
{
   pa_t *pa = (pa_t*)data;
   pa_threaded_mainloop_signal(pa->mainloop, 0);
}

static void pulse_stream_latency_update_cb(pa_stream *s, void *data)
{
   pa_t *pa = (pa_t*)data;
   pa_threaded_mainloop_signal(pa->mainloop, 0);
}

static void pulse_underrun_update_cb(pa_stream *s, void *data)
{
#if 0
   pa_t *pa = (pa_t*)data;

   (void)s;

   RARCH_LOG("[PulseAudio] Underrun (Buffer: %u, Writable size: %u).\n",
         (unsigned)pa->buffer_size,
         (unsigned)pa_stream_writable_size(pa->stream));
#endif
}

static void pulse_buffer_attr_cb(pa_stream *s, void *data)
{
   pa_t *pa = (pa_t*)data;
   const pa_buffer_attr *server_attr = pa_stream_get_buffer_attr(s);
   if (server_attr)
      pa->buffer_size = server_attr->tlength;
      pa->minreq      = server_attr->minreq;

#if 0
   RARCH_LOG("[PulseAudio] Got new buffer size %u.\n", (unsigned)pa->buffer_size);
#endif
}

static void *pulse_init(const char *device, unsigned rate,
      unsigned latency, unsigned block_frames,
      unsigned *new_rate)
{
   pa_sample_spec spec;
   pa_buffer_attr        buffer_attr = {0};
   const pa_buffer_attr *server_attr = NULL;
   pa_t                          *pa = (pa_t*)calloc(1, sizeof(*pa));

   if (!pa)
      return NULL;

   memset(&spec, 0, sizeof(spec));

   pa->devicelist = string_list_new();

   pa->mainloop = pa_threaded_mainloop_new();
   if (!pa->mainloop)
      goto error;

   pa->context = pa_context_new(pa_threaded_mainloop_get_api(pa->mainloop), "RetroArch");
   if (!pa->context)
      goto error;

   pa_context_set_state_callback(pa->context, pulse_context_state_cb, pa);

   /* Code is not prepared to use multiple PulseAudio servers, device is used as sink. */
   if (pa_context_connect(pa->context, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
      goto error;

   pa_threaded_mainloop_lock(pa->mainloop);
   if (pa_threaded_mainloop_start(pa->mainloop) < 0)
      goto error;

   /* The state callback signals on ready or failure; a server that
    * accepts the connection and then says nothing is given up on. */
   while (pa_context_get_state(pa->context) != PA_CONTEXT_READY)
   {
      pa_context_state_t st = pa_context_get_state(pa->context);
      if (st == PA_CONTEXT_FAILED || st == PA_CONTEXT_TERMINATED)
         goto unlock_error;
      if (!pulse_wait_ms(pa, PULSE_CONNECT_WAIT_MS))
      {
         RARCH_ERR("[PulseAudio] Server did not become ready within %u ms.\n",
               PULSE_CONNECT_WAIT_MS);
         goto unlock_error;
      }
   }

   pa_context_get_sink_info_list(pa->context, pa_sinklist_cb, pa);
   /* Checking device against sink list would be tricky due to callback, so it is just set. */
   if (device)
     pa_context_set_default_sink(pa->context, device, NULL, NULL);

   spec.format   = is_little_endian() ? PA_SAMPLE_FLOAT32LE : PA_SAMPLE_FLOAT32BE;
   spec.channels = 2;
   spec.rate     = rate;
   pa->rate      = rate;

   pa->stream    = pa_stream_new(pa->context, "audio", &spec, NULL);
   if (!pa->stream)
      goto unlock_error;

   pa_stream_set_state_callback(pa->stream, pulse_stream_state_cb, pa);
   pa_stream_set_write_callback(pa->stream, pulse_stream_request_cb, pa);
   pa_stream_set_latency_update_callback(pa->stream,
         pulse_stream_latency_update_cb, pa);
   pa_stream_set_underflow_callback(pa->stream, pulse_underrun_update_cb, pa);
   pa_stream_set_buffer_attr_callback(pa->stream, pulse_buffer_attr_cb, pa);

   buffer_attr.maxlength = -1;
   buffer_attr.tlength   = pa_usec_to_bytes(latency * PA_USEC_PER_MSEC, &spec);
   buffer_attr.prebuf    = -1;
   buffer_attr.minreq    = -1;
   buffer_attr.fragsize  = -1;

   if (pa_stream_connect_playback(pa->stream, NULL,
            &buffer_attr, PA_STREAM_ADJUST_LATENCY, NULL, NULL) < 0)
      goto error;

   while (pa_stream_get_state(pa->stream) != PA_STREAM_READY)
   {
      pa_stream_state_t st = pa_stream_get_state(pa->stream);
      if (st == PA_STREAM_FAILED || st == PA_STREAM_TERMINATED)
         goto unlock_error;
      if (!pulse_wait_ms(pa, PULSE_CONNECT_WAIT_MS))
      {
         RARCH_ERR("[PulseAudio] Stream did not become ready within %u ms.\n",
               PULSE_CONNECT_WAIT_MS);
         goto unlock_error;
      }
   }

   server_attr = pa_stream_get_buffer_attr(pa->stream);
   if (server_attr)
   {
      pa->buffer_size = server_attr->tlength;
      pa->minreq      = server_attr->minreq;
      RARCH_LOG("[PulseAudio] Requested %u bytes buffer, got %u.\n",
            (unsigned)buffer_attr.tlength,
            (unsigned)pa->buffer_size);
   }
   else
      pa->buffer_size = buffer_attr.tlength;
      pa->minreq      = buffer_attr.tlength / 4;

   pa_threaded_mainloop_unlock(pa->mainloop);
   pa->is_ready = true;

   return pa;

unlock_error:
   pa_threaded_mainloop_unlock(pa->mainloop);
error:
   pulse_free(pa);
   return NULL;
}

static bool pulse_start(void *data, bool is_shutdown)
{
   bool ret;
   pa_operation *op;
   pa_t *pa = (pa_t*)data;

   if (!pa->is_ready)
      return false;
   if (!pa->is_paused)
      return true;

   pa->success = true; /* In case of spurious wakeup. Not critical. */
   pa_threaded_mainloop_lock(pa->mainloop);
   /* The operation object is ours to release once the callback has
    * reported; it was never being released, one per start or stop. */
   if ((op = pa_stream_cork(pa->stream, false, pulse_stream_success_cb, pa)))
   {
      /* Wake on the operation's callback, or on the bound: a corked
       * stream raises no write callbacks, so nothing else would. */
      while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
         if (!pulse_wait_ms(pa, PULSE_OP_WAIT_MS))
         {
            pa_operation_cancel(op);
            pa->success = false;
            break;
         }
      pa_operation_unref(op);
   }
   ret = pa->success;
   pa_threaded_mainloop_unlock(pa->mainloop);
   pa->is_paused = false;
   return ret;
}

static ssize_t pulse_write(void *data, const void *s, size_t len)
{
   size_t _len = 0;
   pa_t           *pa = (pa_t*)data;
   const uint8_t *buf = (const uint8_t*)s;

   /* Workaround buggy menu code.
    * If a write happens while we're paused, we might never progress. */
   if (pa->is_paused && !pulse_start(pa, false))
      return -1;

   if (!pa->is_ready)
      return 0;

   pa_threaded_mainloop_lock(pa->mainloop);
   while (len)
   {
      size_t writable = MIN(len, pa_stream_writable_size(pa->stream));

      if (writable)
      {
         pa_stream_write(pa->stream, buf, writable, NULL, 0, PA_SEEK_RELATIVE);
         buf     += writable;
         len     -= writable;
         _len    += writable;
         /* Stereo float32, fixed at stream setup. */
         pa->frames_written += writable / (2 * sizeof(float));
      }
      else if (!pa->nonblock)
      {
         /* Wakes on the next write callback. A server that has
          * stopped consuming raises none; the bound ends the write
          * with what went, rather than holding the thread on it. */
         if (!pulse_wait_ms(pa, PULSE_WRITE_WAIT_MS) || !pa->is_ready)
            break;
      }
      else
         break;
   }

   pa_threaded_mainloop_unlock(pa->mainloop);

   return _len;
}

static bool pulse_stop(void *data)
{
   bool ret;
   pa_operation *op;
   pa_t *pa = (pa_t*)data;

   if (!pa->is_ready)
      return false;
   if (pa->is_paused)
      return true;

   pa->success = true; /* In case of spurious wakeup. Not critical. */
   pa_threaded_mainloop_lock(pa->mainloop);
   /* The operation object is ours to release once the callback has
    * reported; it was never being released, one per start or stop. */
   if ((op = pa_stream_cork(pa->stream, true, pulse_stream_success_cb, pa)))
   {
      while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
         if (!pulse_wait_ms(pa, PULSE_OP_WAIT_MS))
         {
            pa_operation_cancel(op);
            pa->success = false;
            break;
         }
      pa_operation_unref(op);
   }
   ret = pa->success;
   pa_threaded_mainloop_unlock(pa->mainloop);
   pa->is_paused = true;
   return ret;
}

static bool pulse_alive(void *data)
{
   pa_t *pa = (pa_t*)data;

   if (!pa || !pa->is_ready)
      return false;
   return !pa->is_paused;
}

static void pulse_set_nonblock_state(void *data, bool state)
{
   pa_t *pa = (pa_t*)data;
   if (pa)
      pa->nonblock = state;
}

static bool pulse_use_float(void *data) { return true; }

static size_t pulse_write_avail(void *data)
{
   size_t _len;
   pa_t *pa = (pa_t*)data;

   if (!pa->is_ready)
      return 0;

   pa_threaded_mainloop_lock(pa->mainloop);
   _len = pa_stream_writable_size(pa->stream);

   audio_driver_set_buffer_size(pa->buffer_size); /* Can change spuriously. */
   {
      /* The sink's own latency behind the stream buffer, for the
       * statistics overlay: sink_usec is the part of
       * pa_stream_get_latency() that is not the server's queue. Only
       * known once timing data has arrived, so the pointer is NULL
       * until then, and it can change with the sink. */
      const pa_timing_info *ti = pa_stream_get_timing_info(pa->stream);
      if (ti && pa->rate)
         audio_driver_set_device_latency((size_t)
               ((uint64_t)ti->sink_usec * pa->rate / 1000000));
   }
   pa_threaded_mainloop_unlock(pa->mainloop);
   return _len;
}

static size_t pulse_buffer_size(void *data)
{
   pa_t *pa = (pa_t*)data;
   return pa->buffer_size;
}

/* Frames the device has taken since the stream opened.
 *
 * PulseAudio has no callback per period to count, so this is ALSA's
 * shape rather than JACK's: everything handed to the server, less what
 * has not been played yet. pa_stream_get_latency() reports that
 * remainder in microseconds - the server's queue plus the sink's own,
 * which is what should be subtracted - and it is only meaningful once
 * timing data has arrived, so a stream that has not got any yet
 * reports nothing rather than a count that would read as a stall.
 *
 * The negative case is real: on a monitor source or right after a
 * flush the latency can come back negative, meaning the server is
 * ahead of the write pointer. Subtracting it would inflate the count. */
static size_t pulse_frames_consumed(void *data)
{
   pa_t     *pa = (pa_t*)data;
   pa_usec_t lat_usec;
   int       negative = 0;
   uint64_t  queued;

   if (!pa || !pa->is_ready || !pa->rate)
      return 0;

   pa_threaded_mainloop_lock(pa->mainloop);
   if (pa_stream_get_latency(pa->stream, &lat_usec, &negative) < 0)
   {
      pa_threaded_mainloop_unlock(pa->mainloop);
      return 0;
   }
   queued = negative ? 0
         : (uint64_t)((double)lat_usec * (double)pa->rate / 1000000.0);
   if (queued > pa->frames_written)
      queued = pa->frames_written;
   {
      size_t out = (size_t)(pa->frames_written - queued);
      pa_threaded_mainloop_unlock(pa->mainloop);
      return out;
   }
}

/* Sleep on the mainloop until at least len bytes are writable. The
 * stream's write callback signals the mainloop as the server drains,
 * so this wakes when room exists and never on a timer. */
static size_t pulse_wait_writable(void *data, size_t len)
{
   size_t _len = 0;
   pa_t *pa    = (pa_t*)data;

   if (!pa->is_ready)
      return 0;

   /* Enough for the server's next request is enough: the write loops
    * on writable space as the server frees it, and before the stream
    * has started playing (prebuf) that partial write is the only way
    * to get it full. */
   if (pa->minreq && len > pa->minreq)
      len = pa->minreq;

   pa_threaded_mainloop_lock(pa->mainloop);
   while (pa->is_ready)
   {
      _len = pa_stream_writable_size(pa->stream);
      if (_len >= len)
         break;
      /* Wakes on the next write callback, i.e. when the server has
       * consumed enough for more room to exist - or on the bound, which
       * hands the pass back as no space coming from this call. */
      if (!pulse_wait_ms(pa, PULSE_WRITE_WAIT_MS))
      {
         _len = 0;
         break;
      }
   }
   if (!pa->is_ready)
      _len = 0;
   audio_driver_set_buffer_size(pa->buffer_size);
   pa_threaded_mainloop_unlock(pa->mainloop);
   return _len;
}

/* Context-free enumeration: a short-lived plain mainloop, connect,
 * list sinks, tear down. Used when there is no driver instance - init
 * failed, or the menu asks before the driver is up - which is exactly
 * when a user needs the list most. The threaded mainloop the driver
 * runs on is not touched. */
struct pulse_enum_state
{
   struct string_list *sl;
   int done;      /* sink list callback saw EOL, or an error ended it */
   int failed;
};

static void pulse_enum_sinklist_cb(pa_context *c,
      const pa_sink_info *l, int eol, void *data)
{
   struct pulse_enum_state *st = (struct pulse_enum_state*)data;
   union string_list_elem_attr attr;
   attr.i = 0;
   if (!st)
      return;
   if (eol > 0)
   {
      st->done = 1;
      return;
   }
   if (eol < 0)
   {
      st->done   = 1;
      st->failed = 1;
      return;
   }
   if (l && l->name)
      string_list_append(st->sl, l->name, attr);
}

static struct string_list *pulse_enumerate_sinks(void)
{
   struct pulse_enum_state st;
   pa_mainloop *ml   = NULL;
   pa_context  *ctx  = NULL;
   pa_operation *op  = NULL;
   int ret;

   st.sl     = string_list_new();
   st.done   = 0;
   st.failed = 0;
   if (!st.sl)
      return NULL;

   if (!(ml = pa_mainloop_new()))
      goto error;
   if (!(ctx = pa_context_new(pa_mainloop_get_api(ml), "RetroArch")))
      goto error;
   if (pa_context_connect(ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0)
      goto error;

   /* Drive the loop until the context settles one way or the other. */
   for (;;)
   {
      pa_context_state_t state = pa_context_get_state(ctx);
      if (state == PA_CONTEXT_READY)
         break;
      if (!PA_CONTEXT_IS_GOOD(state))
         goto error;
      if (pa_mainloop_iterate(ml, 1, &ret) < 0)
         goto error;
   }

   if (!(op = pa_context_get_sink_info_list(ctx, pulse_enum_sinklist_cb, &st)))
      goto error;
   while (!st.done)
   {
      if (pa_mainloop_iterate(ml, 1, &ret) < 0)
         break;
   }
   pa_operation_unref(op);
   op = NULL;
   if (st.failed)
      goto error;

   pa_context_disconnect(ctx);
   pa_context_unref(ctx);
   pa_mainloop_free(ml);
   return st.sl;

error:
   if (op)
      pa_operation_unref(op);
   if (ctx)
   {
      pa_context_disconnect(ctx);
      pa_context_unref(ctx);
   }
   if (ml)
      pa_mainloop_free(ml);
   string_list_free(st.sl);
   return NULL;
}

static void *pulse_device_list_new(void *data)
{
   pa_t *pa = (pa_t*)data;

   /* A live driver already holds the list it built at connect time. */
   if (pa && pa->devicelist)
      return string_list_clone(pa->devicelist);

   return pulse_enumerate_sinks();
}

static void pulse_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;

   if (!s)
      return;

   string_list_free(s);
}

audio_driver_t audio_pulse = {
   pulse_init,
   pulse_write,
   pulse_stop,
   pulse_start,
   pulse_alive,
   pulse_set_nonblock_state,
   pulse_free,
   pulse_use_float,
   "pulse",
   pulse_device_list_new,
   pulse_device_list_free,
   pulse_write_avail,
   pulse_buffer_size,
   NULL, /* write_raw */
   pulse_wait_writable,
   pulse_frames_consumed
};
