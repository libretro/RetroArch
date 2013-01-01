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
#include "../fifo_buffer.h"
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
#define BUFFER_SIZE 4096

typedef struct sl
{
   uint8_t buffer[BUFFER_SIZE];

   SLObjectItf engine_object;
   SLEngineItf engine;

   SLObjectItf output_mix;
   SLObjectItf buffer_queue_object;
   SLPlayItf player;

   fifo_buffer_t *fifo;
   slock_t *lock;
   scond_t *cond;
   bool nonblock;
   unsigned buf_count;
} sl_t;

static void opensl_callback(SLAndroidSimpleBufferQueueItf bq, void *ctx)
{
   sl_t *sl = (sl_t*)ctx;

   slock_lock(sl->lock);
   size_t read_avail = fifo_read_avail(sl->fifo);
   if (read_avail > BUFFER_SIZE)
      read_avail = BUFFER_SIZE;
   fifo_read(sl->fifo, sl->buffer, read_avail);
   slock_unlock(sl->lock);

   memset(sl->buffer + read_avail, 0, BUFFER_SIZE - read_avail);
   (*bq)->Enqueue(bq, sl->buffer, BUFFER_SIZE);

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

   if (sl->fifo)
      fifo_free(sl->fifo);
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

   SLAndroidSimpleBufferQueueItf buffer_queue = NULL;

   SLresult res = 0;
   sl_t *sl = (sl_t*)calloc(1, sizeof(sl_t));
   if (!sl)
      goto error;

   GOTO_IF_FAIL(slCreateEngine(&sl->engine_object, 0, NULL, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->engine_object, SL_BOOLEAN_FALSE));
   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->engine_object, SL_IID_ENGINE, &sl->engine));
   GOTO_IF_FAIL(SLEngineItf_CreateOutputMix(sl->engine, &sl->output_mix, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->output_mix, SL_BOOLEAN_FALSE));

   sl->buf_count = 8;

   RARCH_LOG("[SLES] : Setting audio latency (buffer size: [%d])..\n", sl->buf_count * BUFFER_SIZE);

   fmt_pcm.formatType    = SL_DATAFORMAT_PCM;
   fmt_pcm.numChannels   = 2;
   fmt_pcm.samplesPerSec = rate * 1000; // Samplerate is in milli-Hz.
   fmt_pcm.bitsPerSample = sizeof(int16_t) * 8;
   fmt_pcm.containerSize = sizeof(int16_t) * 8;
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

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_BUFFERQUEUE,
            &buffer_queue));

   sl->cond = scond_new();
   sl->lock = slock_new();
   sl->fifo = fifo_new(BUFFER_SIZE * sl->buf_count);

   (*buffer_queue)->RegisterCallback(buffer_queue, opensl_callback, sl);
   (*buffer_queue)->Enqueue(buffer_queue, sl->buffer, BUFFER_SIZE);

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_PLAY, &sl->player));
   GOTO_IF_FAIL(SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING));

   g_settings.audio.rate_control_delta = 0.006;
   g_settings.audio.rate_control = true;

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
      slock_lock(sl->lock);

      size_t write_avail = fifo_write_avail(sl->fifo);
      if (write_avail > size)
         write_avail = size;

      if (write_avail)
      {
         fifo_write(sl->fifo, buf, write_avail);
         slock_unlock(sl->lock);
         written += write_avail;
         size -= write_avail;
         buf  += write_avail;
      }
      else if (!sl->nonblock)
      {
         scond_wait(sl->cond, sl->lock);
         slock_unlock(sl->lock);
      }
      else
      {
         slock_unlock(sl->lock);
         break;
      }
   }

   return written;
}

static size_t sl_write_avail(void *data)
{
   sl_t *sl = (sl_t*)data;
   slock_lock(sl->lock);
   size_t avail = fifo_write_avail(sl->fifo);
   slock_unlock(sl->lock);
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

