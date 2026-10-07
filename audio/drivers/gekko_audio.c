/*  RetroArch - A frontend for libretro.
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

/* Audio on os/gekko: a ring of small chunks the audio DMA plays in
 * turn.  The DMA's callback, at the start of each chunk, silences the
 * one it retired and queues the next, so a writer that falls behind
 * is heard as silence rather than a repeat. */

#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include <gekko/audio.h>
#include <gekko/thread.h>

#include <boolean.h>
#include <retro_inline.h>

#include "../audio_driver.h"

#define CHUNK_FRAMES 64
#define CHUNK_SIZE   (CHUNK_FRAMES * sizeof(uint32_t))
#define BLOCKS       16

typedef struct
{
   uint32_t data[BLOCKS][CHUNK_FRAMES];
   /* The device clock fit, touched only by device_clock_ppm(): sums
    * in seconds and frames from an anchor, so cancellation does not
    * eat the answer. */
   double   clk_sx, clk_sy, clk_sxx, clk_sxy, clk_n;
   size_t   write_ptr;
   /* Shared with the callback.  One core and no store buffer, so
    * volatile is enough; 32-bit so reads do not tear. */
   volatile uint32_t dma_busy;
   volatile uint32_t dma_next;
   volatile uint32_t dma_write;
   volatile uint32_t chunks;      /* bumped per chunk; writers sleep on it */
   volatile uint32_t consumed;    /* frames played since the start */
   volatile uint32_t underruns;   /* chunks played that were not written */
   /* (position, time) per chunk for the clock fit, between two bumps
    * of clk_seq so a reader can tell a torn pair. */
   volatile uint32_t clk_pos;
   volatile uint32_t clk_tb;
   volatile uint32_t clk_seq;
   uint32_t clk_last_seq;
   uint32_t clk_anchor_pos;
   uint32_t clk_anchor_tb;
   unsigned rate;
   int      clk_ppm;
   uint8_t  clk_have_anchor;
   uint8_t  clk_valid;
   bool     nonblock;
   bool     is_paused;
   bool     stopping;
} gekko_audio_t;

/* Interrupt context: the DMA has started on dma_next. */
static void gekko_audio_chunk(void *data)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   if (wa->stopping)
      return;
   memset(wa->data[wa->dma_busy], 0, CHUNK_SIZE);
   wa->dma_busy = wa->dma_next;
   wa->dma_next = (wa->dma_next + 1) & (BLOCKS - 1);
   if (wa->dma_next == wa->dma_write)
      wa->underruns++;
   gk_dcache_flush(wa->data[wa->dma_next], CHUNK_SIZE);
   gk_audio_queue(wa->data[wa->dma_next], CHUNK_SIZE);
   wa->consumed += CHUNK_FRAMES;
   wa->clk_seq++;
   wa->clk_pos = wa->consumed;
   wa->clk_tb  = (uint32_t)gk_ticks();
   wa->clk_seq++;
   wa->chunks++;
   gk_futex_wake(&wa->chunks, 0x7fffffff);
}

static void *gekko_audio_init(const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   gekko_audio_t *wa = (gekko_audio_t*)memalign(32, sizeof(*wa));
   if (!wa)
      return NULL;
   memset(wa, 0, sizeof(*wa));

   /* The AI runs at 32 or 48 kHz: below 32001, and the 40000-47999
    * range the setting steps down through from 48000, take 32000. */
   wa->rate  = (rate <= 32000 || (rate >= 40000 && rate < 48000))
      ? 32000 : 48000;
   *new_rate = wa->rate;

   wa->dma_write = BLOCKS - 1;
   gk_dcache_flush(wa->data, sizeof(wa->data));
   gk_audio_init(wa->rate);
   gk_audio_set_cb(gekko_audio_chunk, wa);
   gk_audio_queue(wa->data[wa->dma_next], CHUNK_SIZE);
   gk_audio_start();
   return wa;
}

/* The AI takes each frame right channel first: a half-word rotate,
 * not a byte swap. */
static INLINE void gekko_audio_lr_swap(uint32_t *dst, const uint32_t *src,
      size_t len)
{
   size_t n4 = len >> 2;
   while (n4--)
   {
      uint32_t s0 = src[0], s1 = src[1], s2 = src[2], s3 = src[3];
      dst[0] = (s0 >> 16) | (s0 << 16);
      dst[1] = (s1 >> 16) | (s1 << 16);
      dst[2] = (s2 >> 16) | (s2 << 16);
      dst[3] = (s3 >> 16) | (s3 << 16);
      src   += 4;
      dst   += 4;
   }
   for (len &= 3; len; --len)
   {
      uint32_t s = *src++;
      *dst++ = (s >> 16) | (s << 16);
   }
}

/* Sleep until the callback retires another chunk. */
static void gekko_audio_wait_chunk(gekko_audio_t *wa)
{
   uint32_t seen = wa->chunks;
   gk_futex_wait(&wa->chunks, seen, GK_US_TO_TICKS(20000));
}

static ssize_t gekko_audio_write(void *data, const void *buf_, size_t len)
{
   gekko_audio_t *wa  = (gekko_audio_t*)data;
   const uint32_t *buf = (const uint32_t*)buf_;
   size_t frames      = len >> 2;

   while (frames)
   {
      size_t to_write = CHUNK_FRAMES - wa->write_ptr;
      if (frames < to_write)
         to_write = frames;

      /* The chunk to fill is queued or playing: the ring is full. */
      while (wa->dma_write == wa->dma_next || wa->dma_write == wa->dma_busy)
      {
         if (wa->nonblock || wa->stopping || wa->is_paused)
            return (ssize_t)(len - (frames << 2));
         gekko_audio_wait_chunk(wa);
      }

      gekko_audio_lr_swap(wa->data[wa->dma_write] + wa->write_ptr, buf,
            to_write);
      wa->write_ptr += to_write;
      frames        -= to_write;
      buf           += to_write;

      if (wa->write_ptr >= CHUNK_FRAMES)
      {
         wa->write_ptr -= CHUNK_FRAMES;
         wa->dma_write  = (wa->dma_write + 1) & (BLOCKS - 1);
      }
   }
   return (ssize_t)len;
}

static bool gekko_audio_stop(void *data)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   if (!wa)
      return false;
   gk_audio_stop();
   memset(wa->data, 0, sizeof(wa->data));
   gk_dcache_flush(wa->data, sizeof(wa->data));
   wa->is_paused = true;
   return true;
}

static bool gekko_audio_start(void *data, bool is_shutdown)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   if (!wa)
      return false;
   gk_audio_start();
   wa->is_paused = false;
   return true;
}

static bool gekko_audio_alive(void *data)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   return wa && !wa->is_paused;
}

static void gekko_audio_set_nonblock_state(void *data, bool state)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   if (wa)
      wa->nonblock = state;
}

static void gekko_audio_free(void *data)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   if (!wa)
      return;
   wa->stopping = true;
   gk_audio_stop();
   gk_audio_set_cb(NULL, NULL);
   free(wa);
}

static size_t gekko_audio_write_avail(void *data)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   return ((wa->dma_busy - wa->dma_write + BLOCKS) & (BLOCKS - 1))
      * CHUNK_SIZE;
}

static size_t gekko_audio_buffer_size(void *data)
{
   return BLOCKS * CHUNK_SIZE;
}

/* Until len bytes are free (at most half the ring, so it ends), or 0
 * while stopped. */
static size_t gekko_audio_wait_writable(void *data, size_t len)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   if (len > (BLOCKS * CHUNK_SIZE) / 2)
      len = (BLOCKS * CHUNK_SIZE) / 2;
   for (;;)
   {
      size_t avail;
      if (wa->stopping || wa->is_paused)
         return 0;
      if ((avail = gekko_audio_write_avail(wa)) >= len)
         return avail;
      gekko_audio_wait_chunk(wa);
   }
}

static size_t gekko_audio_frames_consumed(void *data)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   return wa ? (size_t)wa->consumed : 0;
}

/* 16-bit PCM is all the AI takes. */
static bool gekko_audio_use_float(void *data) { return false; }

static size_t gekko_audio_underruns(void *data)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   return wa ? (size_t)wa->underruns : 0;
}

static double ticks_to_seconds(uint32_t ticks)
{
   return (double)ticks / (double)gk_tb_hz;
}

/* The AI's clock against the rate reported, in parts per million: a
 * least-squares fit of frames played against time over a window of a
 * second or more.  A measurement; nothing acts on it. */
static bool gekko_audio_device_clock_ppm(void *data, double *ppm)
{
   gekko_audio_t *wa = (gekko_audio_t*)data;
   uint32_t s1 = 0, s2 = 0, pos = 0, tb = 0;
   int tries = 4;

   if (!wa || !wa->rate)
      return false;

   do
   {
      s1  = wa->clk_seq;
      pos = wa->clk_pos;
      tb  = wa->clk_tb;
      s2  = wa->clk_seq;
   } while ((s1 != s2 || (s1 & 1u)) && --tries > 0);

   if (s1 == s2 && !(s1 & 1u) && s1 != wa->clk_last_seq)
   {
      wa->clk_last_seq = s1;
      /* A new anchor at the start, after the DMA restarted, and well
       * inside the wrap of the 32-bit time base sampled. */
      if (     !wa->clk_have_anchor
            || pos - wa->clk_anchor_pos > 0x80000000u
            || (uint32_t)(pos - wa->clk_anchor_pos) > wa->rate * 31u
            || ticks_to_seconds(tb - wa->clk_anchor_tb) > 30.0)
      {
         wa->clk_anchor_pos  = pos;
         wa->clk_anchor_tb   = tb;
         wa->clk_have_anchor = 1;
         wa->clk_valid       = 0;
         wa->clk_sx = wa->clk_sy = wa->clk_sxx = wa->clk_sxy = 0.0;
         wa->clk_n  = 0.0;
      }
      else
      {
         double x = ticks_to_seconds(tb - wa->clk_anchor_tb);
         double y = (double)(uint32_t)(pos - wa->clk_anchor_pos);
         wa->clk_sx  += x;
         wa->clk_sy  += y;
         wa->clk_sxx += x * x;
         wa->clk_sxy += x * y;
         wa->clk_n   += 1.0;
         if (wa->clk_n >= 4.0 && x >= 1.0)
         {
            double denom = wa->clk_n * wa->clk_sxx - wa->clk_sx * wa->clk_sx;
            if (denom > 0.0)
            {
               double slope = (wa->clk_n * wa->clk_sxy
                     - wa->clk_sx * wa->clk_sy) / denom;
               double est   = (slope / (double)wa->rate - 1.0) * 1000000.0;
               if (est > -100000.0 && est < 100000.0)
               {
                  wa->clk_ppm   = (int)est;
                  wa->clk_valid = 1;
               }
            }
         }
      }
   }

   if (!wa->clk_valid)
      return false;
   *ppm = (double)wa->clk_ppm;
   return true;
}

audio_driver_t audio_gx = {
   gekko_audio_init,
   gekko_audio_write,
   gekko_audio_stop,
   gekko_audio_start,
   gekko_audio_alive,
   gekko_audio_set_nonblock_state,
   gekko_audio_free,
   gekko_audio_use_float,
   "gx",
   NULL,
   NULL,
   gekko_audio_write_avail,
   gekko_audio_buffer_size,
   NULL, /* write_raw */
   gekko_audio_wait_writable,
   gekko_audio_frames_consumed,
   gekko_audio_underruns,
   NULL, /* layout */
   NULL, /* frames_consumed_fallback */
   gekko_audio_device_clock_ppm
};
