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
#include <malloc.h>

#ifdef GEKKO
#include <gccore.h>
#include <ogcsys.h>
#include <ogc/lwp_watchdog.h>
#else
#include <cafe/ai.h>
#endif

#include <boolean.h>
#include <features/features_cpu.h>
#include <retro_inline.h>

#include <defines/gx_defines.h>

#include "../audio_driver.h"

#define CHUNK_FRAMES 64
#define CHUNK_SIZE (CHUNK_FRAMES * sizeof(uint32_t))
#define BLOCKS 16

typedef struct
{
   size_t write_ptr;
   uint32_t data[BLOCKS][CHUNK_FRAMES];
   volatile unsigned dma_busy;
   volatile unsigned dma_next;
   volatile unsigned dma_write;
   /* Thread queue the DMA callback signals after every chunk, so a
    * waiter sleeps a chunk at a time instead of spinning. Same pattern
    * the video driver uses for the retrace callback. */
   OSCond dma_cond;
   /* Frames the DMA has played since it started, for the sink rate
    * estimate. Written only by the DMA callback and read by the
    * frontend; volatile rather than atomic because this is a
    * single-core CPU with no store buffer to reorder around, which is
    * the same reason dma_busy and dma_next above are declared this
    * way. 32-bit so the read cannot tear: at 48 kHz it wraps in about
    * a day, which the estimator's differences handle. */
   volatile uint32_t consumed;
   /* Chunks the DMA started on that the writer had not reached: the
    * callback zeroes each chunk as it retires it, so what goes out is
    * silence. Volatile rather than atomic for the reason consumed is. */
   volatile uint32_t underruns;
   /* One (position, time) pair per chunk for the device clock, taken
    * in the callback because that is where the position is exact:
    * sampling consumed from the frontend would be a chunk out, which
    * over any usable window is more error than the figure being
    * measured. clk_seq brackets the pair so the reader can tell it
    * was not caught mid-write.
    *
    * The fit itself is done in device_clock_ppm(), on the frontend's
    * thread. This callback is a DMA interrupt, and floating point
    * there is not a thing to be doing.
    *
    * clk_tb is the PPC timebase, which is what
    * cpu_features_get_perf_counter() reads here - a counter, not a
    * clock. ticks_to_microsecs() is its scale; features_cpu.h offers
    * the counter and no frequency beside it. */
   volatile uint32_t clk_pos;
   volatile uint32_t clk_tb;
   volatile uint32_t clk_seq;
   /* The fit, touched only by device_clock_ppm(). Sums in seconds and
    * frames from the anchor, because a fit on raw values loses its
    * answer to cancellation. The anchor is retaken well inside the
    * 32-bit timebase's wrap. */
   uint32_t clk_last_seq;
   uint32_t clk_anchor_pos;
   uint32_t clk_anchor_tb;
   int      clk_have_anchor;
   int      clk_valid;
   int      clk_ppm;
   double   clk_sx, clk_sy, clk_sxx, clk_sxy, clk_n;
   unsigned rate;
   bool nonblock;
   bool is_paused;
} gx_audio_t;

/* The three IRQ-shared fields inside gx_audio_t are individually
 * declared volatile, so the pointer itself does not need to be
 * volatile-qualified. Doing so would propagate volatile to the 4 KB
 * `data` member as well, killing any optimisation across the hot
 * sample-copy path. */
static gx_audio_t *gx_audio_data = NULL;
static volatile bool stop_audio  = false;

static void gx_audio_dma_callback(void)
{
   gx_audio_t *wa = gx_audio_data;

   if (stop_audio)
      return;

   /* Erase last chunk to avoid repeating audio. */
   memset(wa->data[wa->dma_busy], 0, CHUNK_SIZE);

   wa->dma_busy = wa->dma_next;
   wa->dma_next = (wa->dma_next + 1) & (BLOCKS - 1);

   /* The chunk the DMA is about to take is one the writer has not
    * filled, and the callback erased it as it retired. */
   if (wa->dma_next == wa->dma_write)
      wa->underruns++;

   DCFlushRange(wa->data[wa->dma_next], CHUNK_SIZE);

   AIInitDMA((uint32_t)wa->data[wa->dma_next], CHUNK_SIZE);
   /* A chunk the DMA has finished with: device time. */
   wa->consumed += CHUNK_FRAMES;

   /* The pair the clock fit reads, published between two bumps of the
    * sequence so a reader can tell a torn one. */
   wa->clk_seq++;
   wa->clk_pos = wa->consumed;
   wa->clk_tb  = (uint32_t)cpu_features_get_perf_counter();
   wa->clk_seq++;
   OSSignalCond(wa->dma_cond);
}

/* Frames the DMA has played since it started. The callback fires once
 * per chunk, so counting chunks counts device time; there is no queue
 * to subtract, because the callback is the hardware finishing with a
 * chunk rather than a queue being filled. */
static size_t gx_audio_frames_consumed(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   if (!wa)
      return 0;
   return (size_t)wa->consumed;
}

static void *gx_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   gx_audio_t *wa = (gx_audio_t*)memalign(32, sizeof(*wa));
   if (!wa)
      return NULL;

   memset(wa, 0, sizeof(*wa));

   AIInit(NULL);
   AIRegisterDMACallback(gx_audio_dma_callback);

   /* Ranges 0-32000 (default low) and 40000-47999
      (in settings going down from 48000) -> set to 32000 hz */
   if (rate <= 32000 || (rate >= 40000 && rate < 48000))
   {
      AISetDSPSampleRate(AI_SAMPLERATE_32KHZ);
      *new_rate = 32000;
      wa->rate  = 32000;
   }
   else /* Ranges 32001-39999 (in settings going up from 32000) and 48000-max (default high) -> set to 48000 hz */
   {
      AISetDSPSampleRate(AI_SAMPLERATE_48KHZ);
      *new_rate = 48000;
      wa->rate  = 48000;
   }

   wa->dma_write = BLOCKS - 1;
   DCFlushRange(wa->data, sizeof(wa->data));
   stop_audio    = false;
   OSInitThreadQueue(&wa->dma_cond);

   /* Publish to the IRQ callback only after the struct is fully
    * initialised. AIInitDMA arms the DMA engine which is what kicks
    * the first callback. */
   gx_audio_data = wa;

   AIInitDMA((uint32_t)wa->data[wa->dma_next], CHUNK_SIZE);
   AIStartDMA();

   return wa;
}

/* Wii uses silly R, L, R, L interleaving.
 * This is *not* an endianness swap - it is a 16/16 rotate to match
 * the AI hardware's L/R lane order on big-endian PowerPC. */
static INLINE void gx_audio_lr_swap(
      uint32_t * restrict dst,
      const uint32_t * restrict src, size_t len)
{
   size_t n4 = len >> 2;
   /* Unroll the bulk of the copy. CHUNK_FRAMES is 64, so callers
    * with `to_write == CHUNK_FRAMES` (the common case from gx_audio_write
    * once the buffer is primed) never touch the tail loop. */
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

static ssize_t gx_audio_write(void *data, const void *buf_, size_t len)
{
   size_t       frames = len >> 2;
   const uint32_t *buf = buf_;
   gx_audio_t      *wa = data;

   while (frames)
   {
      size_t to_write = CHUNK_FRAMES - wa->write_ptr;

      if (frames < to_write)
         to_write = frames;

      /* The chunk to fill is the one queued for DMA (dma_next) or the
       * one the DMA is playing (dma_busy): the ring is full.  Sleep on
       * the DMA callback's queue a chunk at a time, as wait_writable()
       * does, rather than spin; and give up when there is no callback
       * coming - stopped or paused, the spin never ended - or when the
       * caller does not want to wait, returning what went in rather
       * than writing over the chunk being played. */
      while (    wa->dma_write == wa->dma_next
              || wa->dma_write == wa->dma_busy)
      {
         if (wa->nonblock || stop_audio || wa->is_paused)
            return (ssize_t)(len - (frames << 2));
         OSSleepThread(wa->dma_cond);
      }

      gx_audio_lr_swap(wa->data[wa->dma_write] + wa->write_ptr,
            buf, to_write);

      wa->write_ptr += to_write;
      frames        -= to_write;
      buf           += to_write;

      if (wa->write_ptr >= CHUNK_FRAMES)
      {
         wa->write_ptr -= CHUNK_FRAMES;
         wa->dma_write = (wa->dma_write + 1) & (BLOCKS - 1);
      }
   }
   return len;
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
   if (wa->dma_cond)
      OSCloseThreadQueue(wa->dma_cond);

   free(data);
}

static size_t gx_audio_write_avail(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   return ((wa->dma_busy - wa->dma_write + BLOCKS)
         & (BLOCKS - 1)) * CHUNK_SIZE;
}

static size_t gx_audio_buffer_size(void *data) { return BLOCKS * CHUNK_SIZE; }

/* Sleep on the DMA callback's queue until at least len bytes of chunks
 * are free, len capped at half the ring so the wait always ends. A
 * chunk completing between the check and the sleep costs one more
 * chunk of waiting, not a hang. Returns the free space then, or 0 while
 * stopped. */
static size_t gx_audio_wait_writable(void *data, size_t len)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   size_t avail;

   if (len > (BLOCKS * CHUNK_SIZE) / 2)
      len = (BLOCKS * CHUNK_SIZE) / 2;

   for (;;)
   {
      if (stop_audio || wa->is_paused)
         return 0;
      avail = gx_audio_write_avail(wa);
      if (avail >= len)
         return avail;
      OSSleepThread(wa->dma_cond);
   }
}
/* Not a stub: the Audio Interface takes signed 16-bit big-endian
 * stereo PCM by DMA and nothing else - AI_CONTROL selects only the
 * rate, and libogc's aesnd/asnd voices are 8- and 16-bit too - so the
 * frontend's float-to-int16 conversion before write() is the only
 * place float can go. The dispatcher derefs ->use_float unconditionally,
 * so the slot cannot be NULL either. */
static bool gx_audio_use_float(void *data) { return false; }

static size_t gx_audio_underruns(void *data)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   return wa ? (size_t)wa->underruns : 0;
}

/* The AI's own clock against the rate this driver reports, in parts
 * per million. What the hardware does with the rate it was asked for
 * is not the rate it was asked for: the AI divides its source clock by
 * a figure that does not land on 48000, and nothing downstream has
 * ever been told. A measurement; nothing acts on it. */
static bool gx_audio_device_clock_ppm(void *data, double *ppm)
{
   gx_audio_t *wa = (gx_audio_t*)data;
   uint32_t s1 = 0, s2 = 0, pos = 0, tb = 0;
   int tries      = 4;

   if (!wa || !wa->rate)
      return false;

   /* A pair the callback was not in the middle of writing. */
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

      /* Retaken on the first pair, when the DMA has restarted and the
       * position went backwards, and well inside the wrap of the
       * 32-bit timebase this samples. The frame bound catches the
       * case the tick bound cannot: nothing reads this while the
       * statistics are off, and a gap long enough to wrap the
       * timebase comes back looking like a short interval. */
      if (     !wa->clk_have_anchor
            || pos - wa->clk_anchor_pos > 0x80000000u
            || (uint32_t)(pos - wa->clk_anchor_pos) > wa->rate * 31u
            || (uint64_t)ticks_to_microsecs(
                  (uint64_t)(uint32_t)(tb - wa->clk_anchor_tb)) > 30000000ull)
      {
         wa->clk_anchor_pos  = pos;
         wa->clk_anchor_tb   = tb;
         wa->clk_have_anchor = 1;
         wa->clk_valid       = 0;
         wa->clk_sx = wa->clk_sy = wa->clk_sxx = wa->clk_sxy = wa->clk_n = 0.0;
      }
      else
      {
         double x = (double)(uint64_t)ticks_to_microsecs(
               (uint64_t)(uint32_t)(tb - wa->clk_anchor_tb)) / 1000000.0;
         double y = (double)(uint32_t)(pos - wa->clk_anchor_pos);

         wa->clk_sx  += x;
         wa->clk_sy  += y;
         wa->clk_sxx += x * x;
         wa->clk_sxy += x * y;
         wa->clk_n   += 1.0;

         /* A second of window at least, as the interface asks. */
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
   NULL, /* write_raw */
   gx_audio_wait_writable,
   gx_audio_frames_consumed,
   gx_audio_underruns,
   NULL, /* layout */
   NULL, /* frames_consumed_fallback */
   gx_audio_device_clock_ppm
};
