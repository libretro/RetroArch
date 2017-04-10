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

#ifdef _WIN32_WINNT 
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <Winerror.h>
#include <initguid.h>
#include <Mmdeviceapi.h>
#include <Mmreg.h>
#include <Audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <Propidl.h>

#include "../audio_driver.h"
#include "../../verbosity.h"
#include "../libretro-common/include/lists/string_list.h"

#define WASAPI_CHECK(bool_exp, err_str, err_exp)\
      if (!(bool_exp)) {\
         wasapi_err(err_str);\
         err_exp; }

#define WASAPI_HR_CHECK(hr, fun_str, err_exp)\
      if (FAILED(hr)) {\
         wasapi_com_err(fun_str, hr);\
         err_exp; }

#define WASAPI_SR_CHECK(bool_exp, fun_str, err_exp)\
      if (!(bool_exp)) {\
         wasapi_sys_err(fun_str);\
         err_exp; }

#define WASAPI_RELEASE(iface)\
      if(iface) {\
         iface->lpVtbl->Release(iface);\
         iface = NULL;\
      }

#define WASAPI_FREE(ptr)\
      if(ptr) {\
         free(ptr);\
         ptr = NULL;\
      }

typedef struct
{
   IMMDevice *device;            /* always valid */
   IAudioClient *client;         /* may be NULL */
   IAudioRenderClient *renderer; /* may be NULL */
   HANDLE write_event;
   void *buffer;                 /* NULL in shared mode */
   size_t buffer_size;           /* in shared mode holds WASAPI engine buffer size */
   size_t buffer_usage;          /* valid in exclusive mode only */
   size_t frame_size;            /* 4 or 8 only */
   unsigned latency;             /* in ms (requested, not real) */
   unsigned frame_rate;
   bool running;
} wasapi_t;

static void wasapi_err(const char *err)
{
   RARCH_ERR("[WASAPI]: %s.\n", err);
}

static void wasapi_com_err(const char *fun, HRESULT hr)
{
   RARCH_ERR("[WASAPI]: %s failed with error 0x%.8X.\n", fun, hr);
}

static void wasapi_sys_err(const char *fun)
{
   RARCH_ERR("[WASAPI]: %s failed with error %d.\n", fun, GetLastError());
}

static bool wasapi_check_device_id(IMMDevice *device, const char *id)
{
   HRESULT hr;
   bool result   = false;
   LPWSTR dev_id = NULL, dev_cmp_id = NULL;
   int id_length = MultiByteToWideChar(CP_ACP, 0, id, -1, NULL, 0);
   WASAPI_SR_CHECK(id_length > 0, "MultiByteToWideChar", goto error);

   dev_cmp_id = (LPWSTR)malloc(id_length * sizeof(WCHAR));
   WASAPI_CHECK(dev_cmp_id, "Out of memory", goto error);

   id_length = MultiByteToWideChar(CP_ACP, 0, id, -1, dev_cmp_id, id_length);
   WASAPI_SR_CHECK(id_length > 0, "MultiByteToWideChar", goto error);
   
   hr = device->lpVtbl->GetId(device, &dev_id);
   WASAPI_HR_CHECK(hr, "IMMDevice::GetId", goto error);

   result = lstrcmpW(dev_cmp_id, dev_id) == 0 ? true : false;

   CoTaskMemFree(dev_id);
   free(dev_cmp_id);

   return result;

error:
   if (dev_id)
      CoTaskMemFree(dev_id);
   if (dev_cmp_id)
      free(dev_cmp_id);

   return false;
}

static IMMDevice *wasapi_init_device(const char *id)
{
   HRESULT hr;
   UINT32 dev_count, i;
   IMMDeviceEnumerator * enumerator = NULL;
   IMMDevice *device                = NULL;
   IMMDeviceCollection * collection = NULL;

   if (id)
      RARCH_LOG("[WASAPI]: Initializing device %s ...\n", id);
   else
      RARCH_LOG("[WASAPI]: Initializing default device ...\n");

   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
   WASAPI_HR_CHECK(hr, "CoCreateInstance", goto error);

   if (id)
   {
      hr = enumerator->lpVtbl->EnumAudioEndpoints(enumerator,
         eRender, DEVICE_STATE_ACTIVE, &collection);
      WASAPI_HR_CHECK(hr, "IMMDeviceEnumerator::EnumAudioEndpoints",
            goto error);

      hr = collection->lpVtbl->GetCount(collection, &dev_count);
      WASAPI_HR_CHECK(hr, "IMMDeviceCollection::GetCount", goto error);

      for (i = 0; i < dev_count; ++i)
      {
         hr = collection->lpVtbl->Item(collection, i, &device);
         WASAPI_HR_CHECK(hr, "IMMDeviceCollection::Item", continue);

         if (wasapi_check_device_id(device, id))
            break;

         device->lpVtbl->Release(device);
         device = NULL;
      }
   }
   else
   {
      hr = enumerator->lpVtbl->GetDefaultAudioEndpoint(enumerator,
            eRender, eConsole, &device);
      WASAPI_HR_CHECK(hr, "IMMDeviceEnumerator::GetDefaultAudioEndpoint",
            goto error);
   }

   if (!device)
      goto error;

   if (collection)
      collection->lpVtbl->Release(collection);
   enumerator->lpVtbl->Release(enumerator);

   RARCH_LOG("[WASAPI]: Device initialized.\n");

   return device;

error:
   if (collection)
      collection->lpVtbl->Release(collection);
   if (enumerator)
      enumerator->lpVtbl->Release(enumerator);

   RARCH_ERR("[WASAPI]: Failed to initialize device.\n");

   return NULL;
}

static IAudioClient *wasapi_init_client(IMMDevice *device, bool exclusive,
      bool format_float, unsigned rate, unsigned latency)
{
   WAVEFORMATEXTENSIBLE wf;
   REFERENCE_TIME default_period, minimum_period;
   IAudioClient *client                           = NULL;
   REFERENCE_TIME buffer_duration, stream_latency = 0;
   AUDCLNT_SHAREMODE share_mode                   = exclusive ?
         AUDCLNT_SHAREMODE_EXCLUSIVE : AUDCLNT_SHAREMODE_SHARED;
   DWORD stream_flags                             = 
      AUDCLNT_STREAMFLAGS_NOPERSIST |
      AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
   UINT32 buffer_length                           = 0;
   HRESULT hr                                     = 
      device->lpVtbl->Activate(device, &IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)&client);

   RARCH_LOG("[WASAPI]: Initializing client "
         "(%s, %s, %uHz, %ums) ...\n",
         exclusive ? "exclusive" : "shared",
         format_float ? "float" : "pcm",
         rate, latency);

   WASAPI_HR_CHECK(hr, "IMMDevice::Activate", goto error);

   hr = client->lpVtbl->GetDevicePeriod(client, &default_period, &minimum_period);
   WASAPI_HR_CHECK(hr, "IAudioClient::GetDevicePeriod", goto error);

   if (!exclusive)
      buffer_duration = 0; /* required for event driven shared mode */
   else
   {
      /* 3 buffers latency (ms) to 2 buffer duration (100ns units) */
      buffer_duration = (double)latency * 10000.0 * 2.0 / 3.0;
      if (buffer_duration < minimum_period)
         buffer_duration = minimum_period;
   }

   wf.Format.nChannels               = 2;
   wf.Format.nSamplesPerSec          = rate;

   if (format_float)
   {
      wf.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
      wf.Format.nAvgBytesPerSec      = rate * 8;
      wf.Format.nBlockAlign          = 8;
      wf.Format.wBitsPerSample       = 32;
      wf.Format.cbSize               = sizeof(WORD) + sizeof(DWORD) + sizeof(GUID);
      wf.Samples.wValidBitsPerSample = 32;
      wf.dwChannelMask               = KSAUDIO_SPEAKER_STEREO;
      wf.SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
   }
   else
   {
      wf.Format.wFormatTag           = WAVE_FORMAT_PCM;
      wf.Format.nAvgBytesPerSec      = rate * 4;
      wf.Format.nBlockAlign          = 4;
      wf.Format.wBitsPerSample       = 16;
      wf.Format.cbSize               = 0;
   }

   hr = client->lpVtbl->Initialize(client, share_mode, stream_flags,
         buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);

   if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
   {
      hr = client->lpVtbl->GetBufferSize(client, &buffer_length);
      WASAPI_HR_CHECK(hr, "IAudioClient::GetBufferSize", goto error);

      client->lpVtbl->Release(client);
      hr = device->lpVtbl->Activate(device, &IID_IAudioClient,
         CLSCTX_ALL, NULL, (void**)&client);
      WASAPI_HR_CHECK(hr, "IMMDevice::Activate", goto error);

      buffer_duration = 10000.0 * 1000.0 / rate * buffer_length + 0.5;
      hr = client->lpVtbl->Initialize(client, share_mode, stream_flags,
         buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
   }

   WASAPI_HR_CHECK(hr, "IAudioClient::Initialize", goto error);

   hr = client->lpVtbl->GetStreamLatency(client, &stream_latency);

   if(hr != S_OK)
      wasapi_com_err("IAudioClient::GetStreamLatency", hr);
   else if (exclusive)
      stream_latency *= 1.5;
   else
      stream_latency += default_period;

   RARCH_LOG("[WASAPI]: Client initialized (max latency %.1fms).\n",
         (double)stream_latency / 10000.0 + 0.05);

   return client;

error:
   if (client)
      client->lpVtbl->Release(client);

   RARCH_ERR("[WASAPI]: Failed to initialize client.\n");

   return NULL;
}

static void *wasapi_init(const char *dev_id, unsigned rate, unsigned latency,
      unsigned u1, unsigned *u2)
{
   HRESULT hr;
   bool exclusive       = false;
   bool com_initialized = false;
   UINT32 frame_count   = 0;
   BYTE *dest           = NULL;
   wasapi_t *w          = (wasapi_t*)calloc(1, sizeof(wasapi_t));

   WASAPI_CHECK(w, "Out of memory", return NULL);

   hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
   WASAPI_HR_CHECK(hr, "CoInitializeEx", goto error);

   com_initialized = true;

   w->device = wasapi_init_device(dev_id);
   if (!w->device)
      goto error;

   if ((w->client = wasapi_init_client(w->device,
         true, true, rate, latency)))
   {
      exclusive = true;
      w->frame_size = 8;
   }
   else if ((w->client = wasapi_init_client(w->device,
         true, false, rate, latency)))
   {
      exclusive = true;
      w->frame_size = 4;
   }
   else if ((w->client = wasapi_init_client(w->device,
         false, true, rate, latency)))
   {
      exclusive = false;
      w->frame_size = 8;
   }
   else if ((w->client = wasapi_init_client(w->device,
         false, false, rate, latency)))
   {
      exclusive = false;
      w->frame_size = 4;
   }
   else
      goto error;
   
   w->frame_rate = rate;
   w->latency = latency;

   hr = w->client->lpVtbl->GetBufferSize(w->client, &frame_count);
   WASAPI_HR_CHECK(hr, "IAudioClient::GetBufferSize", goto error);

   w->buffer_size = frame_count * w->frame_size;
   if (exclusive)
   {
      w->buffer = malloc(w->buffer_size);
      WASAPI_CHECK(w, "Out of memory", goto error);
   }

   hr = w->client->lpVtbl->GetService(w->client,
         &IID_IAudioRenderClient, (void**)&w->renderer);
   WASAPI_HR_CHECK(hr, "IAudioClient::GetService", goto error);

   hr = w->renderer->lpVtbl->GetBuffer(w->renderer, frame_count, &dest);

   if (hr != S_OK)
      wasapi_com_err("IAudioRenderClient::GetBuffer", hr);
   else
   {
      hr = w->renderer->lpVtbl->ReleaseBuffer(w->renderer, frame_count,
            AUDCLNT_BUFFERFLAGS_SILENT);
      WASAPI_HR_CHECK(hr, "IAudioRenderClient::ReleaseBuffer", goto error);
   }

   w->write_event = CreateEventA(NULL, FALSE, FALSE, NULL);
   WASAPI_SR_CHECK(w->write_event, "CreateEventA", goto error);

   hr = w->client->lpVtbl->SetEventHandle(w->client, w->write_event);
   WASAPI_HR_CHECK(hr, "IAudioClient::SetEventHandle", goto error);

   return w;

error:
   WASAPI_RELEASE(w->renderer);
   WASAPI_RELEASE(w->client);      
   WASAPI_RELEASE(w->device);
   if (com_initialized)
      CoUninitialize();
   if (w->write_event)
      CloseHandle(w->write_event);
   WASAPI_FREE(w->buffer);
   free(w);

   return NULL;
}

static bool wasapi_flush(wasapi_t * w, const void * data, size_t size)
{
   BYTE * dest        = NULL;
   UINT32 frame_count = size / w->frame_size;
   HRESULT hr         = w->renderer->lpVtbl->GetBuffer(w->renderer, frame_count, &dest);

   WASAPI_HR_CHECK(hr, "IAudioRenderClient::GetBuffer", return false);

   memcpy(dest, data, size);

   hr = w->renderer->lpVtbl->ReleaseBuffer(w->renderer, frame_count, 0);
   WASAPI_HR_CHECK(hr, "IAudioRenderClient::ReleaseBuffer", return false);

   return true;
}

static ssize_t wasapi_process(wasapi_t *w, const void * data, size_t size)
{
   DWORD ir;
   size_t buffer_avail;
   HRESULT hr;
   bool br        = false;
   ssize_t result = 0;
   UINT32 padding = 0;

   if (w->buffer) /* exclusive mode */
   {
      if (w->buffer_usage == w->buffer_size)
      {
         ir = WaitForSingleObject(w->write_event, INFINITE);
         WASAPI_SR_CHECK(ir == WAIT_OBJECT_0,
               "WaitForSingleObject", return -1);

         br = wasapi_flush(w, w->buffer, w->buffer_size);
         if (!br)
            return -1;

         w->buffer_usage = 0;
      }

      buffer_avail = w->buffer_size - w->buffer_usage;
      result = size < buffer_avail ? size : buffer_avail;
      memcpy(w->buffer + w->buffer_usage, data, result);
      w->buffer_usage += result;
   }
   else /* shared mode */
   {
      ir = WaitForSingleObject(w->write_event, INFINITE);
      WASAPI_SR_CHECK(ir == WAIT_OBJECT_0,
            "WaitForSingleObject", return -1);

      hr = w->client->lpVtbl->GetCurrentPadding(w->client, &padding);
      WASAPI_HR_CHECK(hr, "IAudioClient::GetCurrentPadding", return -1);

      buffer_avail = w->buffer_size - padding * w->frame_size;
      if (buffer_avail)
      {
         result = size < buffer_avail ? size : buffer_avail;
         br = wasapi_flush(w, data, result);
         if (!br)
               return -1;
      }
   }

   return result;
}

static ssize_t wasapi_write(void *wh, const void *data, size_t size, bool u)
{
   size_t writen;
   ssize_t r;
   wasapi_t *w = (wasapi_t*)wh;

   for (writen = 0, r = -1; writen < size && r; writen += r)
   {
      r = wasapi_process(w, data + writen, size - writen);
      if (r == -1)
         return -1;
   }

   return writen;
}

static bool wasapi_stop(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   HRESULT hr  = w->client->lpVtbl->Stop(w->client);

   WASAPI_HR_CHECK(hr, "IAudioClient::Stop", return !w->running);

   w->running = false;

   return true;
}

static bool wasapi_start(void *wh, bool u)
{
   wasapi_t *w = (wasapi_t*)wh;
   HRESULT hr  = w->client->lpVtbl->Start(w->client);

   WASAPI_HR_CHECK(hr, "IAudioClient::Start", return w->running);

   w->running = true;

   return true;
}

static bool wasapi_alive(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   return w->running;
}

static void wasapi_set_nonblock_state(void *u, bool nonblock)
{
   if (nonblock)
      RARCH_ERR("[WASAPI]: Nonblocking mode not supported!!!\n");
}

static void wasapi_free(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   w->renderer->lpVtbl->Release(w->renderer);
   w->client->lpVtbl->Stop(w->client);
   w->client->lpVtbl->Release(w->client);
   w->device->lpVtbl->Release(w->device);
   CoUninitialize();
   CloseHandle(w->write_event);
   free(w->buffer);
   free(w);
}

static bool wasapi_use_float(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   return w->frame_size == 8;
}

static void *wasapi_device_list_new(void *u)
{
   HRESULT hr;
   UINT i;
   PROPVARIANT prop_var;
   int ir;
   union string_list_elem_attr attr;
   IMMDeviceEnumerator *enumerator = NULL;
   IMMDeviceCollection *collection = NULL;
   UINT dev_count                  = 0;
   IMMDevice *device               = NULL;
   LPWSTR dev_id_wstr              = NULL;
   LPWSTR dev_name_wstr            = NULL;
   IPropertyStore *prop_store      = NULL;
   bool prop_var_init              = false;
   bool br                         = false;
   char *dev_id_str                = NULL;
   char *dev_name_str              = NULL;
   struct string_list *sl          = string_list_new();

   RARCH_LOG("[WASAPI]: Enumerating active devices ...\n");
   WASAPI_CHECK(sl, "string_list_new failed", return NULL);

   attr.i = 0;
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
   WASAPI_HR_CHECK(hr, "CoCreateInstance", goto error);

   hr = enumerator->lpVtbl->EnumAudioEndpoints(enumerator,
         eRender, DEVICE_STATE_ACTIVE, &collection);
   WASAPI_HR_CHECK(hr, "IMMDeviceEnumerator::EnumAudioEndpoints", goto error);

   hr = collection->lpVtbl->GetCount(collection, &dev_count);
   WASAPI_HR_CHECK(hr, "IMMDeviceCollection::GetCount", goto error);

   for (i = 0; i < dev_count; ++i)
   {
      hr = collection->lpVtbl->Item(collection, i, &device);
      WASAPI_HR_CHECK(hr, "IMMDeviceCollection::Item", goto error);

      hr = device->lpVtbl->GetId(device, &dev_id_wstr);
      WASAPI_HR_CHECK(hr, "IMMDevice::GetId", goto error);

      ir = WideCharToMultiByte(CP_ACP, 0, dev_id_wstr, -1,
            NULL, 0, NULL, NULL);
      WASAPI_SR_CHECK(ir, "WideCharToMultiByte", goto error);

      dev_id_str = (char *)malloc(ir);
      WASAPI_CHECK(dev_id_str, "Out of memory", goto error);

      ir = WideCharToMultiByte(CP_ACP, 0, dev_id_wstr, -1,
            dev_id_str, ir, NULL, NULL);
      WASAPI_SR_CHECK(ir, "WideCharToMultiByte", goto error);

      hr = device->lpVtbl->OpenPropertyStore(device, STGM_READ, &prop_store);
      WASAPI_HR_CHECK(hr, "IMMDevice::OpenPropertyStore", goto error);

      PropVariantInit(&prop_var);
      prop_var_init = true;
      hr = prop_store->lpVtbl->GetValue(prop_store,
            &PKEY_Device_FriendlyName, &prop_var);
      WASAPI_HR_CHECK(hr, "IPropertyStore::GetValue", goto error);

      ir = WideCharToMultiByte(CP_ACP, 0, prop_var.pwszVal, -1,
            NULL, 0, NULL, NULL);
      WASAPI_SR_CHECK(ir, "WideCharToMultiByte", goto error);

      dev_name_str = (char *)malloc(ir);
      WASAPI_CHECK(dev_name_str, "Out of memory", goto error);

      ir = WideCharToMultiByte(CP_ACP, 0, prop_var.pwszVal, -1,
            dev_name_str, ir, NULL, NULL);
      WASAPI_SR_CHECK(ir, "WideCharToMultiByte", goto error);

      RARCH_LOG("[WASAPI]: %s %s\n", dev_name_str, dev_id_str);

      br = string_list_append(sl, dev_id_str, attr);
      WASAPI_CHECK(br, "string_list_append failed", goto error);

      PropVariantClear(&prop_var);
      prop_var_init = false;
      CoTaskMemFree(dev_id_wstr);
      dev_id_wstr = NULL;
      CoTaskMemFree(dev_name_wstr);
      dev_name_wstr = NULL;
      WASAPI_FREE(dev_id_str);
      WASAPI_FREE(dev_name_str);
      WASAPI_RELEASE(prop_store);
      WASAPI_RELEASE(device);
   }

   collection->lpVtbl->Release(collection);
   enumerator->lpVtbl->Release(enumerator);

   RARCH_LOG("[WASAPI]: Devices enumerated.\n");

   return sl;

error:
   WASAPI_FREE(dev_id_str);
   WASAPI_FREE(dev_name_str);
   if (prop_var_init)
      PropVariantClear(&prop_var);
   WASAPI_RELEASE(prop_store);
   if (dev_id_wstr)
      CoTaskMemFree(dev_id_wstr);
   if (dev_name_wstr)
      CoTaskMemFree(dev_name_wstr);
   WASAPI_RELEASE(device);
   WASAPI_RELEASE(collection);
   WASAPI_RELEASE(enumerator);
   if (sl)
      string_list_free(sl);

   RARCH_ERR("[WASAPI]: Device enumeration failed.\n");

   return NULL;
}

static void wasapi_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   string_list_free(sl);
}

static size_t wasapi_write_avail(void *wh)
{
   HRESULT hr;
   wasapi_t *w    = (wasapi_t*)wh;
   UINT32 padding = 0;

   if (w->buffer)
      return w->buffer_size - w->buffer_usage;

   hr = w->client->lpVtbl->GetCurrentPadding(w->client, &padding);
   WASAPI_HR_CHECK(hr, "IAudioClient::GetCurrentPadding", return 0);
   
   return w->buffer_size - padding * w->frame_size;
}

static size_t wasapi_buffer_size(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   return w->buffer_size;
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
   wasapi_device_list_new,
   wasapi_device_list_free,
   wasapi_write_avail,
   wasapi_buffer_size
};
