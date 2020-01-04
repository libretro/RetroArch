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

#include "../../retroarch.h"
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

   RARCH_LOG("[Audio]: Using libnx_audren driver\n");

   aud = (libnx_audren_t*)calloc(1, sizeof(libnx_audren_t));

   if (!aud)
   {
      RARCH_ERR("[Audio]: struct alloc failed\n");
      goto fail;
   }

   real_latency = MAX(5, latency);
   RARCH_LOG("[Audio]: real_latency is %u\n", real_latency);

   aud->nonblock     = !block_frames;
   aud->buffer_size  = (real_latency * sample_rate / 1000);
   aud->samples      = (aud->buffer_size / num_channels / sizeof(int16_t));
   aud->current_size = 0;
   *new_rate         = sample_rate;

   mempool_size      = (aud->buffer_size * BUFFER_COUNT + (AUDREN_MEMPOOL_ALIGNMENT-1)) &~ (AUDREN_MEMPOOL_ALIGNMENT-1);
   aud->mempool      = memalign(AUDREN_MEMPOOL_ALIGNMENT, mempool_size);
   if (!aud->mempool)
   {
      RARCH_ERR("[Audio]: mempool alloc failed\n");
      goto fail;
   }

   rc = audrenInitialize(&audio_renderer_config);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audio]: audrenInitialize: %x\n", rc);
      goto fail;
   }

   rc = audrvCreate(&aud->drv, &audio_renderer_config, num_channels);
   if (R_FAILED(rc))
   {
      RARCH_ERR("[Audio]: audrvCreate: %x\n", rc);
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
      RARCH_ERR("[Audio]: audrenStartAudioRenderer: %x\n", rc);
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

static size_t libnx_audren_audio_buffer_size(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return 0;

   return aud->buffer_size;
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
      libnx_audren_t* aud, const void *buf, size_t size)
{
   void *dstbuf     = NULL;
   ssize_t free_idx = -1;

   if (!aud->current_wavebuf)
   {
      free_idx = libnx_audren_audio_get_free_wavebuf_idx(aud);
      if (free_idx == -1)
         return 0;

      aud->current_wavebuf = &aud->wavebufs[free_idx];
      aud->current_pool_ptr = aud->mempool + (free_idx * aud->buffer_size);
      aud->current_size = 0;
   }

   if (size > aud->buffer_size - aud->current_size)
      size = aud->buffer_size - aud->current_size;

   dstbuf = aud->current_pool_ptr + aud->current_size;
   memcpy(dstbuf, buf, size);
   armDCacheFlush(dstbuf, size);

   aud->current_size += size;

   if (aud->current_size == aud->buffer_size)
   {
      audrvVoiceAddWaveBuf(&aud->drv, 0, aud->current_wavebuf);

      mutexLock(&aud->update_lock);
      audrvUpdate(&aud->drv);
      mutexUnlock(&aud->update_lock);

      if (!audrvVoiceIsPlaying(&aud->drv, 0))
      {
         audrvVoiceStart(&aud->drv, 0);
      }

      aud->current_wavebuf = NULL;
   }

   return size;
}

static ssize_t libnx_audren_audio_write(void *data,
      const void *buf, size_t size)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;
   size_t written      = 0;

   if (!aud)
      return -1;

   if (aud->nonblock)
   {
      while(written < size)
      {
         written += libnx_audren_audio_append(
               aud, buf + written, size - written);
         if (written != size)
            break;
      }
   }
   else
   {
      while(written < size)
      {
         written += libnx_audren_audio_append(
               aud, buf + written, size - written);
         if (written != size)
         {
            mutexLock(&aud->update_lock);
            audrvUpdate(&aud->drv);
            mutexUnlock(&aud->update_lock);
            audrenWaitFrame();
         }
      }
   }

   return written;
}

static bool libnx_audren_audio_stop(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return false;

   audrvVoiceStop(&aud->drv, 0);

   return true;
}

static bool libnx_audren_audio_start(void *data, bool is_shutdown)
{
   (void)is_shutdown;
   libnx_audren_t *aud = (libnx_audren_t*)data;

   if (!aud)
      return false;

   audrvVoiceStart(&aud->drv, 0);

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

static size_t libnx_audren_audio_write_avail(void *data)
{
   libnx_audren_t *aud = (libnx_audren_t*)data;
   size_t avail;

   if (!aud || !aud->current_wavebuf)
      return 0;

   avail = aud->buffer_size - aud->current_size;

   return avail;
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
};
