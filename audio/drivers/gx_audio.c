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

#include <stdlib.h>
#include <string.h>

#ifdef GEKKO
#include <gccore.h>
#include <ogcsys.h>
#else
#include <cafe/ai.h>
#endif

#include <boolean.h>
#include <retro_inline.h>

#include "../../retroarch.h"
#include "../../defines/gx_defines.h"

typedef struct
{
   uint32_t data[BLOCKS][CHUNK_FRAMES];

   volatile unsigned dma_busy;
   volatile unsigned dma_next;
   volatile unsigned dma_write;
   size_t write_ptr;

   bool nonblock;
   bool is_paused;
} gx_audio_t;

static volatile gx_audio_t *gx_audio_data = NULL;
static volatile bool stop_audio           = false;

static void dma_callback(void)
{
   gx_audio_t *wa = (gx_audio_t*)gx_audio_data;

   if (stop_audio)
      return;

   /* Erase last chunk to avoid repeating audio. */
   memset(wa->data[wa->dma_busy], 0, CHUNK_SIZE);

   wa->dma_busy = wa->dma_next;
   wa->dma_next = (wa->dma_next + 1) & (BLOCKS - 1);

   DCFlushRange(wa->data[wa->dma_next], CHUNK_SIZE);

   AIInitDMA((uint32_t)wa->data[wa->dma_next], CHUNK_SIZE);
}

static void *gx_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   gx_audio_t *wa       = (gx_audio_t*)memalign(32, sizeof(*wa));
   if (!wa)
      return NULL;

   gx_audio_data = (gx_audio_t*)wa;

   memset(wa, 0, sizeof(*wa));

   AIInit(NULL);
   AIRegisterDMACallback(dma_callback);

   //ranges 0-32000 (default low) and 40000-47999 (in settings going down from 48000) -> set to 32000 hz
   if (rate <= 32000 || (rate >= 40000 && rate < 48000))
   {
      AISetDSPSampleRate(AI_SAMPLERATE_32KHZ);
      *new_rate = 32000;
   }
   else //ranges 32001-39999 (in settings going up from 32000) and 48000-max (default high) -> set to 48000 hz
   {
      AISetDSPSampleRate(AI_SAMPLERATE_48KHZ);
      *new_rate = 48000;
   }

   wa->dma_write = BLOCKS - 1;
   DCFlushRange(wa->data, sizeof(wa->data));
   stop_audio = false;
   AIInitDMA((uint32_t)wa->data[wa->dma_next], CHUNK_SIZE);
   AIStartDMA();

   return wa;
}

/* Wii uses silly R, L, R, L interleaving. */
static INLINE void copy_swapped(uint32_t * restrict dst,
      const uint32_t * restrict src, size_t size)
{
   do
   {
      uint32_t s = *src++;
      *dst++ = (s >> 16) | (s << 16);
   }while(--size);
}

static ssize_t gx_audio_write(void *data, const void *buf_, size_t size)
{
   size_t       frames = size >> 2;
   const uint32_t *buf = buf_;
   gx_audio_t      *wa = data;

   while (frames)
   {
      size_t to_write = CHUNK_FRAMES - wa->write_ptr;

      if (frames < to_write)
         to_write = frames;

      /* FIXME: Nonblocking audio should break out of loop
       * when it has nothing to write. */
      while ((wa->dma_write == wa->dma_next ||
               wa->dma_write == wa->dma_busy) && !wa->nonblock);

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

   if (!wa)
      return false;

   AIStopDMA();
   memset(wa->data, 0, sizeof(wa->data));
   DCFlushRange(wa->data, sizeof(wa->data));
   wa->is_paused = true;
   return true;
}

static void gx_audio_set_nonblock_state(void *data, bool state)
{
   gx_audio_t *wa = (gx_audio_t*)data;

   if (wa)
      wa->nonblock = state;
}

static bool gx_audio_start(void *data, bool is_shutdown)
{
   gx_audio_t *wa = (gx_audio_t*)data;

   if (!wa)
      return false;

   AIStartDMA();
   wa->is_paused = false;
   return true;
}

static bool gx_audio_alive(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   if (!wa)
      return false;
   return !wa->is_paused;
}

static void gx_audio_free(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;

   if (!wa)
      return;

   stop_audio = true;
   AIStopDMA();
   AIRegisterDMACallback(NULL);

   free(data);
}

static size_t gx_audio_write_avail(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   return ((wa->dma_busy - wa->dma_write + BLOCKS)
         & (BLOCKS - 1)) * CHUNK_SIZE;
}

static size_t gx_audio_buffer_size(void *data)
{
   (void)data;
   return BLOCKS * CHUNK_SIZE;
}

static bool gx_audio_use_float(void *data)
{
   /* TODO/FIXME - verify */
   (void)data;
   return false;
}

audio_driver_t audio_gx = {
   gx_audio_init,
   gx_audio_write,
   gx_audio_stop,
   gx_audio_start,
   gx_audio_alive,
   gx_audio_set_nonblock_state,
   gx_audio_free,
   gx_audio_use_float,
   "gx",
   NULL,
   NULL,
   gx_audio_write_avail,
   gx_audio_buffer_size,
};
