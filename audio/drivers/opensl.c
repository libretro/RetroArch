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

#include <SLES/OpenSLES.h>
#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>
#endif

#include <string.h>

#include <retro_atomic.h>
#include <retro_math.h>
#include <rthreads/rthreads.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

/* Upper bound on how long a blocking write will wait for the buffer
 * queue callback before giving up and reporting a short write.  Never
 * reached in normal operation - the callback fires as each enqueued
 * block finishes - so the value only decides how long a device that
 * has stopped consuming takes to be noticed.  Matches the flat
 * bail-out timeouts in sdl_audio.c and wasapi.c. */
#define OPENSL_STALL_TIMEOUT_US 256000

/* Helper macros, COM-style. */
#define SLObjectItf_Realize(a, ...) ((*(a))->Realize(a, __VA_ARGS__))
#define SLObjectItf_GetInterface(a, ...) ((*(a))->GetInterface(a, __VA_ARGS__))
#define SLObjectItf_Destroy(a) ((*(a))->Destroy((a)))

#define SLEngineItf_CreateOutputMix(a, ...) ((*(a))->CreateOutputMix(a, __VA_ARGS__))
#define SLEngineItf_CreateAudioPlayer(a, ...) ((*(a))->CreateAudioPlayer(a, __VA_ARGS__))

#define SLPlayItf_SetPlayState(a, ...) ((*(a))->SetPlayState(a, __VA_ARGS__))

typedef struct sl
{
   uint8_t **buffer;
   uint8_t *buffer_chunk;

   SLObjectItf engine_object;
   SLEngineItf engine;

   SLObjectItf output_mix;
   SLObjectItf buffer_queue_object;
   SLAndroidSimpleBufferQueueItf buffer_queue;
   SLPlayItf player;

   slock_t *lock;
   scond_t *cond;
   unsigned buf_size;
   unsigned buf_count;
   unsigned buffer_index;
   unsigned buffer_ptr;
   /* The player was created with a float PCM_EX format (Android, API
    * level 21+); blocks then hold 32-bit float frames and the frontend
    * skips its float-to-int16 pass. */
   bool use_float;
   /* Blocks currently enqueued on the device.  Decremented by
    * opensl_callback on the OpenSL engine thread, incremented and
    * read by the writer thread.  Android is always weakly-ordered
    * ARM/AArch64, so the reads need acquire semantics to pair with
    * the acq_rel RMWs - the previous plain volatile reads had none. */
   retro_atomic_int_t buffered_blocks;
   /* Frames the device has finished with, for the sink rate estimate.
    * The callback fires once per block the device has played, so it is
    * the device's own clock ticking - the same thing the WASAPI pump
    * and the ASIO callback count. Written only by the callback, read by
    * the frontend through sl_frames_consumed(). */
   retro_atomic_size_t consumed;
   unsigned frames_per_block;
   bool nonblock;
   bool is_paused;
} sl_t;

static void opensl_callback(SLAndroidSimpleBufferQueueItf bq, void *ctx)
{
   sl_t *sl = (sl_t*)ctx;
   retro_atomic_fetch_sub_int(&sl->buffered_blocks, 1);
   /* A block the device has played: device time, whatever the writer
    * managed to supply. */
   retro_atomic_fetch_add_size(&sl->consumed, sl->frames_per_block);
   scond_signal(sl->cond);
}

/* Frames the device has taken since the player started. Counting the
 * blocks it has finished with counts device time; there is no queue to
 * subtract, unlike ALSA, because a block is only handed back once it
 * has been played. */
static size_t sl_frames_consumed(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl)
      return 0;
   return retro_atomic_load_acquire_size(&sl->consumed);
}

#define GOTO_IF_FAIL(x) do { \
   if ((res = (x)) != SL_RESULT_SUCCESS) \
      goto error; \
} while (0)

static void sl_free(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl)
      return;

   if (sl->player)
      SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_STOPPED);

   if (sl->buffer_queue_object)
      SLObjectItf_Destroy(sl->buffer_queue_object);

   if (sl->output_mix)
      SLObjectItf_Destroy(sl->output_mix);

   if (sl->engine_object)
      SLObjectItf_Destroy(sl->engine_object);

   if (sl->lock)
      slock_free(sl->lock);
   if (sl->cond)
      scond_free(sl->cond);

   free(sl->buffer);
   free(sl->buffer_chunk);
   free(sl);
}

static void *sl_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   unsigned i;
   unsigned frames_per_block;
   unsigned frame_size                             = 2 * sizeof(int16_t);
   SLDataFormat_PCM fmt_pcm                        = {0};
#if defined(ANDROID) && defined(__ANDROID_API__) && (__ANDROID_API__ >= 21)
   SLAndroidDataFormat_PCM_EX fmt_pcm_ex           = {0};
#endif
   SLDataSource audio_src                          = {0};
   SLDataSink audio_sink                           = {0};
   SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {0};
   SLDataLocator_OutputMix loc_outmix              = {0};
   SLresult res                                    = 0;
   SLInterfaceID                                id = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
   SLboolean                                req    = SL_BOOLEAN_TRUE;
   sl_t                                        *sl = (sl_t*)calloc(1, sizeof(sl_t));

   (void)device;
   if (!sl)
      goto error;

   /* calloc zero-fill is not a portable initializer for an atomic -
    * initialize it explicitly before anything can touch it. */
   retro_atomic_int_init(&sl->buffered_blocks, 0);
   retro_atomic_size_init(&sl->consumed, 0);

   RARCH_LOG("[OpenSL] Requested audio latency: %u ms.\n", latency);

   GOTO_IF_FAIL(slCreateEngine(&sl->engine_object, 0, NULL, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->engine_object, SL_BOOLEAN_FALSE));
   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->engine_object, SL_IID_ENGINE, &sl->engine));

   GOTO_IF_FAIL(SLEngineItf_CreateOutputMix(sl->engine, &sl->output_mix, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->output_mix, SL_BOOLEAN_FALSE));

   /* Sizes in frames first; the byte size follows the format chosen. */
   if (block_frames)
      frames_per_block = block_frames;
   else
      frames_per_block = next_pow2(32 * latency) / 4;
   if (frames_per_block < 2)
      frames_per_block = 2;

   sl->buf_count    = (latency * rate + 500) / 1000;
   sl->buf_count    = (sl->buf_count + frames_per_block / 2) / frames_per_block;

   if (sl->buf_count < 2)
      sl->buf_count = 2;

   loc_bufq.locatorType   = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
   loc_bufq.numBuffers    = sl->buf_count;

   loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
   loc_outmix.outputMix   = sl->output_mix;

   audio_src.pLocator     = &loc_bufq;
   audio_sink.pLocator    = &loc_outmix;

#if defined(ANDROID) && defined(__ANDROID_API__) && (__ANDROID_API__ >= 21)
   /* Float first. API level 21 added SLAndroidDataFormat_PCM_EX; a
    * runtime that refuses it (older device, or a libOpenSLES built
    * without it) fails player creation, and the 16-bit format below is
    * tried in that case. */
   fmt_pcm_ex.formatType     = SL_ANDROID_DATAFORMAT_PCM_EX;
   fmt_pcm_ex.numChannels    = 2;
   fmt_pcm_ex.sampleRate     = rate * 1000; /* milli-Hz */
   fmt_pcm_ex.bitsPerSample  = 32;
   fmt_pcm_ex.containerSize  = 32;
   fmt_pcm_ex.channelMask    = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
   fmt_pcm_ex.endianness     = SL_BYTEORDER_LITTLEENDIAN;
   fmt_pcm_ex.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
   audio_src.pFormat         = &fmt_pcm_ex;

   res = SLEngineItf_CreateAudioPlayer(sl->engine, &sl->buffer_queue_object,
         &audio_src, &audio_sink, 1, &id, &req);
   if (res == SL_RESULT_SUCCESS)
   {
      sl->use_float = true;
      frame_size    = 2 * sizeof(float);
      RARCH_LOG("[OpenSL] Float output.\n");
   }
   else
   {
      sl->buffer_queue_object = NULL;
      RARCH_LOG("[OpenSL] Float output refused (0x%x), using 16-bit.\n",
            (unsigned)res);
   }
#endif

   if (!sl->use_float)
   {
      fmt_pcm.formatType     = SL_DATAFORMAT_PCM;
      fmt_pcm.numChannels    = 2;
      fmt_pcm.samplesPerSec  = rate * 1000; /* Samplerate is in milli-Hz. */
      fmt_pcm.bitsPerSample  = 16;
      fmt_pcm.containerSize  = 16;
      fmt_pcm.channelMask    = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
      fmt_pcm.endianness     = SL_BYTEORDER_LITTLEENDIAN; /* Android only. */
      audio_src.pFormat      = &fmt_pcm;

      GOTO_IF_FAIL(SLEngineItf_CreateAudioPlayer(sl->engine, &sl->buffer_queue_object,
               &audio_src, &audio_sink,
               1, &id, &req));
      frame_size             = 2 * sizeof(int16_t);
   }
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->buffer_queue_object, SL_BOOLEAN_FALSE));

   sl->buf_size          = frames_per_block * frame_size;
   sl->frames_per_block  = frames_per_block;

   sl->buffer       = (uint8_t**)calloc(sizeof(uint8_t*), sl->buf_count);
   if (!sl->buffer)
      goto error;

   sl->buffer_chunk = (uint8_t*)calloc(sl->buf_count, sl->buf_size);
   if (!sl->buffer_chunk)
      goto error;

   for (i = 0; i < sl->buf_count; i++)
      sl->buffer[i] = sl->buffer_chunk + i * sl->buf_size;

   RARCH_LOG("[OpenSL] Setting audio latency: Block size = %u, Blocks = %u, Total = %u...\n",
         sl->buf_size, sl->buf_count, sl->buf_size * sl->buf_count);

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &sl->buffer_queue));

   sl->cond               = scond_new();
   sl->lock               = slock_new();

   (*sl->buffer_queue)->RegisterCallback(sl->buffer_queue, opensl_callback, sl);

   /* Enqueue a bit to get stuff rolling. */
   retro_atomic_store_release_int(&sl->buffered_blocks,
         (int)sl->buf_count);
   sl->buffer_index       = 0;

   for (i = 0; i < sl->buf_count; i++)
      (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[i], sl->buf_size);

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_PLAY, &sl->player));
   GOTO_IF_FAIL(SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING));

   return sl;

error:
   RARCH_ERR("[OpenSL] Couldn't initialize OpenSL ES driver. Error code: %d.\n", (int)res);
   sl_free(sl);
   return NULL;
}

static bool sl_stop(void *data)
{
   sl_t      *sl = (sl_t*)data;
   sl->is_paused = (SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_STOPPED)
         == SL_RESULT_SUCCESS) ? true : false;

   return sl->is_paused ? true : false;
}

static bool sl_alive(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl)
      return false;
   return !sl->is_paused;
}

static void sl_set_nonblock_state(void *data, bool state)
{
   sl_t *sl = (sl_t*)data;
   if (sl)
      sl->nonblock = state;
}

static bool sl_start(void *data, bool is_shutdown)
{
   sl_t      *sl = (sl_t*)data;
   sl->is_paused = (SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING)
         == SL_RESULT_SUCCESS) ? false : true;
   return sl->is_paused ? false : true;
}

static ssize_t sl_write(void *data, const void *s, size_t len)
{
   size_t _len = 0;
   sl_t           *sl = (sl_t*)data;
   const uint8_t *buf = (const uint8_t*)s;

   while (len)
   {
      size_t avail_write;

      if (sl->nonblock)
      {
         if (retro_atomic_load_acquire_int(&sl->buffered_blocks)
               == (int)sl->buf_count)
            break;
      }
      else
      {
         bool signalled = true;
         slock_lock(sl->lock);
         /* Bounded.  opensl_callback is the only thing in this file
          * that ever signals sl->cond, and it does not hold sl->lock
          * while doing so - the predicate is buffered_blocks, updated
          * with an atomic fetch_sub - so a signal raised between the
          * test above and this wait reaches no waiter.  While the
          * device keeps consuming blocks the next callback covers
          * that.  If it has stopped consuming - device loss, a player
          * error - there is no next callback, no shutdown or error
          * signal anywhere in this driver, and the one `return -1`
          * below sits past the wait where a blocked thread can never
          * reach it.  An untimed wait here parked the thread the core
          * runs on for good. */
         while (retro_atomic_load_acquire_int(&sl->buffered_blocks)
               == (int)sl->buf_count)
         {
            if (!(signalled = scond_wait_timeout(sl->cond, sl->lock,
                        OPENSL_STALL_TIMEOUT_US)))
               break;
         }
         slock_unlock(sl->lock);

         /* Report what was enqueued so far, exactly as the nonblock
          * path above does when the queue is full.  Any partial block
          * stays in sl->buffer_ptr for the next call. */
         if (!signalled)
            break;
      }

      avail_write = MIN(sl->buf_size - sl->buffer_ptr, len);

      if (avail_write)
      {
         memcpy(sl->buffer[sl->buffer_index] + sl->buffer_ptr, buf, avail_write);
         sl->buffer_ptr += avail_write;
         buf            += avail_write;
         len            -= avail_write;
         _len           += avail_write;
      }

      if (sl->buffer_ptr >= sl->buf_size)
      {
         SLresult res     = (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[sl->buffer_index], sl->buf_size);

         /* A block the device refused is not on the device: it is not
          * counted, and the index stays on it. */
         if (res != SL_RESULT_SUCCESS)
         {
            RARCH_ERR("[OpenSL] Failed to write. Error: 0x%x.\n", (unsigned)res);
            return -1;
         }
         sl->buffer_index = (sl->buffer_index + 1) % sl->buf_count;
         retro_atomic_fetch_add_int(&sl->buffered_blocks, 1);
         sl->buffer_ptr   = 0;
      }
   }

   return _len;
}

/* Sleep on the condition the buffer-done callback signals until at
 * least len bytes fit, capped at half the queue so the wait always
 * ends. Returns the free space then, or 0 after the stall timeout. */
static size_t sl_wait_writable(void *data, size_t len)
{
   sl_t *sl     = (sl_t*)data;
   size_t total = (size_t)sl->buf_size * sl->buf_count;
   size_t avail;

   if (len > total / 2)
      len = total / 2;

   for (;;)
   {
      int buffered = retro_atomic_load_acquire_int(&sl->buffered_blocks);
      /* Whole blocks not on the device, less the one being filled,
       * plus what is left of that one. With every block enqueued there
       * is no block to fill and the space is nil; said so, rather than
       * left to the unsigned arithmetic that only came to zero because
       * buffer_ptr is always zero in that state. */
      if (buffered >= (int)sl->buf_count)
         avail = 0;
      else
         avail = ((size_t)(sl->buf_count - buffered - 1) * sl->buf_size
               + (sl->buf_size - sl->buffer_ptr));
      if (avail >= len)
         return avail;
      slock_lock(sl->lock);
      if (retro_atomic_load_acquire_int(&sl->buffered_blocks) == buffered
            && !scond_wait_timeout(sl->cond, sl->lock, OPENSL_STALL_TIMEOUT_US))
      {
         slock_unlock(sl->lock);
         return 0;
      }
      slock_unlock(sl->lock);
   }
}

static size_t sl_write_avail(void *data)
{
   sl_t *sl     = (sl_t*)data;
   int buffered = retro_atomic_load_acquire_int(&sl->buffered_blocks);
   return ((sl->buf_count - buffered - 1) * sl->buf_size + (sl->buf_size - (int)sl->buffer_ptr));
}

static size_t sl_buffer_size(void *data)
{
   sl_t *sl = (sl_t*)data;
   return sl->buf_size * sl->buf_count;
}

/* True when init managed to create the player with a float format
 * (Android API level 21+); false on the 16-bit path every other build
 * and runtime takes. */
static bool sl_use_float(void *data)
{
   sl_t *sl = (sl_t*)data;
   return sl->use_float;
}

audio_driver_t audio_opensl = {
   sl_init,
   sl_write,
   sl_stop,
   sl_start,
   sl_alive,
   sl_set_nonblock_state,
   sl_free,
   sl_use_float,
   "opensl",
   NULL,
   NULL,
   sl_write_avail,
   sl_buffer_size,
   NULL, /* write_raw */
   sl_wait_writable,
   sl_frames_consumed
};
