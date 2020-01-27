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
#include <malloc.h>

#include "../../retroarch.h"
#include "../../ctr/ctr_debug.h"

typedef struct
{
   bool nonblock;
   bool playing;
   int channel;
   ndspWaveBuf dsp_buf;

   uint32_t pos;
} ctr_dsp_audio_t;

#define CTR_DSP_AUDIO_COUNT       (1u << 11u)
#define CTR_DSP_AUDIO_COUNT_MASK  (CTR_DSP_AUDIO_COUNT - 1u)
#define CTR_DSP_AUDIO_SIZE        (CTR_DSP_AUDIO_COUNT * sizeof(int16_t) * 2)
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
   ndspChnWaveBufClear(ctr->channel);
   linearFree(ctr->dsp_buf.data_pcm16);
   free(ctr);
   ndspExit();
}

static ssize_t ctr_dsp_audio_write(void *data, const void *buf, size_t size)
{
   u32 pos;
   ctr_dsp_audio_t                           * ctr = (ctr_dsp_audio_t*)data;
   uint32_t sample_pos                             = ndspChnGetSamplePos(ctr->channel);

   if((((sample_pos  - ctr->pos) & CTR_DSP_AUDIO_COUNT_MASK) < (CTR_DSP_AUDIO_COUNT >> 2)) ||
      (((ctr->pos - sample_pos ) & CTR_DSP_AUDIO_COUNT_MASK) < (CTR_DSP_AUDIO_COUNT >> 4)) ||
      (((sample_pos  - ctr->pos) & CTR_DSP_AUDIO_COUNT_MASK) < (size >> 2)))
   {
      if (ctr->nonblock)
         ctr->pos = (sample_pos + (CTR_DSP_AUDIO_COUNT >> 1)) & CTR_DSP_AUDIO_COUNT_MASK;
      else
      {
         do{
            svcSleepThread(100000);
            sample_pos = ndspChnGetSamplePos(ctr->channel);
         }while (((sample_pos - (ctr->pos + (size >>2))) & CTR_DSP_AUDIO_COUNT_MASK) > (CTR_DSP_AUDIO_COUNT >> 1)
                 || (((ctr->pos - (CTR_DSP_AUDIO_COUNT >> 4) - sample_pos) & CTR_DSP_AUDIO_COUNT_MASK) > (CTR_DSP_AUDIO_COUNT >> 1)));
      }
   }

   pos = ctr->pos << 2;

   if((pos + size) > CTR_DSP_AUDIO_SIZE)
   {
      memcpy(ctr->dsp_buf.data_pcm8 + pos, buf,
            (CTR_DSP_AUDIO_SIZE - pos));
      DSP_FlushDataCache(ctr->dsp_buf.data_pcm8 + pos, (CTR_DSP_AUDIO_SIZE - pos));

      memcpy(ctr->dsp_buf.data_pcm8, (uint8_t*) buf + (CTR_DSP_AUDIO_SIZE - pos),
            (pos + size - CTR_DSP_AUDIO_SIZE));
      DSP_FlushDataCache(ctr->dsp_buf.data_pcm8, (pos + size - CTR_DSP_AUDIO_SIZE));
   }
   else
   {
      memcpy(ctr->dsp_buf.data_pcm8 + pos, buf, size);
      DSP_FlushDataCache(ctr->dsp_buf.data_pcm8 + pos, size);
   }

   ctr->pos += size >> 2;
   ctr->pos &= CTR_DSP_AUDIO_COUNT_MASK;

   return size;
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
   if (is_shutdown)
      return true;

   ndspSetMasterVol(1.0);
   ctr->playing = true;

   return true;
}

static void ctr_dsp_audio_set_nonblock_state(void *data, bool state)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   if (ctr)
      ctr->nonblock = state;
}

static bool ctr_dsp_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t ctr_dsp_audio_write_avail(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;

   return (ndspChnGetSamplePos(ctr->channel) - ctr->pos) & CTR_DSP_AUDIO_COUNT_MASK;
}

static size_t ctr_dsp_audio_buffer_size(void *data)
{
   (void)data;
   return CTR_DSP_AUDIO_COUNT;
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
   ctr_dsp_audio_buffer_size
};
