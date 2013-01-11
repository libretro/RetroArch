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

// Simple C interface for XAudio2

#include "xaudio.h"
#include "xaudio-c.h"
#include <stdint.h>
#include <stdio.h>
#include "../../msvc/msvc_compat.h"
#include "../../boolean.h"

#define MAX_BUFFERS 16
#define MAX_BUFFERS_MASK (MAX_BUFFERS - 1)

#undef min
#undef max
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

struct xaudio2 : public IXAudio2VoiceCallback
{
   xaudio2() :
      buf(0), pXAudio2(0), pMasterVoice(0),
      pSourceVoice(0), hEvent(0), buffers(0), bufsize(0),
      bufptr(0), write_buffer(0)
   {}

   virtual ~xaudio2() {}

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
   IXAudio2MasteringVoice *pMasterVoice;
   IXAudio2SourceVoice *pSourceVoice;
   HANDLE hEvent;

   volatile long buffers;
   unsigned bufsize;
   unsigned bufptr;
   unsigned write_buffer;
};

void xaudio2_enumerate_devices(xaudio2_t *xa)
{
   (void)xa;
#ifndef _XBOX
   UINT32 dev_count;
   xa->pXAudio2->GetDeviceCount(&dev_count);
   fprintf(stderr, "XAudio2 devices:\n");
   for (unsigned i = 0; i < dev_count; i++)
   {
      XAUDIO2_DEVICE_DETAILS dev_detail;
      xa->pXAudio2->GetDeviceDetails(i, &dev_detail);
      fwprintf(stderr, L"\t%u: %s\n", i, dev_detail.DisplayName);
   }
#endif
}

static void xaudio2_set_wavefmt(WAVEFORMATEX *wfx,
      unsigned channels, unsigned samplerate)
{
   wfx->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
   wfx->nBlockAlign = channels * sizeof(float);
   wfx->wBitsPerSample = sizeof(float) * 8;

   wfx->nChannels = channels;
   wfx->nSamplesPerSec = samplerate;
   wfx->nAvgBytesPerSec = wfx->nSamplesPerSec * wfx->nBlockAlign;
   wfx->cbSize = 0;
}

xaudio2_t *xaudio2_new(unsigned samplerate, unsigned channels,
      size_t size, unsigned device)
{
#ifndef _XBOX
   CoInitializeEx(0, COINIT_MULTITHREADED);
#endif

   xaudio2_t *handle = new xaudio2;

   WAVEFORMATEX wfx = {0};

   if (FAILED(XAudio2Create(&handle->pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
      goto error;

   if (FAILED(handle->pXAudio2->CreateMasteringVoice(&handle->pMasterVoice,
               channels, samplerate, 0, device, NULL)))
      goto error;

   xaudio2_set_wavefmt(&wfx, channels, samplerate);

   if (FAILED(handle->pXAudio2->CreateSourceVoice(&handle->pSourceVoice, &wfx,
               XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO,
               handle)))
      goto error;

   handle->hEvent = CreateEvent(0, FALSE, FALSE, 0);
   if (!handle->hEvent)
      goto error;

   handle->bufsize = size / MAX_BUFFERS;
   handle->buf = (uint8_t*)calloc(1, handle->bufsize * MAX_BUFFERS);
   if (!handle->buf)
      goto error;

   if (FAILED(handle->pSourceVoice->Start(0)))
      goto error;

   return handle;

error:
   xaudio2_free(handle);
   return NULL;
}

size_t xaudio2_write_avail(xaudio2_t *handle)
{
   return handle->bufsize * (MAX_BUFFERS - handle->buffers - 1);
}

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
   if (!handle)
      return;

   if (handle->pSourceVoice)
   {
      handle->pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
      handle->pSourceVoice->DestroyVoice();
   }

   if (handle->pMasterVoice)
      handle->pMasterVoice->DestroyVoice();

   if (handle->pXAudio2)
      handle->pXAudio2->Release();

   if (handle->hEvent)
      CloseHandle(handle->hEvent);

   free(handle->buf);
   delete handle;
}

