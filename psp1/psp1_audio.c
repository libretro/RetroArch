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


#include <pspaudio.h>

//ToDO
typedef struct
{
   bool nonblocking;
   bool started;   
} psp1_audio_t;

psp1_audio_t audioData;

#define AUDIO_OUT_COUNT 128

#define AUDIO_BUFFER_SIZE (1u<<12u)
#define AUDIO_BUFFER_SIZE_MASK (AUDIO_BUFFER_SIZE-1)

static u32 __attribute__ ( ( aligned ( 64 ) ) ) audioBuffer[AUDIO_BUFFER_SIZE];
static u32 __attribute__ ( ( aligned ( 64 ) ) ) zeroBuffer[AUDIO_OUT_COUNT]= {0};

static bool audioRunning=false;
static SceUID audioThread=0;
static int audioOutputRate;

static u16 readPos=0;
static u16 writePos=0;

int audioMainLoop ( SceSize args, void* argp ) {

	sceAudioSRCChReserve ( AUDIO_OUT_COUNT,audioOutputRate,2 );
	while ( audioRunning ) {		
		if ( ( (u16)(readPos-writePos)& AUDIO_BUFFER_SIZE_MASK ) < (u16)AUDIO_OUT_COUNT*2 ) {
			sceAudioSRCOutputBlocking ( PSP_AUDIO_VOLUME_MAX,zeroBuffer );
		} else {
			sceAudioSRCOutputBlocking ( PSP_AUDIO_VOLUME_MAX,audioBuffer+readPos );
			readPos+=AUDIO_OUT_COUNT;
			readPos&=AUDIO_BUFFER_SIZE_MASK;
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
   audioOutputRate=rate;
   audioThread= sceKernelCreateThread ( "audioMainLoop",audioMainLoop,0x12,0x10000,0,NULL );
   psp_audio_start(NULL);
   return (void*)&audioData;
}

static void psp_audio_free(void *data)
{
   (void)data;
   if(audioThread)
      sceKernelDeleteThread(audioThread);
}

static ssize_t psp_audio_write(void *data, const void *buf, size_t size)
{
   size/=4;
   (void)data;
   (void)buf;
   size_t size1,size2;
   
   size1=size;
   if((writePos+size)>AUDIO_BUFFER_SIZE){
      memcpy(audioBuffer+writePos,buf,(AUDIO_BUFFER_SIZE-writePos)*sizeof(u32));
      memcpy(audioBuffer,buf,(writePos+size-AUDIO_BUFFER_SIZE)*sizeof(u32));
   }else{
      memcpy(audioBuffer+writePos,buf,size*sizeof(u32));
   }
   
   writePos+=size;
   writePos&=AUDIO_BUFFER_SIZE_MASK;
   return size;
}

static bool psp_audio_stop(void *data)
{
   (void)data;
   if(!audioThread)
      return false;
   
   audioRunning=false;
   return (sceKernelWaitThreadEnd(audioThread,NULL) >= 0);
}

static bool psp_audio_start(void *data)
{
   (void)data;
   if(!audioThread)
      return false;
   
   audioRunning=true;
   return (sceKernelStartThread ( audioThread,0,NULL ) >= 0);
}

static void psp_audio_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
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



