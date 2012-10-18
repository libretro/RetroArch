/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#include "../driver.h"
#include "../general.h"

#include <SLES/OpenSLES.h>
#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>
#define ISLDataLocator_BufferQueue SLDataLocator_AndroidSimpleBufferQueue
#define ISL_DATALOCATOR_BUFFERQUEUE SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE
#else
#define ISLDataLocator_BufferQueue SLDataLocator_BufferQueue
#define ISL_DATALOCATOR_BUFFERQUEUE SL_DATALOCATOR_BUFFERQUEUE
#endif

#define MAX_NUMBER_INTERFACES 3

#include <time.h>
#include <string.h>

typedef struct CallbackCntxt_
{
   SLPlayItf  playItf;
   SLint16*   pDataBase;   /* Base address of local audio data storage */
   SLint16*   pData;       /* Current address of local audio data storage */
   SLuint32   size;
} CallbackCntxt;

typedef struct sl
{
   SLObjectItf                 sl;
   SLresult                    res_ptr;
   SLEngineItf                 EngineItf;
   CallbackCntxt               cntxt;
   SLDataSource                audioSource;
   ISLDataLocator_BufferQueue  bufferQueue;
   SLDataFormat_PCM            pcm;
   SLDataSink                  audioSink;
   SLDataLocator_OutputMix     locator_outputmix;
   SLObjectItf                 player;
   SLPlayItf                   playItf;
   SLBufferQueueItf            bufferQueueItf;
   SLBufferQueueState          state;
   SLObjectItf                 OutputMix;
   SLVolumeItf                 volumeItf;
   SLboolean                   required[MAX_NUMBER_INTERFACES];
   SLInterfaceID               iidArray[MAX_NUMBER_INTERFACES];
   bool                        nonblock;
} sl_t;

/* Local stoarge for audio data in 16 bit words */
#define AUDIO_DATA_STORAGE_SIZE 4096
/* Audio data buffer size in 16 bit words, 8 data segments are used in this example */
#define AUDIO_DATA_BUFFER_SIZE 4096/8

/* Local storage for Audio data */
SLint16 pcmData[AUDIO_DATA_STORAGE_SIZE];

static inline void sl_generate_error_msg(const char * funcname, SLresult res)
{
   RARCH_ERR("%s failed, error code: [%d].\n", funcname, res);
}

void BufferQueueCallback(SLBufferQueueItf queueItf, void *pContext)
{
   SLresult res;
   CallbackCntxt *pCntxt = (CallbackCntxt*)pContext;

   if(pCntxt->pData < (pCntxt->pDataBase + pCntxt->size))
   {
      res = (*queueItf)->Enqueue(queueItf, (void*)pCntxt->pData,
         2 * AUDIO_DATA_BUFFER_SIZE); /* Size given in bytes */

      if(res != SL_RESULT_SUCCESS)
      {
	      sl_generate_error_msg("queueItf->Enqueue()", res);
	      return;
      }

      pCntxt->pData += AUDIO_DATA_BUFFER_SIZE;
   }
}

static void sl_init_extended(sl_t *sl_ctx)
{
   int i;

   /* Get the SL Engine Interface */

   sl_ctx->res_ptr = (*sl_ctx->sl)->GetInterface(sl_ctx->sl, SL_IID_ENGINE, (void*)&sl_ctx->EngineItf);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->sl->GetInterface(SL_IID_ENGINE)", sl_ctx->res_ptr);
      return;
   }

   /* Initialize arrays required[] and iidArray[] */
   for (i = 0; i < MAX_NUMBER_INTERFACES; i++)
   {
      sl_ctx->required[i] = SL_BOOLEAN_FALSE;
      sl_ctx->iidArray[i] = SL_IID_NULL;
   }

   /* Unsupported feature on Android NDK it seems */

   /* Set arrays required[] and iidArray[] for VOLUME interface */
   sl_ctx->required[0] = SL_BOOLEAN_TRUE;
   sl_ctx->iidArray[0] = SL_IID_VOLUME;

   /* Create Output Mix object to be used by player */
   sl_ctx->res_ptr = (*sl_ctx->EngineItf)->CreateOutputMix(sl_ctx->EngineItf, &sl_ctx->OutputMix, 1,
      sl_ctx->iidArray, sl_ctx->required);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("EngineItf->CreateOutputMix", sl_ctx->res_ptr);
      return;
   }

   /* Realizing the Output Mix object in synchonous mode */
   sl_ctx->res_ptr = (*sl_ctx->OutputMix)->Realize(sl_ctx->OutputMix, SL_BOOLEAN_FALSE);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("OutputMix->Realize(SL_BOOLEAN_FALSE)", sl_ctx->res_ptr);
      return;
   }

   /* Setup the data source structure for the buffer queue */
   sl_ctx->bufferQueue.locatorType = ISL_DATALOCATOR_BUFFERQUEUE;
   sl_ctx->bufferQueue.numBuffers  = 2;

   /* Setup the format of the content in the buffer queue */
   sl_ctx->pcm.formatType    = SL_DATAFORMAT_PCM;
   sl_ctx->pcm.numChannels   = 2;
   sl_ctx->pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
   sl_ctx->pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
   sl_ctx->pcm.containerSize = 16;
   sl_ctx->pcm.channelMask   = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
   sl_ctx->pcm.endianness    = SL_BYTEORDER_LITTLEENDIAN;

   sl_ctx->audioSource.pFormat  = (void*)&sl_ctx->pcm;
   sl_ctx->audioSource.pLocator = (void*)&sl_ctx->bufferQueue;

   /* Setup the data sink structure */
   sl_ctx->locator_outputmix.locatorType  = SL_DATALOCATOR_OUTPUTMIX;
   sl_ctx->locator_outputmix.outputMix    = sl_ctx->OutputMix;
   sl_ctx->audioSink.pLocator             = (void*)&sl_ctx->locator_outputmix;
   sl_ctx->audioSink.pFormat              = NULL;

   /* Initialize the context for buffer queue callbacks */
   sl_ctx->cntxt.pDataBase = (void*)&pcmData;
   sl_ctx->cntxt.pData     = sl_ctx->cntxt.pDataBase;
   sl_ctx->cntxt.size      = sizeof(pcmData);

   /* Set arrays required[] and iidArray[] for SEEK interface
   *  (PlayItf)is implicit) */
   sl_ctx->required[0] = SL_BOOLEAN_TRUE;
   sl_ctx->iidArray[0] = SL_IID_BUFFERQUEUE;

   /* Create the music player */
   sl_ctx->res_ptr = (*sl_ctx->EngineItf)->CreateAudioPlayer(sl_ctx->EngineItf, &sl_ctx->player,
      &sl_ctx->audioSource, &sl_ctx->audioSink, 1, sl_ctx->iidArray, sl_ctx->required);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->EngineItf->CreateAudioPlayer()", sl_ctx->res_ptr);
      return;
   }

   /* Realizing the player in synchronous mode. */
   sl_ctx->res_ptr = (*sl_ctx->player)->Realize(sl_ctx->player, SL_BOOLEAN_FALSE);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->player->Realize(SL_BOOLEAN_FALSE)", sl_ctx->res_ptr);
      return;
   }

   /* Get seek and play interfaces */
   sl_ctx->res_ptr = (*sl_ctx->player)->GetInterface(sl_ctx->player, SL_IID_PLAY, (void*)&sl_ctx->playItf);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->player->GetInterface(SL_IID_PLAY)", sl_ctx->res_ptr);
      return;
   }

   sl_ctx->res_ptr = (*sl_ctx->player)->GetInterface(sl_ctx->player, SL_IID_BUFFERQUEUE,
      (void*)&sl_ctx->bufferQueueItf);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->player->GetInterface(SL_IID_BUFFERQUEUE)", sl_ctx->res_ptr);
      return;
   }

   /* Setup to receive buffer queue event callbacks */
   sl_ctx->res_ptr = (*sl_ctx->bufferQueueItf)->RegisterCallback(sl_ctx->bufferQueueItf,
      BufferQueueCallback, NULL);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->bufferQueueItf->RegisterCallback(BufferQueueCallback)", sl_ctx->res_ptr);
      return;
   }

   /* Before we start set volume to -3dB (-300mB) */
   sl_ctx->res_ptr = (*sl_ctx->volumeItf)->SetVolumeLevel(sl_ctx->volumeItf, -300);

   if(sl_ctx->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->volumeItf->SetVolumeLevel(-300)", sl_ctx->res_ptr);
      return;
   }
}

static void *sl_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   sl_t *sl = (sl_t*)calloc(1, sizeof(sl_t));
   if (!sl)
      goto error;

   SLEngineOption EngineOption[] = 
   {
      (SLuint32) SL_ENGINEOPTION_THREADSAFE,
      (SLuint32) SL_BOOLEAN_TRUE
   };

   sl->res_ptr = slCreateEngine(&sl->sl, 1, EngineOption, 0, NULL, NULL);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl_ctx->sl->GetInterface(SL_IID_ENGINE)", sl->res_ptr);
      goto error;
   }

   /* Realizing the SL Engine in synchronous mode */
   sl->res_ptr = (*sl->sl)->Realize(sl->sl, SL_BOOLEAN_FALSE);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
   {
      sl_generate_error_msg("sl->sl->Realize(SL_BOOLEAN_FALSE)", sl->res_ptr);
      goto error;
   }

   sl_init_extended(sl);

   return sl;

error:
   if (sl)
   {
      (*sl->sl)->Destroy(sl->sl);
      free(sl);
   }
   return NULL;
}

static bool sl_stop(void *data)
{
   (void)data;
   return true;
}

static void sl_set_nonblock_state(void *data, bool state)
{
   sl_t *sl = (sl_t*)data;
   sl->nonblock = state;
}

static bool sl_start(void *data)
{
   (void)data;
   return true;
}

static void sl_free(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (sl)
   {
      (*sl->sl)->Destroy(sl->sl);
   }
   free(sl);
}

static ssize_t sl_write(void *data, const void *buf, size_t size)
{
   (void)data;
   (void)buf;
   (void)size;

   return 0;
}

const audio_driver_t audio_opensl = {
   sl_init,
   sl_write,
   sl_stop,
   sl_start,
   sl_set_nonblock_state,
   sl_free,
   NULL,
   "opensl"
};
