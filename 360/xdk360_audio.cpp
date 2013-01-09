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

#include "../driver.h"
#include <stdlib.h>
#include <xtl.h>
#include "xaudio-c/xaudio.h"
#include "../general.h"

#define MAX_BUFFERS 16
#define MAX_BUFFERS_MASK 15

struct XAudio : public IXAudio2VoiceCallback
{
   XAudio() :
      buf(0), pXAudio2(0), pMasteringVoice(0),
      pSourceVoice(0), nonblock(false), bufsize(0),
      bufptr(0), write_buffer(0), buffers(0), hEvent(0)
   {}

   ~XAudio()
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

   bool init(unsigned rate, unsigned latency)
   {
      size_t bufsize_ = latency * rate / 1000;
      size_t size = bufsize_ * 2 * sizeof(int16_t);

      RARCH_LOG("XAudio2: Requesting %d ms latency, using %d ms latency.\n", latency, (int)bufsize_ * 1000 / rate);

      if (FAILED(XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
         return false;

      if (FAILED(pXAudio2->CreateMasteringVoice(&pMasteringVoice, 2, rate, 0, 0, NULL)))
         return false;

      WAVEFORMATEX wfx = {0};
      wfx.wFormatTag = WAVE_FORMAT_PCM;
      wfx.nChannels = 2;
      wfx.nSamplesPerSec = rate;
      wfx.nBlockAlign = 2 * sizeof(int16_t);
      wfx.wBitsPerSample = sizeof(int16_t) * 8;
      wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
      wfx.cbSize = 0;

      if (FAILED(pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx,
                  XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO, this, 0, 0)))
         return false;

      hEvent = CreateEvent(0, FALSE, FALSE, 0);

      bufsize = size / MAX_BUFFERS;
      buf = (uint8_t*)malloc(bufsize * MAX_BUFFERS);
      memset(buf, 0, bufsize * MAX_BUFFERS);

      if (FAILED(pSourceVoice->Start(0, XAUDIO2_COMMIT_NOW)))
         return false;

      return true;
   }

   // It's really 16-bit, but we have to byteswap.
   size_t write(const uint8_t *buffer, size_t size)
   {
      if (nonblock)
      {
         size_t avail = bufsize * (MAX_BUFFERS - buffers - 1);
         if (avail == 0)
            return 0;
         if (avail < size)
            size = avail;
      }

      unsigned bytes = size;
      while (bytes)
      {
         unsigned need = min(bytes, bufsize - bufptr);
         uint8_t *base_write = buf + write_buffer * bufsize + bufptr;
         memcpy(base_write, buffer, need);

         bufptr += need;
         buffer += need;
         bytes -= need;

         if (bufptr == bufsize)
         {
            while (buffers == MAX_BUFFERS - 1)
               WaitForSingleObject(hEvent, INFINITE);

            XAUDIO2_BUFFER xa2buffer = {0};
            xa2buffer.AudioBytes = bufsize;
            xa2buffer.pAudioData = buf + write_buffer * bufsize;

            if (FAILED(pSourceVoice->SubmitSourceBuffer(&xa2buffer, NULL)))
               return 0;

            InterlockedIncrement(&buffers);
            bufptr = 0;
            write_buffer = (write_buffer + 1) & MAX_BUFFERS_MASK;
         }
      }

      return size;
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

static void *xa_init(const char *device, unsigned rate, unsigned latency)
{
   if (latency < 8)
      latency = 8; // Do not allow shenanigans.

   XAudio *xa = new XAudio;
   if (!xa->init(rate, latency))
      goto error;

   return xa;

error:
   RARCH_ERR("Failed to init XAudio2.\n");
   delete xa;
   return NULL;
}

static ssize_t xa_write(void *data, const void *buf, size_t size)
{
   XAudio *xa = (XAudio*)data;
   size_t ret = xa->write((const uint8_t*)buf, size);

   if (ret == 0 && !xa->nonblock)
      return -1;
   return ret;
}

static bool xa_stop(void *data)
{
   (void)data;
   return true;
}

static void xa_set_nonblock_state(void *data, bool state)
{
   XAudio *xa = (XAudio*)data;
   xa->nonblock = state;
}

static bool xa_start(void *data)
{
   (void)data;
   return true;
}

static void xa_free(void *data)
{
   XAudio *xa = (XAudio*)data;
   if (xa)
      delete xa;
}

static bool xa_use_float(void *data)
{
   (void)data;
   return false;
}

const audio_driver_t audio_xdk360 = {
   xa_init,
   xa_write,
   xa_stop,
   xa_start,
   xa_set_nonblock_state,
   xa_free,
   xa_use_float,
   "xdk360"
};
