/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025      - OlyB
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <boolean.h>
#include <retro_atomic.h>

#include <emscripten/wasm_worker.h>
#include <emscripten/webaudio.h>
#include <emscripten/atomic.h>
#include "../../frontend/drivers/platform_emscripten.h"

#include <retro_spsc.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#define WORKLET_STACK_SIZE 4096

/* Builds whose writer parks on the worklet's pulls for room: the
 * threaded one on its own thread, the asyncify ones on the browser
 * thread. The fake-block build goes back to the browser instead. */
#if defined(PROXY_TO_PTHREAD) || defined(EMSCRIPTEN_FULL_ASYNCIFY) \
   || defined(EMSCRIPTEN_AUDIO_ASYNC_BLOCK)
#define AUDIOWORKLET_PARKS 1
#endif

/* additional buffer size (for EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK only) */
/* if this is too small, frames may be dropped and content could run too fast. */
/* very large slow-motion rate values may be too large for this; avoid anything higher than 6 or 7. */
#define EXTERNAL_BLOCK_BUFFER_MS 128

/* One ring of outgoing samples.  A ring the driver lets go of while
 * the worklet may still be reading it waits on a list until it is not;
 * see audioworklet_retire(). */
typedef struct audioworklet_ring
{
   retro_spsc_t q;
   struct audioworklet_ring *next;
} audioworklet_ring_t;

/* Init, as the worklet's callbacks report it. */
#define AUDIOWORKLET_INIT_PENDING 0
#define AUDIOWORKLET_INIT_DONE    1
#define AUDIOWORKLET_INIT_ERROR   2

typedef struct audioworklet_data
{
   uint8_t *worklet_stack;
   /* Outgoing samples: main thread writes, the worklet pulls.
    * retro_spsc rounds capacity up to a power of two; ring_size is the
    * size asked for (visible buffer plus the external-block reserve)
    * and the producer never fills past it, so room is what the fifo
    * of that size reported.  NULL: no ring, and the driver does not
    * come up.  The producer's own pointer; the worklet reads
    * pull_ring. */
   audioworklet_ring_t *ring;
   /* Rings let go of while a pull was under way; freed once
    * pull_seq has moved past retired_seq. */
   audioworklet_ring_t *retired;
   /* The ring the worklet pulls from, NULL while the driver is down.
    * The worklet counts each pull in and out on pull_seq (odd while
    * one is under way), and both sides fence between that and this,
    * so a ring taken down is either seen by no pull or seen under a
    * pull_seq the free reads as odd. */
   retro_atomic_ptr_t pull_ring;
   size_t visible_buffer_size;
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   size_t write_avail_diff;
#endif
   size_t ring_size;
   EMSCRIPTEN_WEBAUDIO_T context;
   /* Quanta the worklet had to fill with silence because the ring came
    * up short, or because it could not get at the ring at all. Counted
    * here and not said: this is the render callback, and a line from
    * it costs part of the quantum it is reporting on. */
   retro_atomic_size_t underruns;
   /* Frames a write could not get into the ring and gave up on. Counted
    * on the write and logged as one total when the driver is freed: a
    * line per write is a line per frame, on the audio path. */
   retro_atomic_size_t dropped;
   retro_atomic_int_t pull_seq;
   int retired_seq;
   /* AUDIOWORKLET_INIT_*, stored by the init callbacks on the browser
    * thread and notified there; the threaded build's init waits on it. */
   uint32_t init_state;
#ifdef AUDIOWORKLET_PARKS
   /* Bumped by the worklet after every pull; a writer out of room
    * parks on it, with waiting set so the worklet notifies only then. */
   uint32_t wake_seq;
   uint32_t waiting;
#endif
   unsigned rate;
   unsigned latency;
   bool nonblock;
   bool initing;
#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
   bool block_requested;
#endif
   volatile bool running;         /* currently only used by RetroArch */
   volatile bool driver_running;  /* whether the driver is running (buffer allocated) */
   volatile bool context_running; /* whether the AudioContext is running */
} audioworklet_data_t;

/* We only ever want to create 1 worklet, so we need to keep its data even if the driver is inactive. */
static audioworklet_data_t *audioworklet_static_data = NULL;

/* Room in the ring against the size asked for.  Producer-side query -
 * write(), write_avail() and the external-block waits all run on the
 * main thread - so it is exact for the caller: the worklet only ever
 * frees space, never takes it. */
static size_t audioworklet_room(const audioworklet_data_t *audioworklet)
{
   size_t room, excess;
   if (!audioworklet->ring)
      return 0;
   room   = retro_spsc_write_avail(&audioworklet->ring->q);
   excess = audioworklet->ring->q.capacity - audioworklet->ring_size;
   return room > excess ? room - excess : 0;
}

/* Interleaved stereo from the ring straight into the planar output. */
static void audioworklet_deinterleave(float *left, float *right,
      const float *src, unsigned frames)
{
   unsigned i;
   for (i = 0; i < frames; i++)
   {
      left[i]  = src[i * 2];
      right[i] = src[i * 2 + 1];
   }
}

/* Note that we cannot allocate any heap in here. Nor lock, wait or
 * spin: this is the browser's render thread. */
static bool audioworklet_process_cb(int numInputs, const AudioSampleFrame *inputs,
   int numOutputs, AudioSampleFrame *outputs,
   int numParams, const AudioParamFrame *params,
   void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   unsigned frames         = outputs[0].samplesPerChannel;
   unsigned writing_frames = 0;
   float *left             = outputs[0].data;
   float *right            = outputs[0].data + frames;
   audioworklet_ring_t *ring;

   /* TODO: do we need to pay attention to audioworklet->running here too? */
   retro_atomic_fetch_add_seq_cst_int(&audioworklet->pull_seq, 1);
   retro_atomic_thread_fence_seq_cst();
   ring = (audioworklet_ring_t*)retro_atomic_load_acquire_ptr(
         &audioworklet->pull_ring);
   if (ring)
   {
      const void *first, *second;
      size_t first_bytes;
      size_t got = retro_spsc_read_begin_pair(&ring->q, &first,
            &first_bytes, &second, frames * 2 * sizeof(float));
      /* Whole frames: every write is, and the ring's power-of-two
       * capacity puts its wrap on a frame. */
      unsigned first_frames = (unsigned)(first_bytes / (2 * sizeof(float)));
      writing_frames        = (unsigned)(got / (2 * sizeof(float)));
      audioworklet_deinterleave(left, right, (const float*)first,
            first_frames);
      audioworklet_deinterleave(left + first_frames, right + first_frames,
            (const float*)second, writing_frames - first_frames);
      retro_spsc_read_end(&ring->q, writing_frames * 2 * sizeof(float));
   }
   retro_atomic_fetch_add_seq_cst_int(&audioworklet->pull_seq, 1);

   if (ring)
   {
#ifdef AUDIOWORKLET_PARKS
      emscripten_atomic_add_u32(&audioworklet->wake_seq, 1);
      if (emscripten_atomic_load_u32(&audioworklet->waiting))
         emscripten_atomic_notify(&audioworklet->wake_seq, 1);
#endif
      /* Only where audio was expected: a driver that is not running is
       * not being starved. */
      if (writing_frames < frames)
         retro_atomic_fetch_add_size(&audioworklet->underruns, 1);
   }

   if (writing_frames < frames)
   {
      unsigned zero_frames = frames - writing_frames;
      memset(left  + writing_frames, 0, zero_frames * sizeof(float));
      memset(right + writing_frames, 0, zero_frames * sizeof(float));
   }

   return true;
}

/* The init callbacks run on the browser thread. The threaded build
 * waits for them on init_state; the asyncify builds await the promise
 * audioworklet_init_wait_js() left. */
static void audioworklet_init_report(audioworklet_data_t *audioworklet,
      uint32_t state)
{
   emscripten_atomic_store_u32(&audioworklet->init_state, state);
#ifdef PROXY_TO_PTHREAD
   emscripten_atomic_notify(&audioworklet->init_state,
         EMSCRIPTEN_NOTIFY_ALL_WAITERS);
#elif defined(EMSCRIPTEN_ASYNCIFY)
   EM_ASM({
      if (Module.rarchAudioWorkletInitWake)
         Module.rarchAudioWorkletInitWake();
   });
#endif
}

static void audioworklet_processor_inited_cb(EMSCRIPTEN_WEBAUDIO_T context, bool success, void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   int outputChannelCounts[1] = { 2 };
   EmscriptenAudioWorkletNodeCreateOptions opts = { 0, 1, outputChannelCounts };
   EMSCRIPTEN_AUDIO_WORKLET_NODE_T worklet_node;

   if (!success)
   {
      RARCH_ERR("[AudioWorklet] Failed to init AudioWorkletProcessor.\n");
      audioworklet_init_report(audioworklet, AUDIOWORKLET_INIT_ERROR);
      return;
   }

   worklet_node = emscripten_create_wasm_audio_worklet_node(context, "retroarch", &opts, audioworklet_process_cb, audioworklet);
   emscripten_audio_node_connect(worklet_node, context, 0, 0);

   audioworklet_init_report(audioworklet, AUDIOWORKLET_INIT_DONE);
}

static void audioworklet_thread_inited_cb(EMSCRIPTEN_WEBAUDIO_T context, bool success, void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   WebAudioWorkletProcessorCreateOptions opts = { "retroarch", 0 };

   if (success)
      emscripten_create_wasm_audio_worklet_processor_async(context, &opts,
            audioworklet_processor_inited_cb, audioworklet);
   else
   {
      RARCH_ERR("[AudioWorklet] Failed to init worklet thread! Is the worklet file in the right place?\n");
      audioworklet_init_report(audioworklet, AUDIOWORKLET_INIT_ERROR);
   }
}

static void audioworklet_ctx_statechange_cb(void *data, bool state)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   audioworklet->context_running = state;
}

static void audioworklet_ctx_create(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   audioworklet->context = emscripten_create_audio_context(0);
   audioworklet->rate = EM_ASM_INT({
      return emscriptenGetAudioObject($0).sampleRate;
   }, audioworklet->context);
   audioworklet->context_running = EM_ASM_INT({
      let ac = emscriptenGetAudioObject($0);
      ac.addEventListener("statechange", function() {
         getWasmTableEntry($2)($1, ac.state == "running");
      });
      return ac.state == "running";
   }, audioworklet->context, audioworklet, audioworklet_ctx_statechange_cb);

   emscripten_start_wasm_audio_worklet_thread_async(audioworklet->context,
      audioworklet->worklet_stack, WORKLET_STACK_SIZE, audioworklet_thread_inited_cb, audioworklet);
}

static void audioworklet_alloc_buffer(void *data)
{
   size_t buffer_size;
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   audioworklet->visible_buffer_size = (audioworklet->latency * audioworklet->rate * 2 * sizeof(float)) / 1000;
   buffer_size = audioworklet->visible_buffer_size;
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   audioworklet->write_avail_diff = (EXTERNAL_BLOCK_BUFFER_MS * audioworklet->rate * 2 * sizeof(float)) / 1000;
   buffer_size += audioworklet->write_avail_diff;
#endif
   audioworklet->ring_size = buffer_size;
   audioworklet->ring      = (audioworklet_ring_t*)calloc(1, sizeof(*audioworklet->ring));
   if (audioworklet->ring && !retro_spsc_init(&audioworklet->ring->q, buffer_size))
   {
      free(audioworklet->ring);
      audioworklet->ring = NULL;
   }
   if (!audioworklet->ring)
      RARCH_ERR("[AudioWorklet] Failed to allocate the %lu-byte buffer.\n",
            (unsigned long)buffer_size);
   RARCH_LOG("[AudioWorklet] Buffer size: %lu bytes.\n", audioworklet->visible_buffer_size);
}

static void audioworklet_ring_free(audioworklet_ring_t *ring)
{
   while (ring)
   {
      audioworklet_ring_t *next = ring->next;
      retro_spsc_free(&ring->q);
      free(ring);
      ring = next;
   }
}

/* Frees the rings let go of during a pull, once that pull is over. Any
 * pull after it found pull_ring NULL, or a newer ring. */
static void audioworklet_retired_drain(audioworklet_data_t *audioworklet)
{
   if (     audioworklet->retired
         && retro_atomic_load_seq_cst_int(&audioworklet->pull_seq)
            != audioworklet->retired_seq)
   {
      audioworklet_ring_free(audioworklet->retired);
      audioworklet->retired = NULL;
   }
}

/* The ring goes to the worklet: driver up. */
static void audioworklet_publish(audioworklet_data_t *audioworklet)
{
   audioworklet->driver_running = true;
   retro_atomic_store_release_ptr(&audioworklet->pull_ring,
         audioworklet->ring);
}

/* The ring is taken from the worklet: driver down. Freed now if no pull
 * is under way, else once the one that is has finished - nobody waits
 * for it. */
static void audioworklet_retire(audioworklet_data_t *audioworklet)
{
   int seq;
   audioworklet_ring_t *ring = audioworklet->ring;
   audioworklet->driver_running = false;
   audioworklet->ring           = NULL;
   (void)retro_atomic_exchange_ptr(&audioworklet->pull_ring, NULL);
   retro_atomic_thread_fence_seq_cst();
   seq = retro_atomic_load_seq_cst_int(&audioworklet->pull_seq);
   audioworklet_retired_drain(audioworklet);
   if (!ring)
      return;
   if (!(seq & 1))
   {
      audioworklet_ring_free(ring);
      return;
   }
   ring->next             = audioworklet->retired;
   audioworklet->retired  = ring;
   audioworklet->retired_seq = seq;
}

static void audioworklet_init_error(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   RARCH_ERR("[AudioWorklet] Failed to initialize driver.\n");
   audioworklet_static_data = NULL;
   audioworklet_ring_free(audioworklet->ring);
   audioworklet->ring = NULL;
   /* A worklet that never reported may still: its callbacks hold this,
    * so it stays. One that reported is done with it. */
   if (emscripten_atomic_load_u32(&audioworklet->init_state)
         == AUDIOWORKLET_INIT_PENDING)
      return;
   free(audioworklet->worklet_stack);
   free(audioworklet);
}

static bool audioworklet_resume_ctx(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   if (!audioworklet->context_running)
   {
      MAIN_THREAD_ASYNC_EM_ASM({
         emscriptenGetAudioObject($0).resume();
      }, audioworklet->context);
   }
   return audioworklet->context_running;
}

/* Bounds. A wait for room gives up after this many park watchdogs in a
 * row with no pull; init waits this long for the worklet module. */
#define AUDIOWORKLET_WAIT_LAPS       8
#ifndef AUDIOWORKLET_INIT_WAIT_MS
#define AUDIOWORKLET_INIT_WAIT_MS    30000
#endif
#ifndef AUDIOWORKLET_PARK_WAIT_NS
#define AUDIOWORKLET_PARK_WAIT_NS    3000000
#endif

#ifdef AUDIOWORKLET_PARKS
#ifndef PROXY_TO_PTHREAD
/* The browser thread cannot Atomics.wait; it awaits the same notify.
 * Returns whether the watchdog ended it. */
EM_ASYNC_JS(int, audioworklet_park_js, (uint32_t *seq, uint32_t key, int ms), {
   var r = Atomics.waitAsync(HEAP32, seq >> 2, key | 0, ms);
   return (r.async ? await r.value : r.value) == 'timed-out';
});
#endif

/* Parks until the worklet's next pull, while there is less room than
 * @need. waiting goes up before the key is read and the room checked
 * again, so a pull that frees room either shows in that check or
 * notifies; the timeout is a watchdog. Returns false when only the
 * watchdog ended it: callers spend their laps on those alone, since a
 * pull freed room and the browser runs its quanta in bursts, so a gap
 * between them is not the context stopping. */
static bool audioworklet_park(audioworklet_data_t *audioworklet, size_t need)
{
   uint32_t key;
   bool pulled = true;
   emscripten_atomic_store_u32(&audioworklet->waiting, 1);
   key = emscripten_atomic_load_u32(&audioworklet->wake_seq);
   if (audioworklet_room(audioworklet) < need)
#ifdef PROXY_TO_PTHREAD
      pulled = emscripten_atomic_wait_u32(&audioworklet->wake_seq, key,
            AUDIOWORKLET_PARK_WAIT_NS) != ATOMICS_WAIT_TIMED_OUT;
#else
      pulled = !audioworklet_park_js(&audioworklet->wake_seq, key,
            (int)(AUDIOWORKLET_PARK_WAIT_NS / 1000000));
#endif
   emscripten_atomic_store_u32(&audioworklet->waiting, 0);
   return pulled;
}
#endif

#if defined(EMSCRIPTEN_ASYNCIFY) && !defined(PROXY_TO_PTHREAD)
/* The asyncify builds wait on the browser thread, where the init
 * callbacks run: so the wait yields to it and resumes when one of them
 * wakes it. The timeout is a watchdog. */
EM_ASYNC_JS(void, audioworklet_init_wait_js, (uint32_t *state, int ms), {
   await new Promise(function(resolve) {
      var timer;
      if (Atomics.load(HEAPU32, state >> 2))
      {
         resolve();
         return;
      }
      Module.rarchAudioWorkletInitWake = function() {
         clearTimeout(timer);
         Module.rarchAudioWorkletInitWake = null;
         resolve();
      };
      timer = setTimeout(Module.rarchAudioWorkletInitWake, ms);
   });
});
#endif

/* Until the worklet reports, or the watchdog runs out: a module that
 * never loads and never errors is an init failure, not a frontend that
 * never starts. Returns the state it ended in. */
#if !defined(EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK) || defined(EMSCRIPTEN_AUDIO_ASYNC_BLOCK)
static uint32_t audioworklet_init_wait(audioworklet_data_t *audioworklet)
{
   uint32_t state;
#if defined(PROXY_TO_PTHREAD)
   /* The callbacks run on the browser thread, not this one, and store
    * init_state before they notify it, so one wait sees it change. */
   emscripten_atomic_wait_u32(&audioworklet->init_state,
         AUDIOWORKLET_INIT_PENDING,
         (int64_t)AUDIOWORKLET_INIT_WAIT_MS * 1000000);
#elif defined(EMSCRIPTEN_ASYNCIFY)
   audioworklet_init_wait_js(&audioworklet->init_state, AUDIOWORKLET_INIT_WAIT_MS);
#endif
   state = emscripten_atomic_load_u32(&audioworklet->init_state);
   if (state == AUDIOWORKLET_INIT_PENDING)
      RARCH_ERR("[AudioWorklet] The worklet did not initialize within %u ms.\n",
            AUDIOWORKLET_INIT_WAIT_MS);
   return state;
}
#endif

static void *audioworklet_init(const char *device, unsigned rate,
   unsigned latency,  unsigned *new_rate)
{
   audioworklet_data_t *audioworklet;
   if (audioworklet_static_data)
   {
      if (audioworklet_static_data->driver_running || audioworklet_static_data->initing)
      {
         RARCH_ERR("[AudioWorklet] Tried to start already running driver.\n");
         return NULL;
      }
      RARCH_LOG("[AudioWorklet] Reusing old context.\n");
      audioworklet          = audioworklet_static_data;
      audioworklet->latency = latency;
      *new_rate             = audioworklet->rate;
      RARCH_LOG("[AudioWorklet] Device rate: %d Hz.\n", *new_rate);
      audioworklet_retired_drain(audioworklet);
      audioworklet_alloc_buffer(audioworklet);
      if (!audioworklet->ring)
         return NULL;
      audioworklet_resume_ctx(audioworklet);
      audioworklet_publish(audioworklet);
      return audioworklet;
   }

   audioworklet = (audioworklet_data_t*)calloc(1, sizeof(audioworklet_data_t));
   if (!audioworklet)
      return NULL;
   audioworklet->worklet_stack = memalign(16, WORKLET_STACK_SIZE);
   if (!audioworklet->worklet_stack)
   {
      free(audioworklet);
      return NULL;
   }
   audioworklet_static_data = audioworklet;

   audioworklet->latency = latency;
   platform_emscripten_run_on_browser_thread_sync(audioworklet_ctx_create, audioworklet);
   *new_rate             = audioworklet->rate;
   RARCH_LOG("[AudioWorklet] Device rate: %d Hz.\n", *new_rate);
   audioworklet->initing = true;
   audioworklet_alloc_buffer(audioworklet);
   retro_atomic_size_init(&audioworklet->underruns, 0);
   retro_atomic_size_init(&audioworklet->dropped, 0);

#ifndef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   /* TODO: can MIN_ASYNCIFY block here too? */
   {
      uint32_t state = audioworklet_init_wait(audioworklet);
      audioworklet->initing = false;
      /* No ring, no driver: the same exit as a worklet that failed. */
      if (state != AUDIOWORKLET_INIT_DONE || !audioworklet->ring)
      {
         audioworklet_init_error(audioworklet);
         return NULL;
      }
   }
   audioworklet_publish(audioworklet);
#elif defined(EMSCRIPTEN_AUDIO_FAKE_BLOCK)
   audioworklet->block_requested = true;
   platform_emscripten_enter_fake_block(1);
#endif
   /* external block: will be handled later */

   return audioworklet;
}

static size_t audioworklet_write_begin(void *data, size_t len, void **region)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   void  *ptr = NULL;
   size_t max_write, span;
   *region    = NULL;
   if (!audioworklet->driver_running || !audioworklet_resume_ctx(audioworklet))
      return 0;
   max_write = audioworklet_room(audioworklet);
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   if (audioworklet->nonblock)
   {
      if (max_write > audioworklet->write_avail_diff)
         max_write -= audioworklet->write_avail_diff;
      else
         max_write = 0;
   }
#endif
   span = retro_spsc_write_begin(&audioworklet->ring->q, &ptr);
   if (span > max_write)
      span = max_write;
   if (span > len)
      span = len;
   span -= span % (2 * sizeof(float));
   if (!span || !ptr)
   {
      retro_spsc_write_end(&audioworklet->ring->q, 0);
      return 0;
   }
   *region = ptr;
   return span;
}

static ssize_t audioworklet_write_end(void *data, size_t len)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   len -= len % (2 * sizeof(float));
   retro_spsc_write_end(&audioworklet->ring->q, len);
#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
   if (audioworklet_room(audioworklet) < audioworklet->write_avail_diff)
   {
      audioworklet->block_requested = true;
      platform_emscripten_enter_fake_block(1);
   }
#endif
   return (ssize_t)len;
}

static ssize_t audioworklet_write(void *data, const void *s, size_t ss)
{
   size_t avail;
   size_t max_write;
   size_t to_write_frames;
   size_t to_write_bytes;
   size_t _len          = 0;
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   const float *samples = (const float*)s;
   size_t num_frames    = ss / 2 / sizeof(float);
#ifndef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   int laps             = AUDIOWORKLET_WAIT_LAPS;
#endif

   /* too early! might happen with external blocking */
   if (!audioworklet->driver_running)
      return 0;

   /* don't write audio if the context isn't running, just try to start it */
   if (!audioworklet_resume_ctx(audioworklet))
      return 0;

   while (num_frames)
   {
      /* No lock: SPSC, and this thread is the producer. */
      avail = audioworklet_room(audioworklet);
      max_write = avail;
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
      /* make sure we don't write into the blocking buffer for nonblock */
      if (audioworklet->nonblock)
      {
         if (max_write > audioworklet->write_avail_diff)
            max_write -= audioworklet->write_avail_diff;
         else
            max_write = 0;
      }
#endif
      to_write_frames = MIN(num_frames, max_write / 2 / sizeof(float));
      if (to_write_frames)
      {
         to_write_bytes = to_write_frames * 2 * sizeof(float);
         avail      -= to_write_bytes;
         retro_spsc_write(&audioworklet->ring->q, samples, to_write_bytes);
         num_frames -= to_write_frames;
         samples    += (to_write_frames * 2);
         _len       += to_write_bytes;
      }

#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
      /* see if we're over the threshold to go to fake block */
      if (avail < audioworklet->write_avail_diff)
      {
         audioworklet->block_requested = true;
         platform_emscripten_enter_fake_block(1);
      }
#endif
      if (num_frames && !audioworklet->nonblock)
         retro_atomic_fetch_add_size(&audioworklet->dropped, num_frames);
      break;
#else
      if (audioworklet->nonblock || !num_frames)
         break;
      /* Bounded: a context that will not run - suspended by the
       * autoplay policy until the page is clicked, or by the browser
       * for a background tab - frees nothing, and this returns what
       * went rather than holding the thread. */
      if (--laps < 0)
      {
         retro_atomic_fetch_add_size(&audioworklet->dropped, num_frames);
         break;
      }
      if (audioworklet_park(audioworklet, 2 * sizeof(float)))
         laps = AUDIOWORKLET_WAIT_LAPS;
      /* try resuming, on the off chance that the context was interrupted while blocking */
      audioworklet_resume_ctx(audioworklet);
#endif
   }

   return _len;
}

#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
/* returns true if fake block should continue */
bool audioworklet_external_block(void)
{
   audioworklet_data_t *audioworklet = audioworklet_static_data;

   if (audioworklet)
   {
#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
      if (!audioworklet->block_requested)
         return false;
#endif

      if (audioworklet->initing)
      {
         uint32_t state;
#ifdef EMSCRIPTEN_AUDIO_ASYNC_BLOCK
         state = audioworklet_init_wait(audioworklet);
#else
         /* The fake block goes back to the browser until a callback
          * has reported. */
         if ((state = emscripten_atomic_load_u32(&audioworklet->init_state))
               == AUDIOWORKLET_INIT_PENDING)
            return true;
#endif
         audioworklet->initing = false;
         if (state != AUDIOWORKLET_INIT_DONE || !audioworklet->ring)
         {
            audioworklet_init_error(audioworklet);
            abort();
            return false;
         }
         audioworklet_publish(audioworklet);
      }
      if (!audioworklet->driver_running)
         return false;

#ifdef EMSCRIPTEN_AUDIO_ASYNC_BLOCK
      {
         /* Parks on the worklet's pulls until the reserve is free.
          * Bounded: a context that will not run frees nothing, and the
          * block ends with the audio dropped rather than never. */
         int laps = AUDIOWORKLET_WAIT_LAPS;
         while (     laps >= 0
               && audioworklet_room(audioworklet) < audioworklet->write_avail_diff)
         {
            audioworklet_resume_ctx(audioworklet);
            if (audioworklet_park(audioworklet, audioworklet->write_avail_diff))
               laps = AUDIOWORKLET_WAIT_LAPS;
            else
               laps--;
         }
      }
#else
      while (audioworklet_room(audioworklet) < audioworklet->write_avail_diff)
      {
         audioworklet_resume_ctx(audioworklet);
         return true;
      }
#endif

#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
      audioworklet->block_requested = false;
      platform_emscripten_exit_fake_block();
      return true; /* return to RAF if needed */
#endif
   }
   return false;
}
#endif

/* called on program exit */
void audioworklet_close(void)
{
   audioworklet_data_t *audioworklet = audioworklet_static_data;

   if (!audioworklet)
      return;

   MAIN_THREAD_EM_ASM({
      emscriptenGetAudioObject($0).close();
   }, audioworklet->context);
}

static bool audioworklet_stop(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   audioworklet->running = false;
   return true;
}

static bool audioworklet_start(void *data, bool is_shutdown)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   audioworklet->running = true;
   return true;
}

static bool audioworklet_alive(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   return audioworklet->running;
}

static void audioworklet_set_nonblock_state(void *data, bool state)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   audioworklet->nonblock = state;
}

static void audioworklet_free(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   /* that's not good... this shouldn't happen? */
   if (!audioworklet->driver_running)
   {
      RARCH_ERR("[AudioWorklet] Tried to free before done initing.\n");
      return;
   }

   audioworklet_retire(audioworklet);
   {
      size_t dropped = retro_atomic_load_acquire_size(&audioworklet->dropped);
      if (dropped)
         RARCH_WARN("[AudioWorklet] %lu frames dropped: the context did not take them.\n",
               (unsigned long)dropped);
   }
   MAIN_THREAD_ASYNC_EM_ASM({
      emscriptenGetAudioObject($0).suspend();
   }, audioworklet->context);
}

static size_t audioworklet_write_avail(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   size_t avail = audioworklet_room(audioworklet);
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   if (avail > audioworklet->write_avail_diff)
      return avail - audioworklet->write_avail_diff;
   return 0;
#else
   return avail;
#endif
}

static size_t audioworklet_buffer_size(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   return audioworklet->visible_buffer_size;
}

static bool audioworklet_use_float(void *data) { return true; }

static size_t audioworklet_underruns(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   return audioworklet
      ? retro_atomic_load_acquire_size(&audioworklet->underruns) : 0;
}

audio_driver_t audio_audioworklet = {
   audioworklet_init,
   audioworklet_write,
   audioworklet_stop,
   audioworklet_start,
   audioworklet_alive,
   audioworklet_set_nonblock_state,
   audioworklet_free,
   audioworklet_use_float,
   "audioworklet",
   NULL,
   NULL,
   audioworklet_write_avail,
   audioworklet_buffer_size,
   NULL, /* write_raw */
   NULL, /* wait_writable */
   NULL, /* frames_consumed */
   audioworklet_underruns,
   NULL, /* layout */
   NULL, /* frames_consumed_fallback */
   NULL, /* device_clock_ppm */
   NULL, /* thread_grant */
   audioworklet_write_begin,
   audioworklet_write_end
};
