/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <boolean.h>

#include <compat/msvc.h>

#include "xaudio.h"

#include "../../driver.h"
#include "../../general.h"

typedef struct xaudio2 xaudio2_t;

#define MAX_BUFFERS      16

#define MAX_BUFFERS_MASK (MAX_BUFFERS - 1)

#undef min
#undef max
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

typedef struct
{
   xaudio2_t *xa;
   bool nonblock;
   bool is_paused;
   size_t bufsize;
} xa_t;

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
      InterlockedDecrement((LONG volatile*)&buffers);
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

   unsigned long volatile buffers;
   unsigned bufsize;
   unsigned bufptr;
   unsigned write_buffer;
};

#if 0
static void xaudio2_enumerate_devices(xaudio2_t *xa)
{
   uint32_t dev_count = 0;
   unsigned i = 0;

   (void)xa;
   (void)i;
   (void)dev_count;
#ifndef _XBOX
   xa->pXAudio2->GetDeviceCount(&dev_count);
   fprintf(stderr, "XAudio2 devices:\n");

   for (i = 0; i < dev_count; i++)
   {
      XAUDIO2_DEVICE_DETAILS dev_detail;
      xa->pXAudio2->GetDeviceDetails(i, &dev_detail);
      fwprintf(stderr, L"\t%u: %s\n", i, dev_detail.DisplayName);
   }
#endif
}
#endif

static void xaudio2_set_wavefmt(WAVEFORMATEX *wfx,
      unsigned channels, unsigned samplerate)
{
   wfx->wFormatTag      = WAVE_FORMAT_IEEE_FLOAT;
   wfx->nBlockAlign     = channels * sizeof(float);
   wfx->wBitsPerSample  = sizeof(float) * 8;

   wfx->nChannels       = channels;
   wfx->nSamplesPerSec  = samplerate;
   wfx->nAvgBytesPerSec = wfx->nSamplesPerSec * wfx->nBlockAlign;
   wfx->cbSize          = 0;
}

static void xaudio2_free(xaudio2_t *handle)
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

static xaudio2_t *xaudio2_new(unsigned samplerate, unsigned channels,
      size_t size, unsigned device)
{
   xaudio2_t *handle;
   WAVEFORMATEX wfx = {0};

#ifndef _XBOX
   CoInitializeEx(0, COINIT_MULTITHREADED);
#endif

   handle = new xaudio2;

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

   handle->hEvent  = CreateEvent(0, FALSE, FALSE, 0);
   if (!handle->hEvent)
      goto error;

   handle->bufsize = size / MAX_BUFFERS;
   handle->buf     = (uint8_t*)calloc(1, handle->bufsize * MAX_BUFFERS);
   if (!handle->buf)
      goto error;

   if (FAILED(handle->pSourceVoice->Start(0)))
      goto error;

   return handle;

error:
   xaudio2_free(handle);
   return NULL;
}

static size_t xaudio2_write_avail(xaudio2_t *handle)
{
   return handle->bufsize * (MAX_BUFFERS - handle->buffers - 1);
}

static size_t xaudio2_write(xaudio2_t *handle, const void *buf, size_t bytes_)
{
   unsigned bytes = bytes_;
   const uint8_t *buffer = (const uint8_t*)buf;

   while (bytes)
   {
      unsigned need   = min(bytes, handle->bufsize - handle->bufptr);

      memcpy(handle->buf + handle->write_buffer * handle->bufsize + handle->bufptr,
            buffer, need);

      handle->bufptr += need;
      buffer         += need;
      bytes          -= need;

      if (handle->bufptr == handle->bufsize)
      {
         XAUDIO2_BUFFER xa2buffer = {0};

         while (handle->buffers == MAX_BUFFERS - 1)
            WaitForSingleObject(handle->hEvent, INFINITE);

         xa2buffer.AudioBytes = handle->bufsize;
         xa2buffer.pAudioData = handle->buf + handle->write_buffer * handle->bufsize;

         if (FAILED(handle->pSourceVoice->SubmitSourceBuffer(&xa2buffer, NULL)))
            return 0;

         InterlockedIncrement((LONG volatile*)&handle->buffers);
         handle->bufptr       = 0;
         handle->write_buffer = (handle->write_buffer + 1) & MAX_BUFFERS_MASK;
      }
   }

   return bytes_;
}

static void *xa_init(const char *device, unsigned rate, unsigned latency)
{
   size_t bufsize;
   unsigned device_index = 0;
   xa_t *xa = (xa_t*)calloc(1, sizeof(*xa));
   if (!xa)
      return NULL;

   if (latency < 8)
      latency = 8; /* Do not allow shenanigans. */

   bufsize = latency * rate / 1000;

   RARCH_LOG("XAudio2: Requesting %u ms latency, using %d ms latency.\n",
         latency, (int)bufsize * 1000 / rate);

   xa->bufsize = bufsize * 2 * sizeof(float);

   if (device)
      device_index = strtoul(device, NULL, 0);

   xa->xa = xaudio2_new(rate, 2, xa->bufsize, device_index);
   if (!xa->xa)
   {
      RARCH_ERR("Failed to init XAudio2.\n");
      free(xa);
      return NULL;
   }

   return xa;
}

static ssize_t xa_write(void *data, const void *buf, size_t size)
{
   size_t ret;
   xa_t *xa = (xa_t*)data;

   if (xa->nonblock)
   {
      size_t avail = xaudio2_write_avail(xa->xa);

      if (avail == 0)
         return 0;
      if (avail < size)
         size = avail;
   }

   ret = xaudio2_write(xa->xa, buf, size);
   if (ret == 0 && size > 0)
      return -1;
   return ret;
}

static bool xa_stop(void *data)
{
   xa_t *xa = (xa_t*)data;
   xa->is_paused = true;
   return true;
}

static bool xa_alive(void *data)
{
   xa_t *xa = (xa_t*)data;
   if (!xa)
      return false;
   return !xa->is_paused;
}

static void xa_set_nonblock_state(void *data, bool state)
{
   xa_t *xa = (xa_t*)data;
   if (xa)
      xa->nonblock = state;
}

static bool xa_start(void *data)
{
   xa_t *xa = (xa_t*)data;
   xa->is_paused = false;
   return true;
}

static bool xa_use_float(void *data)
{
   (void)data;
   return true;
}

static void xa_free(void *data)
{
   xa_t *xa = (xa_t*)data;

   if (!xa)
      return;

   if (xa->xa)
      xaudio2_free(xa->xa);
   free(xa);
}

static size_t xa_write_avail(void *data)
{
   xa_t *xa = (xa_t*)data;
   return xaudio2_write_avail(xa->xa);
}

static size_t xa_buffer_size(void *data)
{
   xa_t *xa = (xa_t*)data;
   return xa->bufsize;
}

audio_driver_t audio_xa = {
   xa_init,
   xa_write,
   xa_stop,
   xa_start,
   xa_alive,
   xa_set_nonblock_state,
   xa_free,
   xa_use_float,
   "xaudio",
   xa_write_avail,
   xa_buffer_size,
};
