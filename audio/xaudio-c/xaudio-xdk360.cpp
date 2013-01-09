/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdlib.h>
#include <xtl.h>
#include "xaudio-xdk360.h"
#include "xaudio-c.h"

#define MAX_BUFFERS 16
#define MAX_BUFFERS_MASK 15

struct xaudio2 : public IXAudio2VoiceCallback
{
   xaudio2() :
      buf(0), pXAudio2(0), pMasteringVoice(0),
      pSourceVoice(0), nonblock(false), bufsize(0),
      bufptr(0), write_buffer(0), buffers(0), hEvent(0)
   {}

   ~xaudio2()
   {
      if (pSourceVoice)
      {
         pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
         pSourceVoice->DestroyVoice();
      }

      if (pMasteringVoice)
         pMasteringVoice->DestroyVoice();

      if (pXAudio2)
         pXAudio2->Release();

      if (hEvent)
         CloseHandle(hEvent);

      free(buf);
   }

   STDMETHOD_(void, OnBufferStart) (void *) {}
   STDMETHOD_(void, OnBufferEnd) (void *) 
   {
      InterlockedDecrement(&buffers);
      SetEvent(hEvent);
   }
   STDMETHOD_(void, OnLoopEnd) (void *) {}
   STDMETHOD_(void, OnStreamEnd) () {}
   STDMETHOD_(void, OnVoiceError) (void *, HRESULT) {}
   STDMETHOD_(void, OnVoiceProcessingPassEnd) () {}
   STDMETHOD_(void, OnVoiceProcessingPassStart) (UINT32) {}

   uint8_t *buf;
   IXAudio2 *pXAudio2;
   IXAudio2MasteringVoice *pMasteringVoice;
   IXAudio2SourceVoice *pSourceVoice;
   bool nonblock;
   unsigned bufsize;
   unsigned bufptr;
   unsigned write_buffer;
   volatile long buffers;
   HANDLE hEvent;
};

void xaudio2_enumerate_devices(xaudio2_t *xa)
{
   (void)xa;
}

xaudio2_t *xaudio2_new(unsigned samplerate, unsigned channels,
    size_t size, unsigned device)
{
   (void)device;
   WAVEFORMATEX wfx = {0};
   xaudio2 *handle = new xaudio2;
   if (!handle)
      return NULL;

    if (FAILED(XAudio2Create(&handle->pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
        goto error;

    if (FAILED(handle->pXAudio2->CreateMasteringVoice(&handle->pMasteringVoice, channels,
samplerate, 0, 0, NULL)))
        goto error;

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = channels;
    wfx.nSamplesPerSec = samplerate;
    wfx.nBlockAlign = channels * sizeof(int16_t);
    wfx.wBitsPerSample = sizeof(int16_t) * 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    if (FAILED(handle->pXAudio2->CreateSourceVoice(&handle->pSourceVoice, &wfx,
                XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO, handle, 0, 0)))
        goto error;

    handle->hEvent = CreateEvent(0, FALSE, FALSE, 0);

    handle->bufsize = size / MAX_BUFFERS;
    handle->buf = (uint8_t*)calloc(1, handle->bufsize * MAX_BUFFERS);
    memset(handle->buf, 0, handle->bufsize * MAX_BUFFERS);

    if (FAILED(handle->pSourceVoice->Start(0, XAUDIO2_COMMIT_NOW)))
        goto error;

   return handle;

error:
   RARCH_ERR("Failed to init XAudio2 (for Xbox 360).\n");
   delete handle;
   return NULL;
}

size_t xaudio2_write_avail(xaudio2_t *handle)
{
   return handle->bufsize * (MAX_BUFFERS - handle->buffers - 1);
}

// It's really 16-bit, but we have to byteswap.
size_t xaudio2_write(xaudio2_t *handle, const void *buf, size_t bytes_)
{
   unsigned bytes = bytes_;
   const uint8_t *buffer = (const uint8_t*)buf;
   while (bytes)
   {
      unsigned need = min(bytes, handle->bufsize - handle->bufptr);
      memcpy(handle->buf + handle->write_buffer * handle->bufsize + handle->bufptr,
            buffer, need);

      handle->bufptr += need;
      buffer += need;
      bytes -= need;

      if (handle->bufptr == handle->bufsize)
      {
         while (handle->buffers == MAX_BUFFERS - 1)
            WaitForSingleObject(handle->hEvent, INFINITE);

         XAUDIO2_BUFFER xa2buffer = {0};
         xa2buffer.AudioBytes = handle->bufsize;
         xa2buffer.pAudioData = handle->buf + handle->write_buffer * handle->bufsize;

         if (FAILED(handle->pSourceVoice->SubmitSourceBuffer(&xa2buffer, NULL)))
            return 0;

         InterlockedIncrement(&handle->buffers);
         handle->bufptr = 0;
         handle->write_buffer = (handle->write_buffer + 1) & MAX_BUFFERS_MASK;
      }
   }

   return bytes_;
}

void xaudio2_free(xaudio2_t *handle)
{
   xaudio2 *xa = (xaudio2*)handle;
   if (xa)
      delete xa;
}
