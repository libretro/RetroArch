/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 Daniel De Matteis
 *  Copyright (C) 2023 Jesse Talavera-Greenberg
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

#include "wasapi.h"
#include <stdio.h>
#include <guiddef.h>
#include "microphone/microphone_driver.h"
#include "queues/fifo_queue.h"
#include "configuration.h"
#include "verbosity.h"
#include "string/stdstring.h"
#include "mmdevice_common.h"

void wasapi_log_hr(HRESULT hr, char* buffer, size_t length)
{
   FormatMessage(
         FORMAT_MESSAGE_IGNORE_INSERTS |
         FORMAT_MESSAGE_FROM_SYSTEM |
         FORMAT_MESSAGE_FROM_HMODULE,
         NULL,
         hr,
         MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
         buffer,
         length - 1,
         NULL);
}

const char *hresult_name(HRESULT hr)
{
   switch (hr)
   {
      case E_NOINTERFACE:
         return "E_NOINTERFACE";
      case E_POINTER:
         return "E_POINTER";
      case E_OUTOFMEMORY:
         return "E_OUTOFMEMORY";
      case E_INVALIDARG:
         return "E_INVALIDARG";
      case AUDCLNT_E_DEVICE_INVALIDATED:
         return "AUDCLNT_E_DEVICE_INVALIDATED";
      case AUDCLNT_E_ALREADY_INITIALIZED:
         return "AUDCLNT_E_ALREADY_INITIALIZED";
      case AUDCLNT_E_WRONG_ENDPOINT_TYPE:
         return "AUDCLNT_E_WRONG_ENDPOINT_TYPE";
      case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
         return "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";
      case AUDCLNT_E_BUFFER_SIZE_ERROR:
         return "AUDCLNT_E_BUFFER_SIZE_ERROR";
      case AUDCLNT_E_CPUUSAGE_EXCEEDED:
         return "AUDCLNT_E_CPUUSAGE_EXCEEDED";
      case AUDCLNT_E_DEVICE_IN_USE:
         return "AUDCLNT_E_DEVICE_IN_USE";
      case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
         return "AUDCLNT_E_ENDPOINT_CREATE_FAILED";
      case AUDCLNT_E_INVALID_DEVICE_PERIOD:
         return "AUDCLNT_E_INVALID_DEVICE_PERIOD";
      case AUDCLNT_E_UNSUPPORTED_FORMAT:
         return "AUDCLNT_E_UNSUPPORTED_FORMAT";
      case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
         return "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
      case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:
         return "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL";
      case AUDCLNT_E_SERVICE_NOT_RUNNING:
         return "AUDCLNT_E_SERVICE_NOT_RUNNING";
      default:
         return "<unknown>";
   }
}

const char *wave_subtype_name(const GUID *guid)
{
   if (IsEqualGUID(guid, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))
   {
      return "KSDATAFORMAT_SUBTYPE_IEEE_FLOAT";
   }

   return "<unknown sub-format>";
}

const char *wave_format_name(const WAVEFORMATEXTENSIBLE *format)
{
   switch (format->Format.wFormatTag)
   {
      case WAVE_FORMAT_PCM:
         return "WAVE_FORMAT_PCM";
      case WAVE_FORMAT_EXTENSIBLE:
         return wave_subtype_name(&format->SubFormat);
      default:
         return "<unknown>";
   }
}

const char *sharemode_name(AUDCLNT_SHAREMODE mode)
{
   switch (mode)
   {
      case AUDCLNT_SHAREMODE_SHARED:
         return "shared";
      case AUDCLNT_SHAREMODE_EXCLUSIVE:
         return "exclusive";
      default:
         return "<unknown>";
   }
}

const char* wasapi_error(DWORD error)
{
   static char error_message[256];

   FormatMessage(
         FORMAT_MESSAGE_IGNORE_INSERTS |
         FORMAT_MESSAGE_FROM_SYSTEM,
         NULL,
         error,
         MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
         error_message,
         sizeof(error_message) - 1,
         NULL);

   return error_message;
}

static const char* wasapi_data_flow_name(EDataFlow data_flow)
{
   switch (data_flow)
   {
      case eCapture:
         return "eCapture";
      case eRender:
         return "eRender";
      case eAll:
         return "eAll";
      default:
         return "<unknown>";
   }
}

static unsigned wasapi_pref_rate(unsigned i)
{
   const unsigned r[] = { 48000, 44100, 96000, 192000, 32000 };

   if (i >= sizeof(r) / sizeof(unsigned))
      return 0;

   return r[i];
}

static void wasapi_set_format(WAVEFORMATEXTENSIBLE *wf,
      bool float_fmt, unsigned rate, unsigned channels)
{
   wf->Format.nChannels               = channels;
   wf->Format.nSamplesPerSec          = rate;

   if (float_fmt)
   {
      wf->Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
      wf->Format.nAvgBytesPerSec      = rate * 8;
      wf->Format.nBlockAlign          = 8;
      wf->Format.wBitsPerSample       = 32;
      wf->Format.cbSize               = sizeof(WORD) + sizeof(DWORD) + sizeof(GUID);
      wf->Samples.wValidBitsPerSample = 32;
      wf->dwChannelMask               = channels == 1 ? KSAUDIO_SPEAKER_MONO : KSAUDIO_SPEAKER_STEREO;
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

static IAudioClient *wasapi_init_client_ex(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
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
   {
      RARCH_ERR("[WASAPI]: IMMDevice::Activate failed (%s): %s",
         hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
      return NULL;
   }

   hr = _IAudioClient_GetDevicePeriod(client, NULL, &minimum_period);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get device period of exclusive-mode client (%s): %s",
         hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
      goto error;
   }

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
         wasapi_set_format(&wf, float_fmt_res, rate_res, channels);
         hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
               AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
               buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
         if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
         {
            RARCH_WARN("[WASAPI] Unaligned buffer size: %s", wasapi_error(HRESULT_CODE(hr)));
            hr = _IAudioClient_GetBufferSize(client, &buffer_length);
            if (FAILED(hr))
            {
               RARCH_ERR("[WASAPI] Failed to get buffer size of client (%s): %s",
                  hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
               goto error;
            }

            IFACE_RELEASE(client);
            hr                     = _IMMDevice_Activate(device,
                  IID_IAudioClient,
                  CLSCTX_ALL, NULL, (void**)&client);
            if (FAILED(hr))
            {
               RARCH_ERR("[WASAPI] IMMDevice::Activate failed (%s): %s",
                  hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
               return NULL;
            }

            buffer_duration = 10000.0 * 1000.0 / rate_res * buffer_length + 0.5;
            hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
         }
         if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
         {
            IFACE_RELEASE(client);
            hr                     = _IMMDevice_Activate(device,
                  IID_IAudioClient,
                  CLSCTX_ALL, NULL, (void**)&client);
            if (FAILED(hr))
            {
               RARCH_ERR("[WASAPI] IMMDevice::Activate failed (%s): %s",
                  hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
               return NULL;
            }

            hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
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
   {
      RARCH_ERR("[WASAPI]: Failed to create exclusive-mode client with %s: %s",
         hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
      goto error;
   }

   *float_fmt = float_fmt_res;
   *rate      = rate_res;

   RARCH_LOG("[WASAPI]: Initialized exclusive %s client at %uHz, latency %ums\n",
      float_fmt_res ? "float" : "pcm", rate_res, latency);

   return client;

error:
   IFACE_RELEASE(client);

   return NULL;
}

static IAudioClient *wasapi_init_client_sh(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
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
   { /* If we couldn't create the IAudioClient... */
      RARCH_ERR("[WASAPI]: Failed to create %s IAudioClient with %s: %s", hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
      return NULL;
   }

   /* once for float, once for pcm (requested first) */
   for (i = 0; i < 2; ++i)
   {
      rate_res = *rate;
      if (i == 1)
         float_fmt_res = !float_fmt_res;

      /* for requested rate (first) and all preferred rates */
      for (j = 0; rate_res; ++j)
      {
         wasapi_set_format(&wf, float_fmt_res, rate_res, channels);
         hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
               AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
               0, 0, (WAVEFORMATEX*)&wf, NULL);

         if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
         {
            HRESULT hr;
            IFACE_RELEASE(client);
            hr           = _IMMDevice_Activate(device,
                  IID_IAudioClient,
                  CLSCTX_ALL, NULL, (void**)&client);
            if (FAILED(hr))
               return NULL;

            hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
                  0, 0, (WAVEFORMATEX*)&wf, NULL);
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
   {
      RARCH_ERR("[WASAPI]: IAudioClient::Initialize failed with %s: %s", hresult_name(hr),
                wasapi_error(HRESULT_CODE(hr)));
      goto error;
   }

   *float_fmt = float_fmt_res;
   *rate      = rate_res;

   RARCH_LOG("[WASAPI]: Initialized shared %s client at %uHz, latency %ums\n",
      float_fmt_res ? "float" : "pcm", rate_res, latency);

   return client;

error:
   IFACE_RELEASE(client);

   return NULL;
}

IMMDevice *wasapi_init_device(const char *id, EDataFlow data_flow)
{
   HRESULT hr;
   UINT32 dev_count, i;
   IMMDeviceEnumerator *enumerator = NULL;
   IMMDevice *device               = NULL;
   IMMDeviceCollection *collection = NULL;
   const char *data_flow_name      = wasapi_data_flow_name(data_flow);
   char error_message[256]         = {0};

   if (id)
   {
      RARCH_LOG("[WASAPI]: Initializing %s device \"%s\" ...\n", data_flow_name, id);
   }
   else
   {
      RARCH_LOG("[WASAPI]: Initializing default %s device.. \n", data_flow_name);
   }

#ifdef __cplusplus
   hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         IID_IMMDeviceEnumerator, (void **)&enumerator);
#else
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                         &IID_IMMDeviceEnumerator, (void **)&enumerator);
#endif
   if (FAILED(hr))
   {
      wasapi_log_hr(hr, error_message, sizeof(error_message));
      RARCH_ERR("[WASAPI]: Failed to create device enumerator: %s\n", error_message);
      goto error;
   }

   if (id)
   { /* If a specific device was requested... */
      int32_t idx_found        = -1;
      struct string_list *list = (struct string_list*)mmdevice_list_new(NULL, data_flow);

      if (!list)
      {
         RARCH_ERR("[WASAPI]: Failed to allocate %s device list\n", data_flow_name);
         goto error;
      }

      if (list->elems)
      { /* If any devices were found... */
         unsigned d;
         for (d = 0; d < list->size; d++)
         {
            RARCH_LOG("[WASAPI]: %u : %s\n", d, list->elems[d].data);
            if (string_is_equal(id, list->elems[d].data))
            {
               idx_found = d;
               break;
            }
         }
         /* Index was not found yet based on name string,
          * just assume id is a one-character number index. */

         if (idx_found == -1 && isdigit(id[0]))
         {
            idx_found = strtoul(id, NULL, 0);
            RARCH_LOG("[WASAPI]: Fallback, %s device index is a single number index instead: %u.\n", data_flow_name, idx_found);
         }
      }
      string_list_free(list);

      if (idx_found == -1)
         idx_found = 0;

      hr = _IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,
                                                   data_flow, DEVICE_STATE_ACTIVE, &collection);
      if (FAILED(hr))
      {
         wasapi_log_hr(hr, error_message, sizeof(error_message));
         goto error;
      }

      hr = _IMMDeviceCollection_GetCount(collection, &dev_count);
      if (FAILED(hr))
      {
         wasapi_log_hr(hr, error_message, sizeof(error_message));
         goto error;
      }

      for (i = 0; i < dev_count; ++i)
      {
         hr = _IMMDeviceCollection_Item(collection, i, &device);
         if (FAILED(hr))
         {
            wasapi_log_hr(hr, error_message, sizeof(error_message));
            goto error;
         }

         if (i == idx_found)
            break;

         IFACE_RELEASE(device);
      }
   }
   else
   {
      hr = _IMMDeviceEnumerator_GetDefaultAudioEndpoint(
            enumerator, data_flow, eConsole, &device);
      if (FAILED(hr))
      {
         wasapi_log_hr(hr, error_message, sizeof(error_message));
         goto error;
      }
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
      RARCH_WARN("[WASAPI]: Failed to initialize %s device \"%s\"\n", data_flow_name, id);
   }
   else
   {
      RARCH_ERR("[WASAPI]: Failed to initialize default %s device\n", data_flow_name);
   }

   return NULL;
}

IAudioClient *wasapi_init_client(IMMDevice *device, bool *exclusive,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
{
   HRESULT hr;
   IAudioClient *client;
   double latency_res;
   REFERENCE_TIME device_period  = 0;
   REFERENCE_TIME stream_latency = 0;
   UINT32 buffer_length          = 0;

   RARCH_DBG("[WASAPI]: Requesting %s %s client (rate=%uHz, latency=%ums).\n",
      *exclusive ? "exclusive" : "shared",
      *float_fmt ? "float" : "pcm", *rate, latency);

   if (*exclusive)
   {
      client = wasapi_init_client_ex(device, float_fmt, rate, latency, channels);
      if (!client)
      {
         RARCH_WARN("[WASAPI] Failed to initialize exclusive client, attempting shared client.\n");
         client = wasapi_init_client_sh(device, float_fmt, rate, latency, channels);
         if (client)
            *exclusive = false;
      }
   }
   else
   {
      client = wasapi_init_client_sh(device, float_fmt, rate, latency, channels);
      if (!client)
      {
         RARCH_WARN("[WASAPI] Failed to initialize shared client, attempting exclusive client.\n");
         client = wasapi_init_client_ex(device, float_fmt, rate, latency, channels);
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