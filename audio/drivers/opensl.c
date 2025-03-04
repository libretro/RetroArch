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

#include <rthreads/rthreads.h>

#include "../audio_driver.h"

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
   volatile unsigned buffered_blocks;
   bool nonblock;
   bool is_paused;
} sl_t;

static void opensl_callback(SLAndroidSimpleBufferQueueItf bq, void *ctx)
{
   sl_t *sl = (sl_t*)ctx;
   __sync_fetch_and_sub(&sl->buffered_blocks, 1);
   scond_signal(sl->cond);
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
   SLDataFormat_PCM fmt_pcm                        = {0};
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

   RARCH_LOG("[OpenSL]: Requested audio latency: %u ms.\n", latency);

   GOTO_IF_FAIL(slCreateEngine(&sl->engine_object, 0, NULL, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->engine_object, SL_BOOLEAN_FALSE));
   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->engine_object, SL_IID_ENGINE, &sl->engine));

   GOTO_IF_FAIL(SLEngineItf_CreateOutputMix(sl->engine, &sl->output_mix, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->output_mix, SL_BOOLEAN_FALSE));

   if (block_frames)
      sl->buf_size  = block_frames * 4;
   else
      sl->buf_size  = next_pow2(32 * latency);

   sl->buf_count    = (latency * 4 * rate + 500) / 1000;
   sl->buf_count    = (sl->buf_count + sl->buf_size / 2) / sl->buf_size;

   if (sl->buf_count < 2)
      sl->buf_count = 2;

   sl->buffer       = (uint8_t**)calloc(sizeof(uint8_t*), sl->buf_count);
   if (!sl->buffer)
      goto error;

   sl->buffer_chunk = (uint8_t*)calloc(sl->buf_count, sl->buf_size);
   if (!sl->buffer_chunk)
      goto error;

   for (i = 0; i < sl->buf_count; i++)
      sl->buffer[i] = sl->buffer_chunk + i * sl->buf_size;

   RARCH_LOG("[OpenSL]: Setting audio latency: Block size = %u, Blocks = %u, Total = %u ...\n",
         sl->buf_size, sl->buf_count, sl->buf_size * sl->buf_count);

   fmt_pcm.formatType     = SL_DATAFORMAT_PCM;
   fmt_pcm.numChannels    = 2;
   fmt_pcm.samplesPerSec  = rate * 1000; /* Samplerate is in milli-Hz. */
   fmt_pcm.bitsPerSample  = 16;
   fmt_pcm.containerSize  = 16;
   fmt_pcm.channelMask    = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
   fmt_pcm.endianness     = SL_BYTEORDER_LITTLEENDIAN; /* Android only. */

   audio_src.pLocator     = &loc_bufq;
   audio_src.pFormat      = &fmt_pcm;

   loc_bufq.locatorType   = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
   loc_bufq.numBuffers    = sl->buf_count;

   loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
   loc_outmix.outputMix   = sl->output_mix;

   audio_sink.pLocator    = &loc_outmix;

   GOTO_IF_FAIL(SLEngineItf_CreateAudioPlayer(sl->engine, &sl->buffer_queue_object,
            &audio_src, &audio_sink,
            1, &id, &req));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->buffer_queue_object, SL_BOOLEAN_FALSE));

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &sl->buffer_queue));

   sl->cond               = scond_new();
   sl->lock               = slock_new();

   (*sl->buffer_queue)->RegisterCallback(sl->buffer_queue, opensl_callback, sl);

   /* Enqueue a bit to get stuff rolling. */
   sl->buffered_blocks    = sl->buf_count;
   sl->buffer_index       = 0;

   for (i = 0; i < sl->buf_count; i++)
      (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[i], sl->buf_size);

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_PLAY, &sl->player));
   GOTO_IF_FAIL(SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING));

   return sl;

error:
   RARCH_ERR("[OpenSL]: Couldn't initialize OpenSL ES driver, error code: [%d].\n", (int)res);
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
   sl_t           *sl = (sl_t*)data;
   size_t     written = 0;
   const uint8_t *buf = (const uint8_t*)s;

   while (len)
   {
      size_t avail_write;

      if (sl->nonblock)
      {
         if (sl->buffered_blocks == sl->buf_count)
            break;
      }
      else
      {
         slock_lock(sl->lock);
         while (sl->buffered_blocks == sl->buf_count)
            scond_wait(sl->cond, sl->lock);
         slock_unlock(sl->lock);
      }

      avail_write = MIN(sl->buf_size - sl->buffer_ptr, len);

      if (avail_write)
      {
         memcpy(sl->buffer[sl->buffer_index] + sl->buffer_ptr, buf, avail_write);
         sl->buffer_ptr += avail_write;
         buf            += avail_write;
         len            -= avail_write;
         written        += avail_write;
      }

      if (sl->buffer_ptr >= sl->buf_size)
      {
         SLresult res     = (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[sl->buffer_index], sl->buf_size);
         sl->buffer_index = (sl->buffer_index + 1) % sl->buf_count;
         __sync_fetch_and_add(&sl->buffered_blocks, 1);
         sl->buffer_ptr   = 0;

         if (res != SL_RESULT_SUCCESS)
         {
            RARCH_ERR("[OpenSL]: Failed to write! (Error: 0x%x)\n", (unsigned)res);
            return -1;
         }
      }
   }

   return written;
}

static size_t sl_write_avail(void *data)
{
   sl_t *sl = (sl_t*)data;
   return ((sl->buf_count - (int)sl->buffered_blocks - 1) * sl->buf_size + (sl->buf_size - (int)sl->buffer_ptr));
}

static size_t sl_buffer_size(void *data)
{
   sl_t *sl = (sl_t*)data;
   return sl->buf_size * sl->buf_count;
}

static bool sl_use_float(void *data) { return false; }

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
};
