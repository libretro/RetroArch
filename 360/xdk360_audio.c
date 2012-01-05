/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../driver.h"
#include <stdlib.h>
#include <xtl.h>
#include <xaudio2.h>
#include "../general.h"

#define MAX_BUFFERS 16
#define MAX_BUFFERS_MASK 15

typedef struct
{
   uint8_t *buf;
   IXAudio2 * pXAudio2;
   IXAudio2MasteringVoice *pMasteringVoice;
   IXAudio2SourceVoice *pSourceVoice;
   bool nonblock;
   unsigned bufsize;
   unsigned bufptr;
   unsigned write_buffer;
   volatile long buffers;
   HANDLE hEvent;
} xa_t;

static void *xa_init(const char *device, unsigned rate, unsigned latency)
{
   HRESULT hr;
   unsigned flags;
   WAVEFORMATEXTENSIBLE wfx;

   flags = 0;

   if (latency < 8)
      latency = 8; // Do not allow shenanigans.

   xa_t *xa = (xa_t*)calloc(1, sizeof(*xa));
   if (!xa)
   	goto error;

   size_t bufsize = latency * rate / 1000;
   size_t size = bufsize * 2 * sizeof(float);

   SSNES_LOG("XAudio2: Requesting %d ms latency, using %d ms latency.\n", latency, (int)bufsize * 1000 / rate);

   if( FAILED( hr = XAudio2Create(&xa->pXAudio2, flags)))
   	goto error;

   if (FAILED(xa->pXAudio2->CreateMasteringVoice(&xa->pMasteringVoice, 2, rate, 0, 0, NULL)))
      goto error;

   //wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
   //wfx.nChannels = 2;
   //wfx.nSamplesPerSec = rate;
   //wfx.nBlockAlign = channels * sizeof(float);
   //wfx.wBitsPerSample = sizeof(float) * 8;
   //wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
   //wfx.cbSize = 0;

   if( FAILED( hr = xa->pXAudio2->CreateSourceVoice( &xa->pSourceVoice, ( WAVEFORMATEX*)&wfx ) ) )
   	goto error;

   xa->bufsize = size / MAX_BUFFERS;
   xa->buf = (uint8_t*)calloc(1, xa->bufsize * MAX_BUFFERS);

   if (!xa->buf)
   	goto error;

   if( FAILED(xa->pSourceVoice->Start(0)))
        goto error;

   if (!xa->pXAudio2)
	   goto error;

   return xa;

error:
   SSNES_ERR("Failed to init XAudio2.\n");
   free(xa);
   return NULL;
}

static size_t xaudio2_write(xa_t *handle, const void *buf, size_t bytes_)
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

         if (FAILED(handle->pSourceVoice->SubmitSourceBuffer(&xa2buffer)))
            return 0;

         InterlockedIncrement(&handle->buffers);
         handle->bufptr = 0;
         handle->write_buffer = (handle->write_buffer + 1) & MAX_BUFFERS_MASK;
      }
   }

   return bytes_;
}

static ssize_t xa_write(void *data, const void *buf, size_t size)
{
   size_t avail;
   xa_t *xa = (xa_t*)data;
   if (xa->nonblock)
   {
      avail = (xa->bufsize * (MAX_BUFFERS - xa->buffers - 1));
      if (avail == 0)
         return 0;
      if (avail < size)
         size = avail;
   }

   size_t ret = xaudio2_write(xa, buf, size);
   if (ret == 0)
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
   xa_t *xa = (xa_t*)data;
   xa->nonblock = state;
}

static bool xa_start(void *data)
{
   (void)data;
   return true;
}

static bool xa_use_float(void *data)
{
   (void)data;
   return true;
}

static void xaudio2_free(xa_t *handle)
{
   if (handle)
   {
      if (handle->pSourceVoice)
      {
         handle->pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
         handle->pSourceVoice->DestroyVoice();
      }

      if (handle->pMasteringVoice)
         handle->pMasteringVoice->DestroyVoice();

      if (handle->pXAudio2)
         handle->pXAudio2->Release();

      if (handle->hEvent)
         CloseHandle(handle->hEvent);

      free(handle->buf);
      free(handle);
   }
}

static void xa_free(void *data)
{
   xa_t *xa = (xa_t*)data;
   if (xa)
   {
      if (xa->pXAudio2)
         xaudio2_free(xa);
      free(xa->pXAudio2);
   }
}

const audio_driver_t audio_xa = {
   xa_init,
   xa_write,
   xa_stop,
   xa_start,
   xa_set_nonblock_state,
   xa_free,
   xa_use_float,
   "xdk360_xaudio"
};
