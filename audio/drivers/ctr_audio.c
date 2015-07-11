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
   bool playing;
   int16_t* l;
   int16_t* r;

   uint32_t l_paddr;
   uint32_t r_paddr;

   uint32_t pos;

   uint32_t playpos;
   uint32_t cpu_ticks_per_sample;
   uint64_t cpu_ticks_last;

   int rate;

} ctr_audio_t;

#define CTR_AUDIO_COUNT       (1u << 11u)
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

   ctr->l = linearAlloc(CTR_AUDIO_SIZE);
   ctr->r = linearAlloc(CTR_AUDIO_SIZE);

   memset(ctr->l, 0, CTR_AUDIO_SIZE);
   memset(ctr->r, 0, CTR_AUDIO_SIZE);

   ctr->l_paddr = osConvertVirtToPhys((u32)ctr->l);
   ctr->r_paddr = osConvertVirtToPhys((u32)ctr->r);

   ctr->pos  = 0;
   ctr->rate = rate;
   ctr->cpu_ticks_per_sample = CSND_TIMER(rate) * 4;

   GSPGPU_FlushDataCache(NULL, (u8*)ctr->l_paddr, CTR_AUDIO_SIZE);
   GSPGPU_FlushDataCache(NULL, (u8*)ctr->r_paddr, CTR_AUDIO_SIZE);
   csndPlaySound(0x8, SOUND_LOOPMODE(CSND_LOOPMODE_NORMAL)| SOUND_FORMAT(CSND_ENCODING_PCM16),
                 rate, 1.0, -1.0, ctr->l, ctr->l, CTR_AUDIO_SIZE);

   csndPlaySound(0x9, SOUND_LOOPMODE(CSND_LOOPMODE_NORMAL)| SOUND_FORMAT(CSND_ENCODING_PCM16),
                 rate, 1.0, 1.0, ctr->r, ctr->r, CTR_AUDIO_SIZE);

   ctr->playpos = 0;
   ctr->cpu_ticks_last = svcGetSystemTick();
   ctr->playing = true;

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

   uint64_t current_tick = svcGetSystemTick();
   uint32_t samples_played = (current_tick - ctr->cpu_ticks_last) / ctr->cpu_ticks_per_sample;
   ctr->playpos = (ctr->playpos + samples_played) & CTR_AUDIO_COUNT_MASK;
   ctr->cpu_ticks_last += samples_played * ctr->cpu_ticks_per_sample;


   if((((ctr->playpos  - ctr->pos) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 2)) ||
      (((ctr->pos - ctr->playpos ) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 4)) ||
      (((ctr->playpos  - ctr->pos) & CTR_AUDIO_COUNT_MASK) < (size >> 2)))
   {
      if (ctr->nonblocking)
         ctr->pos = (ctr->playpos + (CTR_AUDIO_COUNT >> 1)) & CTR_AUDIO_COUNT_MASK;
      else
      {
         do{
            /* todo: compute the correct sleep period */
            rarch_sleep(1);
            current_tick = svcGetSystemTick();
            samples_played = (current_tick - ctr->cpu_ticks_last) / ctr->cpu_ticks_per_sample;
            ctr->playpos = (ctr->playpos + samples_played) & CTR_AUDIO_COUNT_MASK;
            ctr->cpu_ticks_last += samples_played * ctr->cpu_ticks_per_sample;
         }while (((ctr->playpos - ctr->pos) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 1)
                 || (((ctr->pos - ctr->playpos) & CTR_AUDIO_COUNT_MASK) < (CTR_AUDIO_COUNT >> 4)));
      }
   }

   for (i = 0; i < (size >> 1); i += 2)
   {
      ctr->l[ctr->pos] = src[i];
      ctr->r[ctr->pos] = src[i + 1];
      ctr->pos++;
      ctr->pos &= CTR_AUDIO_COUNT_MASK;
   }
   GSPGPU_FlushDataCache(NULL, (u8*)ctr->l, CTR_AUDIO_SIZE);
   GSPGPU_FlushDataCache(NULL, (u8*)ctr->r, CTR_AUDIO_SIZE);


   RARCH_PERFORMANCE_STOP(ctraudio_f);

   return size;
}

static bool ctr_audio_stop(void *data)
{   
   ctr_audio_t* ctr = (ctr_audio_t*)data;

   /* using SetPlayState would make tracking the playback
    * position more difficult */

//   CSND_SetPlayState(0x8, 0);
//   CSND_SetPlayState(0x9, 0);

   /* setting the channel volume to 0 seems to make it
    * impossible to set it back to full volume later */

   CSND_SetVol(0x8, 0x00000001, 0);
   CSND_SetVol(0x9, 0x00010000, 0);
   csndExecCmds(false);

   ctr->playing = false;

   return true;
}

static bool ctr_audio_alive(void *data)
{
   ctr_audio_t* ctr = (ctr_audio_t*)data;
   return ctr->playing;
}

static bool ctr_audio_start(void *data)
{
   ctr_audio_t* ctr = (ctr_audio_t*)data;
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   /* prevents restarting audio when the menu
    * is toggled off on shutdown */

   if (system->shutdown)
      return true;

//   CSND_SetPlayState(0x8, 1);
//   CSND_SetPlayState(0x9, 1);

   CSND_SetVol(0x8, 0x00008000, 0);
   CSND_SetVol(0x9, 0x80000000, 0);

   csndExecCmds(false);

   ctr->playing = true;

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

static size_t ctr_audio_write_avail(void *data)
{
   /* stub */
   (void)data;
   return 0;
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
   ctr_audio_write_avail,
   NULL
};
