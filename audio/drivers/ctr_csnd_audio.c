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
#include <retro_miscellaneous.h>
#include <retro_timers.h>

#include "../../retroarch.h"

typedef struct
{
   bool nonblock;
   bool playing;
   int16_t* l;
   int16_t* r;

   uint32_t l_paddr;
   uint32_t r_paddr;

   uint32_t pos;

   uint32_t playpos;
   uint64_t cpu_ticks_last;
} ctr_csnd_audio_t;

#define CTR_CSND_AUDIO_COUNT       (1u << 11u)
#define CTR_CSND_AUDIO_COUNT_MASK  (CTR_CSND_AUDIO_COUNT - 1u)
#define CTR_CSND_AUDIO_SIZE        (CTR_CSND_AUDIO_COUNT * sizeof(int16_t))
#define CTR_CSND_AUDIO_SIZE_MASK   (CTR_CSND_AUDIO_SIZE  - 1u)

#define CTR_CSND_AUDIO_RATE              32730
#define CTR_CSND_TICKS_PER_SAMPLE   2048
#define CTR_CSND_CPU_TICKS_PER_SAMPLE    (CTR_CSND_TICKS_PER_SAMPLE * 4)

static void ctr_csnd_audio_update_playpos(ctr_csnd_audio_t* ctr)
{
   uint64_t current_tick   = svcGetSystemTick();
   uint32_t samples_played = (current_tick - ctr->cpu_ticks_last)
      / CTR_CSND_CPU_TICKS_PER_SAMPLE;

   ctr->playpos   = (ctr->playpos + samples_played) & CTR_CSND_AUDIO_COUNT_MASK;
   ctr->cpu_ticks_last += samples_played * CTR_CSND_CPU_TICKS_PER_SAMPLE;
}

Result csndPlaySound_custom(int chn, u32 flags, float vol, float pan,
      void* data0, void* data1, u32 size)
{
	u32 paddr0   = 0;
   u32 paddr1   = 0;
	int encoding = (flags >> 12) & 3;
	int loopMode = (flags >> 10) & 3;

	if (!(csndChannels & BIT(chn)))
		return 1;

	if (!loopMode)
      flags |= SOUND_ONE_SHOT;

	if (encoding != CSND_ENCODING_PSG)
	{
		if (data0)
         paddr0 = osConvertVirtToPhys(data0);
		if (data1)
         paddr1 = osConvertVirtToPhys(data1);

		if (data0 && encoding == CSND_ENCODING_ADPCM)
		{
			int adpcmSample = ((s16*)data0)[-2];
			int adpcmIndex  = ((u8*)data0)[-2];
			CSND_SetAdpcmState(chn, 0, adpcmSample, adpcmIndex);
		}
	}

	flags &= ~0xFFFF001F;
	flags |= SOUND_ENABLE | SOUND_CHANNEL(chn) | (CTR_CSND_TICKS_PER_SAMPLE << 16);

	u32 volumes = CSND_VOL(vol, pan);
	CSND_SetChnRegs(flags, paddr0, paddr1, size, volumes, volumes);

	if (loopMode == CSND_LOOPMODE_NORMAL && paddr1 > paddr0)
	{
		/* Now that the first block is playing,
       * configure the size of the subsequent blocks */
		size -= paddr1 - paddr0;
		CSND_SetBlock(chn, 1, paddr1, size);
	}

	return 0;
}

static void *ctr_csnd_audio_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   ctr_csnd_audio_t *ctr = (ctr_csnd_audio_t*)calloc(1, sizeof(ctr_csnd_audio_t));

   if (!ctr)
      return NULL;

   (void)device;
   (void)rate;
   (void)latency;

   *new_rate                 = CTR_CSND_AUDIO_RATE;

   ctr->l                    = linearAlloc(CTR_CSND_AUDIO_SIZE);
   ctr->r                    = linearAlloc(CTR_CSND_AUDIO_SIZE);

   memset(ctr->l, 0, CTR_CSND_AUDIO_SIZE);
   memset(ctr->r, 0, CTR_CSND_AUDIO_SIZE);

   ctr->l_paddr              = osConvertVirtToPhys(ctr->l);
   ctr->r_paddr              = osConvertVirtToPhys(ctr->r);

   ctr->pos                  = 0;

   GSPGPU_FlushDataCache((void*)ctr->l_paddr, CTR_CSND_AUDIO_SIZE);
   GSPGPU_FlushDataCache((void*)ctr->r_paddr, CTR_CSND_AUDIO_SIZE);
   csndPlaySound_custom(0x8, SOUND_LOOPMODE(CSND_LOOPMODE_NORMAL)| SOUND_FORMAT(CSND_ENCODING_PCM16),
                 1.0, -1.0, ctr->l, ctr->l, CTR_CSND_AUDIO_SIZE);

   csndPlaySound_custom(0x9, SOUND_LOOPMODE(CSND_LOOPMODE_NORMAL)| SOUND_FORMAT(CSND_ENCODING_PCM16),
                 1.0,  1.0, ctr->r, ctr->r, CTR_CSND_AUDIO_SIZE);

   csndExecCmds(true);
   ctr->playpos              = 0;
   ctr->cpu_ticks_last       = svcGetSystemTick();
   ctr->playing              = true;

   return ctr;
}

static void ctr_csnd_audio_free(void *data)
{
   ctr_csnd_audio_t* ctr = (ctr_csnd_audio_t*)data;

#if 0
   csndExit();
#endif
   CSND_SetPlayState(0x8, 0);
   CSND_SetPlayState(0x9, 0);
   csndExecCmds(false);

   linearFree(ctr->l);
   linearFree(ctr->r);

   free(ctr);
}

static ssize_t ctr_csnd_audio_write(void *data, const void *buf, size_t size)
{
   int i;
   uint32_t samples_played                     = 0;
   uint64_t current_tick                       = 0;
   const uint16_t                         *src = buf;
   ctr_csnd_audio_t                       *ctr = (ctr_csnd_audio_t*)data;

   (void)data;
   (void)buf;
   (void)samples_played;
   (void)current_tick;

   ctr_csnd_audio_update_playpos(ctr);

   if((((ctr->playpos  - ctr->pos) & CTR_CSND_AUDIO_COUNT_MASK) < (CTR_CSND_AUDIO_COUNT >> 2)) ||
      (((ctr->pos - ctr->playpos ) & CTR_CSND_AUDIO_COUNT_MASK) < (CTR_CSND_AUDIO_COUNT >> 4)) ||
      (((ctr->playpos  - ctr->pos) & CTR_CSND_AUDIO_COUNT_MASK) < (size >> 2)))
   {
      if (ctr->nonblock)
         ctr->pos = (ctr->playpos + (CTR_CSND_AUDIO_COUNT >> 1)) & CTR_CSND_AUDIO_COUNT_MASK;
      else
      {
         do{
            /* todo: compute the correct sleep period */
            retro_sleep(1);
            ctr_csnd_audio_update_playpos(ctr);
         }while (((ctr->playpos - ctr->pos) & CTR_CSND_AUDIO_COUNT_MASK) < (CTR_CSND_AUDIO_COUNT >> 1)
               || (((ctr->pos - ctr->playpos) & CTR_CSND_AUDIO_COUNT_MASK) < (CTR_CSND_AUDIO_COUNT >> 4)));
      }
   }

   for (i = 0; i < (size >> 1); i += 2)
   {
      ctr->l[ctr->pos] = src[i];
      ctr->r[ctr->pos] = src[i + 1];
      ctr->pos++;
      ctr->pos &= CTR_CSND_AUDIO_COUNT_MASK;
   }

   GSPGPU_FlushDataCache(ctr->l, CTR_CSND_AUDIO_SIZE);
   GSPGPU_FlushDataCache(ctr->r, CTR_CSND_AUDIO_SIZE);

   return size;
}

static bool ctr_csnd_audio_stop(void *data)
{
   ctr_csnd_audio_t* ctr = (ctr_csnd_audio_t*)data;

   /* using SetPlayState would make tracking the playback
    * position more difficult */

#if 0
   CSND_SetPlayState(0x8, 0);
   CSND_SetPlayState(0x9, 0);
#endif

   /* setting the channel volume to 0 seems to make it
    * impossible to set it back to full volume later */

   CSND_SetVol(0x8, 0x00000001, 0);
   CSND_SetVol(0x9, 0x00010000, 0);
   csndExecCmds(false);

   ctr->playing = false;

   return true;
}

static bool ctr_csnd_audio_alive(void *data)
{
   ctr_csnd_audio_t* ctr = (ctr_csnd_audio_t*)data;
   return ctr->playing;
}

static bool ctr_csnd_audio_start(void *data, bool is_shutdown)
{
   ctr_csnd_audio_t* ctr = (ctr_csnd_audio_t*)data;

   /* Prevents restarting audio when the menu
    * is toggled off on shutdown */
   if (is_shutdown)
      return true;

#if 0
   CSND_SetPlayState(0x8, 1);
   CSND_SetPlayState(0x9, 1);
#endif

   CSND_SetVol(0x8, 0x00008000, 0);
   CSND_SetVol(0x9, 0x80000000, 0);

   csndExecCmds(false);

   ctr->playing = true;

   return true;
}

static void ctr_csnd_audio_set_nonblock_state(void *data, bool state)
{
   ctr_csnd_audio_t* ctr = (ctr_csnd_audio_t*)data;
   if (ctr)
      ctr->nonblock = state;
}

static bool ctr_csnd_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t ctr_csnd_audio_write_avail(void *data)
{
   ctr_csnd_audio_t* ctr = (ctr_csnd_audio_t*)data;

   ctr_csnd_audio_update_playpos(ctr);
   return (ctr->playpos - ctr->pos) & CTR_CSND_AUDIO_COUNT_MASK;
}

static size_t ctr_csnd_audio_buffer_size(void *data)
{
   (void)data;
   return CTR_CSND_AUDIO_COUNT;
}

audio_driver_t audio_ctr_csnd = {
   ctr_csnd_audio_init,
   ctr_csnd_audio_write,
   ctr_csnd_audio_stop,
   ctr_csnd_audio_start,
   ctr_csnd_audio_alive,
   ctr_csnd_audio_set_nonblock_state,
   ctr_csnd_audio_free,
   ctr_csnd_audio_use_float,
   "csnd",
   NULL,
   NULL,
   ctr_csnd_audio_write_avail,
   ctr_csnd_audio_buffer_size
};
