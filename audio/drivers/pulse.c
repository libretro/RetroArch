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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <lists/string_list.h>

#include <pulse/pulseaudio.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include <retro_spsc.h>
#include <rthreads/retro_eventcount.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

typedef struct
{
   pa_threaded_mainloop *mainloop;
   pa_context *context;
   uint32_t layout;   /* the frontend's mask the stream carries */
   pa_stream *stream;
   /* The server's buffer and its request granularity; wait_writable()
    * only needs minreq of room to make progress, since the write
    * itself fills in the rest as it frees. Set from the server's
    * thread when it changes them, read by the frontend's. */
   retro_atomic_size_t buffer_size;
   retro_atomic_size_t minreq;
   struct string_list *devicelist;
   bool nonblock;
   bool success;
   bool is_paused;
   /* Cleared from the server's thread when the stream or the context
    * ends; read by the writer without the mainloop lock. */
   retro_atomic_int_t ready;
   /* Set by the timeout event pulse_wait_ms() arms. */
   bool timed_out;
   /* Frames handed to the server since the stream opened, for the sink
    * rate estimate; the device's own count is this less whatever is
    * still queued. Written by the drain on the server's thread and
    * read by frames_consumed(), both under the mainloop lock. */
   uint64_t frames_written;
   unsigned rate;
   /* What the server last said, from its own thread, for the reads the
    * frontend makes every frame: the writable size, after each drain;
    * the sink's own latency behind the stream, in frames, from the
    * latency update. Read without the mainloop lock. Lowered before a
    * drain takes from the ring, so the room the writer works out from
    * it and the ring's fill errs short, never long. */
   retro_atomic_size_t writable_cached;
   retro_atomic_size_t sink_frames_cached;
   /* Times the server ran out of audio for this stream, from its own
    * underflow callback. One atomic add there, read by the frontend. */
   retro_atomic_size_t underruns;
   /* The sink's clock against the rate the stream was opened at,
    * fitted from the timing info the server already hands over:
    * read_index is what the sink has played, in bytes, and timestamp
    * is when that was true. A fit over every sample rather than two
    * points, because noise on a single anchor divides by the window
    * and reads as drift.
    *
    * Accumulated in the latency-update callback, on the mainloop's
    * thread, and published as one int in ppm. Nothing acts on it. */
   unsigned frame_bytes;
   uint64_t clk_anchor_pos;
   int64_t  clk_anchor_us;
   int      clk_have_anchor;
   double   clk_sx, clk_sy, clk_sxx, clk_sxy, clk_n;
   retro_atomic_int_t clk_ppm; /* AUDIO_CLOCK_PPM_NONE until known */
   /* Audio on its way to the server. write() fills it on the
    * frontend's thread with no lock; the server's thread moves it into
    * the stream, under the mainloop lock it already holds, from the
    * write callback and from the kick below. */
   retro_spsc_t ring;
   /* A blocking write parks here until a drain makes room or the
    * stream ends. */
   retro_eventcount_t room;
   /* 1 when a drain left the ring empty with the server still wanting
    * more. No write callback comes until the server consumes, so the
    * write that next fills the ring has to wake the server's thread
    * itself. Exchanged on both sides: a write landing as the drain
    * finishes either sees the flag or is seen by the drain's look
    * after it. */
   retro_atomic_int_t starved;
   /* That wake: a pipe the mainloop watches. A byte written to it is a
    * syscall, not a lock. */
   int kick_fd[2];
   pa_io_event *kick_ev;
   bool ring_ok;
   bool room_ok;
} pa_t;

/* The writer takes no lock. Every pa_* call must hold the mainloop
 * lock, so the frontend's thread makes none on the write path: it
 * fills the ring, and the server's thread - which holds that lock for
 * every callback anyway - moves the ring into the stream. A blocking
 * write parks on the eventcount every drain notifies. The mainloop's
 * own wait is kept for what is rare and has to call into the library
 * from the frontend's thread: connecting, cork and uncork, and the
 * latency query behind frames_consumed(). */

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

#define PULSE_READY(pa) (retro_atomic_load_acquire_int(&(pa)->ready) != 0)

/* Ends the stream for the writer: a parked write wakes and sees it. */
static void pulse_set_unready(pa_t *pa)
{
   retro_atomic_store_release_int(&pa->ready, 0);
   if (pa->room_ok)
      retro_eventcount_notify(&pa->room);
}

static void pulse_free(void *data)
{
   pa_t *pa = (pa_t*)data;

   if (!pa)
      return;

   if (pa->mainloop)
      pa_threaded_mainloop_stop(pa->mainloop);

   /* The mainloop's thread is gone: nothing drains or kicks now. */
   if (pa->kick_ev)
      pa_threaded_mainloop_get_api(pa->mainloop)->io_free(pa->kick_ev);
   if (pa->kick_fd[0] >= 0)
      close(pa->kick_fd[0]);
   if (pa->kick_fd[1] >= 0)
      close(pa->kick_fd[1]);

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

   if (pa->ring_ok)
      retro_spsc_free(&pa->ring);
   if (pa->room_ok)
      retro_eventcount_free(&pa->room);

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
         pulse_set_unready(pa);
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      case PA_CONTEXT_TERMINATED:
         pulse_set_unready(pa);
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
         retro_atomic_store_release_int(&pa->ready, 1);
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      case PA_STREAM_UNCONNECTED:
      case PA_STREAM_FAILED:
      case PA_STREAM_TERMINATED:
         pulse_set_unready(pa);
         pa_threaded_mainloop_signal(pa->mainloop, 0);
         break;
      default:
         break;
   }
}

/* On the server's thread, the mainloop lock held: moves whole frames
 * from the ring into the stream while both have them, then publishes
 * what the stream can still take and wakes a parked writer. */
static void pulse_drain(pa_t *pa)
{
   size_t fb = pa->frame_bytes;

   if (!pa->stream || !pa->ring_ok || !fb)
      return;

   for (;;)
   {
      size_t writable = pa_stream_writable_size(pa->stream);
      size_t queued   = retro_spsc_read_avail(&pa->ring);
      size_t want     = MIN(writable, queued);
      size_t n;
      void  *dst      = NULL;

      want -= want % fb;
      n     = want;
      if (n)
      {
         /* The server's buffer may come back smaller than asked. */
         if (pa_stream_begin_write(pa->stream, &dst, &n) < 0 || !dst)
            break;
         if (n > want)
            n = want;
         n -= n % fb;
         if (!n)
         {
            pa_stream_cancel_write(pa->stream);
            break;
         }
         /* Room the writer sees is the writable size less the ring's
          * fill: lowering the first before the second keeps it short. */
         retro_atomic_store_release_size(&pa->writable_cached,
               writable - n);
         retro_spsc_read(&pa->ring, dst, n);
         if (pa_stream_write(pa->stream, dst, n, NULL, 0,
                  PA_SEEK_RELATIVE) < 0)
            break;
         pa->frames_written += n / fb;
         continue;
      }

      retro_atomic_store_release_size(&pa->writable_cached, writable);
      if (!writable || queued >= fb)
         break;
      /* Dry, with room the server wants filled. Flag it, then look
       * once more: a write that landed before the flag is seen here,
       * one that lands after it sees the flag and kicks. */
      retro_atomic_exchange_int(&pa->starved, 1);
      if (retro_spsc_read_avail(&pa->ring) < fb)
         break;
      retro_atomic_exchange_int(&pa->starved, 0);
   }

   if (pa->room_ok)
      retro_eventcount_notify(&pa->room);
}

static void pulse_stream_request_cb(pa_stream *s, size_t len, void *data)
{
   pa_t *pa = (pa_t*)data;
   (void)s;
   (void)len;
   pulse_drain(pa);
}

/* The writer's wake, on the server's thread with the lock held. */
static void pulse_kick_cb(pa_mainloop_api *a, pa_io_event *e, int fd,
      pa_io_event_flags_t events, void *data)
{
   char sink[64];
   (void)a; (void)e; (void)events;
   while (read(fd, sink, sizeof(sink)) > 0) { }
   pulse_drain((pa_t*)data);
}

/* One (position, time) pair from the server's timing info into the
 * fit. read_index is the sink's play position in bytes; it can step
 * back on a rewind or a flush, which restarts the fit rather than
 * reading as an enormous negative drift. */
static void pulse_clock_sample(pa_t *pa, const pa_timing_info *ti)
{
   uint64_t pos;
   int64_t  us;
   double   x, y, denom;

   if (!pa->frame_bytes || ti->read_index < 0)
      return;

   pos = (uint64_t)ti->read_index / pa->frame_bytes;
   us  = (int64_t)ti->timestamp.tv_sec * 1000000
       + (int64_t)ti->timestamp.tv_usec;

   if (!pa->clk_have_anchor || pos < pa->clk_anchor_pos
         || us <= pa->clk_anchor_us)
   {
      pa->clk_anchor_pos  = pos;
      pa->clk_anchor_us   = us;
      pa->clk_have_anchor = 1;
      pa->clk_sx = pa->clk_sy = pa->clk_sxx = pa->clk_sxy = pa->clk_n = 0.0;
      retro_atomic_store_release_int(&pa->clk_ppm, AUDIO_CLOCK_PPM_NONE);
      return;
   }

   /* Seconds and frames from the anchor: a fit on the raw values
    * loses its answer to cancellation. */
   x = (double)(us - pa->clk_anchor_us) / 1000000.0;
   y = (double)(pos - pa->clk_anchor_pos);

   pa->clk_sx  += x;
   pa->clk_sy  += y;
   pa->clk_sxx += x * x;
   pa->clk_sxy += x * y;
   pa->clk_n   += 1.0;

   /* A second of window at least, as the interface says. */
   if (pa->clk_n < 4.0 || x < 1.0)
      return;

   denom = pa->clk_n * pa->clk_sxx - pa->clk_sx * pa->clk_sx;
   if (denom <= 0.0)
      return;

   {
      double slope = (pa->clk_n * pa->clk_sxy - pa->clk_sx * pa->clk_sy)
            / denom;
      double ppm   = (slope / (double)pa->rate - 1.0) * 1000000.0;
      if (ppm > -100000.0 && ppm < 100000.0)
      {
         retro_atomic_store_release_int(&pa->clk_ppm, (int)ppm);
      }
   }
}

static void pulse_stream_latency_update_cb(pa_stream *s, void *data)
{
   pa_t *pa = (pa_t*)data;
   /* On the mainloop thread, the lock held: the timing info is
    * current here. sink_usec is the part of the latency that is not
    * the server's queue; NULL until timing data has arrived. */
   const pa_timing_info *ti = pa_stream_get_timing_info(s);
   if (ti && pa->rate)
   {
      retro_atomic_store_release_size(&pa->sink_frames_cached,
            (size_t)((uint64_t)ti->sink_usec * pa->rate / 1000000));
      pulse_clock_sample(pa, ti);
   }
   pa_threaded_mainloop_signal(pa->mainloop, 0);
}

/* The server telling us this stream ran dry. Counted, not logged: it
 * arrives on the mainloop's thread while audio is playing, which is
 * the one place a log line costs what it is reporting on. */
static void pulse_underrun_update_cb(pa_stream *s, void *data)
{
   pa_t *pa = (pa_t*)data;

   (void)s;

   if (pa)
      retro_atomic_fetch_add_size(&pa->underruns, 1);
}

static void pulse_buffer_attr_cb(pa_stream *s, void *data)
{
   pa_t *pa = (pa_t*)data;
   const pa_buffer_attr *server_attr = pa_stream_get_buffer_attr(s);
   if (server_attr)
   {
      retro_atomic_store_release_size(&pa->buffer_size, server_attr->tlength);
      retro_atomic_store_release_size(&pa->minreq, server_attr->minreq);
   }
}

/* A channel map of the layout's positions in the mask's ascending-bit
 * order, which is the order the frames carry. A count never decides
 * a position: the two six-channel layouts differ in the rear pair and
 * each is given as itself. */
static void pulse_channel_map(uint32_t layout, pa_channel_map *map)
{
   static const struct { uint32_t bit; pa_channel_position_t pos; } table[] = {
      { AUDIO_SPEAKER_FRONT_LEFT,            PA_CHANNEL_POSITION_FRONT_LEFT            },
      { AUDIO_SPEAKER_FRONT_RIGHT,           PA_CHANNEL_POSITION_FRONT_RIGHT           },
      { AUDIO_SPEAKER_FRONT_CENTER,          PA_CHANNEL_POSITION_FRONT_CENTER          },
      { AUDIO_SPEAKER_LOW_FREQUENCY,         PA_CHANNEL_POSITION_LFE                   },
      { AUDIO_SPEAKER_BACK_LEFT,             PA_CHANNEL_POSITION_REAR_LEFT             },
      { AUDIO_SPEAKER_BACK_RIGHT,            PA_CHANNEL_POSITION_REAR_RIGHT            },
      { AUDIO_SPEAKER_FRONT_LEFT_OF_CENTER,  PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER  },
      { AUDIO_SPEAKER_FRONT_RIGHT_OF_CENTER, PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER },
      { AUDIO_SPEAKER_BACK_CENTER,           PA_CHANNEL_POSITION_REAR_CENTER           },
      { AUDIO_SPEAKER_SIDE_LEFT,             PA_CHANNEL_POSITION_SIDE_LEFT             },
      { AUDIO_SPEAKER_SIDE_RIGHT,            PA_CHANNEL_POSITION_SIDE_RIGHT            },
   };
   size_t i;
   pa_channel_map_init(map);
   for (i = 0; i < ARRAY_SIZE(table); i++)
      if (layout & table[i].bit)
         map->map[map->channels++] = table[i].pos;
}

static void *pulse_init(const char *device, unsigned rate,
      unsigned latency, 
      unsigned *new_rate)
{
   pa_sample_spec spec;
   pa_channel_map map;
   pa_buffer_attr        buffer_attr = {0};
   const pa_buffer_attr *server_attr = NULL;
   pa_t                          *pa = (pa_t*)calloc(1, sizeof(*pa));

   if (!pa)
      return NULL;
   pa->kick_fd[0] = pa->kick_fd[1] = -1;
   retro_atomic_int_init(&pa->ready, 0);
   retro_atomic_int_init(&pa->starved, 0);
   retro_atomic_size_init(&pa->buffer_size, 0);
   retro_atomic_size_init(&pa->minreq, 0);
   retro_atomic_size_init(&pa->writable_cached, 0);
   retro_atomic_size_init(&pa->sink_frames_cached, 0);
   retro_atomic_size_init(&pa->underruns, 0);
   retro_atomic_int_init(&pa->clk_ppm, AUDIO_CLOCK_PPM_NONE);

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

   /* The layout the frontend asked for, as a channel map of its
    * positions in the mask's order: the server routes each to the
    * sink's channel of that position, or remixes where the sink
    * lacks one, so what is asked is what is carried. */
   pa->layout    = audio_driver_requested_layout();
   spec.format   = is_little_endian() ? PA_SAMPLE_FLOAT32LE : PA_SAMPLE_FLOAT32BE;
   spec.channels = (uint8_t)audio_layout_channels(pa->layout);
   spec.rate     = rate;
   pulse_channel_map(pa->layout, &map);
   pa->rate        = rate;
   pa->frame_bytes = (unsigned)pa_frame_size(&spec);

   pa->stream    = pa_stream_new(pa->context, "audio", &spec, &map);
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

   /* The ring is in place before the stream can ask for audio. It only
    * ever holds what the stream has room for, so twice the requested
    * buffer covers a server that grants more than was asked. */
   if (!(pa->ring_ok = retro_spsc_init(&pa->ring,
               (size_t)buffer_attr.tlength * 2)))
      goto unlock_error;
   if (!(pa->room_ok = retro_eventcount_init(&pa->room)))
      goto unlock_error;
   if (pipe(pa->kick_fd) < 0)
   {
      pa->kick_fd[0] = pa->kick_fd[1] = -1;
      goto unlock_error;
   }
   fcntl(pa->kick_fd[0], F_SETFL, fcntl(pa->kick_fd[0], F_GETFL) | O_NONBLOCK);
   fcntl(pa->kick_fd[1], F_SETFL, fcntl(pa->kick_fd[1], F_GETFL) | O_NONBLOCK);
   fcntl(pa->kick_fd[0], F_SETFD, FD_CLOEXEC);
   fcntl(pa->kick_fd[1], F_SETFD, FD_CLOEXEC);
   if (!(pa->kick_ev = pa_threaded_mainloop_get_api(pa->mainloop)->io_new(
               pa_threaded_mainloop_get_api(pa->mainloop), pa->kick_fd[0],
               PA_IO_EVENT_INPUT, pulse_kick_cb, pa)))
      goto unlock_error;

   if (pa_stream_connect_playback(pa->stream, NULL,
            &buffer_attr, PA_STREAM_ADJUST_LATENCY, NULL, NULL) < 0)
      goto unlock_error;

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
      retro_atomic_store_release_size(&pa->buffer_size, server_attr->tlength);
      retro_atomic_store_release_size(&pa->minreq, server_attr->minreq);
      RARCH_LOG("[PulseAudio] Requested %u bytes buffer, got %u.\n",
            (unsigned)buffer_attr.tlength,
            (unsigned)server_attr->tlength);
   }
   else
   {
      retro_atomic_store_release_size(&pa->buffer_size, buffer_attr.tlength);
      retro_atomic_store_release_size(&pa->minreq, buffer_attr.tlength / 4);
   }

   /* Seeded here, under the lock; every drain keeps it. */
   retro_atomic_store_release_size(&pa->writable_cached,
         pa_stream_writable_size(pa->stream));
   retro_atomic_store_release_size(&pa->sink_frames_cached, 0);
   retro_atomic_store_release_int(&pa->ready, 1);
   pa_threaded_mainloop_unlock(pa->mainloop);

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

   if (!PULSE_READY(pa))
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

/* What the writer may add: what the stream can take less what the
 * ring already holds for it, in whole frames, and never more than the
 * ring has free. The frontend sees the server's buffer as before; the
 * ring only carries it across. */
static size_t pulse_room(pa_t *pa)
{
   size_t fb       = pa->frame_bytes;
   size_t free_now = retro_spsc_write_avail(&pa->ring);
   size_t used     = pa->ring.capacity - free_now;
   size_t writable = retro_atomic_load_acquire_size(&pa->writable_cached);
   size_t room     = writable > used ? writable - used : 0;

   if (room > free_now)
      room = free_now;
   return room - room % fb;
}

/* Parks until the room reaches want or the stream ends, bounded: a
 * server that has stopped consuming drains nothing, and the bound
 * hands the call back rather than holding the thread on it. */
static bool pulse_park(pa_t *pa, size_t want, unsigned ms)
{
   int key = retro_eventcount_prepare_wait(&pa->room);
   if (pulse_room(pa) >= want || !PULSE_READY(pa))
   {
      retro_eventcount_cancel_wait(&pa->room);
      return true;
   }
   return retro_eventcount_commit_wait_timeout(&pa->room, key,
         (int64_t)ms * 1000);
}

/* The server's thread drained the ring dry and is waiting on no
 * callback: wake it. A full pipe means a wake is already pending. */
static void pulse_kick(pa_t *pa)
{
   if (retro_atomic_exchange_int(&pa->starved, 0))
   {
      char c = 0;
      while (write(pa->kick_fd[1], &c, 1) < 0 && errno == EINTR) { }
   }
}

static void pulse_push(pa_t *pa, const void *s, size_t len)
{
   retro_spsc_write(&pa->ring, s, len);
   pulse_kick(pa);
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

   len -= len % pa->frame_bytes;
   while (len && PULSE_READY(pa))
   {
      /* Once: MIN() would evaluate it twice, and the room can grow
       * between the two. */
      size_t room = pulse_room(pa);
      size_t n    = MIN(len, room);

      if (n)
      {
         pulse_push(pa, buf, n);
         buf  += n;
         len  -= n;
         _len += n;
      }
      else if (pa->nonblock || !pulse_park(pa, pa->frame_bytes,
               PULSE_WRITE_WAIT_MS))
         break;
   }

   return _len;
}

/* The lend pair: the ring the server's thread drains, handed out span
 * by span; a stream that is paused or not ready refuses, and write()
 * restarts it as before. */
static size_t pulse_write_begin(void *data, size_t len, void **region)
{
   pa_t  *pa   = (pa_t*)data;
   void  *ptr  = NULL;
   size_t room, span;
   *region     = NULL;
   if (!pa || !pa->ring_ok || pa->is_paused || !PULSE_READY(pa))
      return 0;
   room = pulse_room(pa);
   span = retro_spsc_write_begin(&pa->ring, &ptr);
   if (span > room)
      span = room;
   if (span > len)
      span = len;
   span -= span % pa->frame_bytes;
   if (!span || !ptr)
   {
      retro_spsc_write_end(&pa->ring, 0);
      return 0;
   }
   *region = ptr;
   return span;
}

static ssize_t pulse_write_end(void *data, size_t len)
{
   pa_t *pa = (pa_t*)data;
   len     -= len % pa->frame_bytes;
   retro_spsc_write_end(&pa->ring, len);
   if (len)
      pulse_kick(pa);
   return (ssize_t)len;
}

static bool pulse_stop(void *data)
{
   bool ret;
   pa_operation *op;
   pa_t *pa = (pa_t*)data;

   if (!PULSE_READY(pa))
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

   if (!pa || !PULSE_READY(pa))
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

static uint32_t pulse_layout(void *data)
{
   pa_t *pa = (pa_t*)data;
   return pa ? pa->layout : AUDIO_LAYOUT_STEREO;
}

/* Read every frame by the frontend; served from what the server's
 * thread last said, with no lock. See writable_cached. */
static size_t pulse_write_avail(void *data)
{
   pa_t *pa = (pa_t*)data;
   size_t sink;

   if (!PULSE_READY(pa))
      return 0;

   /* Can change spuriously. */
   audio_driver_set_buffer_size(
         retro_atomic_load_acquire_size(&pa->buffer_size));
   sink = retro_atomic_load_acquire_size(&pa->sink_frames_cached);
   if (sink)
      audio_driver_set_device_latency(sink);
   return pulse_room(pa);
}

static size_t pulse_buffer_size(void *data)
{
   pa_t *pa = (pa_t*)data;
   return retro_atomic_load_acquire_size(&pa->buffer_size);
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

   if (!pa || !PULSE_READY(pa) || !pa->rate)
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

/* Park until at least len bytes may be written. Every drain notifies,
 * and the server drains as it consumes, so this wakes when room exists
 * and never on a timer; the bound is a watchdog for a server that has
 * stopped. */
static size_t pulse_wait_writable(void *data, size_t len)
{
   size_t _len   = 0;
   pa_t *pa      = (pa_t*)data;
   size_t minreq = retro_atomic_load_acquire_size(&pa->minreq);

   if (!PULSE_READY(pa))
      return 0;

   /* Enough for the server's next request is enough: the write loops
    * on writable space as the server frees it, and before the stream
    * has started playing (prebuf) that partial write is the only way
    * to get it full. */
   if (minreq && len > minreq)
      len = minreq;
   if (len < pa->frame_bytes)
      len = pa->frame_bytes;

   while (PULSE_READY(pa))
   {
      _len = pulse_room(pa);
      if (_len >= len)
         break;
      /* The bound hands the pass back as no space coming from this
       * call. */
      if (!pulse_park(pa, len, PULSE_WRITE_WAIT_MS))
      {
         _len = 0;
         break;
      }
   }
   if (!PULSE_READY(pa))
      _len = 0;
   audio_driver_set_buffer_size(
         retro_atomic_load_acquire_size(&pa->buffer_size));
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

static size_t pulse_underruns(void *data)
{
   pa_t *pa = (pa_t*)data;
   return pa ? retro_atomic_load_acquire_size(&pa->underruns) : 0;
}

static bool pulse_device_clock_ppm(void *data, double *ppm)
{
   pa_t *pa = (pa_t*)data;
   int v;
   if (!pa)
      return false;
   v = retro_atomic_load_acquire_int(&pa->clk_ppm);
   if (v == AUDIO_CLOCK_PPM_NONE)
      return false;
   *ppm = (double)v;
   return true;
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
   pulse_frames_consumed,
   pulse_underruns,
   pulse_layout,
   NULL, /* frames_consumed_fallback */
   pulse_device_clock_ppm,
   NULL, /* thread_grant */
   pulse_write_begin,
   pulse_write_end
};
