/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
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

#include "../audio_driver.h"
#include "../../configuration.h"
#include "../../performance.h"


#define CTR_DSP_BUFFERS_COUNT (1u << 4u)
#define CTR_DSP_BUFFERS_MASK  (CTR_DSP_BUFFERS_COUNT - 1)

typedef struct
{
   bool nonblocking;
   bool playing;

   int channel;
   ndspWaveBuf dsp_buf[CTR_DSP_BUFFERS_COUNT];
   uint32_t dsp_buf_id;
   uint8_t* linear_buffer;

} ctr_dsp_audio_t;

#define CTR_DSP_AUDIO_COUNT       (1u << 11u)
#define CTR_DSP_AUDIO_COUNT_MASK  (CTR_DSP_AUDIO_COUNT - 1u)
#define CTR_DSP_AUDIO_SIZE        (CTR_DSP_AUDIO_COUNT * sizeof(int16_t) * 2)
#define CTR_DSP_AUDIO_SIZE_MASK   (CTR_DSP_AUDIO_SIZE  - 1u)


#ifndef DEBUG_HOLD
void wait_for_input(void);
#define PRINTFPOS(X,Y) "\x1b["#X";"#Y"H"
#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)
#define DEBUG_VAR(X) printf( "%-20s: 0x%08X\n", #X, (u32)(X))
#define DEBUG_VAR64(X) printf( #X"\r\t\t\t\t : 0x%016llX\n", (u64)(X))
#endif


static uint32_t ctr_get_queued_samples(ctr_dsp_audio_t* ctr)
{
   uint32_t sample_pos, sample_count;
   uint16_t buf_seq;

   buf_seq = ndspChnGetWaveBufSeq(ctr->channel);

   if(!buf_seq)
      return 0;

   sample_pos = ndspChnGetSamplePos(ctr->channel);
   sample_count = 0;

   int buf_id = ctr->dsp_buf_id;
   do
   {
      buf_id++;
      buf_id&= CTR_DSP_BUFFERS_MASK;
   }
   while(ctr->dsp_buf[buf_id].sequence_id != buf_seq);
   ndspWaveBuf* current = &ctr->dsp_buf[buf_id];

   sample_count = current->nsamples - sample_pos;
   while((current = current->next))
   {
      sample_count += current->nsamples;
   }
   return sample_count;
}



static void *ctr_dsp_audio_init(const char *device, unsigned rate, unsigned latency)
{
   ctr_dsp_audio_t *ctr;
   settings_t *settings = config_get_ptr();

   (void)device;
   (void)rate;
   (void)latency;

   if (ndspInit() < 0)
      return NULL;

   ctr = (ctr_dsp_audio_t*)calloc(1, sizeof(ctr_dsp_audio_t));

   if (!ctr)
      return NULL;

//   memset(ctr, 0, sizeof(*ctr));

   settings->audio.out_rate  = 32730;

   ctr->channel = 0;
   ctr->dsp_buf_id = 0;

   ndspSetOutputMode(NDSP_OUTPUT_STEREO);
   ndspSetClippingMode(NDSP_CLIP_SOFT); //??
//   ndspSetClippingMode(NDSP_CLIP_NORMAL); //??
   ndspSetOutputCount(CTR_DSP_BUFFERS_COUNT);
   ndspChnReset(ctr->channel);
//   ndspChnInitParams(ctr->channel);
   ndspChnSetFormat(ctr->channel, NDSP_FORMAT_STEREO_PCM16);
   ndspChnSetInterp(ctr->channel, NDSP_INTERP_NONE);
//   ndspChnSetInterp(ctr->channel, NDSP_INTERP_LINEAR);
//   ndspChnSetInterp(ctr->channel, NDSP_INTERP_POLYPHASE);
   ndspChnSetRate(ctr->channel, 32728.0f);
//   ndspChnSetRate(ctr->channel, CTR_DSP_AUDIO_RATE);
//   ndspChnSetRate(ctr->channel, 0x7F00);
//   ndspChnSetMix(ctr->channel, (float[12]){1.0, 1.0, 0.0});
   ndspChnWaveBufClear(ctr->channel);

   ctr->linear_buffer = linearAlloc(CTR_DSP_BUFFERS_COUNT * CTR_DSP_AUDIO_SIZE);

   int i;
   for (i = 0; i < CTR_DSP_BUFFERS_COUNT; i++)
      ctr->dsp_buf[i].data_pcm16 = (uint16_t*)(ctr->linear_buffer + i * CTR_DSP_AUDIO_SIZE);

   return ctr;
}

static void ctr_dsp_audio_free(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;

   linearFree(ctr->linear_buffer);
   free(ctr);
   ndspExit();
}

static ssize_t ctr_dsp_audio_write(void *data, const void *buf, size_t size)
{
   static struct retro_perf_counter ctraudio_dsp_f = {0};
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;

   ndspWaveBuf* current_buf = &ctr->dsp_buf[ctr->dsp_buf_id];
   static int dropped=0;
   uint32_t queued = ctr_get_queued_samples(ctr);

   if(size > CTR_DSP_AUDIO_SIZE)
   {
      DEBUG_VAR(size);
      ctr_dsp_audio_write(ctr, buf, CTR_DSP_AUDIO_SIZE);
      buf = (const void*)((u32)buf + CTR_DSP_AUDIO_SIZE);
      size = size - CTR_DSP_AUDIO_SIZE;
   }

   dropped++;
   if(ctr->nonblocking)
   {
      if((current_buf->status == NDSP_WBUF_QUEUED) || (current_buf->status == NDSP_WBUF_PLAYING))
         return size;

      if((queued + (size>>2)) > 0x2000)
         return size;
   }
   else
   {
      if(queued > (0x1000 + (size>>2)))
      {
//         printf(PRINTFPOS(24,0)"queued  :0x%08X \n", ctr_get_queued_samples(ctr));
         while(ctr_get_queued_samples(ctr) > 160 )
            svcSleepThread(100000);
      }
   }
   dropped--;


   rarch_perf_init(&ctraudio_dsp_f, "ctraudio_dsp_f");
   retro_perf_start(&ctraudio_dsp_f);

   memcpy(current_buf->data_pcm16, buf, size);
   current_buf->nsamples = size>>2;

   DSP_FlushDataCache(current_buf->data_pcm16, size);
   DSP_FlushDataCache(ctr->dsp_buf, sizeof(ctr->dsp_buf));
   gfxFlushBuffers();

   ndspChnWaveBufAdd(ctr->channel, current_buf);
   retro_perf_stop(&ctraudio_dsp_f);

//   printf(PRINTFPOS(25,0)"dropped:0x%08X queued  :0x%08X \n", dropped, queued);
//   printf(PRINTFPOS(26,0)"current:0x%08X nsamples:0x%08X \n", ctr->dsp_buf_id, current_buf->nsamples);
//   printf(PRINTFPOS(27,0)"dropped:0x%08X count   :0x%08X \n", ndspGetDroppedFrames(), ndspGetFrameCount());
//   printf(PRINTFPOS(28,0)"Pos    :0x%08X Seq     :0x%08X \n", ndspChnGetSamplePos(ctr->channel), ndspChnGetWaveBufSeq(ctr->channel));

   ctr->dsp_buf_id++;
   ctr->dsp_buf_id &= CTR_DSP_BUFFERS_MASK;

   return size;
}

static bool ctr_dsp_audio_stop(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;

   ndspChnWaveBufClear(ctr->channel);
   ctr->playing = false;
   int i;
   memset(ctr->dsp_buf, 0, sizeof(ctr->dsp_buf));
   for (i = 0; i < CTR_DSP_BUFFERS_COUNT; i++)
      ctr->dsp_buf[i].data_pcm16 = (uint16_t*)(ctr->linear_buffer + i * CTR_DSP_AUDIO_SIZE);

   return true;
}

static bool ctr_dsp_audio_alive(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   return ctr->playing;
}

static bool ctr_dsp_audio_start(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   /* Prevents restarting audio when the menu
    * is toggled off on shutdown */

   if (system->shutdown)
      return true;

   ctr->playing = true;

   return true;
}

static void ctr_dsp_audio_set_nonblock_state(void *data, bool state)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   if (ctr)
      ctr->nonblocking = state;
}

static bool ctr_dsp_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t ctr_dsp_audio_write_avail(void *data)
{
   ctr_dsp_audio_t* ctr = (ctr_dsp_audio_t*)data;
   uint32_t queued_samples = ctr_get_queued_samples(ctr);

   return queued_samples < 0x1000 ? 0x1000 - queued_samples : 0;
}

static size_t ctr_dsp_audio_buffer_size(void *data)
{
   (void)data;
   return 0x1000;
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
   ctr_dsp_audio_write_avail,
   ctr_dsp_audio_buffer_size
};

