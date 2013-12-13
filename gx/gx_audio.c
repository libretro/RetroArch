/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include <stdlib.h>
#include <stdbool.h>
#include "../general.h"
#include <malloc.h>
#include <string.h>

#include <gccore.h>
#include <ogcsys.h>

#define CHUNK_FRAMES 64
#define CHUNK_SIZE (CHUNK_FRAMES * sizeof(uint32_t))
#define BLOCKS 16

typedef struct
{
   uint32_t data[BLOCKS][CHUNK_FRAMES];

   volatile unsigned dma_busy;
   volatile unsigned dma_next;
   volatile unsigned dma_write;
   size_t write_ptr;

   lwpq_t cond;
   bool nonblock;
} gx_audio_t;

static volatile gx_audio_t *gx_audio_data;

static void dma_callback(void)
{
   gx_audio_t *wa = (gx_audio_t*)gx_audio_data;
   // erase last chunk to avoid repeating audio
   memset(wa->data[wa->dma_busy], 0, CHUNK_SIZE);

   wa->dma_busy = wa->dma_next;
   wa->dma_next = (wa->dma_next + 1) & (BLOCKS - 1);

   DCFlushRange(wa->data[wa->dma_next], CHUNK_SIZE);
   AUDIO_InitDMA((uint32_t)wa->data[wa->dma_next], CHUNK_SIZE);

   LWP_ThreadSignal(wa->cond);
}

static void *gx_audio_init(const char *device, unsigned rate, unsigned latency)
{
   gx_audio_t *wa = (gx_audio_t*)memalign(32, sizeof(*wa));
   if (!wa)
      return NULL;

   gx_audio_data = wa;

   memset(wa, 0, sizeof(*wa));

   AUDIO_Init(NULL);
   AUDIO_RegisterDMACallback(dma_callback);

   if (rate < 33000)
   {
      AUDIO_SetDSPSampleRate(AI_SAMPLERATE_32KHZ);
      g_settings.audio.out_rate = 32000;
   }
   else
   {
      AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
      g_settings.audio.out_rate = 48000;
   }

   LWP_InitQueue(&wa->cond);

   wa->dma_write = BLOCKS - 1;
   DCFlushRange(wa->data, sizeof(wa->data));
   AUDIO_InitDMA((uint32_t)wa->data[wa->dma_next], CHUNK_SIZE);
   AUDIO_StartDMA();

   return wa;
}

// Wii uses silly R, L, R, L interleaving ...
static inline void copy_swapped(uint32_t * restrict dst, const uint32_t * restrict src, size_t size)
{
   do
   {
      uint32_t s = *src++;
      *dst++ = (s >> 16) | (s << 16);
   }while(--size);
}

static ssize_t gx_audio_write(void *data, const void *buf_, size_t size)
{
   gx_audio_t *wa = data;

   size_t frames = size >> 2;
   const uint32_t *buf = buf_;
   while (frames)
   {
      size_t to_write = CHUNK_FRAMES - wa->write_ptr;
      if (frames < to_write)
         to_write = frames;

      // FIXME: Nonblocking audio should break out of loop when it has nothing to write.
      while ((wa->dma_write == wa->dma_next || wa->dma_write == wa->dma_busy) && !wa->nonblock)
         LWP_ThreadSleep(wa->cond);

      copy_swapped(wa->data[wa->dma_write] + wa->write_ptr, buf, to_write);

      wa->write_ptr += to_write;
      frames -= to_write;
      buf += to_write;

      if (wa->write_ptr >= CHUNK_FRAMES)
      {
         wa->write_ptr -= CHUNK_FRAMES;
         wa->dma_write = (wa->dma_write + 1) & (BLOCKS - 1);
      }
   }

   return size;
}

static bool gx_audio_stop(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   AUDIO_StopDMA();
   memset(wa->data, 0, sizeof(wa->data));
   DCFlushRange(wa->data, sizeof(wa->data));
   return true;
}

static void gx_audio_set_nonblock_state(void *data, bool state)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   wa->nonblock = state;
}

static bool gx_audio_start(void *data)
{
   (void)data;
   AUDIO_StartDMA();
   return true;
}

static void gx_audio_free(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   AUDIO_StopDMA();
   AUDIO_RegisterDMACallback(NULL);
   if (wa && wa->cond)
   {
      LWP_CloseQueue(wa->cond);
      wa->cond = 0;
   }
   if (data)
      free(data);
   wa = NULL;
}

static size_t gx_audio_write_avail(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   return ((wa->dma_busy - wa->dma_write + BLOCKS) & (BLOCKS - 1)) * CHUNK_SIZE;
}

static size_t gx_audio_buffer_size(void *data)
{
   (void)data;
   return BLOCKS * CHUNK_SIZE;
}

const audio_driver_t audio_gx = {
   .init = gx_audio_init,
   .write = gx_audio_write,
   .stop = gx_audio_stop,
   .start = gx_audio_start,
   .set_nonblock_state = gx_audio_set_nonblock_state,
   .free = gx_audio_free,
   .ident = "gx",
   .write_avail = gx_audio_write_avail,
   .buffer_size = gx_audio_buffer_size,
};

