/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
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

#define CHUNK_FRAMES 256
#define CHUNK_SIZE (CHUNK_FRAMES * sizeof(uint32_t))
#define BLOCKS 4

typedef struct
{
   uint32_t data[BLOCKS][CHUNK_FRAMES];

   volatile unsigned dma_busy;
   volatile unsigned dma_next;
   volatile unsigned dma_write;
   size_t write_ptr;

   lwpq_t cond;
   bool nonblock;
} wii_audio_t;

static wii_audio_t *g_audio;

static void dma_callback(void)
{
   g_audio->dma_busy = g_audio->dma_next;
   g_audio->dma_next = (g_audio->dma_next + 1) & (BLOCKS - 1);

   DCFlushRange(g_audio->data[g_audio->dma_next], CHUNK_SIZE);
   AUDIO_InitDMA((u32)g_audio->data[g_audio->dma_next], CHUNK_SIZE);

   LWP_ThreadSignal(g_audio->cond);
}

static void *wii_audio_init(const char *device, unsigned rate, unsigned latency)
{
   static bool inited = false;
   if (!inited)
   {
      AUDIO_Init(NULL);
      AUDIO_SetDSPSampleRate(AI_SAMPLERATE_48KHZ);
      g_settings.audio.out_rate = 48000;
      AUDIO_RegisterDMACallback(dma_callback);
      inited = true;
   }

   g_audio = memalign(32, sizeof(*g_audio));
   memset(g_audio, 0, sizeof(*g_audio));
   LWP_InitQueue(&g_audio->cond);

   g_audio->dma_write = BLOCKS - 1;
   DCFlushRange(g_audio->data, sizeof(g_audio->data));
   AUDIO_InitDMA((u32)g_audio->data[g_audio->dma_next], CHUNK_SIZE);
   AUDIO_StartDMA();

   return g_audio;
}

static ssize_t wii_audio_write(void *data, const void *buf_, size_t size)
{
   wii_audio_t *wa = data;

   size_t frames = size >> 2;
   const uint32_t *buf = buf_;
   while (frames)
   {
      size_t to_write = CHUNK_FRAMES - wa->write_ptr;
      if (frames < to_write)
         to_write = frames;

      while ((wa->dma_write == wa->dma_next || wa->dma_write == wa->dma_busy) && !wa->nonblock)
         LWP_ThreadSleep(wa->cond);

      memcpy(wa->data[wa->dma_write] + wa->write_ptr, buf, to_write * sizeof(uint32_t));
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

static bool wii_audio_stop(void *data)
{
   (void)data;
   AUDIO_StopDMA();
   return true;
}

static void wii_audio_set_nonblock_state(void *data, bool state)
{
   wii_audio_t *wa = data;
   wa->nonblock = state;
}

static bool wii_audio_start(void *data)
{
   (void)data;
   AUDIO_StartDMA();
   return true;
}

static void wii_audio_free(void *data)
{
   AUDIO_StopDMA();
   if (data)
      free(data);
   g_audio = NULL;
}

const audio_driver_t audio_wii = {
   .init = wii_audio_init,
   .write = wii_audio_write,
   .stop = wii_audio_stop,
   .start = wii_audio_start,
   .set_nonblock_state = wii_audio_set_nonblock_state,
   .free = wii_audio_free,
   .ident = "wii"
};

