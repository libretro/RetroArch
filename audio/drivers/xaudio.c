/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#if !defined(_XBOX) && (_MSC_VER == 1310)
#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif
#endif

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <boolean.h>

#include <compat/msvc.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <lists/string_list.h>

#if defined(_MSC_VER) && (_WIN32_WINNT <= _WIN32_WINNT_WIN2K)
/* needed for CoInitializeEx */
#define _WIN32_DCOM
#endif

#include "xaudio.h"

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
#include "../common/mmdevice_common.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct xaudio2 xaudio2_t;

#define MAX_BUFFERS      16

#define MAX_BUFFERS_MASK (MAX_BUFFERS - 1)

#ifndef COINIT_MULTITHREADED
#define COINIT_MULTITHREADED 0x00
#endif

#define XAUDIO2_WRITE_AVAILABLE(handle) ((handle)->bufsize * (MAX_BUFFERS - (handle)->buffers - 1))

typedef struct
{
   xaudio2_t *xa;
   bool nonblock;
   bool is_paused;
   size_t bufsize;
} xa_t;

/* Forward declarations */
static void *xa_list_new(void *u);

#if defined(__cplusplus) && !defined(CINTERFACE)
struct xaudio2 : public IXAudio2VoiceCallback
#else
struct xaudio2
#endif
{
#if defined(__cplusplus) && !defined(CINTERFACE)
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
#else
   const IXAudio2VoiceCallbackVtbl *lpVtbl;
#endif

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

#if !defined(__cplusplus) || defined(CINTERFACE)
static void WINAPI voice_on_buffer_end(IXAudio2VoiceCallback *handle_, void *data)
{
   xaudio2_t *handle = (xaudio2_t*)handle_;
   (void)data;
   InterlockedDecrement((LONG volatile*)&handle->buffers);
   SetEvent(handle->hEvent);
}

static void WINAPI dummy_voidp(IXAudio2VoiceCallback *handle, void *data) { (void)handle; (void)data; }
static void WINAPI dummy_nil(IXAudio2VoiceCallback *handle) { (void)handle; }
static void WINAPI dummy_uint32(IXAudio2VoiceCallback *handle, UINT32 dummy) { (void)handle; (void)dummy; }
static void WINAPI dummy_voidp_hresult(IXAudio2VoiceCallback *handle, void *data, HRESULT dummy) { (void)handle; (void)data; (void)dummy; }

const struct IXAudio2VoiceCallbackVtbl voice_vtable = {
   dummy_uint32,
   dummy_nil,
   dummy_nil,
   dummy_voidp,
   voice_on_buffer_end,
   dummy_voidp,
   dummy_voidp_hresult,
};
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
      IXAudio2SourceVoice_Stop(handle->pSourceVoice,
            0, XAUDIO2_COMMIT_NOW);
      IXAudio2SourceVoice_DestroyVoice(handle->pSourceVoice);
   }

   if (handle->pMasterVoice)
   {
      IXAudio2MasteringVoice_DestroyVoice(handle->pMasterVoice);
   }

   if (handle->pXAudio2)
   {
      IXAudio2_Release(handle->pXAudio2);
   }

   if (handle->hEvent)
      CloseHandle(handle->hEvent);

   free(handle->buf);

#if defined(__cplusplus) && !defined(CINTERFACE)
   delete handle;
#else
   free(handle);
#endif
}

static xaudio2_t *xaudio2_new(unsigned samplerate, unsigned channels,
      size_t size, const char *device)
{
   int32_t idx_found        = -1;
   WAVEFORMATEX wfx         = {0};
   struct string_list *list = NULL;
#if defined(__cplusplus) && !defined(CINTERFACE)
   xaudio2_t *handle        = new xaudio2;
#else
   xaudio2_t *handle        = (xaudio2_t*)calloc(1, sizeof(*handle));
#endif

   if (!handle)
      goto error;

   list                     = (struct string_list*)xa_list_new(NULL);

#if !defined(__cplusplus) || defined(CINTERFACE)
   handle->lpVtbl = &voice_vtable;
#endif

   if (FAILED(XAudio2Create(&handle->pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
      goto error;

   if (device)
   {
      /* Search for device name first */
      if (list && list->elems)
      {
         if (list->elems)
         {
            unsigned i;
            for (i = 0; i < list->size; i++)
            {
               if (string_is_equal(device, list->elems[i].data))
               {
                  idx_found       = i;
                  break;
               }
            }
            /* Index was not found yet based on name string,
             * just assume id is a one-character number index. */

            if (idx_found == -1)
            {
               if (isdigit(device[0]))
               {
                  RARCH_LOG("[XAudio2]: Fallback, device index is a single number index instead: %d.\n", idx_found);
                  idx_found = strtoul(device, NULL, 0);
               }
            }
         }
      }
   }

   if (idx_found == -1)
      idx_found = 0;

#if (_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
   {
      wchar_t *temp = NULL;
      if (device)
         temp = utf8_to_utf16_string_alloc((const char*)list->elems[idx_found].userdata);

      if (FAILED(IXAudio2_CreateMasteringVoice(handle->pXAudio2, &handle->pMasterVoice, channels, samplerate, 0, (LPCWSTR)(uintptr_t)temp, NULL, AudioCategory_GameEffects)))
      {
         free(temp);
         goto error;
      }
      if (temp)
         free(temp);
   }
#else
   if (FAILED(IXAudio2_CreateMasteringVoice(handle->pXAudio2, &handle->pMasterVoice, channels, samplerate, 0, idx_found, NULL)))
      goto error;
#endif

   xaudio2_set_wavefmt(&wfx, channels, samplerate);

   if (FAILED(IXAudio2_CreateSourceVoice(handle->pXAudio2,
               &handle->pSourceVoice, &wfx,
               XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO,
               (IXAudio2VoiceCallback*)handle, 0, 0)))
      goto error;

   handle->hEvent  = CreateEvent(0, FALSE, FALSE, 0);
   if (!handle->hEvent)
      goto error;

   handle->bufsize = size / MAX_BUFFERS;
   handle->buf     = (uint8_t*)calloc(1, handle->bufsize * MAX_BUFFERS);
   if (!handle->buf)
      goto error;

   if (FAILED(IXAudio2SourceVoice_Start(handle->pSourceVoice, 0,
               XAUDIO2_COMMIT_NOW)))
      goto error;

   if (list)
      string_list_free(list);
   return handle;

error:
   if (list)
      string_list_free(list);
   xaudio2_free(handle);
   return NULL;
}

static void *xa_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   size_t bufsize;
   xa_t *xa              = (xa_t*)calloc(1, sizeof(*xa));
   if (!xa)
      return NULL;

   if (latency < 8)
      latency = 8; /* Do not allow shenanigans. */

   bufsize = latency * rate / 1000;

   RARCH_LOG("[XAudio2]: Requesting %u ms latency, using %d ms latency.\n",
         latency, (int)bufsize * 1000 / rate);

   xa->bufsize = bufsize * 2 * sizeof(float);

   xa->xa = xaudio2_new(rate, 2, xa->bufsize, device);
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
   unsigned bytes;
   xa_t *xa              = (xa_t*)data;
   xaudio2_t *handle     = xa->xa;
   const uint8_t *buffer = (const uint8_t*)buf;

   if (xa->nonblock)
   {
      size_t avail = XAUDIO2_WRITE_AVAILABLE(xa->xa);

      if (avail == 0)
         return 0;
      if (avail < size)
         size = avail;
   }

   bytes = size;

   while (bytes)
   {
      unsigned need   = MIN(bytes, handle->bufsize - handle->bufptr);

      memcpy(handle->buf + handle->write_buffer *
            handle->bufsize + handle->bufptr,
            buffer, need);

      handle->bufptr += need;
      buffer         += need;
      bytes          -= need;

      if (handle->bufptr == handle->bufsize)
      {
         XAUDIO2_BUFFER xa2buffer;

         while (handle->buffers == MAX_BUFFERS - 1)
            WaitForSingleObject(handle->hEvent, INFINITE);

         xa2buffer.Flags      = 0;
         xa2buffer.AudioBytes = handle->bufsize;
         xa2buffer.pAudioData = handle->buf + handle->write_buffer * handle->bufsize;
         xa2buffer.PlayBegin  = 0;
         xa2buffer.PlayLength = 0;
         xa2buffer.LoopBegin  = 0;
         xa2buffer.LoopLength = 0;
         xa2buffer.LoopCount  = 0;
         xa2buffer.pContext   = NULL;

         if (FAILED(IXAudio2SourceVoice_SubmitSourceBuffer(
                     handle->pSourceVoice, &xa2buffer, NULL)))
         {
            if (size > 0)
               return -1;
            return 0;
         }

         InterlockedIncrement((LONG volatile*)&handle->buffers);
         handle->bufptr       = 0;
         handle->write_buffer = (handle->write_buffer + 1) & MAX_BUFFERS_MASK;
      }
   }

   return size;
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

static bool xa_start(void *data, bool is_shutdown)
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
   return XAUDIO2_WRITE_AVAILABLE(xa->xa);
}

static size_t xa_buffer_size(void *data)
{
   xa_t *xa = (xa_t*)data;
   return xa->bufsize;
}

static void xa_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

static void *xa_list_new(void *u)
{
#if defined(_XBOX) || !(_WIN32_WINNT >= 0x0602 /*_WIN32_WINNT_WIN8*/)
   unsigned i;
   union string_list_elem_attr attr;
   uint32_t dev_count              = 0;
   IXAudio2 *ixa2                  = NULL;
   struct string_list *sl          = string_list_new();

   if (!sl)
      return NULL;

   attr.i = 0;

   if (FAILED(XAudio2Create(&ixa2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
      return NULL;

   IXAudio2_GetDeviceCount(ixa2, &dev_count);

   for (i = 0; i < dev_count; i++)
   {
      XAUDIO2_DEVICE_DETAILS dev_detail;
      if (IXAudio2_GetDeviceDetails(ixa2, i, &dev_detail) == S_OK)
      {
         char *str = utf16_to_utf8_string_alloc(dev_detail.DisplayName);

         if (str)
         {
            string_list_append(sl, str, attr);
            free(str);
         }
      }
   }

   IXAudio2_Release(ixa2);

   return sl;
#elif defined(__WINRT__)
   return NULL;
#else
   return mmdevice_list_new(u);
#endif
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
   xa_list_new,
   xa_device_list_free,
   xa_write_avail,
   xa_buffer_size,
};
