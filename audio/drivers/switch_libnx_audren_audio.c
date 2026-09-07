/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2019      - misson20000
 *  Copyright (C) 2019      - m4xw
 *  Copyright (C) 2019      - lifajucejo
 *  Copyright (C) 2019      - p-sam
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
#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include <switch.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

#define BUFFER_COUNT 5

static const int sample_rate           = 48000;
static const int num_channels          = 2;
static const uint8_t sink_channels[]   = { 0, 1 };

static const AudioRendererConfig audio_renderer_config =
{
   .output_rate     = AudioRendererOutputRate_48kHz,
   .num_voices      = 24,
   .num_effects     = 0,
   .num_sinks       = 1,
   .num_mix_objs    = 1,
   .num_mix_buffers = 2,
};

typedef struct
{
   AudioDriver drv;
   void* mempool;
   AudioDriverWaveBuf wavebufs[BUFFER_COUNT];
   AudioDriverWaveBuf* current_wavebuf;
   void* current_pool_ptr;
   size_t current_size;
   size_t buffer_size;
   size_t samples;
   /* Held for every call into the renderer and for the wave buffer
    * states and the buffer being filled: write_avail() reads them on
    * the frontend's thread while the writer, on the audio thread, adds
    * and starts. */
   Mutex update_lock;
   bool nonblock;
} libnx_audren_t;

static void *libnx_audren_audio_init(
      const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   unsigned i, j;
   libnx_audren_t *aud;
   Result rc;
   int mpid;
   size_t mempool_size;
   unsigned real_latency;

   RARCH_LOG("[Audren] Using libnx_audren driver.\n");

   aud = (libnx_audren_t*)calloc(1, sizeof(libnx_audren_t));

   if (!aud)
   {
      RARCH_ERR("[Audren] struct alloc failed.\n");
      goto fail;
   }

   real_latency = MAX(5, latency);
   RARCH_LOG("[Audren] real_latency is %u.\n", real_latency);

   /* Blocking until the frontend says otherwise: it sets non-blocking
    * at init only when audio sync is off. */
   aud->nonblock     = false;
   /* The latency setting is split across the BUFFER_COUNT wave buffers
    * the renderer plays in turn, so that the buffers together hold the
    * setting and rate control - which sees the total, below - holds it
    * half full. Each buffer is sized in bytes of int16 stereo, a whole
    * number of frames. It used to be sized in frames of the whole
    * setting used as bytes: a quarter of the setting per buffer, five
    * of them in flight, and one of them reported. */
   aud->buffer_size  = ((size_t)real_latency * sample_rate / 1000)
         * num_channels * sizeof(int16_t) / BUFFER_COUNT;
   aud->buffer_size -= aud->buffer_size % (num_channels * sizeof(int16_t));
   if (aud->buffer_size < 64 * num_channels * sizeof(int16_t))
      aud->buffer_size = 64 * num_channels * sizeof(int16_t);
   aud->samples      = (aud->buffer_size / num_channels / sizeof(int16_t));
   aud->current_size = 0;
   RARCH_LOG("[Audren] %u ms as %u wave buffers of %u frames (%u ms each).\n",
         real_latency, (unsigned)BUFFER_COUNT, (unsigned)aud->samples,
         (unsigned)(aud->samples * 1000 / sample_rate));
   *new_rate         = sample_rate;

   mempool_size      = (aud->buffer_size * BUFFER_COUNT + (AUDREN_MEMPOOL_ALIGNMENT-1)) &~ (AUDREN_MEMPOOL_ALIGNMENT-1);
   aud->mempool      = memalign(AUDREN_MEMPOOL_ALIGNMENT, mempool_size);
   if (!aud->mempool)
   {
      RARCH_ERR("[Audren] mempool alloc failed.\n");
      goto fail;
   }

   rc = audrenInitialize(&audio_renderer_config);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audren] audrenInitialize: %x.\n", rc);
      goto fail;
   }

   rc = audrvCreate(&aud->drv, &audio_renderer_config, num_channels);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audren] audrvCreate: %x\n", rc);
      goto fail_init;
   }

   for(i = 0; i < BUFFER_COUNT; i++)
   {
      aud->wavebufs[i].data_raw = aud->mempool;
      aud->wavebufs[i].size = mempool_size;
      aud->wavebufs[i].start_sample_offset = i * aud->samples;
      aud->wavebufs[i].end_sample_offset = aud->wavebufs[i].start_sample_offset + aud->samples;
   }

   aud->current_wavebuf = NULL;

   mpid = audrvMemPoolAdd(&aud->drv, aud->mempool, mempool_size);
   audrvMemPoolAttach(&aud->drv, mpid);

   audrvDeviceSinkAdd(&aud->drv, AUDREN_DEFAULT_DEVICE_NAME, num_channels, sink_channels);

   rc = audrenStartAudioRenderer();
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audren] audrenStartAudioRenderer: %x.\n", rc);
   }

   audrvVoiceInit(&aud->drv, 0, num_channels, PcmFormat_Int16, sample_rate);
   audrvVoiceSetDestinationMix(&aud->drv, 0, AUDREN_FINAL_MIX_ID);
   for(i = 0; i < num_channels; i++)
   {
      for(j = 0; j < num_channels; j++)
      {
         audrvVoiceSetMixFactor(&aud->drv, 0, i == j ? 1.0f : 0.0f, i, j);
      }
   }

   mutexInit(&aud->update_lock);
   *new_rate = sample_rate;

   return aud;

fail_init:
   audrenExit();

fail:
   if (aud)
   {
      if (aud->mempool)
         free(aud->mempool);

      free(aud);
   }

   return NULL;
}

/* Free wave buffers, in bytes, plus what is left of the one being
 * filled: the room the driver has across all its stages. States are
 * brought up to date first, as the renderer only reports a finished
 * buffer on an update. */
static size_t libnx_audren_audio_room(libnx_audren_t *aud)
{
   size_t   free_bufs = 0;
   size_t   room;
   unsigned i;

   mutexLock(&aud->update_lock);
   audrvUpdate(&aud->drv);

   for (i = 0; i < BUFFER_COUNT; i++)
      if (     aud->wavebufs[i].state == AudioDriverWaveBufState_Free
            || aud->wavebufs[i].state == AudioDriverWaveBufState_Done)
         free_bufs++;

   /* The one being filled is still in the free state; it is counted
    * by what is left of it, not as a whole. */
   if (aud->current_wavebuf)
   {
      if (free_bufs)
         free_bufs--;
      room = free_bufs * aud->buffer_size
            + (aud->buffer_size - aud->current_size);
   }
   else
      room = free_bufs * aud->buffer_size;
   mutexUnlock(&aud->update_lock);
   return room;
}

static size_t libnx_audren_audio_buffer_size(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return 0;

   /* All the buffers: the most the driver holds between write() and
    * the renderer consuming it. */
   return aud->buffer_size * BUFFER_COUNT;
}

static ssize_t libnx_audren_audio_get_free_wavebuf_idx(libnx_audren_t* aud)
{
   unsigned i;

   for (i = 0; i < BUFFER_COUNT; i++)
   {
      if (
            aud->wavebufs[i].state == AudioDriverWaveBufState_Free
         || aud->wavebufs[i].state == AudioDriverWaveBufState_Done)
         return i;
   }

   return -1;
}

static size_t libnx_audren_audio_append(
      libnx_audren_t* aud, const void *s, size_t len)
{
   void *dstbuf     = NULL;
   ssize_t free_idx = -1;

   mutexLock(&aud->update_lock);
   if (!aud->current_wavebuf)
   {
      free_idx = libnx_audren_audio_get_free_wavebuf_idx(aud);
      if (free_idx == -1)
      {
         mutexUnlock(&aud->update_lock);
         return 0;
      }

      aud->current_wavebuf = &aud->wavebufs[free_idx];
      aud->current_pool_ptr = aud->mempool + (free_idx * aud->buffer_size);
      aud->current_size = 0;
   }

   if (len > aud->buffer_size - aud->current_size)
      len = aud->buffer_size - aud->current_size;

   dstbuf = aud->current_pool_ptr + aud->current_size;
   memcpy(dstbuf, s, len);
   armDCacheFlush(dstbuf, len);

   aud->current_size += len;

   if (aud->current_size == aud->buffer_size)
   {
      audrvVoiceAddWaveBuf(&aud->drv, 0, aud->current_wavebuf);
      audrvUpdate(&aud->drv);
      if (!audrvVoiceIsPlaying(&aud->drv, 0))
         audrvVoiceStart(&aud->drv, 0);
      aud->current_wavebuf = NULL;
   }
   mutexUnlock(&aud->update_lock);

   return len;
}

static ssize_t libnx_audren_audio_write(void *data,
      const void *s, size_t len)
{
   size_t _len = 0;
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return -1;

   if (aud->nonblock)
   {
      while (_len < len)
      {
         _len += libnx_audren_audio_append(
               aud, s + _len, len - _len);
         if (_len != len)
            break;
      }
   }
   else
   {
      while (_len < len)
      {
         _len += libnx_audren_audio_append(
               aud, s + _len, len - _len);
         if (_len != len)
         {
            mutexLock(&aud->update_lock);
            audrvUpdate(&aud->drv);
            mutexUnlock(&aud->update_lock);
            audrenWaitFrame();
         }
      }
   }

   return _len;
}

static bool libnx_audren_audio_stop(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return false;

   mutexLock(&aud->update_lock);
   audrvVoiceStop(&aud->drv, 0);
   mutexUnlock(&aud->update_lock);

   return true;
}

static bool libnx_audren_audio_start(void *data, bool is_shutdown)
{
   (void)is_shutdown;
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return false;

   mutexLock(&aud->update_lock);
   audrvVoiceStart(&aud->drv, 0);
   mutexUnlock(&aud->update_lock);

   return true;
}

static bool libnx_audren_audio_alive(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return false;

   return true;
}

static void libnx_audren_audio_free(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return;

   audrvVoiceStop(&aud->drv, 0);
   audrvClose(&aud->drv);
   audrenExit();

   if (aud->mempool)
   {
      free(aud->mempool);
   }

   free(aud);
}

static bool libnx_audren_audio_use_float(void *data)
{
   (void)data;
   return false; /* force S16 */
}

/* Waits, a renderer frame at a time, until the wave buffer being
 * filled has room or a free wave buffer exists for the next append to
 * take. Returns the room then, or 0 on error. A whole wave buffer is
 * at least a frame of audio, so progress does not depend on len. */
static size_t libnx_audren_audio_wait_writable(void *data, size_t len)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return 0;
   /* Capped at half the buffers, so the wait always has an end within
    * a playing renderer's reach. */
   if (len > aud->buffer_size * BUFFER_COUNT / 2)
      len = aud->buffer_size * BUFFER_COUNT / 2;

   for (;;)
   {
      size_t room = libnx_audren_audio_room(aud);
      if (room >= len)
         return room;
      audrenWaitFrame();
   }
}

static size_t libnx_audren_audio_write_avail(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return 0;

   return libnx_audren_audio_room(aud);
}

static void libnx_audren_audio_set_nonblock_state(void *data, bool state)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return;

   aud->nonblock    = state;
}

audio_driver_t audio_switch_libnx_audren = {
   libnx_audren_audio_init,
   libnx_audren_audio_write,
   libnx_audren_audio_stop,
   libnx_audren_audio_start,
   libnx_audren_audio_alive,
   libnx_audren_audio_set_nonblock_state,
   libnx_audren_audio_free,
   libnx_audren_audio_use_float,
   "switch_audren",
   NULL, /* device_list_new */
   NULL, /* device_list_free */
   libnx_audren_audio_write_avail,
   libnx_audren_audio_buffer_size,
   NULL, /* write_raw */
   libnx_audren_audio_wait_writable
};
