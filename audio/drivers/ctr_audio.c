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

#include "../../general.h"
#include "../../driver.h"
#include "../../performance.h"


typedef struct
{
   bool nonblocking;
   int16_t* l;
   int16_t* r;
   int16_t* silence;

   uint32_t l_paddr;
   uint32_t r_paddr;
   uint32_t silence_paddr;

   uint32_t pos;
   int rate;

} ctr_audio_t;

#define CTR_AUDIO_COUNT       (1u << 12u)
#define CTR_AUDIO_COUNT_MASK  (CTR_AUDIO_COUNT - 1u)
#define CTR_AUDIO_SIZE        (CTR_AUDIO_COUNT * sizeof(int16_t))
#define CTR_AUDIO_SIZE_MASK   (CTR_AUDIO_SIZE  - 1u)

static void *ctr_audio_init(const char *device, unsigned rate, unsigned latency)
{


   (void)device;
   (void)rate;
   (void)latency;

//   if(!csndInit())
//      return NULL;

   ctr_audio_t *ctr = (ctr_audio_t*)calloc(1, sizeof(ctr_audio_t));

   ctr->l       = linearAlloc(CTR_AUDIO_SIZE);
   ctr->r       = linearAlloc(CTR_AUDIO_SIZE);
   ctr->silence = linearAlloc(CTR_AUDIO_SIZE);

   memset(ctr->l,       0, CTR_AUDIO_SIZE);
   memset(ctr->r,       0, CTR_AUDIO_SIZE);
   memset(ctr->silence, 0, CTR_AUDIO_SIZE);

   ctr->l_paddr       = osConvertVirtToPhys((u32)ctr->l);
   ctr->r_paddr       = osConvertVirtToPhys((u32)ctr->r);
   ctr->silence_paddr = osConvertVirtToPhys((u32)ctr->silence);

   ctr->pos  = 0;
   ctr->rate = rate;

   GSPGPU_FlushDataCache(NULL, (u8*)ctr->silence, CTR_AUDIO_SIZE);
   csndPlaySound(0x8, SOUND_LOOPMODE(CSND_LOOPMODE_NORMAL)| SOUND_FORMAT(CSND_ENCODING_PCM16),
                 rate, ctr->silence, ctr->silence, CTR_AUDIO_SIZE);

   csndPlaySound(0x9, SOUND_LOOPMODE(CSND_LOOPMODE_NORMAL)| SOUND_FORMAT(CSND_ENCODING_PCM16),
                 rate, ctr->silence, ctr->silence, CTR_AUDIO_SIZE);

   CSND_SetVol(0x8, 0xFFFF, 0);
   CSND_SetVol(0x9, 0, 0xFFFF);
   csndExecCmds(false);

   return ctr;
}

static void ctr_audio_free(void *data)
{
   ctr_audio_t* ctr = (ctr_audio_t*)data;

//   csndExit();
   CSND_SetPlayState(0x8, 0);
   CSND_SetPlayState(0x9, 0);
   csndExecCmds(false);

   linearFree(ctr->l);
   linearFree(ctr->r);
   linearFree(ctr->silence);

   free(ctr);
}

static ssize_t ctr_audio_write(void *data, const void *buf, size_t size)
{
   (void)data;
   (void)buf;

   ctr_audio_t* ctr = (ctr_audio_t*)data;

   int i;
   const uint16_t* src = buf;

   RARCH_PERFORMANCE_INIT(ctraudio_f);
   RARCH_PERFORMANCE_START(ctraudio_f);

   CSND_ChnInfo channel_info;
   csndGetState(0x8, &channel_info);

   uint32_t playpos;
   if((channel_info.samplePAddr >= (ctr->l_paddr)) &&
      (channel_info.samplePAddr <  (ctr->l_paddr + CTR_AUDIO_SIZE)))
   {
      playpos = (channel_info.samplePAddr - ctr->l_paddr) / sizeof(uint16_t);
   }
   else
   {
      CSND_SetBlock(0x8, 1, ctr->l_paddr, CTR_AUDIO_SIZE);
      CSND_SetBlock(0x9, 1, ctr->r_paddr, CTR_AUDIO_SIZE);
      csndExecCmds(false);
      playpos = 0;
   }

   if((((playpos  - ctr->pos) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 2)) ||
      (((ctr->pos - playpos ) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 4)) ||
      (((playpos  - ctr->pos) & CTR_AUDIO_COUNT_MASK) < (size >> 2)))
   {
      if (ctr->nonblocking)
         ctr->pos = (playpos + (CTR_AUDIO_COUNT >> 1)) & CTR_AUDIO_COUNT_MASK;
      else
      {
         do{
            svcSleepThread(100000);
//            svcSleepThread(((s64)(CTR_AUDIO_COUNT >> 8) * 1000000000) / ctr->rate);
            csndGetState(0x8, &channel_info);
            playpos = (channel_info.samplePAddr - ctr->l_paddr) / sizeof(uint16_t);
         }while (((playpos - ctr->pos) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 1)
                 || (((ctr->pos - playpos) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 4)));
      }
   }


   for (i = 0; i < (size >> 1); i += 2)
   {
      ctr->l[ctr->pos] = src[i];
      ctr->r[ctr->pos] = src[i + 1];
      ctr->pos++;
      ctr->pos &= CTR_AUDIO_COUNT_MASK;
   }

   RARCH_PERFORMANCE_STOP(ctraudio_f);

   return size;
}

static bool ctr_audio_stop(void *data)
{   
   ctr_audio_t* ctr = (ctr_audio_t*)data;

   CSND_SetBlock(0x8, 1, ctr->silence_paddr, CTR_AUDIO_SIZE);
   CSND_SetBlock(0x9, 1, ctr->silence_paddr, CTR_AUDIO_SIZE);
   csndExecCmds(false);

   return true;
}

static bool ctr_audio_alive(void *data)
{
   (void)data;
   return true;
}

static bool ctr_audio_start(void *data)
{
   ctr_audio_t* ctr = (ctr_audio_t*)data;

//   csndPlaySound(0x8, SOUND_LOOPMODE(CSND_LOOPMODE_NORMAL)| SOUND_FORMAT(CSND_ENCODING_PCM16),
//                 ctr->rate, ctr->l_paddr + ((ctr->pos + (CTR_AUDIO_SIZE / 2)) & CTR_AUDIO_SIZE_MASK),
//                 (u32*)ctr->silence_paddr, CTR_AUDIO_SIZE);

   CSND_SetBlock(0x8, 1, ctr->l_paddr, CTR_AUDIO_SIZE);
   CSND_SetBlock(0x9, 1, ctr->r_paddr, CTR_AUDIO_SIZE);
   csndExecCmds(false);

   return true;
}

static void ctr_audio_set_nonblock_state(void *data, bool state)
{
   ctr_audio_t* ctr = (ctr_audio_t*)data;
   if (ctr)
      ctr->nonblocking = state;
}

static bool ctr_audio_use_float(void *data)
{
   (void)data;
   return false;
}

audio_driver_t audio_ctr = {
   ctr_audio_init,
   ctr_audio_write,
   ctr_audio_stop,
   ctr_audio_start,
   ctr_audio_alive,
   ctr_audio_set_nonblock_state,
   ctr_audio_free,
   ctr_audio_use_float,
   "ctr",
   NULL,
   NULL
};
