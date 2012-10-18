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
#define ISL_IID_BUFFERQUEUE SL_IID_ANDROIDSIMPLEBUFFERQUEUE
#else
#define ISLDataLocator_BufferQueue SLDataLocator_BufferQueue
#define ISL_DATALOCATOR_BUFFERQUEUE SL_DATALOCATOR_BUFFERQUEUE
#define ISL_IID_BUFFERQUEUE SL_IID_BUFFERQUEUE
#endif

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
   bool                        nonblock;
} sl_t;

/* Local stoarge for audio data in 16 bit words */
#define AUDIO_DATA_STORAGE_SIZE 4096
/* Audio data buffer size in 16 bit words, 8 data segments are used in this example */
#define AUDIO_DATA_BUFFER_SIZE 4096/8

/* Local storage for Audio data */
SLint16 pcmData[AUDIO_DATA_STORAGE_SIZE];

void BufferQueueCallback(SLBufferQueueItf queueItf, void *pContext)
{
   SLresult res;
   CallbackCntxt *pCntxt = (CallbackCntxt*)pContext;

   if(pCntxt->pData < (pCntxt->pDataBase + pCntxt->size))
   {
      res = (*queueItf)->Enqueue(queueItf, (void*)pCntxt->pData,
         2 * AUDIO_DATA_BUFFER_SIZE); /* Size given in bytes */

      if(res != SL_RESULT_SUCCESS)
         RARCH_WARN("queueItf->Enqueue() encountered a problem.\n");

      pCntxt->pData += AUDIO_DATA_BUFFER_SIZE;
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
      goto error;

   /* Realizing the SL Engine in synchronous mode */
   sl->res_ptr = (*sl->sl)->Realize(sl->sl, SL_BOOLEAN_FALSE);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   /* Get interface for IID_ENGINE */

   sl->res_ptr = (*sl->sl)->GetInterface(sl->sl, SL_IID_ENGINE, (void*)&sl->EngineItf);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   /* VOLUME interface */

   /* NOTES by Google: 
   * Please see section "Supported features from OpenSL ES 1.0.1",
   * subsection "Objects and interfaces" in <NDKroot>/docs/opensles/index.html
   * You'll see that the cell for object Output mix interface Volume is white.
   * This means it is not a supported combination.
   * As a workaround, use the Volume interface on the Audio player object. */

   /* This seems to fail right now:
   *  libOpenSLES(19315): class OutputMix interface 0 requested but unavailable MPH=43
   */

#if 1
   const SLInterfaceID ids[] = {SL_IID_VOLUME};
   const SLboolean     req[] = {SL_BOOLEAN_FALSE};

   /* Create Output Mix object to be used by player */
   sl->res_ptr = (*sl->EngineItf)->CreateOutputMix(sl->EngineItf, &sl->OutputMix, 1,
      ids, req);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   /* Realizing the Output Mix object in synchonous mode */
   sl->res_ptr = (*sl->OutputMix)->Realize(sl->OutputMix, SL_BOOLEAN_FALSE);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;
#endif

   /* TODO: hardcode for now, make this use the parameters in some way later */
   SLuint32 rate_sl = SL_SAMPLINGRATE_48;
   SLuint8  bits_sl = 16;

   /* Setup the data source structure for the buffer queue */
   sl->bufferQueue.locatorType = ISL_DATALOCATOR_BUFFERQUEUE;
   sl->bufferQueue.numBuffers  = 2;

   /* Setup the format of the content in the buffer queue */
   sl->pcm.formatType    = SL_DATAFORMAT_PCM;
   sl->pcm.numChannels   = 2;
   sl->pcm.samplesPerSec = rate_sl;
   sl->pcm.bitsPerSample = bits_sl;
   sl->pcm.containerSize = bits_sl;
   sl->pcm.channelMask   = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
   sl->pcm.endianness    = SL_BYTEORDER_LITTLEENDIAN;

   sl->audioSource.pFormat  = (void*)&sl->pcm;
   sl->audioSource.pLocator = (void*)&sl->bufferQueue;

   /* Setup the data sink structure */
   sl->locator_outputmix.locatorType  = SL_DATALOCATOR_OUTPUTMIX;
   sl->locator_outputmix.outputMix    = sl->OutputMix;
   sl->audioSink.pLocator             = (void*)&sl->locator_outputmix;
   sl->audioSink.pFormat              = NULL;

   /* Initialize the context for buffer queue callbacks */
   sl->cntxt.pDataBase = (void*)&pcmData;
   sl->cntxt.pData     = sl->cntxt.pDataBase;
   sl->cntxt.size      = sizeof(pcmData);

   const SLInterfaceID ids1[] = {ISL_IID_BUFFERQUEUE};
   const SLboolean req1[] = {SL_BOOLEAN_TRUE};

   /* Create the music player */
   sl->res_ptr = (*sl->EngineItf)->CreateAudioPlayer(sl->EngineItf, &sl->player,
      &sl->audioSource, &sl->audioSink, 1, ids1, req1);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   /* Realizing the player in synchronous mode. */
   sl->res_ptr = (*sl->player)->Realize(sl->player, SL_BOOLEAN_FALSE);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   /* Get interface for IID_PLAY */
   sl->res_ptr = (*sl->player)->GetInterface(sl->player, SL_IID_PLAY, (void*)&sl->playItf);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   /* Get interface for IID_BUFFERQUEUE */
   sl->res_ptr = (*sl->player)->GetInterface(sl->player, ISL_IID_BUFFERQUEUE,
      (void*)&sl->bufferQueueItf);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   /* Setup to receive buffer queue event callbacks */
   sl->res_ptr = (*sl->bufferQueueItf)->RegisterCallback(sl->bufferQueueItf,
      BufferQueueCallback, NULL);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

#if 0
   /* Before we start set volume to -3dB (-300mB) */
   /* TODO: Necessary or not? Let's not do this for now */

   sl->res_ptr = (*sl->volumeItf)->SetVolumeLevel(sl->volumeItf, -300);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;
#endif

   /* Setup for audio playback */

   sl->res_ptr = (*sl->playItf)->SetPlayState(sl->playItf, SL_PLAYSTATE_PLAYING);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      goto error;

   return sl;

error:
   RARCH_ERR("Couldn't initialize OpenSL ES driver, error code: [%d].\n", sl->res_ptr);
   if (sl)
   {
      (*sl->sl)->Destroy(sl->sl);
      free(sl);
   }
   return NULL;
}

static bool sl_stop(void *data)
{
   sl_t *sl = (sl_t*)data;

   /* Stop player */
   sl->res_ptr = (*sl->playItf)->SetPlayState(sl->playItf, SL_PLAYSTATE_STOPPED);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      RARCH_ERR("SetPlayState (SL_PLAYSTATE_STOPPED) failed.\n");

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
   sl_t *sl = (sl_t*)data;

   sl->res_ptr = (*sl->playItf)->SetPlayState(sl->playItf, SL_PLAYSTATE_PLAYING);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      RARCH_ERR("SetPlayState (SL_PLAYSTATE_PLAYING) failed.\n");

   return true;
}

static void sl_free(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (sl)
   {
      sl->res_ptr = (*sl->playItf)->SetPlayState(sl->playItf, SL_PLAYSTATE_STOPPED);
      (*sl->sl)->Destroy(sl->sl);
   }
   free(sl);
}

static ssize_t sl_write(void *data, const void *buf, size_t size)
{
   /* not sure about where to use buf, what to return */
   sl_t *sl = (sl_t*)data;

   sl->res_ptr = (*sl->bufferQueueItf)->GetState(sl->bufferQueueItf, &sl->state);

   if(sl->res_ptr != SL_RESULT_SUCCESS)
      RARCH_ERR("GetState() failed.\n");

   while(sl->state.count)
      (*sl->bufferQueueItf)->GetState(sl->bufferQueueItf, &sl->state);

   return size;
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
