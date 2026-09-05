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
#include <malloc.h>

#include "../audio_driver.h"
#include "../../ctr/ctr_debug.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

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
   bool nonblock;
   bool playing;
} ctr_dsp_audio_t;

#define CTR_DSP_AUDIO_COUNT       (1u << 11u)
#define CTR_DSP_AUDIO_COUNT_MASK  (CTR_DSP_AUDIO_COUNT - 1u)
#define CTR_DSP_AUDIO_SIZE        (CTR_DSP_AUDIO_COUNT * sizeof(int16_t) * 2)

static void ctr_dsp_audio_frame_cb(void *data)
{
   ctr_dsp_audio_t *ctr = (ctr_dsp_audio_t*)data;
   uint32_t pos         = ndspChnGetSamplePos(ctr->channel);

   /* Unsigned subtraction and the mask do the unwrap: the difference is
    * right across the wrap without a comparison. Only this callback
    * writes either field. */
   retro_atomic_fetch_add_size(&ctr->consumed,
         (size_t)((pos - ctr->last_pos) & CTR_DSP_AUDIO_COUNT_MASK));
   ctr->last_pos = pos;

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
      unsigned block_frames,
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
   LightEvent_Init(&ctr->frame_event, RESET_ONESHOT);
   ndspSetCallback(ctr_dsp_audio_frame_cb, ctr);

   if (!ctr)
      return NULL;

   *new_rate    = 32730;

   ctr->channel = 0;

   ndspSetOutputMode(NDSP_OUTPUT_STEREO);
   ndspSetClippingMode(NDSP_CLIP_SOFT); /* ?? */
   ndspSetOutputCount(1);
   ndspChnReset(ctr->channel);
   ndspChnSetFormat(ctr->channel, NDSP_FORMAT_STEREO_PCM16);
   ndspChnSetInterp(ctr->channel, NDSP_INTERP_NONE);
   ndspChnSetRate(ctr->channel, 32728.0f);
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

   if (want > (CTR_DSP_AUDIO_COUNT >> 1))
      want = CTR_DSP_AUDIO_COUNT >> 1;

   for (;;)
   {
      uint32_t avail;
      if (!ctr->playing)
         return 0;
      avail = (ndspChnGetSamplePos(ctr->channel) - ctr->pos)
            & CTR_DSP_AUDIO_COUNT_MASK;
      if (avail >= want)
         return avail * 2 * sizeof(int16_t);
      LightEvent_Wait(&ctr->frame_event);
   }
}

/* Both in bytes of int16 stereo, as the interface asks: the ring is
 * CTR_DSP_AUDIO_COUNT frames and pos advances a frame per four bytes
 * written. They were reported in frames, a quarter of the truth, which
 * left rate control's ratio right and fast-forward's byte bound wrong. */
static size_t ctr_dsp_audio_buffer_size(void *data)
{
   return CTR_DSP_AUDIO_COUNT * 2 * sizeof(int16_t);
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
   ctr_dsp_audio_frames_consumed
};
