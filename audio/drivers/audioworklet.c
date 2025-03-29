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
#include <boolean.h>

#include <emscripten/wasm_worker.h>
#include <emscripten/webaudio.h>
#include <emscripten/atomic.h>
#include "../../frontend/drivers/platform_emscripten.h"

#include <queues/fifo_queue.h>
#include <retro_timers.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#define WORKLET_STACK_SIZE 4096

/* additional buffer size (for EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK only) */
/* if this is too small, frames may be dropped and content could run too fast. */
/* very large slow-motion rate values may be too large for this; avoid anything higher than 6 or 7. */
#define EXTERNAL_BLOCK_BUFFER_MS 128

typedef struct audioworklet_data
{
   uint8_t *worklet_stack;
   uint32_t write_avail_bytes; /* atomic */
   size_t visible_buffer_size;
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
   size_t write_avail_diff;
#endif
#ifdef PROXY_TO_PTHREAD
   emscripten_lock_t trywrite_lock;
   emscripten_condvar_t trywrite_cond;
#endif
   emscripten_lock_t buffer_lock;
   EMSCRIPTEN_WEBAUDIO_T context;
   float *tmpbuf;
   fifo_buffer_t *buffer;
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
   volatile bool init_done;
   volatile bool init_error;
} audioworklet_data_t;

/* We only ever want to create 1 worklet, so we need to keep its data even if the driver is inactive. */
static audioworklet_data_t *audioworklet_static_data = NULL;

/* Note that we cannot allocate any heap in here. */
static bool audioworklet_process_cb(int numInputs, const AudioSampleFrame *inputs,
   int numOutputs, AudioSampleFrame *outputs,
   int numParams, const AudioParamFrame *params,
   void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   size_t avail;
   size_t max_read;
   unsigned writing_frames = 0;
   int i;

   /* TODO: do we need to pay attention to audioworklet->running here too? */
   if (audioworklet->driver_running)
   {
      /* can't use Atomics.wait in AudioWorklet  */
      /* busyspin is safe as of emscripten 4.0.4 */
      if (!emscripten_lock_busyspin_wait_acquire(&audioworklet->buffer_lock, 2.5))
      {
         printf("[WARN] [AudioWorklet] Worklet: could not acquire lock\n");
         return true;
      }

      avail = FIFO_READ_AVAIL(audioworklet->buffer);
      max_read = MIN(avail, outputs[0].samplesPerChannel * 2 * sizeof(float));

      if (max_read)
      {
         fifo_read(audioworklet->buffer, audioworklet->tmpbuf, max_read);
         emscripten_atomic_add_u32(&audioworklet->write_avail_bytes, max_read);
      }
      emscripten_lock_release(&audioworklet->buffer_lock);
#ifdef PROXY_TO_PTHREAD
      emscripten_condvar_signal(&audioworklet->trywrite_cond, 1);
#endif

      writing_frames = max_read / 2 / sizeof(float);
      for (i = 0; i < writing_frames; i++)
      {
         outputs[0].data[i]                                = audioworklet->tmpbuf[i * 2];
         outputs[0].data[outputs[0].samplesPerChannel + i] = audioworklet->tmpbuf[i * 2 + 1];
      }
   }

   if (writing_frames < outputs[0].samplesPerChannel)
   {
      int zero_frames = outputs[0].samplesPerChannel - writing_frames;
      memset(outputs[0].data + writing_frames,                                0, zero_frames * sizeof(float));
      memset(outputs[0].data + writing_frames + outputs[0].samplesPerChannel, 0, zero_frames * sizeof(float));
   }

   return true;
}

static void audioworklet_processor_inited_cb(EMSCRIPTEN_WEBAUDIO_T context, bool success, void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   int outputChannelCounts[1] = { 2 };
   EmscriptenAudioWorkletNodeCreateOptions opts = { 0, 1, outputChannelCounts };
   EMSCRIPTEN_AUDIO_WORKLET_NODE_T worklet_node;

   if (!success)
   {
      RARCH_ERR("[AudioWorklet] Failed to init AudioWorkletProcessor!\n");
      audioworklet->init_error = true;
      audioworklet->init_done = true;
      return;
   }

   worklet_node = emscripten_create_wasm_audio_worklet_node(context, "retroarch", &opts, audioworklet_process_cb, audioworklet);
   emscripten_audio_node_connect(worklet_node, context, 0, 0);

   audioworklet->init_done = true;
}

static void audioworklet_thread_inited_cb(EMSCRIPTEN_WEBAUDIO_T context, bool success, void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   WebAudioWorkletProcessorCreateOptions opts = { "retroarch", 0 };

   if (!success)
   {
      RARCH_ERR("[AudioWorklet] Failed to init worklet thread! Is the worklet file in the right place?\n");
      audioworklet->init_error = true;
      audioworklet->init_done = true;
      return;
   }

   emscripten_create_wasm_audio_worklet_processor_async(context, &opts, audioworklet_processor_inited_cb, audioworklet);
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

   audioworklet->tmpbuf = memalign(16, emscripten_audio_context_quantum_size(audioworklet->context) * 2 * sizeof(float));
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
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   size_t buffer_size;
   audioworklet->visible_buffer_size = (audioworklet->latency * audioworklet->rate * 2 * sizeof(float)) / 1000;
   buffer_size = audioworklet->visible_buffer_size;
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
   audioworklet->write_avail_diff = (EXTERNAL_BLOCK_BUFFER_MS * audioworklet->rate * 2 * sizeof(float)) / 1000;
   buffer_size += audioworklet->write_avail_diff;
#endif
   audioworklet->buffer = fifo_new(buffer_size);
   emscripten_atomic_store_u32(&audioworklet->write_avail_bytes, buffer_size);
   RARCH_LOG("[AudioWorklet] Buffer size: %lu bytes.\n", audioworklet->visible_buffer_size);
}

static void audioworklet_init_error(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

   RARCH_ERR("[AudioWorklet] Failed to initialize driver!\n");
   free(audioworklet->worklet_stack);
   free(audioworklet->tmpbuf);
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

static void *audioworklet_init(const char *device, unsigned rate,
   unsigned latency, unsigned block_frames, unsigned *new_rate)
{
   audioworklet_data_t *audioworklet;
   if (audioworklet_static_data)
   {
      if (audioworklet_static_data->driver_running || audioworklet_static_data->initing)
      {
         RARCH_ERR("[AudioWorklet] Tried to start already running driver!\n");
         return NULL;
      }
      RARCH_LOG("[AudioWorklet] Reusing old context.\n");
      audioworklet = audioworklet_static_data;
      audioworklet->latency = latency;
      *new_rate = audioworklet->rate;
      RARCH_LOG("[AudioWorklet] Device rate: %d Hz.\n", *new_rate);
      audioworklet_alloc_buffer(audioworklet);
      audioworklet_resume_ctx(audioworklet);
      audioworklet->driver_running = true;
      return audioworklet;
   }

   audioworklet = (audioworklet_data_t*)calloc(1, sizeof(audioworklet_data_t));
   if (!audioworklet)
      return NULL;
   audioworklet->worklet_stack = memalign(16, WORKLET_STACK_SIZE);
   if (!audioworklet->worklet_stack)
      return NULL;
   audioworklet_static_data = audioworklet;

   audioworklet->latency = latency;
   platform_emscripten_run_on_browser_thread_sync(audioworklet_ctx_create, audioworklet);
   *new_rate = audioworklet->rate;
   RARCH_LOG("[AudioWorklet] Device rate: %d Hz.\n", *new_rate);
   audioworklet->initing = true;
   audioworklet_alloc_buffer(audioworklet);
   emscripten_lock_init(&audioworklet->buffer_lock);
#ifdef PROXY_TO_PTHREAD
   emscripten_lock_init(&audioworklet->trywrite_lock);
   emscripten_condvar_init(&audioworklet->trywrite_cond);
#endif

#ifndef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
   /* TODO: can MIN_ASYNCIFY block here too? */
   while (!audioworklet->init_done)
      retro_sleep(1);
   audioworklet->initing = false;
   if (audioworklet->init_error)
   {
      audioworklet_init_error(audioworklet);
      return NULL;
   }
   audioworklet->driver_running = true;
#elif defined(EMSCRIPTEN_AUDIO_FAKE_BLOCK)
   audioworklet->block_requested = true;
   platform_emscripten_enter_fake_block(1);
#endif
   /* external block: will be handled later */

   return audioworklet;
}

static ssize_t audioworklet_write(void *data, const void *s, size_t ss)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   const float *samples = (const float*)s;
   size_t num_frames = ss / 2 / sizeof(float);
   size_t written = 0;
   size_t to_write_frames;
   size_t to_write_bytes;
   size_t avail;
   size_t max_write;

   /* too early! might happen with external blocking */
   if (!audioworklet->driver_running)
      return 0;

   /* don't write audio if the context isn't running, just try to start it */
   if (!audioworklet_resume_ctx(audioworklet))
      return 0;

   while (num_frames)
   {
#ifdef PROXY_TO_PTHREAD
      if (!emscripten_lock_wait_acquire(&audioworklet->buffer_lock, 2500000))
#else
      if (!emscripten_lock_busyspin_wait_acquire(&audioworklet->buffer_lock, 2.5))
#endif
      {
         RARCH_WARN("[AudioWorklet] Main thread: could not acquire lock\n");
         break;
      }

      avail = FIFO_WRITE_AVAIL(audioworklet->buffer);
      max_write = avail;
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
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
         avail -= to_write_bytes;
         fifo_write(audioworklet->buffer, samples, to_write_bytes);
         emscripten_atomic_store_u32(&audioworklet->write_avail_bytes, (uint32_t)avail);
         num_frames -= to_write_frames;
         samples += (to_write_frames * 2);
         written += to_write_frames;
      }

      emscripten_lock_release(&audioworklet->buffer_lock);

#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
      /* see if we're over the threshold to go to fake block */
      if (avail < audioworklet->write_avail_diff)
      {
         audioworklet->block_requested = true;
         platform_emscripten_enter_fake_block(1);
      }
#endif
      if (num_frames && !audioworklet->nonblock)
         RARCH_WARN("[AudioWorklet] Dropping %lu frames.\n", num_frames);
      break;
#endif
      if (audioworklet->nonblock || !num_frames)
         break;
#if defined(PROXY_TO_PTHREAD)
      emscripten_condvar_wait(&audioworklet->trywrite_cond, &audioworklet->trywrite_lock, 3000000);
#elif defined(EMSCRIPTEN_FULL_ASYNCIFY)
      retro_sleep(1);
#else /* equivalent to defined(EMSCRIPTEN_AUDIO_BUSYWAIT) */
      while (emscripten_atomic_load_u32(&audioworklet->write_avail_bytes) < 2 * sizeof(float))
         audioworklet_resume_ctx(audioworklet);
#endif
      /* try resuming, on the off chance that the context was interrupted while blocking */
      audioworklet_resume_ctx(audioworklet);
   }

   return written;
}

#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
/* returns true if fake block should continue */
bool audioworklet_external_block(void)
{
   audioworklet_data_t *audioworklet = audioworklet_static_data;

#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
   if (!audioworklet->block_requested)
      return false;
#endif

   while (audioworklet->initing && !audioworklet->init_done)
#ifdef EMSCRIPTEN_AUDIO_ASYNC_BLOCK
      retro_sleep(1);
#else
      return true;
#endif
   if (audioworklet->init_done && !audioworklet->driver_running)
   {
      audioworklet->initing = false;
      if (audioworklet->init_error)
      {
         audioworklet_init_error(audioworklet);
         abort();
         return false;
      }
      audioworklet->driver_running = true;
   }
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
   if (!audioworklet->driver_running)
      return false;

   while (emscripten_atomic_load_u32(&audioworklet->write_avail_bytes) < audioworklet->write_avail_diff)
   {
      audioworklet_resume_ctx(audioworklet);
#ifdef EMSCRIPTEN_AUDIO_ASYNC_BLOCK
      retro_sleep(1);
#else
      return true;
#endif
   }
#endif

#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
   audioworklet->block_requested = false;
   platform_emscripten_exit_fake_block();
   return true; /* return to RAF if needed */
#endif
   return false;
}
#endif

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
      RARCH_ERR("[AudioWorklet] Tried to free before done initing!\n");
      return;
   }

#ifdef PROXY_TO_PTHREAD
   if (!emscripten_lock_wait_acquire(&audioworklet->buffer_lock, 10000000))
#else
   if (!emscripten_lock_busyspin_wait_acquire(&audioworklet->buffer_lock, 10))
#endif
   {
      RARCH_ERR("[AudioWorklet] Main thread: could not acquire lock to free buffer!\n");
      return;
   }
   audioworklet->driver_running = false;
   fifo_free(audioworklet->buffer);
   emscripten_lock_release(&audioworklet->buffer_lock);
   MAIN_THREAD_ASYNC_EM_ASM({
      emscriptenGetAudioObject($0).suspend();
   }, audioworklet->context);
}

static size_t audioworklet_write_avail(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;

#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
   size_t avail = emscripten_atomic_load_u32(&audioworklet->write_avail_bytes);
   if (avail > audioworklet->write_avail_diff)
      return avail - audioworklet->write_avail_diff;
   return 0;
#else
   return emscripten_atomic_load_u32(&audioworklet->write_avail_bytes);
#endif
}

static size_t audioworklet_buffer_size(void *data)
{
   audioworklet_data_t *audioworklet = (audioworklet_data_t*)data;
   return audioworklet->visible_buffer_size;
}

static bool audioworklet_use_float(void *data) { return true; }

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
   audioworklet_buffer_size
};
