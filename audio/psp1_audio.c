/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2014      - Aliaspider (To be replaced with real name)
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
typedef struct psp1_audio
{
   bool nonblocking;
   uint32_t* buffer;
   uint32_t* zeroBuffer;
   SceUID thread;
   int rate;

   volatile bool running;
   volatile uint16_t readPos;
   volatile uint16_t writePos;
} psp1_audio_t;

#define AUDIO_OUT_COUNT 512u
#define AUDIO_BUFFER_SIZE (1u<<13u)
#define AUDIO_BUFFER_SIZE_MASK (AUDIO_BUFFER_SIZE-1)

static int audioMainLoop(SceSize args, void* argp)
{
   psp1_audio_t* psp = *((psp1_audio_t**)argp);

   sceAudioSRCChReserve(AUDIO_OUT_COUNT, psp->rate, 2);

   while (psp->running)
   {
      uint16_t readPos = psp->readPos; // get a non volatile copy

      if (((uint16_t)(psp->writePos - readPos) & AUDIO_BUFFER_SIZE_MASK) < (AUDIO_OUT_COUNT * 2))
         sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, psp->zeroBuffer);
      else
      {
         sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, psp->buffer + readPos);
         readPos += AUDIO_OUT_COUNT;
         readPos &= AUDIO_BUFFER_SIZE_MASK;
         psp->readPos = readPos;
      }
   }

   sceAudioSRCChRelease();
   sceKernelExitThread(0);
   return 0;
}

static void *psp_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)latency;

   psp1_audio_t* psp = (psp1_audio_t*)calloc(1, sizeof(psp1_audio_t));

   if (!psp)
      return NULL;

   psp->buffer      = (uint32_t*)memalign(64, AUDIO_BUFFER_SIZE * sizeof(uint32_t));  // cache aligned, not necessary but helpful
   memset(psp->buffer, 0, AUDIO_BUFFER_SIZE * sizeof(uint32_t));

   psp->zeroBuffer  = (uint32_t*)memalign(64, AUDIO_OUT_COUNT   * sizeof(uint32_t));
   memset(psp->zeroBuffer, 0, AUDIO_OUT_COUNT * sizeof(uint32_t));

   psp->readPos     = 0;
   psp->writePos    = 0;
   psp->rate        = rate;
   psp->thread      = sceKernelCreateThread ("audioMainLoop", audioMainLoop, 0x08, 0x10000, 0, NULL);
   psp->nonblocking = false;

   psp->running     = true;
   sceKernelStartThread(psp->thread, sizeof(psp1_audio_t*), &psp);

   return psp;
}

static void psp_audio_free(void *data)
{
   psp1_audio_t* psp = (psp1_audio_t*)data;
   if(!psp)
      return;

   psp->running = false;
   SceUInt timeout = 100000;
   sceKernelWaitThreadEnd(psp->thread, &timeout);
   sceKernelDeleteThread(psp->thread);

   free(psp->buffer);
   free(psp->zeroBuffer);
   free(psp);
}

static ssize_t psp_audio_write(void *data, const void *buf, size_t size)
{
   uint16_t sampleCount;
   psp1_audio_t* psp = (psp1_audio_t*)data;
   uint16_t writePos = psp->writePos;

   sampleCount= size / sizeof(uint32_t);

//   if (psp->nonblocking)
//   {
//      /* TODO */
//   }

   if((writePos + sampleCount) > AUDIO_BUFFER_SIZE)
   {
      memcpy(psp->buffer + writePos, buf, (AUDIO_BUFFER_SIZE - writePos) * sizeof(uint32_t));
      memcpy(psp->buffer, (uint32_t*) buf + (AUDIO_BUFFER_SIZE - writePos), (writePos + sampleCount - AUDIO_BUFFER_SIZE) * sizeof(uint32_t));
   }
   else
      memcpy(psp->buffer + writePos, buf, size);

   writePos  += sampleCount;
   writePos  &= AUDIO_BUFFER_SIZE_MASK;
   psp->writePos = writePos;

   return sampleCount;
}

static bool psp_audio_stop(void *data)
{
   SceKernelThreadRunStatus runStatus;
   psp1_audio_t* psp = (psp1_audio_t*)data;

   runStatus.size = sizeof(SceKernelThreadRunStatus);

   if (sceKernelReferThreadRunStatus(psp->thread, &runStatus) < 0) // error
      return false;
   if (runStatus.status == PSP_THREAD_STOPPED)
      return false;

   psp->running = false;
   SceUInt timeout = 100000;
   sceKernelWaitThreadEnd(psp->thread, &timeout);

   return true;
}

static bool psp_audio_start(void *data)
{
   SceKernelThreadRunStatus runStatus;
   psp1_audio_t* psp = (psp1_audio_t*)data;

   runStatus.size = sizeof(SceKernelThreadRunStatus);

   if (sceKernelReferThreadRunStatus(psp->thread, &runStatus) < 0) // error
      return false;
   if (runStatus.status != PSP_THREAD_STOPPED)
      return false;

   psp->running = true;

   sceKernelStartThread(psp->thread, sizeof(psp1_audio_t*), &psp);

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

static size_t psp_write_avail(void *data)
{
   /* TODO */
   psp1_audio_t* psp = (psp1_audio_t*)data;
   return AUDIO_BUFFER_SIZE - ((uint16_t)(psp->writePos - psp->readPos) & AUDIO_BUFFER_SIZE_MASK);
}

static size_t psp_buffer_size(void *data)
{
   /* TODO */
   return AUDIO_BUFFER_SIZE;
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
   psp_write_avail,
   psp_buffer_size,
};
