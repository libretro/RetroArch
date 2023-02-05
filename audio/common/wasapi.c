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
      /* Standard error codes */
      case E_INVALIDARG:
         return "E_INVALIDARG";
      case E_NOINTERFACE:
         return "E_NOINTERFACE";
      case E_OUTOFMEMORY:
         return "E_OUTOFMEMORY";
      case E_POINTER:
         return "E_POINTER";
      /* Standard success codes */
      case S_FALSE:
         return "S_FALSE";
      case S_OK:
         return "S_OK";
      /* AUDCLNT error codes */
      case AUDCLNT_E_ALREADY_INITIALIZED:
         return "AUDCLNT_E_ALREADY_INITIALIZED";
      case AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL:
         return "AUDCLNT_E_BUFDURATION_PERIOD_NOT_EQUAL";
      case AUDCLNT_E_BUFFER_ERROR:
         return "AUDCLNT_E_BUFFER_ERROR";
      case AUDCLNT_E_BUFFER_OPERATION_PENDING:
         return "AUDCLNT_E_BUFFER_OPERATION_PENDING";
      case AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED:
         return "AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED";
      case AUDCLNT_E_BUFFER_SIZE_ERROR:
         return "AUDCLNT_E_BUFFER_SIZE_ERROR";
      case AUDCLNT_E_CPUUSAGE_EXCEEDED:
         return "AUDCLNT_E_CPUUSAGE_EXCEEDED";
      case AUDCLNT_E_DEVICE_IN_USE:
         return "AUDCLNT_E_DEVICE_IN_USE";
      case AUDCLNT_E_DEVICE_INVALIDATED:
         return "AUDCLNT_E_DEVICE_INVALIDATED";
      case AUDCLNT_E_ENDPOINT_CREATE_FAILED:
         return "AUDCLNT_E_ENDPOINT_CREATE_FAILED";
      case AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED:
         return "AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED";
      case AUDCLNT_E_INVALID_DEVICE_PERIOD:
         return "AUDCLNT_E_INVALID_DEVICE_PERIOD";
      case AUDCLNT_E_INVALID_SIZE:
         return "AUDCLNT_E_INVALID_SIZE";
      case AUDCLNT_E_NOT_INITIALIZED:
         return "AUDCLNT_E_NOT_INITIALIZED";
      case AUDCLNT_E_OUT_OF_ORDER:
         return "AUDCLNT_E_OUT_OF_ORDER";
      case AUDCLNT_E_SERVICE_NOT_RUNNING:
         return "AUDCLNT_E_SERVICE_NOT_RUNNING";
      case AUDCLNT_E_UNSUPPORTED_FORMAT:
         return "AUDCLNT_E_UNSUPPORTED_FORMAT";
      case AUDCLNT_E_WRONG_ENDPOINT_TYPE:
         return "AUDCLNT_E_WRONG_ENDPOINT_TYPE";
      /* AUDCLNT success codes */
      case AUDCLNT_S_BUFFER_EMPTY:
         return "AUDCLNT_S_BUFFER_EMPTY";
      /* Something else; probably from an API that we started using
       * after mic support was implemented */
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
                              bool float_fmt, unsigned rate, unsigned channels);
/**
 * Selects a device format
 * @param[in,out] format The place where the chosen format will be written,
 * as well as the first format checked.
 * @param client TODO
 * @param mode todo
 * @return true if successful, false if there was an error or a suitable format wasn't found
 */
static bool wasapi_select_device_format(WAVEFORMATEXTENSIBLE *format, IAudioClient *client, AUDCLNT_SHAREMODE mode, unsigned channels)
{
   static const unsigned preferred_rates[] = { 48000, 44100, 96000, 192000, 32000 };
   const bool preferred_formats[] = {format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE, format->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE};
   WAVEFORMATEXTENSIBLE *suggested_format = NULL;
   bool result = false;
   HRESULT hr = _IAudioClient_IsFormatSupported(client, mode,
      (const WAVEFORMATEX *) format, (WAVEFORMATEX **) &suggested_format);
   /* The Windows docs say that casting these arguments to WAVEFORMATEX* is okay. */
   switch (hr)
   {
      case S_OK:
         /* The requested format is okay without any changes */
         RARCH_DBG("[WASAPI]: Desired format (%s, %u-channel, %uHz) can be used as-is.\n",
            wave_format_name(format), format->Format.nChannels, format->Format.nSamplesPerSec);
         result = true;
         break;
      case S_FALSE:
         /* The requested format is unsupported, but Windows has suggested a similar one. */
         // TODO: Check that the suggested format meets RetroArch's requirements
         RARCH_DBG("[WASAPI]: Windows suggests a format of (%s, %u-channel, %uHz).\n",
            wave_format_name(suggested_format), suggested_format->Format.nChannels, suggested_format->Format.nSamplesPerSec);
         *format = *suggested_format;
         result = true;
         break;
      case AUDCLNT_E_UNSUPPORTED_FORMAT:
      { /* The requested format is unsupported, and Windows was unable to suggest another.
         * Usually happens with exclusive mode. */
         int i, j;
         WAVEFORMATEXTENSIBLE possible_format;
         HRESULT format_check_hr;
         RARCH_WARN("[WASAPI]: Requested format not supported, and Windows could not suggest one. RetroArch will do so.\n");
         for (i = 0; i < ARRAY_SIZE(preferred_formats); ++i)
         {
            for (j = 0; j < ARRAY_SIZE(preferred_rates); ++j)
            {
               wasapi_set_format(&possible_format, preferred_formats[i], preferred_rates[j], channels);
               format_check_hr = _IAudioClient_IsFormatSupported(client, mode, (const WAVEFORMATEX *) &possible_format, NULL);
               if (SUCCEEDED(format_check_hr))
               {
                  *format = possible_format;
                  result = true;
                  RARCH_DBG("[WASAPI]: RetroArch suggests a format of (%s, %u-channel, %uHz).\n",
                            wave_format_name(format), format->Format.nChannels, format->Format.nSamplesPerSec);
                  goto done;
               }
            }
         }
      }
      default:
         /* Something else went wrong. */
         RARCH_ERR("[WASAPI]: Failed to select client format: %s\n", hresult_name(hr));
         result = false;
         break;
   }
done:
   if (suggested_format)
   { /* IAudioClient::IsFormatSupported allocates a format object */
      CoTaskMemFree(suggested_format);
   }

   return result;
}

static void wasapi_set_format(WAVEFORMATEXTENSIBLE *wf,
      bool float_fmt, unsigned rate, unsigned channels)
{
   WORD wBitsPerSample = float_fmt ? 32 : 16;
   WORD nBlockAlign = (channels * wBitsPerSample) / 8;
   DWORD nAvgBytesPerSec = rate * nBlockAlign;

   wf->Format.nChannels       = channels;
   wf->Format.nSamplesPerSec  = rate;
   wf->Format.nAvgBytesPerSec = nAvgBytesPerSec;
   wf->Format.nBlockAlign     = nBlockAlign;
   wf->Format.wBitsPerSample  = wBitsPerSample;

   if (float_fmt)
   {
      wf->Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
      wf->Format.cbSize               = sizeof(WORD) + sizeof(DWORD) + sizeof(GUID);
      wf->Samples.wValidBitsPerSample = wBitsPerSample;
      wf->dwChannelMask               = channels == 1 ? KSAUDIO_SPEAKER_MONO : KSAUDIO_SPEAKER_STEREO;
      wf->SubFormat                   = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
   }
   else
   {
      wf->Format.wFormatTag           = WAVE_FORMAT_PCM;
      wf->Format.cbSize               = 0;
      wf->Samples.wValidBitsPerSample = 0;
      wf->dwChannelMask               = 0;
      memset(&wf->SubFormat, 0, sizeof(wf->SubFormat));
   }
}

static IAudioClient *wasapi_init_client_ex(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
{
   WAVEFORMATEXTENSIBLE wf;
   int i, j;
   IAudioClient *client           = NULL;
   REFERENCE_TIME minimum_period  = 0;
   REFERENCE_TIME buffer_duration = 0;
   UINT32 buffer_length           = 0;
   HRESULT hr                     = _IMMDevice_Activate(device,
         IID_IAudioClient,
         CLSCTX_ALL, NULL, (void**)&client);

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: IMMDevice::Activate failed: %s\n", hresult_name(hr));
      return NULL;
   }

   hr = _IAudioClient_GetDevicePeriod(client, NULL, &minimum_period);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get device period of exclusive-mode client: %s\n", hresult_name(hr));
      goto error;
   }

   /* buffer_duration is in 100ns units */
   buffer_duration = latency * 10000.0;
   if (buffer_duration < minimum_period)
      buffer_duration = minimum_period;

   wasapi_set_format(&wf, *float_fmt, *rate, channels);
   RARCH_LOG("[WASAPI]: Requesting format: %u-bit %u-channel client with %s samples at %uHz\n",
      wf.Format.wBitsPerSample,
      wf.Format.nChannels, wave_format_name(&wf), wf.Format.nSamplesPerSec);

   if (wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_EXCLUSIVE, channels))
   {
      RARCH_LOG("[WASAPI]: Using format: %u-bit %u-channel client with %s samples at %uHz\n",
         wf.Format.wBitsPerSample,
         wf.Format.nChannels, wave_format_name(&wf), wf.Format.nSamplesPerSec);
   }
   else
   {
      RARCH_ERR("[WASAPI]: Failed to select a suitable device format\n");
      goto error;
   }

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

      buffer_duration = 10000.0 * 1000.0 / (*rate) * buffer_length + 0.5;
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
   }

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to create exclusive-mode client with %s: %s",
         hresult_name(hr), wasapi_error(HRESULT_CODE(hr)));
      goto error;
   }

   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM;
   *rate      = wf.Format.nSamplesPerSec;

   RARCH_LOG("[WASAPI]: Initialized exclusive %s client at %uHz, latency %ums\n",
      *float_fmt ? "float" : "pcm", *rate, latency);

   return client;

error:
   IFACE_RELEASE(client);

   return NULL;
}

static IAudioClient *wasapi_init_client_sh(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
{
   WAVEFORMATEXTENSIBLE wf;
   IAudioClient *client = NULL;
   bool float_fmt_res   = *float_fmt;
   unsigned rate_res    = *rate;
   HRESULT hr           = _IMMDevice_Activate(device,
         IID_IAudioClient,
         CLSCTX_ALL, NULL, (void**)&client);

   if (FAILED(hr))
   { /* If we couldn't create the IAudioClient... */
      RARCH_ERR("[WASAPI]: Failed to create %s IAudioClient: %s\n", hresult_name(hr));
      return NULL;
   }

   wasapi_set_format(&wf, float_fmt_res, rate_res, channels);

   if (wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_SHARED, channels))
   {
      RARCH_LOG("[WASAPI]: Requesting %u-channel shared-mode client with %s samples at %uHz\n",
         wf.Format.nChannels, wave_format_name(&wf), wf.Format.nSamplesPerSec);
   }
   else
   {
      RARCH_ERR("[WASAPI]: Failed to select a suitable device format\n");
      goto error;
   }

   hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
         AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
         0, 0, (WAVEFORMATEX*)&wf, NULL);

   if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
   {
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

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: IAudioClient::Initialize failed: %s\n", hresult_name(hr));
      goto error;
   }

   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM;
   *rate      = wf.Format.nSamplesPerSec;

   RARCH_LOG("[WASAPI]: Initialized shared %s client at %uHz\n",
      wave_format_name(&wf), *rate);

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
   {
      hr = _IAudioClient_GetDevicePeriod(client, NULL, &device_period);
      if (SUCCEEDED(hr))
      {
         RARCH_LOG("[WASAPI]: Minimum exclusive-mode device period is %uns (= %.1fms)\n",
            device_period * 100, (double)device_period * 100 / 1e6);
      }
      /* device_period is in 100ns units */
   }
   else
   {
      hr = _IAudioClient_GetDevicePeriod(client, &device_period, NULL);
      if (SUCCEEDED(hr))
      {
         RARCH_LOG("[WASAPI]: Default shared-mode device period is %uns (= %.1fms)\n",
                   device_period * 100, (double)device_period * 100 / 1e6);
      }
   }

   if (FAILED(hr))
   {
      RARCH_WARN("[WASAPI]: IAudioClient::GetDevicePeriod failed: %s\n", hresult_name(hr));
   }

   if (!*exclusive)
   {
      hr = _IAudioClient_GetStreamLatency(client, &stream_latency);
      if (SUCCEEDED(hr))
      {
         RARCH_LOG("[WASAPI]: Shared stream latency is %uns (= %.1fms)\n",
            stream_latency * 100, (double)stream_latency * 100 / 1e6);
      }
      else
      {
         RARCH_WARN("[WASAPI]: IAudioClient::GetStreamLatency failed: %s\n", hresult_name(hr));
      }
   }

   hr = _IAudioClient_GetBufferSize(client, &buffer_length);
   if (SUCCEEDED(hr))
   {
      size_t num_samples = buffer_length * channels;
      size_t num_bytes = num_samples * (*float_fmt ? sizeof(float) : sizeof(int16_t));
      RARCH_LOG("[WASAPI]: Endpoint buffer size is %u audio frames (= %u samples, = %u bytes)\n",
         buffer_length, num_samples, num_bytes);
   }
   else
   {
      RARCH_WARN("[WASAPI]: IAudioClient::GetBufferSize failed: %s.\n", hresult_name(hr));
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