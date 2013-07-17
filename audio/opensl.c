/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "../general.h"
#include "../thread.h"

#include <SLES/OpenSLES.h>
#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>
#endif

// Helper macros, COM-style!
#define SLObjectItf_Realize(a, ...) ((*(a))->Realize(a, __VA_ARGS__))
#define SLObjectItf_GetInterface(a, ...) ((*(a))->GetInterface(a, __VA_ARGS__))
#define SLObjectItf_Destroy(a) ((*(a))->Destroy((a)))

#define SLEngineItf_CreateOutputMix(a, ...) ((*(a))->CreateOutputMix(a, __VA_ARGS__))
#define SLEngineItf_CreateAudioPlayer(a, ...) ((*(a))->CreateAudioPlayer(a, __VA_ARGS__))

#define SLPlayItf_SetPlayState(a, ...) ((*(a))->SetPlayState(a, __VA_ARGS__))

// TODO: Are these sane?
#define BUFFER_SIZE (2 * 1024)
#define BUFFER_COUNT 16

typedef struct sl
{
   uint8_t buffer[BUFFER_COUNT][BUFFER_SIZE];
   unsigned buffer_index;
   unsigned buffer_ptr;
   volatile unsigned buffered_blocks;

   SLObjectItf engine_object;
   SLEngineItf engine;

   SLObjectItf output_mix;
   SLObjectItf buffer_queue_object;
   SLAndroidSimpleBufferQueueItf buffer_queue;
   SLPlayItf player;

   slock_t *lock;
   scond_t *cond;
   bool nonblock;
   unsigned buf_count;
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
} while(0)

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

   free(sl);
}

static void *sl_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;

   SLDataFormat_PCM fmt_pcm = {0};
   SLDataSource audio_src   = {0};
   SLDataSink audio_sink    = {0};

   SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {0};
   SLDataLocator_OutputMix loc_outmix              = {0};

   SLInterfaceID id = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
   SLboolean req    = SL_BOOLEAN_TRUE;

   SLresult res = 0;
   sl_t *sl = (sl_t*)calloc(1, sizeof(sl_t));
   if (!sl)
      goto error;

   GOTO_IF_FAIL(slCreateEngine(&sl->engine_object, 0, NULL, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->engine_object, SL_BOOLEAN_FALSE));
   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->engine_object, SL_IID_ENGINE, &sl->engine));

   GOTO_IF_FAIL(SLEngineItf_CreateOutputMix(sl->engine, &sl->output_mix, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->output_mix, SL_BOOLEAN_FALSE));

   sl->buf_count = (latency * 4 * out_rate + 500) / 1000;
   sl->buf_count = (sl->buf_count + BUFFER_SIZE / 2) / BUFFER_SIZE;
   sl->buf_count = min(sl->buf_count, BUFFER_COUNT);

   RARCH_LOG("[SLES] : Setting audio latency (buffer size: [%d]) ...\n", sl->buf_count * BUFFER_SIZE);

   fmt_pcm.formatType    = SL_DATAFORMAT_PCM;
   fmt_pcm.numChannels   = 2;
   fmt_pcm.samplesPerSec = rate * 1000; // Samplerate is in milli-Hz.
   fmt_pcm.bitsPerSample = 16;
   fmt_pcm.containerSize = 16;
   fmt_pcm.channelMask   = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
   fmt_pcm.endianness    = SL_BYTEORDER_LITTLEENDIAN; // Android only.

   audio_src.pLocator = &loc_bufq;
   audio_src.pFormat  = &fmt_pcm;

   loc_bufq.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
   loc_bufq.numBuffers  = sl->buf_count;

   loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
   loc_outmix.outputMix   = sl->output_mix;

   audio_sink.pLocator = &loc_outmix;

   GOTO_IF_FAIL(SLEngineItf_CreateAudioPlayer(sl->engine, &sl->buffer_queue_object,
            &audio_src, &audio_sink,
            1, &id, &req));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->buffer_queue_object, SL_BOOLEAN_FALSE));

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &sl->buffer_queue));

   sl->cond = scond_new();
   sl->lock = slock_new();

   (*sl->buffer_queue)->RegisterCallback(sl->buffer_queue, opensl_callback, sl);

   // Enqueue a bit to get stuff rolling.
   sl->buffered_blocks = sl->buf_count;
   sl->buffer_index = 0;
   for (unsigned i = 0; i < sl->buf_count; i++)
      (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[i], BUFFER_SIZE);

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_PLAY, &sl->player));
   GOTO_IF_FAIL(SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING));

   return sl;

error:
   RARCH_ERR("Couldn't initialize OpenSL ES driver, error code: [%d].\n", (int)res);
   sl_free(sl);
   return NULL;
}

static bool sl_stop(void *data)
{
   sl_t *sl = (sl_t*)data;
   return SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_STOPPED) == SL_RESULT_SUCCESS;
}

static void sl_set_nonblock_state(void *data, bool state)
{
   sl_t *sl = (sl_t*)data;
   sl->nonblock = state;
}

static bool sl_start(void *data)
{
   sl_t *sl = (sl_t*)data;
   return SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING) == SL_RESULT_SUCCESS;
}


static ssize_t sl_write(void *data, const void *buf_, size_t size)
{
   sl_t *sl = (sl_t*)data;

   size_t written = 0;
   const uint8_t *buf = (const uint8_t*)buf_;

   while (size)
   {
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

      size_t avail_write = min(BUFFER_SIZE - sl->buffer_ptr, size);
      if (avail_write)
      {
         memcpy(sl->buffer[sl->buffer_index] + sl->buffer_ptr, buf, avail_write);
         sl->buffer_ptr += avail_write;
         buf            += avail_write;
         size           -= avail_write;
         written        += avail_write;
      }

      if (sl->buffer_ptr >= BUFFER_SIZE)
      {
         SLresult res = (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[sl->buffer_index], BUFFER_SIZE);
         sl->buffer_index = (sl->buffer_index + 1) % sl->buf_count;
         __sync_fetch_and_add(&sl->buffered_blocks, 1);
         sl->buffer_ptr = 0;

         if (res != SL_RESULT_SUCCESS)
         {
            RARCH_ERR("[OpenSL]: Failed to write! (Error: 0x%x)\n", (unsigned)res);
            return -1;
         }
      }
   }

   //RARCH_LOG("Blocks: %u\n", sl->buffered_blocks);

   return written;
}

static size_t sl_write_avail(void *data)
{
   sl_t *sl = (sl_t*)data;
   size_t avail = (sl->buf_count - (int)sl->buffered_blocks - 1) * BUFFER_SIZE + (BUFFER_SIZE - (int)sl->buffer_ptr);
   return avail;
}

static size_t sl_buffer_size(void *data)
{
   sl_t *sl = (sl_t*)data;
   return BUFFER_SIZE * sl->buf_count;
}

const audio_driver_t audio_opensl = {
   sl_init,
   sl_write,
   sl_stop,
   sl_start,
   sl_set_nonblock_state,
   sl_free,
   NULL,
   "opensl",
   sl_write_avail,
   sl_buffer_size,
};

