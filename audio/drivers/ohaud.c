/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2025      - OpenHarmony contributors
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
#include <stdint.h>
#include <string.h>

#include <ohaudio/native_audiostreambuilder.h>
#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#define OHAUDIO_CHANNELS        2
#define OHAUDIO_BYTES_PER_FRAME (OHAUDIO_CHANNELS * sizeof(int16_t))
#define OHAUDIO_DEFAULT_BLOCK_FRAMES 256
#define OHAUDIO_WAIT_TIMEOUT_US 50000

typedef struct ohaudio
{
   OH_AudioRenderer *renderer;
   fifo_buffer_t *fifo;
   slock_t *lock;
   scond_t *cond;
   size_t buffer_size;
   bool is_paused;
   bool is_interrupted;
   bool is_shutting_down;
   bool nonblock;
} ohaudio_t;

static int32_t ohaudio_write_cb(OH_AudioRenderer *renderer,
      void *user_data, void *buffer, int32_t length)
{
   size_t read_size;
   size_t avail;
   ohaudio_t *oha = (ohaudio_t*)user_data;

   (void)renderer;

   if (!oha || !buffer || length <= 0)
      return 0;

   slock_lock(oha->lock);
   avail     = FIFO_READ_AVAIL(oha->fifo);
   read_size = avail < (size_t)length ? avail : (size_t)length;
   read_size -= read_size % OHAUDIO_BYTES_PER_FRAME;

   scond_signal(oha->cond);
   if (read_size)
      fifo_read(oha->fifo, buffer, read_size);
   slock_unlock(oha->lock);

   if (read_size < (size_t)length)
      memset((uint8_t*)buffer + read_size, 0, (size_t)length - read_size);
   return 0;
}

static void ohaudio_interrupt_cb(OH_AudioRenderer *renderer,
      void *user_data, OH_AudioInterrupt_ForceType type,
      OH_AudioInterrupt_Hint hint)
{
   ohaudio_t *oha = (ohaudio_t*)user_data;
   bool should_start = false;

   (void)type;

   if (!oha)
      return;

   slock_lock(oha->lock);

   switch (hint)
   {
      case AUDIOSTREAM_INTERRUPT_HINT_RESUME:
      case AUDIOSTREAM_INTERRUPT_HINT_UNDUCK:
      case AUDIOSTREAM_INTERRUPT_HINT_UNMUTE:
         oha->is_interrupted = false;
         oha->is_paused      = false;
         should_start         = hint == AUDIOSTREAM_INTERRUPT_HINT_RESUME;
         break;
      case AUDIOSTREAM_INTERRUPT_HINT_PAUSE:
      case AUDIOSTREAM_INTERRUPT_HINT_STOP:
      case AUDIOSTREAM_INTERRUPT_HINT_MUTE:
         oha->is_interrupted = true;
         oha->is_paused      = true;
         fifo_clear(oha->fifo);
         break;
      case AUDIOSTREAM_INTERRUPT_HINT_DUCK:
      case AUDIOSTREAM_INTERRUPT_HINT_NONE:
      default:
         break;
   }

   scond_signal(oha->cond);
   slock_unlock(oha->lock);

   if (should_start
         && OH_AudioRenderer_Start(renderer) != AUDIOSTREAM_SUCCESS)
   {
      slock_lock(oha->lock);
      oha->is_interrupted = true;
      oha->is_paused      = true;
      scond_signal(oha->cond);
      slock_unlock(oha->lock);
   }

   RARCH_LOG("[OHAudio] Interrupt event: type=%d hint=%d.\n",
         (int)type, (int)hint);
}

static void ohaudio_free(void *data)
{
   ohaudio_t *oha = (ohaudio_t*)data;

   if (!oha)
      return;

   if (oha->lock && oha->cond)
   {
      slock_lock(oha->lock);
      oha->is_shutting_down = true;
      scond_signal(oha->cond);
      slock_unlock(oha->lock);
   }

   if (oha->renderer)
   {
      OH_AudioRenderer_Stop(oha->renderer);
      OH_AudioRenderer_Release(oha->renderer);
   }

   if (oha->fifo)
      fifo_free(oha->fifo);
   if (oha->lock)
      slock_free(oha->lock);
   if (oha->cond)
      scond_free(oha->cond);

   free(oha);
}

static void *ohaudio_init(const char *device, unsigned rate,
      unsigned latency, unsigned block_frames, unsigned *new_rate)
{
   size_t buffer_size;
   unsigned callback_frames               = block_frames
      ? block_frames : OHAUDIO_DEFAULT_BLOCK_FRAMES;
   OH_AudioRenderer_Callbacks callbacks = {0};
   OH_AudioStreamBuilder *builder       = NULL;
   OH_AudioStream_Result res            = AUDIOSTREAM_SUCCESS;
   ohaudio_t *oha                       = (ohaudio_t*)calloc(1, sizeof(*oha));

   (void)device;

   if (!oha)
      return NULL;

   buffer_size = (latency * rate * OHAUDIO_BYTES_PER_FRAME + 500) / 1000;
   if (block_frames)
      buffer_size = block_frames * OHAUDIO_BYTES_PER_FRAME * 4;
   if (buffer_size < 4096)
      buffer_size = 4096;

   oha->buffer_size = buffer_size;
   oha->fifo        = fifo_new(buffer_size);
   oha->lock        = slock_new();
   oha->cond        = scond_new();

   if (!oha->fifo || !oha->lock || !oha->cond)
      goto error;

   callbacks.OH_AudioRenderer_OnWriteData = ohaudio_write_cb;

   RARCH_LOG("[OHAudio] Requested audio latency: %u ms.\n", latency);

   res = OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
   if (res != AUDIOSTREAM_SUCCESS)
      goto error;

   if ((res = OH_AudioStreamBuilder_SetSamplingRate(builder, (int32_t)rate))
         != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetChannelCount(builder, OHAUDIO_CHANNELS))
         != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetSampleFormat(builder,
               AUDIOSTREAM_SAMPLE_S16LE)) != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetEncodingType(builder,
               AUDIOSTREAM_ENCODING_TYPE_RAW)) != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetRendererInfo(builder,
               AUDIOSTREAM_USAGE_GAME)) != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetLatencyMode(builder,
               AUDIOSTREAM_LATENCY_MODE_FAST)) != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetFrameSizeInCallback(builder,
               (int32_t)callback_frames)) != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, oha))
         != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_SetRendererInterruptCallback(builder,
               ohaudio_interrupt_cb, oha)) != AUDIOSTREAM_SUCCESS)
      goto error;
   if ((res = OH_AudioStreamBuilder_GenerateRenderer(builder, &oha->renderer))
         != AUDIOSTREAM_SUCCESS)
      goto error;

   OH_AudioStreamBuilder_Destroy(builder);
   builder = NULL;

   res = OH_AudioRenderer_Start(oha->renderer);
   if (res != AUDIOSTREAM_SUCCESS)
      goto error;

   if (new_rate)
      *new_rate = rate;

   RARCH_LOG("[OHAudio] Initialized with %u Hz, %u byte buffer, %u callback frames.\n",
         rate, (unsigned)buffer_size, callback_frames);
   return oha;

error:
   RARCH_ERR("[OHAudio] Failed to initialize audio driver: %d.\n", (int)res);
   if (builder)
      OH_AudioStreamBuilder_Destroy(builder);
   ohaudio_free(oha);
   return NULL;
}

static bool ohaudio_stop(void *data)
{
   ohaudio_t *oha = (ohaudio_t*)data;
   bool paused;

   if (!oha || !oha->renderer)
      return false;

   paused = OH_AudioRenderer_Pause(oha->renderer) == AUDIOSTREAM_SUCCESS;

   slock_lock(oha->lock);
   oha->is_paused = paused;
   scond_signal(oha->cond);
   slock_unlock(oha->lock);

   return paused;
}

static bool ohaudio_start(void *data, bool is_shutdown)
{
   ohaudio_t *oha = (ohaudio_t*)data;

   (void)is_shutdown;

   if (!oha || !oha->renderer)
      return false;

   if (OH_AudioRenderer_Start(oha->renderer) != AUDIOSTREAM_SUCCESS)
      return false;

   slock_lock(oha->lock);
   oha->is_paused      = false;
   oha->is_interrupted = false;
   scond_signal(oha->cond);
   slock_unlock(oha->lock);

   return true;
}

static bool ohaudio_alive(void *data)
{
   ohaudio_t *oha = (ohaudio_t*)data;

   if (!oha)
      return false;

   return !oha->is_paused && !oha->is_interrupted && !oha->is_shutting_down;
}

static void ohaudio_set_nonblock_state(void *data, bool state)
{
   ohaudio_t *oha = (ohaudio_t*)data;
   if (oha)
      oha->nonblock = state;
}

static ssize_t ohaudio_write(void *data, const void *buf, size_t len)
{
   size_t written = 0;
   ohaudio_t *oha = (ohaudio_t*)data;

   if (!oha || !oha->renderer || !buf)
      return -1;

   len -= len % OHAUDIO_BYTES_PER_FRAME;
   if (!len)
      return 0;

   if (oha->nonblock)
   {
      size_t write_size;

      slock_lock(oha->lock);
      if (oha->is_paused || oha->is_interrupted || oha->is_shutting_down)
      {
         slock_unlock(oha->lock);
         return 0;
      }

      write_size = FIFO_WRITE_AVAIL(oha->fifo);
      if (write_size > len)
         write_size = len;
      write_size -= write_size % OHAUDIO_BYTES_PER_FRAME;
      if (write_size)
         fifo_write(oha->fifo, buf, write_size);
      slock_unlock(oha->lock);

      return (ssize_t)(write_size / OHAUDIO_BYTES_PER_FRAME);
   }

   while (written < len)
   {
      size_t write_size;

      slock_lock(oha->lock);
      if (oha->is_paused || oha->is_interrupted || oha->is_shutting_down)
      {
         slock_unlock(oha->lock);
         break;
      }

      write_size = FIFO_WRITE_AVAIL(oha->fifo);

      if (!write_size)
      {
         scond_wait_timeout(oha->cond, oha->lock, OHAUDIO_WAIT_TIMEOUT_US);
         slock_unlock(oha->lock);
         continue;
      }

      if (write_size > len - written)
         write_size = len - written;
      write_size -= write_size % OHAUDIO_BYTES_PER_FRAME;

      if (write_size)
      {
         fifo_write(oha->fifo, (const uint8_t*)buf + written, write_size);
         written += write_size;
      }

      slock_unlock(oha->lock);
   }

   return (ssize_t)(written / OHAUDIO_BYTES_PER_FRAME);
}

static size_t ohaudio_write_avail(void *data)
{
   size_t val;
   ohaudio_t *oha = (ohaudio_t*)data;

   if (!oha || !oha->fifo)
      return 0;

   slock_lock(oha->lock);
   val = (oha->is_paused || oha->is_interrupted || oha->is_shutting_down)
      ? 0 : FIFO_WRITE_AVAIL(oha->fifo);
   slock_unlock(oha->lock);

   return val;
}

static size_t ohaudio_buffer_size(void *data)
{
   ohaudio_t *oha = (ohaudio_t*)data;
   return oha ? oha->buffer_size : 0;
}

static bool ohaudio_use_float(void *data)
{
   (void)data;
   return false;
}

audio_driver_t audio_ohaudio = {
   ohaudio_init,
   ohaudio_write,
   ohaudio_stop,
   ohaudio_start,
   ohaudio_alive,
   ohaudio_set_nonblock_state,
   ohaudio_free,
   ohaudio_use_float,
   "ohaudio",
   NULL,
   NULL,
   ohaudio_write_avail,
   ohaudio_buffer_size,
   NULL
};
