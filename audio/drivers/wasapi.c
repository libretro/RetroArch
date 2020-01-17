/*  RetroArch - A frontend for libretro.
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

#include <stdlib.h>

#include <lists/string_list.h>
#include <queues/fifo_queue.h>
#include <string/stdstring.h>

#include "../common/mmdevice_common.h"
#include "../common/mmdevice_common_inline.h"

#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../configuration.h"

typedef struct
{
   bool exclusive;
   bool nonblock;
   bool running;
   size_t frame_size;     /* 4 or 8 only */
   size_t engine_buffer_size;
   HANDLE write_event;
   IMMDevice          *device;
   IAudioClient       *client;
   IAudioRenderClient *renderer;
   fifo_buffer_t      *buffer; /* NULL in unbuffered shared mode */
} wasapi_t;

static IMMDevice *wasapi_init_device(const char *id)
{
   HRESULT hr;
   UINT32 dev_count, i;
   IMMDeviceEnumerator *enumerator = NULL;
   IMMDevice *device               = NULL;
   IMMDeviceCollection *collection = NULL;

   if (id)
   {
      RARCH_LOG("[WASAPI]: Initializing device %s ...\n", id);
   }
   else
   {
      RARCH_LOG("[WASAPI]: Initializing default device.. \n");
   }

#ifdef __cplusplus
   hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         IID_IMMDeviceEnumerator, (void **)&enumerator);
#else
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
#endif
   if (FAILED(hr))
      goto error;

   if (id)
   {
      int32_t idx_found        = -1;
      struct string_list *list = (struct string_list*)mmdevice_list_new(NULL);

      /* Search for device name first */
      if (list)
      {
         if (list->elems)
         {
            unsigned i;
            for (i = 0; i < list->size; i++)
            {
               RARCH_LOG("[WASAPI]: %d : %s\n", i, list->elems[i].data);
               if (string_is_equal(id, list->elems[i].data))
               {
                  idx_found = i;
                  break;
               }
            }
            /* Index was not found yet based on name string,
             * just assume id is a one-character number index. */

            if (idx_found == -1 && isdigit(id[0]))
            {
               idx_found = strtoul(id, NULL, 0);
               RARCH_LOG("[WASAPI]: Fallback, device index is a single number index instead: %d.\n", idx_found);

            }
         }
         string_list_free(list);
      }

      if (idx_found == -1)
         idx_found = 0;

      hr = _IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,
            eRender, DEVICE_STATE_ACTIVE, &collection);
      if (FAILED(hr))
         goto error;

      hr = _IMMDeviceCollection_GetCount(collection, &dev_count);
      if (FAILED(hr))
         goto error;

      for (i = 0; i < dev_count; ++i)
      {
         hr = _IMMDeviceCollection_Item(collection, i, &device);
         if (FAILED(hr))
            goto error;

         if (i == idx_found)
            break;

         IFACE_RELEASE(device);
      }
   }
   else
   {
      hr = _IMMDeviceEnumerator_GetDefaultAudioEndpoint(
            enumerator, eRender, eConsole, &device);
      if (FAILED(hr))
         goto error;
   }

   if (!device)
      goto error;

   IFACE_RELEASE(collection);
   IFACE_RELEASE(enumerator);

   return device;

error:
   IFACE_RELEASE(collection);
   IFACE_RELEASE(enumerator);

   if (id)
   {
      RARCH_WARN("[WASAPI]: Failed to initialize device.\n");
   }
   else
   {
      RARCH_ERR("[WASAPI]: Failed to initialize device.\n");
   }

   return NULL;
}

static unsigned wasapi_pref_rate(unsigned i)
{
   const unsigned r[] = { 48000, 44100, 96000, 192000, 32000 };

   if (i >= sizeof(r) / sizeof(unsigned))
      return 0;

   return r[i];
}

static void wasapi_set_format(WAVEFORMATEXTENSIBLE *wf,
      bool float_fmt, unsigned rate)
{
   wf->Format.nChannels               = 2;
   wf->Format.nSamplesPerSec          = rate;

   if (float_fmt)
   {
      wf->Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
      wf->Format.nAvgBytesPerSec      = rate * 8;
      wf->Format.nBlockAlign          = 8;
      wf->Format.wBitsPerSample       = 32;
      wf->Format.cbSize               = sizeof(WORD) + sizeof(DWORD) + sizeof(GUID);
      wf->Samples.wValidBitsPerSample = 32;
      wf->dwChannelMask               = KSAUDIO_SPEAKER_STEREO;
      wf->SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
   }
   else
   {
      wf->Format.wFormatTag           = WAVE_FORMAT_PCM;
      wf->Format.nAvgBytesPerSec      = rate * 4;
      wf->Format.nBlockAlign          = 4;
      wf->Format.wBitsPerSample       = 16;
      wf->Format.cbSize               = 0;
   }
}

static IAudioClient *wasapi_init_client_sh(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency)
{
   WAVEFORMATEXTENSIBLE wf;
   int i, j;
   IAudioClient *client = NULL;
   bool float_fmt_res   = *float_fmt;
   unsigned rate_res    = *rate;
   HRESULT hr           = _IMMDevice_Activate(device,
         IID_IAudioClient,
         CLSCTX_ALL, NULL, (void**)&client);
   if (FAILED(hr))
      return NULL;

   /* once for float, once for pcm (requested first) */
   for (i = 0; i < 2; ++i)
   {
      rate_res = *rate;
      if (i == 1)
         float_fmt_res = !float_fmt_res;

      /* for requested rate (first) and all preferred rates */
      for (j = 0; rate_res; ++j)
      {
         RARCH_LOG("[WASAPI]: Initializing client (shared, %s, %uHz, %ums) ...\n",
               float_fmt_res ? "float" : "pcm", rate_res, latency);

         wasapi_set_format(&wf, float_fmt_res, rate_res);
#ifdef __cplusplus
         hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
               AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
               0, 0, (WAVEFORMATEX*)&wf, NULL);
#else
         hr = client->lpVtbl->Initialize(client, AUDCLNT_SHAREMODE_SHARED,
               AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
               0, 0, (WAVEFORMATEX*)&wf, NULL);
#endif

         if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
         {
            HRESULT hr;
            IFACE_RELEASE(client);
            hr           = _IMMDevice_Activate(device,
                  IID_IAudioClient,
                  CLSCTX_ALL, NULL, (void**)&client);
            if (FAILED(hr))
               return NULL;

#ifdef __cplusplus
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  0, 0, (WAVEFORMATEX*)&wf, NULL);
#else
            hr = client->lpVtbl->Initialize(client, AUDCLNT_SHAREMODE_SHARED,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  0, 0, (WAVEFORMATEX*)&wf, NULL);
#endif
         }
         if (hr != AUDCLNT_E_UNSUPPORTED_FORMAT)
         {
            i = 2; /* break from outer loop too */
            break;
         }

         RARCH_WARN("[WASAPI]: Unsupported format.\n");
         rate_res = wasapi_pref_rate(j);
         if (rate_res == *rate) /* requested rate is allready tested */
            rate_res = wasapi_pref_rate(++j); /* skip it */
      }
   }

   if (FAILED(hr))
      goto error;

   *float_fmt = float_fmt_res;
   *rate      = rate_res;

   return client;

error:
   IFACE_RELEASE(client);

   return NULL;
}

static IAudioClient *wasapi_init_client_ex(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency)
{
   WAVEFORMATEXTENSIBLE wf;
   int i, j;
   IAudioClient *client           = NULL;
   bool float_fmt_res             = *float_fmt;
   unsigned rate_res              = *rate;
   REFERENCE_TIME minimum_period  = 0;
   REFERENCE_TIME buffer_duration = 0;
   UINT32 buffer_length           = 0;
   HRESULT hr                     = _IMMDevice_Activate(device,
         IID_IAudioClient,
         CLSCTX_ALL, NULL, (void**)&client);
   if (FAILED(hr))
      return NULL;

   hr = _IAudioClient_GetDevicePeriod(client, NULL, &minimum_period);
   if (FAILED(hr))
      goto error;

   /* buffer_duration is in 100ns units */
   buffer_duration = latency * 10000.0;
   if (buffer_duration < minimum_period)
      buffer_duration = minimum_period;

   /* once for float, once for pcm (requested first) */
   for (i = 0; i < 2; ++i)
   {
      rate_res = *rate;
      if (i == 1)
         float_fmt_res = !float_fmt_res;

      /* for requested rate (first) and all preferred rates */
      for (j = 0; rate_res; ++j)
      {
         RARCH_LOG("[WASAPI]: Initializing client (exclusive, %s, %uHz, %ums) ...\n",
               float_fmt_res ? "float" : "pcm", rate_res, latency);

         wasapi_set_format(&wf, float_fmt_res, rate_res);
#ifdef __cplusplus
         hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
               AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
               buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
#else
         hr = client->lpVtbl->Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
               AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
               buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
#endif
         if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
         {
            hr = _IAudioClient_GetBufferSize(client, &buffer_length);
            if (FAILED(hr))
               goto error;

            IFACE_RELEASE(client);
            hr                     = _IMMDevice_Activate(device,
                  IID_IAudioClient,
                  CLSCTX_ALL, NULL, (void**)&client);
            if (FAILED(hr))
               return NULL;

            buffer_duration = 10000.0 * 1000.0 / rate_res * buffer_length + 0.5;
#ifdef __cplusplus
            hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
#else
            hr = client->lpVtbl->Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
#endif
         }
         if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
         {
            IFACE_RELEASE(client);
            hr                     = _IMMDevice_Activate(device,
                  IID_IAudioClient,
                  CLSCTX_ALL, NULL, (void**)&client);
            if (FAILED(hr))
               return NULL;

#ifdef __cplusplus
            hr = client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
#else
            hr = client->lpVtbl->Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
#endif
         }
         if (hr != AUDCLNT_E_UNSUPPORTED_FORMAT)
         {
            if (hr == AUDCLNT_E_DEVICE_IN_USE)
               goto error;

            if (hr == AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED)
               goto error;

            i = 2; /* break from outer loop too */
            break;
         }

         RARCH_WARN("[WASAPI]: Unsupported format.\n");
         rate_res = wasapi_pref_rate(j);
         if (rate_res == *rate) /* requested rate is allready tested */
            rate_res = wasapi_pref_rate(++j); /* skip it */
      }
   }

   if (FAILED(hr))
      goto error;

   *float_fmt = float_fmt_res;
   *rate      = rate_res;

   return client;

error:
   IFACE_RELEASE(client);

   return NULL;
}

static IAudioClient *wasapi_init_client(IMMDevice *device, bool *exclusive,
      bool *float_fmt, unsigned *rate, unsigned latency)
{
   HRESULT hr;
   IAudioClient *client;
   double latency_res;
   REFERENCE_TIME device_period  = 0;
   REFERENCE_TIME stream_latency = 0;
   UINT32 buffer_length          = 0;

   if (*exclusive)
   {
      client = wasapi_init_client_ex(device, float_fmt, rate, latency);
      if (!client)
      {
         client = wasapi_init_client_sh(device, float_fmt, rate, latency);
         if (client)
            *exclusive = false;
      }
   }
   else
   {
      client = wasapi_init_client_sh(device, float_fmt, rate, latency);
      if (!client)
      {
         client = wasapi_init_client_ex(device, float_fmt, rate, latency);
         if (client)
            *exclusive = true;
      }
   }

   if (!client)
      return NULL;

   /* next calls are allowed to fail (we losing info only) */

   if (*exclusive)
      hr = _IAudioClient_GetDevicePeriod(client, NULL, &device_period);
   else
      hr = _IAudioClient_GetDevicePeriod(client, &device_period, NULL);

   if (FAILED(hr))
   {
      RARCH_WARN("[WASAPI]: IAudioClient::GetDevicePeriod failed with error 0x%.8X.\n", hr);
   }

   if (!*exclusive)
   {
      hr = _IAudioClient_GetStreamLatency(client, &stream_latency);
      if (FAILED(hr))
      {
         RARCH_WARN("[WASAPI]: IAudioClient::GetStreamLatency failed with error 0x%.8X.\n", hr);
      }
   }

   hr = _IAudioClient_GetBufferSize(client, &buffer_length);
   if (FAILED(hr))
   {
      RARCH_WARN("[WASAPI]: IAudioClient::GetBufferSize failed with error 0x%.8X.\n", hr);
   }

   if (*exclusive)
      latency_res = (double)buffer_length * 1000.0 / (*rate);
   else
      latency_res = (double)(stream_latency + device_period) / 10000.0;

   RARCH_LOG("[WASAPI]: Client initialized (%s, %s, %uHz, %.1fms).\n",
         *exclusive ? "exclusive" : "shared",
         *float_fmt ? "float" : "pcm", *rate, latency_res);

   RARCH_LOG("[WASAPI]: Client's buffer length is %u frames (%.1fms).\n",
         buffer_length, (double)buffer_length * 1000.0 / (*rate));

   RARCH_LOG("[WASAPI]: Device period is %.1fms (%lld frames).\n",
         (double)device_period / 10000.0, device_period * (*rate) / 10000000);

   return client;
}

static void *wasapi_init(const char *dev_id, unsigned rate, unsigned latency,
      unsigned u1, unsigned *u2)
{
   HRESULT hr;
   UINT32 frame_count        = 0;
   REFERENCE_TIME dev_period = 0;
   BYTE *dest                = NULL;
   settings_t *settings      = config_get_ptr();
   bool float_format         = settings->bools.audio_wasapi_float_format;
   int sh_buffer_length      = settings->ints.audio_wasapi_sh_buffer_length;
   wasapi_t *w               = (wasapi_t*)calloc(1, sizeof(wasapi_t));

   if (!w)
      return NULL;

   w->exclusive              = settings->bools.audio_wasapi_exclusive_mode;
   w->device                 = wasapi_init_device(dev_id);
   if (!w->device && dev_id)
      w->device = wasapi_init_device(NULL);
   if (!w->device)
      goto error;

   w->client = wasapi_init_client(w->device,
         &w->exclusive, &float_format, &rate, latency);
   if (!w->client)
      goto error;

   hr = _IAudioClient_GetBufferSize(w->client, &frame_count);
   if (FAILED(hr))
      goto error;

   w->frame_size         = float_format ? 8 : 4;
   w->engine_buffer_size = frame_count * w->frame_size;

   if (w->exclusive)
   {
      w->buffer = fifo_new(w->engine_buffer_size);
      if (!w->buffer)
         goto error;

      RARCH_LOG("[WASAPI]: Intermediate buffer length is %u frames (%.1fms).\n",
            frame_count, (double)frame_count * 1000.0 / rate);
   }
   else if (sh_buffer_length)
   {
      if (sh_buffer_length < 0)
      {
         hr = _IAudioClient_GetDevicePeriod(w->client, &dev_period, NULL);
         if (FAILED(hr))
            goto error;

         sh_buffer_length = dev_period * rate / 10000000;
      }

      w->buffer = fifo_new(sh_buffer_length * w->frame_size);
      if (!w->buffer)
         goto error;

      RARCH_LOG("[WASAPI]: Intermediate buffer length is %u frames (%.1fms).\n",
            sh_buffer_length, (double)sh_buffer_length * 1000.0 / rate);
   }
   else
   {
      RARCH_LOG("[WASAPI]: Intermediate buffer is off. \n");
   }

   w->write_event = CreateEventA(NULL, FALSE, FALSE, NULL);
   if (!w->write_event)
      goto error;

   hr = _IAudioClient_SetEventHandle(w->client, w->write_event);
   if (FAILED(hr))
      goto error;

   hr = _IAudioClient_GetService(w->client,
         IID_IAudioRenderClient, (void**)&w->renderer);
   if (FAILED(hr))
      goto error;

   hr = _IAudioRenderClient_GetBuffer(w->renderer, frame_count, &dest);
   if (FAILED(hr))
      goto error;

   hr = _IAudioRenderClient_ReleaseBuffer(
         w->renderer, frame_count,
         AUDCLNT_BUFFERFLAGS_SILENT);
   if (FAILED(hr))
      goto error;

   hr = _IAudioClient_Start(w->client);
   if (FAILED(hr))
      goto error;
   w->running  = true;
   w->nonblock = !settings->bools.audio_sync;

   return w;

error:
   IFACE_RELEASE(w->renderer);
   IFACE_RELEASE(w->client);
   IFACE_RELEASE(w->device);
   if (w->write_event)
      CloseHandle(w->write_event);
   if (w->buffer)
      fifo_free(w->buffer);
   free(w);

   return NULL;
}

static bool wasapi_flush(wasapi_t * w, const void * data, size_t size)
{
   BYTE *dest         = NULL;
   UINT32 frame_count = size / w->frame_size;

   if (FAILED(_IAudioRenderClient_GetBuffer(
               w->renderer, frame_count, &dest)))
      return false;

   memcpy(dest, data, size);
   if (FAILED(_IAudioRenderClient_ReleaseBuffer(
               w->renderer, frame_count,
               0)))
      return false;

   return true;
}

static bool wasapi_flush_buffer(wasapi_t * w, size_t size)
{
   BYTE *dest         = NULL;
   UINT32 frame_count = size / w->frame_size;
   if (FAILED(_IAudioRenderClient_GetBuffer(
         w->renderer, frame_count, &dest)))
      return false;

   fifo_read(w->buffer, dest, size);
   if (FAILED(_IAudioRenderClient_ReleaseBuffer(
         w->renderer, frame_count,
         0)))
      return false;

   return true;
}

static ssize_t wasapi_write_sh_buffer(wasapi_t *w, const void * data, size_t size)
{
   ssize_t written    = -1;
   UINT32 padding     = 0;
   size_t write_avail = fifo_write_avail(w->buffer);

   if (!write_avail)
   {
      size_t read_avail  = 0;
      if (!(WaitForSingleObject(w->write_event, INFINITE) == WAIT_OBJECT_0))
         return -1;

      if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
         return -1;

      read_avail  = fifo_read_avail(w->buffer);
      write_avail = w->engine_buffer_size - padding * w->frame_size;
      written     = read_avail < write_avail ? read_avail : write_avail;
      if (written)
         if (!wasapi_flush_buffer(w, written))
            return -1;
   }

   write_avail = fifo_write_avail(w->buffer);
   written     = size < write_avail ? size : write_avail;
   if (written)
      fifo_write(w->buffer, data, written);

   return written;
}

static ssize_t wasapi_write_sh(wasapi_t *w, const void * data, size_t size)
{
   size_t write_avail = 0;
   ssize_t written    = -1;
   UINT32 padding     = 0;

   if (!(WaitForSingleObject(w->write_event, INFINITE) == WAIT_OBJECT_0))
      return -1;

   if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
      return -1;

   write_avail = w->engine_buffer_size - padding * w->frame_size;
   if (!write_avail)
      return 0;

   written = size < write_avail ? size : write_avail;
   if (written)
      if (!wasapi_flush(w, data, written))
         return -1;

   return written;
}

static ssize_t wasapi_write_sh_nonblock(wasapi_t *w, const void * data, size_t size)
{
   size_t write_avail = 0;
   ssize_t written    = -1;
   UINT32 padding     = 0;

   if (w->buffer)
   {
      write_avail = fifo_write_avail(w->buffer);
      if (!write_avail)
      {
         size_t read_avail  = 0;
         if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
            return -1;

         read_avail  = fifo_read_avail(w->buffer);
         write_avail = w->engine_buffer_size - padding * w->frame_size;
         written     = read_avail < write_avail ? read_avail : write_avail;
         if (written)
            if (!wasapi_flush_buffer(w, written))
               return -1;
      }

      write_avail = fifo_write_avail(w->buffer);
      written     = size < write_avail ? size : write_avail;
      if (written)
         fifo_write(w->buffer, data, written);
   }
   else
   {
      if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
         return -1;

      if (!(write_avail = w->engine_buffer_size - padding * w->frame_size))
         return 0;

      written = size < write_avail ? size : write_avail;
      if (written)
         if (!wasapi_flush(w, data, written))
            return -1;
   }

   return written;
}

static ssize_t wasapi_write_ex(wasapi_t *w, const void * data, size_t size, DWORD ms)
{
   ssize_t written    = 0;
   size_t write_avail = fifo_write_avail(w->buffer);

   if (!write_avail)
   {
      if (WaitForSingleObject(w->write_event, ms) != WAIT_OBJECT_0)
         return -1;

      if (!wasapi_flush_buffer(w, w->engine_buffer_size))
         return -1;

      write_avail = w->engine_buffer_size;
   }

   written = size < write_avail ? size : write_avail;
   fifo_write(w->buffer, data, written);

   return written;
}

static ssize_t wasapi_write(void *wh, const void *data, size_t size)
{
   size_t written = 0;
   wasapi_t *w    = (wasapi_t*)wh;

   if (w->nonblock)
   {
      if (w->exclusive)
         return wasapi_write_ex(w, data, size, 0);
      return wasapi_write_sh_nonblock(w, data, size);
   }

   if (w->exclusive)
   {
      ssize_t ir;
      for (ir = -1; written < size; written += ir)
      {
         ir = wasapi_write_ex(w, (char*)data + written, size - written, INFINITE);
         if (ir == -1)
            return -1;
      }
   }
   else
   {
      ssize_t ir;
      if (w->buffer)
      {
         for (ir = -1; written < size; written += ir)
         {
            ir = wasapi_write_sh_buffer(w, (char*)data + written, size - written);
            if (ir == -1)
               return -1;
         }
      }
      else
      {
         for (ir = -1; written < size; written += ir)
         {
            ir = wasapi_write_sh(w, (char*)data + written, size - written);
            if (ir == -1)
               return -1;
         }
      }
   }

   return written;
}

static bool wasapi_stop(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   if (FAILED(_IAudioClient_Stop(w->client)))
      return !w->running;

   w->running = false;

   return true;
}

static bool wasapi_start(void *wh, bool u)
{
   wasapi_t *w = (wasapi_t*)wh;
   HRESULT  hr = _IAudioClient_Start(w->client);

   if (hr == AUDCLNT_E_NOT_STOPPED)
      return true;

   if (FAILED(hr))
      return w->running;

   w->running = true;

   return true;
}

static bool wasapi_alive(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   return w->running;
}

static void wasapi_set_nonblock_state(void *wh, bool nonblock)
{
   wasapi_t *w = (wasapi_t*)wh;

   RARCH_LOG("[WASAPI]: Sync %s.\n", nonblock ? "off" : "on");

   w->nonblock = nonblock;
}

static void wasapi_free(void *wh)
{
   DWORD ir;
   wasapi_t *w        = (wasapi_t*)wh;
   HANDLE write_event = w->write_event;

   IFACE_RELEASE(w->renderer);
   if (w->client)
      _IAudioClient_Stop(w->client);
   IFACE_RELEASE(w->client);
   IFACE_RELEASE(w->device);
   if (w->buffer)
      fifo_free(w->buffer);
   free(w);

   ir = WaitForSingleObject(write_event, 20);
   if (ir == WAIT_FAILED)
   {
      RARCH_ERR("[WASAPI]: WaitForSingleObject failed with error %d.\n", GetLastError());
   }

   /* If event isn't signaled log and leak */
   if (!(ir == WAIT_OBJECT_0))
      return;

   CloseHandle(write_event);
}

static bool wasapi_use_float(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   return w->frame_size == 8;
}

static void wasapi_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

static size_t wasapi_write_avail(void *wh)
{
   wasapi_t *w    = (wasapi_t*)wh;
   UINT32 padding = 0;

   if (w->buffer)
      return fifo_write_avail(w->buffer);

   if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
      return 0;

   return w->engine_buffer_size - padding * w->frame_size;
}

static size_t wasapi_buffer_size(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (!w->exclusive && w->buffer)
      return w->buffer->size;

   return w->engine_buffer_size;
}

audio_driver_t audio_wasapi = {
   wasapi_init,
   wasapi_write,
   wasapi_stop,
   wasapi_start,
   wasapi_alive,
   wasapi_set_nonblock_state,
   wasapi_free,
   wasapi_use_float,
   "wasapi",
   mmdevice_list_new,
   wasapi_device_list_free,
   wasapi_write_avail,
   wasapi_buffer_size
};
