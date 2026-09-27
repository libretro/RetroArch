/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <3ds.h>
#include <string.h>

#include <retro_atomic.h>
#include <features/features_cpu.h>
#include <malloc.h>

#include "../audio_driver.h"
#include "../../ctr/ctr_debug.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

/* libctru defines this from 1.5 on; it is the SoC clock by sixteen. */
#ifndef SYSCLOCK_ARM11
#define SYSCLOCK_ARM11 (16756991u * 8u * 2u)
#endif

/* The rate the channel is set to and the rate the frontend is told,
 * which have to be the one number: the channel interpolates with
 * NDSP_INTERP_NONE, so anything the frontend resamples to that the
 * channel is not playing at is error with nothing to correct it. */
#define CTR_DSP_AUDIO_RATE 32728

typedef struct
{
   ndspWaveBuf dsp_buf; /* TODO/FIXME - find out alignment */
   int channel;
   uint32_t pos;
   /* Signalled from the DSP frame callback so a waiter wakes once per
    * DSP frame instead of sleeping and polling the sample position. */
   LightEvent frame_event;
   /* The channel's sample position last time the frame callback looked,
    * and the frames it has played since the channel started, for the
    * sink rate estimate. ndspChnGetSamplePos() wraps at
    * CTR_DSP_AUDIO_COUNT - 2048 samples, about 43 ms at 48 kHz - so it
    * has to be unwrapped more often than that. The DSP frame callback
    * fires every few milliseconds, which is where this is done; doing
    * it in frames_consumed() would not work, since the estimator reads
    * that once every four seconds and would miss ninety wraps. */
   uint32_t last_pos;
   retro_atomic_size_t consumed;
   /* The DSP's clock against the rate the channel is set to, fitted
    * from the sample position this callback already reads and
    * svcGetSystemTick(), which counts in the ARM11's domain rather
    * than the DSP's. Accumulated in the callback, which owns all of
    * it, and published as one int in ppm.
    *
    * cpu_features_get_perf_counter() and not
    * cpu_features_get_time_usec(): the counter is svcGetSystemTick()
    * here, the ARM11's own tick; the clock is osGetTime(), which
    * resolves in milliseconds - a hundred parts per million of
    * quantisation on a ten second window, against a figure measured
    * in single parts. SYSCLOCK_ARM11 is the counter's scale, since
    * features_cpu.h offers no frequency beside it.
    *
    * A measurement. Nothing acts on it. */
   uint64_t clk_pos;
   uint64_t clk_anchor_pos;
   uint64_t clk_anchor_tick;
   int      clk_have_anchor;
   double   clk_sx, clk_sy, clk_sxx, clk_sxy, clk_n;
   retro_atomic_int_t clk_ppm; /* AUDIO_CLOCK_PPM_NONE until known */
   bool nonblock;
   bool playing;
} ctr_dsp_audio_t;

#define CTR_DSP_AUDIO_COUNT       (1u << 11u)
#define CTR_DSP_AUDIO_COUNT_MASK  (CTR_DSP_AUDIO_COUNT - 1u)
#define CTR_DSP_AUDIO_SIZE        (CTR_DSP_AUDIO_COUNT * sizeof(int16_t) * 2)

/* One (position, tick) pair into the fit. Runs on the DSP callback's
 * own thread, which owns every field it touches here. */
static void ctr_dsp_audio_clock_sample(ctr_dsp_audio_t *ctr, uint32_t frames)
{
   uint64_t tick = (uint64_t)cpu_features_get_perf_counter();
   double   x, y, denom;

   ctr->clk_pos += frames;

   /* Taken on the first frame, on a position that went backwards, and
    * again once the window has run long enough to be worth
    * restarting. */
   if (     !ctr->clk_have_anchor
         || tick <= ctr->clk_anchor_tick
         || (tick - ctr->clk_anchor_tick) > (uint64_t)SYSCLOCK_ARM11 * 30)
   {
      ctr->clk_anchor_pos  = ctr->clk_pos;
      ctr->clk_anchor_tick = tick;
      ctr->clk_have_anchor = 1;
      ctr->clk_sx = ctr->clk_sy = ctr->clk_sxx = ctr->clk_sxy
                  = ctr->clk_n = 0.0;
      retro_atomic_store_release_int(&ctr->clk_ppm, AUDIO_CLOCK_PPM_NONE);
      return;
   }

   /* Seconds and frames from the anchor: a fit on the raw values
    * loses its answer to cancellation. */
   x = (double)(tick - ctr->clk_anchor_tick) / (double)SYSCLOCK_ARM11;
   y = (double)(ctr->clk_pos - ctr->clk_anchor_pos);

   ctr->clk_sx  += x;
   ctr->clk_sy  += y;
   ctr->clk_sxx += x * x;
   ctr->clk_sxy += x * y;
   ctr->clk_n   += 1.0;

   /* A second of window at least, as the interface asks. */
   if (ctr->clk_n < 4.0 || x < 1.0)
      return;

   denom = ctr->clk_n * ctr->clk_sxx - ctr->clk_sx * ctr->clk_sx;
   if (denom <= 0.0)
      return;

   {
      double slope = (ctr->clk_n * ctr->clk_sxy - ctr->clk_sx * ctr->clk_sy)
            / denom;
      double ppm   = (slope / (double)CTR_DSP_AUDIO_RATE - 1.0) * 1000000.0;
      if (ppm > -100000.0 && ppm < 100000.0)
      {
         retro_atomic_store_release_int(&ctr->clk_ppm, (int)ppm);
      }
   }
}

static void ctr_dsp_audio_frame_cb(void *data)
{
   ctr_dsp_audio_t *ctr = (ctr_dsp_audio_t*)data;
   uint32_t pos         = ndspChnGetSamplePos(ctr->channel);
   /* Unsigned subtraction and the mask do the unwrap: the difference is
    * right across the wrap without a comparison. Only this callback
    * writes either field. */
   uint32_t frames      = (pos - ctr->last_pos) & CTR_DSP_AUDIO_COUNT_MASK;

   retro_atomic_fetch_add_size(&ctr->consumed, (size_t)frames);
   ctr->last_pos = pos;

   ctr_dsp_audio_clock_sample(ctr, frames);

   LightEvent_Signal(&ctr->frame_event);
}

/* Frames the channel has played since it started; see the note on
 * last_pos above for why the unwrap lives in the callback. */
static size_t ctr_dsp_audio_frames_consumed(void *data)
{
   ctr_dsp_audio_t *ctr = (ctr_dsp_audio_t*)data;
   if (!ctr)
      return 0;
   return retro_atomic_load_acquire_size(&ctr->consumed);
}
#define CTR_DSP_AUDIO_SIZE_MASK   (CTR_DSP_AUDIO_SIZE  - 1u)

static void *ctr_dsp_audio_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   ctr_dsp_audio_t *ctr = NULL;

   (void)device;
   (void)rate;
   (void)latency;

   if (ndspInit() < 0)
      return NULL;

   ctr = (ctr_dsp_audio_t*)calloc(1, sizeof(ctr_dsp_audio_t));
   if (!ctr)
   {
      ndspExit();
      return NULL;
   }
   retro_atomic_int_init(&ctr->clk_ppm, AUDIO_CLOCK_PPM_NONE);
   LightEvent_Init(&ctr->frame_event, RESET_ONESHOT);
   ndspSetCallback(ctr_dsp_audio_frame_cb, ctr);

   *new_rate    = CTR_DSP_AUDIO_RATE;

   ctr->channel = 0;

   ndspSetOutputMode(NDSP_OUTPUT_STEREO);
   ndspSetClippingMode(NDSP_CLIP_SOFT); /* ?? */
   ndspSetOutputCount(1);
   ndspChnReset(ctr->channel);
   ndspChnSetFormat(ctr->channel, NDSP_FORMAT_STEREO_PCM16);
   ndspChnSetInterp(ctr->channel, NDSP_INTERP_NONE);
   ndspChnSetRate(ctr->channel, (float)CTR_DSP_AUDIO_RATE);
   ndspChnWaveBufClear(ctr->channel);

   ctr->dsp_buf.data_pcm16 = linearAlloc(CTR_DSP_AUDIO_SIZE);
   memset(ctr->dsp_buf.data_pcm16, 0, CTR_DSP_AUDIO_SIZE);
   DSP_FlushDataCache(ctr->dsp_buf.data_pcm16, CTR_DSP_AUDIO_SIZE);

   ctr->dsp_buf.looping = true;
   ctr->dsp_buf.nsamples = CTR_DSP_AUDIO_COUNT;

   ndspChnWaveBufAdd(ctr->channel, &ctr->dsp_buf);

   ctr->pos = 0;
   ctr->playing = true;

   ndspSetMasterVol(1.0);

   return ctr;
}

static void ctr_dsp_audio_free(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   ndspSetCallback(NULL, NULL);
   ndspChnWaveBufClear(ctr->channel);
   linearFree(ctr->dsp_buf.data_pcm16);
   free(ctr);
   ndspExit();
}

/* How many 100 ms polls a blocking write waits for the channel to
 * advance before giving up on it. */
#define CTR_DSP_AUDIO_WAIT_LAPS 20

/* LightEvent_Wait() has no timeout, so a DSP that has stopped
 * signalling its frame event - reset, or the channel dropped coming
 * back from sleep - never returns from it. Never reached while the DSP
 * runs: it signals every frame, so this only decides how long a
 * stopped one takes to be noticed. */
#define CTR_DSP_AUDIO_STALL_TIMEOUT_NS 256000000LL
#define CTR_DSP_AUDIO_WAIT_WRITABLE_LAPS 8

/* LightEvent_WaitTimeout() is libctru 2's; on 1.x the wait is the
 * try-wait polled in slices short against the DSP's frame. Non-zero is
 * the timeout either way. */
#ifdef USE_CTRULIB_2
#define ctr_dsp_audio_frame_wait(ctr, ns) \
   LightEvent_WaitTimeout(&(ctr)->frame_event, (ns))
#else
#define CTR_DSP_AUDIO_POLL_SLICE_NS 500000LL

static int ctr_dsp_audio_frame_wait(ctr_dsp_audio_t *ctr, s64 timeout_ns)
{
   s64 left = timeout_ns;

   while (left > 0)
   {
      if (LightEvent_TryWait(&ctr->frame_event))
         return 0;
      svcSleepThread(CTR_DSP_AUDIO_POLL_SLICE_NS);
      left -= CTR_DSP_AUDIO_POLL_SLICE_NS;
   }
   return 1;
}
#endif

static ssize_t ctr_dsp_audio_write(void *data, const void *buf, size_t len)
{
   u32 pos;
   ctr_dsp_audio_t     *ctr = (ctr_dsp_audio_t*)data;
   uint32_t sample_pos      = ndspChnGetSamplePos(ctr->channel);

   if (  (((sample_pos  - ctr->pos)   & CTR_DSP_AUDIO_COUNT_MASK) < (CTR_DSP_AUDIO_COUNT >> 2))
      || (((ctr->pos    - sample_pos) & CTR_DSP_AUDIO_COUNT_MASK) < (CTR_DSP_AUDIO_COUNT >> 4))
      || (((sample_pos  - ctr->pos)   & CTR_DSP_AUDIO_COUNT_MASK) < (len >> 2)))
   {
      if (ctr->nonblock)
         ctr->pos = (sample_pos + (CTR_DSP_AUDIO_COUNT >> 1)) & CTR_DSP_AUDIO_COUNT_MASK;
      else
      {
         /* Poll the channel position while the DSP plays this out. A
          * channel that has stopped advancing - the DSP reset, the
          * system back from sleep with the channel dropped - never
          * satisfies the test below, so the poll is capped and the
          * write then returns having written nothing. */
         int laps = CTR_DSP_AUDIO_WAIT_LAPS;
         do
         {
            svcSleepThread(100000);

            /* Run aptMainLoop to update APT state if DSP state
             * changed, this prevents a hang on sleep. */
            if (!aptMainLoop())
            {
               retroarch_main_quit();
               return -1;
            }
            if (--laps < 0)
               return 0;

            sample_pos = ndspChnGetSamplePos(ctr->channel);
         }while (    ((sample_pos - (ctr->pos + (len >>2))) & CTR_DSP_AUDIO_COUNT_MASK) > (CTR_DSP_AUDIO_COUNT >> 1)
                 || (((ctr->pos - (CTR_DSP_AUDIO_COUNT >> 4) - sample_pos) & CTR_DSP_AUDIO_COUNT_MASK) > (CTR_DSP_AUDIO_COUNT >> 1)));
      }
   }

   pos = ctr->pos << 2;

   if ((pos + len) > CTR_DSP_AUDIO_SIZE)
   {
      memcpy(ctr->dsp_buf.data_pcm8 + pos, buf,
            (CTR_DSP_AUDIO_SIZE - pos));
      DSP_FlushDataCache(ctr->dsp_buf.data_pcm8 + pos, (CTR_DSP_AUDIO_SIZE - pos));

      memcpy(ctr->dsp_buf.data_pcm8, (uint8_t*) buf + (CTR_DSP_AUDIO_SIZE - pos),
            (pos + len - CTR_DSP_AUDIO_SIZE));
      DSP_FlushDataCache(ctr->dsp_buf.data_pcm8, (pos + len - CTR_DSP_AUDIO_SIZE));
   }
   else
   {
      memcpy(ctr->dsp_buf.data_pcm8 + pos, buf, len);
      DSP_FlushDataCache(ctr->dsp_buf.data_pcm8 + pos, len);
   }

   ctr->pos += len >> 2;
   ctr->pos &= CTR_DSP_AUDIO_COUNT_MASK;

   return len;
}

static bool ctr_dsp_audio_stop(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;

   ndspSetMasterVol(0.0);
   ctr->playing = false;

   return true;
}

static bool ctr_dsp_audio_alive(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   return ctr->playing;
}

static bool ctr_dsp_audio_start(void *data, bool is_shutdown)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;

   /* Prevents restarting audio when the menu
    * is toggled off on shutdown */
   if (!is_shutdown)
   {
      ndspSetMasterVol(1.0);
      ctr->playing = true;
   }

   return true;
}

static void ctr_dsp_audio_set_nonblock_state(void *data, bool state)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   if (ctr)
      ctr->nonblock = state;
}

static bool ctr_dsp_audio_use_float(void *data) { return false; }

static size_t ctr_dsp_audio_write_avail(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;

   return ((ndspChnGetSamplePos(ctr->channel) - ctr->pos) & CTR_DSP_AUDIO_COUNT_MASK)
         * 2 * sizeof(int16_t);
}

/* Sleep on the DSP frame event until the ring has room for len bytes
 * ahead of the play position, in the same frame units write_avail()
 * reports, capped at half the ring so the wait always ends. Runs on the
 * audio thread, so unlike the write's own wait it does not pump the
 * applet main loop. Returns the free space then, or 0 when not playing. */
static size_t ctr_dsp_audio_wait_writable(void *data, size_t len)
{
   ctr_dsp_audio_t *ctr = (ctr_dsp_audio_t*)data;
   uint32_t want        = (uint32_t)(len >> 2);
   /* Each wait ends on a timeout; this ends the loop when the DSP
    * keeps calling back but never frees enough. */
   int laps             = CTR_DSP_AUDIO_WAIT_WRITABLE_LAPS;

   if (want > (CTR_DSP_AUDIO_COUNT >> 1))
      want = CTR_DSP_AUDIO_COUNT >> 1;

   for (;;)
   {
      uint32_t avail;
      if (!ctr->playing)
         break;
      avail = (ndspChnGetSamplePos(ctr->channel) - ctr->pos)
            & CTR_DSP_AUDIO_COUNT_MASK;
      if (avail >= want)
         return avail * 2 * sizeof(int16_t);
      if (--laps < 0)
         break;
      /* Non-zero is the timeout. */
      if (ctr_dsp_audio_frame_wait(ctr, CTR_DSP_AUDIO_STALL_TIMEOUT_NS))
         break;
   }
   return 0;
}

/* Both in bytes of int16 stereo, as the interface asks: the ring is
 * CTR_DSP_AUDIO_COUNT frames and pos advances a frame per four bytes
 * written. They were reported in frames, a quarter of the truth, which
 * left rate control's ratio right and fast-forward's byte bound wrong. */
static size_t ctr_dsp_audio_buffer_size(void *data)
{
   return CTR_DSP_AUDIO_COUNT * 2 * sizeof(int16_t);
}

static bool ctr_dsp_audio_device_clock_ppm(void *data, double *ppm)
{
   ctr_dsp_audio_t *ctr = (ctr_dsp_audio_t*)data;
   int v;
   if (!ctr)
      return false;
   v = retro_atomic_load_acquire_int(&ctr->clk_ppm);
   if (v == AUDIO_CLOCK_PPM_NONE)
      return false;
   *ppm = (double)v;
   return true;
}

audio_driver_t audio_ctr_dsp = {
   ctr_dsp_audio_init,
   ctr_dsp_audio_write,
   ctr_dsp_audio_stop,
   ctr_dsp_audio_start,
   ctr_dsp_audio_alive,
   ctr_dsp_audio_set_nonblock_state,
   ctr_dsp_audio_free,
   ctr_dsp_audio_use_float,
   "dsp",
   NULL,
   NULL,
   ctr_dsp_audio_write_avail,
   ctr_dsp_audio_buffer_size,
   NULL, /* write_raw */
   ctr_dsp_audio_wait_writable,
   ctr_dsp_audio_frames_consumed,
   NULL, /* underruns */
   NULL, /* layout */
   NULL, /* frames_consumed_fallback */
   ctr_dsp_audio_device_clock_ppm
};
