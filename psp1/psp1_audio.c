/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "../general.h"
#include "../driver.h"

#include <pspkernel.h>
#include <pspaudio.h>
#include <stdint.h>

//ToDO
typedef struct
{
   bool nonblocking;
   bool started;
   bool quit_thread;
   uint32_t* buffer;
   uint32_t* zeroBuffer;
   SceUID thread;
   int rate;     
   uint16_t readPos;
   uint16_t writePos;
} psp1_audio_t;

#define AUDIO_OUT_COUNT 128

#define AUDIO_BUFFER_SIZE (1u<<12u)
#define AUDIO_BUFFER_SIZE_MASK (AUDIO_BUFFER_SIZE-1)

int audioMainLoop(SceSize args, void* argp)
{
   psp1_audio_t* psp = (psp1_audio_t*)driver.audio_data;
   (void)argp;

	sceAudioSRCChReserve(AUDIO_OUT_COUNT, psp->rate, 2);

	while (!psp->quit_thread)
   {
      if (((uint16_t)(psp->writePos - psp->readPos) & AUDIO_BUFFER_SIZE_MASK) < (uint16_t)AUDIO_OUT_COUNT * 2)
         sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, psp->zeroBuffer);
      else
      {
         sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, psp->buffer + psp->readPos);
         psp->readPos += AUDIO_OUT_COUNT;
         psp->readPos &= AUDIO_BUFFER_SIZE_MASK;
      }
   }

	sceAudioSRCChRelease();
	sceKernelExitThread(0);
	return 0;
}

static bool psp_audio_start(void *data);

static void *psp_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)latency;

   psp1_audio_t* psp = (psp1_audio_t*)calloc(1, sizeof(psp1_audio_t));

   if (!psp)
      return NULL;

   psp->buffer      = (uint32_t*)calloc(AUDIO_BUFFER_SIZE, sizeof(uint32_t));
   psp->readPos     = 0;
   psp->writePos    = 0;
   psp->zeroBuffer  = (uint32_t*)calloc(AUDIO_OUT_COUNT, sizeof(uint32_t));
   psp->rate        = rate;
   psp->thread      = sceKernelCreateThread ("audioMainLoop", audioMainLoop, 0x12, 0x10000, 0, NULL);
   psp->nonblocking = true;

   return psp;
}

static void psp_audio_free(void *data)
{
   psp1_audio_t* psp = (psp1_audio_t*)data;

   psp->quit_thread     = true;

   sceKernelDeleteThread(psp->thread);
}

static ssize_t psp_audio_write(void *data, const void *buf, size_t size)
{
   uint16_t sampleCount;
   psp1_audio_t* psp = (psp1_audio_t*)data;
   // ToDo : add support for blocking audio
   
   sampleCount= size / sizeof(uint32_t);
   
   if (psp->nonblocking)
   {
      /* TODO */
   }
   else
   {
      if((psp->writePos + sampleCount) > AUDIO_BUFFER_SIZE)
      {
         memcpy(psp->buffer + psp->writePos, buf, (AUDIO_BUFFER_SIZE - psp->writePos) * sizeof(uint32_t));
         memcpy(psp->buffer, buf, (psp->writePos + sampleCount - AUDIO_BUFFER_SIZE) * sizeof(uint32_t));
      }
      else
         memcpy(psp->buffer + psp->writePos, buf, size);
   }
   
   psp->writePos  += sampleCount;
   psp->writePos  &= AUDIO_BUFFER_SIZE_MASK;

   return sampleCount;
}

static bool psp_audio_stop(void *data)
{
   psp1_audio_t* psp = (psp1_audio_t*)data;
   
   if (psp->started)
   {
      sceKernelWaitThreadEnd(psp->thread, NULL);
      psp->started = false;
   }

   return true;
}

static bool psp_audio_start(void *data)
{
   psp1_audio_t* psp = (psp1_audio_t*)data;

   if (!psp->started)
   {
      sceKernelStartThread(psp->thread, sizeof(psp1_audio_t*), &data);
      psp->started     = true;
   }

   return true;
}

static void psp_audio_set_nonblock_state(void *data, bool toggle)
{
   psp1_audio_t* psp = (psp1_audio_t*)data;
   psp->nonblocking = toggle;
}

static bool psp_audio_use_float(void *data)
{
   (void)data;
   return false;
}

const audio_driver_t audio_psp1 = {
   psp_audio_init,
   psp_audio_write,
   psp_audio_stop,
   psp_audio_start,
   psp_audio_set_nonblock_state,
   psp_audio_free,
   psp_audio_use_float,
   "psp1",
};
