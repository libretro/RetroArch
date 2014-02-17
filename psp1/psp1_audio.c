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
   bool running;
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


 
int audioMainLoop ( SceSize args, void* argp ) {
   psp1_audio_t* aud=*((psp1_audio_t**)argp);
   
	sceAudioSRCChReserve ( AUDIO_OUT_COUNT,aud->rate,2 );
	while ( aud->running ) {		
		if ( ( (uint16_t)(aud->writePos-aud->readPos)& AUDIO_BUFFER_SIZE_MASK ) < (uint16_t)AUDIO_OUT_COUNT*2 ) {
			sceAudioSRCOutputBlocking ( PSP_AUDIO_VOLUME_MAX,aud->zeroBuffer );
		} else {
			sceAudioSRCOutputBlocking ( PSP_AUDIO_VOLUME_MAX,aud->buffer+aud->readPos );
			aud->readPos+=AUDIO_OUT_COUNT;
			aud->readPos&=AUDIO_BUFFER_SIZE_MASK;
		}


	}
	sceAudioSRCChRelease();
	sceKernelExitThread ( 0 );
	return 0;
}
static bool psp_audio_start(void *data);
static void *psp_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)latency;
   psp1_audio_t* data=calloc(1,sizeof(psp1_audio_t));
   data->buffer=calloc(AUDIO_BUFFER_SIZE,sizeof(uint32_t));
   data->readPos=0;
   data->writePos=0;
   data->zeroBuffer=calloc(AUDIO_OUT_COUNT,sizeof(uint32_t));
   data->nonblocking=true;
   data->rate=rate;
   data->running=false;
   data->thread = sceKernelCreateThread ( "audioMainLoop",audioMainLoop,0x12,0x10000,0,NULL );
      
   psp_audio_start(data);
   return (void*)data;
}

static void psp_audio_free(void *data)
{
   psp1_audio_t* aud=(psp1_audio_t*)data;
   sceKernelDeleteThread(aud->thread);
}

static ssize_t psp_audio_write(void *data, const void *buf, size_t size)
{
   psp1_audio_t* aud;
   uint16_t sampleCount;
   // ToDo : add support for blocking audio
   
   aud=(psp1_audio_t*)data;      
   sampleCount=size/sizeof(uint32_t);
   
   if((aud->writePos+sampleCount)>AUDIO_BUFFER_SIZE){
      memcpy(aud->buffer+aud->writePos,buf,(AUDIO_BUFFER_SIZE-aud->writePos)*sizeof(uint32_t));
      memcpy(aud->buffer,buf,(aud->writePos+sampleCount-AUDIO_BUFFER_SIZE)*sizeof(uint32_t));
   }else{
      memcpy(aud->buffer+aud->writePos,buf,size);
   }
   
   aud->writePos+=sampleCount;
   aud->writePos&=AUDIO_BUFFER_SIZE_MASK;
   return sampleCount;
}

static bool psp_audio_stop(void *data)
{
   psp1_audio_t* aud=(psp1_audio_t*)data;
   if(aud->thread <= 0) // ToDO: verify that this is the correct way to check a thread ID for validity
      return false;
   
   aud->running=false;
   return (sceKernelWaitThreadEnd(aud->thread,NULL) >= 0);
}

static bool psp_audio_start(void *data)
{
   psp1_audio_t* aud=(psp1_audio_t*)data;
   if(aud->thread <= 0)
      return false;
   
   aud->running=true;
   return (sceKernelStartThread ( aud->thread,sizeof(psp1_audio_t*),&data ) >= 0);
}

static void psp_audio_set_nonblock_state(void *data, bool state)
{
   psp1_audio_t* aud=(psp1_audio_t*)data;
   aud->nonblocking=state;
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



