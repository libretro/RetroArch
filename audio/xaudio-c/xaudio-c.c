/*
	Simple C interface for XAudio2
	Author: Hans-Kristian Arntzen
	License: Public Domain
*/

#include "xaudio-c.h"
#include "xaudio.h"
#include <stdint.h>

#define MAX_BUFFERS 16
#define MAX_BUFFERS_MASK (MAX_BUFFERS - 1)

struct xaudio2
{
   const IXAudio2VoiceCallbackVtbl *lpVtbl;

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

static void WINAPI voice_on_buffer_end(void *handle_, void *data)
{
   (void)data;
   xaudio2_t *handle = (xaudio2_t*)handle_;
   InterlockedDecrement(&handle->buffers);
   SetEvent(handle->hEvent);
}

static void WINAPI dummy_voidp(void *handle, void *data) { (void)handle; (void)data; }
static void WINAPI dummy_nil(void *handle) { (void)handle; }
static void WINAPI dummy_uint32(void *handle, UINT32 dummy) { (void)handle; (void)dummy; }
static void WINAPI dummy_voidp_hresult(void *handle, void *data, HRESULT dummy) { (void)handle; (void)data; (void)dummy; }

const struct IXAudio2VoiceCallbackVtbl voice_vtable = {
   dummy_uint32,
   dummy_nil,
   dummy_nil,
   dummy_voidp,
   voice_on_buffer_end,
   dummy_voidp,
   dummy_voidp_hresult,
};
   
xaudio2_t *xaudio2_new(unsigned samplerate, unsigned channels, size_t size)
{
   xaudio2_t *handle = (xaudio2_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->lpVtbl = &voice_vtable;
   CoInitializeEx(0, COINIT_MULTITHREADED);
   WAVEFORMATEX wfx = {0};

   if (FAILED(XAudio2Create(&handle->pXAudio2)))
      goto error;

   if (FAILED(IXAudio2_CreateMasteringVoice(handle->pXAudio2,
               &handle->pMasterVoice, channels, samplerate, 0, 0, 0)))
      goto error;

   wfx.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
   wfx.nChannels = channels;
   wfx.nSamplesPerSec = samplerate;
   wfx.nBlockAlign = channels * sizeof(float);
   wfx.wBitsPerSample = sizeof(float) * 8;
   wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
   wfx.cbSize = 0;

   if (FAILED(IXAudio2_CreateSourceVoice(handle->pXAudio2,
               &handle->pSourceVoice, &wfx,
               XAUDIO2_VOICE_NOSRC, XAUDIO2_DEFAULT_FREQ_RATIO,
               (IXAudio2VoiceCallback*)handle, 0, 0)))
      goto error;

   handle->hEvent = CreateEvent(0, FALSE, FALSE, 0);
   if (!handle->hEvent)
      goto error;

   IXAudio2SourceVoice_Start(handle->pSourceVoice, 0, XAUDIO2_COMMIT_NOW);

   handle->bufsize = size / MAX_BUFFERS;
   handle->buf = (uint8_t*)calloc(1, handle->bufsize * MAX_BUFFERS);
   if (!handle->buf)
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

         if (FAILED(IXAudio2SourceVoice_SubmitSourceBuffer(handle->pSourceVoice, &xa2buffer, NULL)))
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
   if (handle)
   {
      if (handle->pSourceVoice)
      {
         IXAudio2SourceVoice_Stop(handle->pSourceVoice, 0, XAUDIO2_COMMIT_NOW);
         IXAudio2SourceVoice_DestroyVoice(handle->pSourceVoice);
      }

      if (handle->pMasterVoice)
         IXAudio2MasteringVoice_DestroyVoice(handle->pMasterVoice);

      if (handle->pXAudio2)
         IXAudio2_Release(handle->pXAudio2);

      if (handle->hEvent)
         CloseHandle(handle->hEvent);

      free(handle->buf);
      free(handle);
   }
}

