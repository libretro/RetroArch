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
#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"
#endif
#include <string/stdstring.h>
#include <queues/fifo_queue.h>
#include <lists/string_list.h>

#include "mmdevice_common.h"

#include "../../configuration.h"
#include "../../verbosity.h"

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
         break;
   }

   return "<unknown>";
}

static const char *wave_subtype_name(const GUID *guid)
{
   if (!memcmp(guid, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)))
      return "KSDATAFORMAT_SUBTYPE_IEEE_FLOAT";
   return "<unknown sub-format>";
}

static const char *wave_format_name(const WAVEFORMATEXTENSIBLE *format)
{
   switch (format->Format.wFormatTag)
   {
      case WAVE_FORMAT_PCM:
         return "WAVE_FORMAT_PCM";
      case WAVE_FORMAT_EXTENSIBLE:
         return wave_subtype_name(&format->SubFormat);
      default:
         break;
   }

   return "<unknown>";
}

const char* wasapi_error(DWORD error)
{
   static char error_message[256];

   FormatMessage(
           FORMAT_MESSAGE_IGNORE_INSERTS
         | FORMAT_MESSAGE_FROM_SYSTEM,
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
         break;
   }

   return "<unknown>";
}

static void wasapi_set_format(WAVEFORMATEXTENSIBLE *wf,
      bool float_fmt, unsigned rate, unsigned channels)
{
   WORD wBitsPerSample        = float_fmt ? 32 : 16;
   WORD nBlockAlign           = (channels * wBitsPerSample) / 8;
   DWORD nAvgBytesPerSec      = rate * nBlockAlign;

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

/**
 * @param[in] format The format to check.
 * @return \c true if \c format is suitable for RetroArch.
 */
static bool wasapi_is_format_suitable(const WAVEFORMATEXTENSIBLE *format)
{
   /* RetroArch only supports mono mic input and stereo speaker output */
   if (!format || format->Format.nChannels == 0 || format->Format.nChannels > 2)
      return false;

   switch (format->Format.wFormatTag)
   {
      case WAVE_FORMAT_PCM:
         if (format->Format.wBitsPerSample != 16)
            /* Integer samples must be 16-bit */
            return false;
         break;
      case WAVE_FORMAT_EXTENSIBLE:
         if (!(!memcmp(&format->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID))))
            /* RetroArch doesn't support any other subformat */
            return false;

         if (format->Format.wBitsPerSample != 32)
            /* floating-point samples must be 32-bit */
            return false;
         break;
      default:
         /* Other formats are unsupported */
         return false;
   }

   return true;
}

/**
 * Selects a sample format suitable for the given device.
 * @param[in,out] format The place where the chosen format will be written,
 * as well as the first format checked.
 * @param[in] client The audio client (i.e. device handle) for which a format will be selected.
 * @param[in] mode The device mode (shared or exclusive) that \c client will use.
 * @param[in] channels The number of channels that will constitute one audio frame.
 * @return \c true if successful, \c false if a suitable format wasn't found or there was an error.
 * If \c true, the selected format will be written to \c format.
 * If \c false, the value referred by \c format will be unchanged.
 */
static bool wasapi_select_device_format(WAVEFORMATEXTENSIBLE *format, IAudioClient *client, AUDCLNT_SHAREMODE mode, unsigned channels)
{
   /* Try the requested sample format first, then try the other one. */
   WAVEFORMATEXTENSIBLE *suggested_format  = NULL;
   bool result                             = false;
   HRESULT hr                              = _IAudioClient_IsFormatSupported(
         client, mode,
         (const WAVEFORMATEX *)format,
         (WAVEFORMATEX **)&suggested_format);
   /* The Windows docs say that casting these arguments to WAVEFORMATEX* is okay. */

   switch (hr)
   {
      case S_OK:
         /* The requested format is okay without any changes. */
         result = true;
         break;
      case S_FALSE:
         /* The requested format is unsupported, but Windows has suggested a similar one. */
         RARCH_DBG("[WASAPI]: Windows suggests a format of (%s, %u-channel, %uHz).\n",
               wave_format_name(suggested_format),
               suggested_format->Format.nChannels,
               suggested_format->Format.nSamplesPerSec);

         if (wasapi_is_format_suitable(suggested_format))
         {
            *format = *suggested_format;
            result  = true;
         }
         else
         {
            RARCH_ERR("[WASAPI]: Windows suggested a format, but RetroArch can't use it.\n");
         }
         break;
      case AUDCLNT_E_UNSUPPORTED_FORMAT:
      {
         /* The requested format is unsupported
          * and Windows was unable to suggest another.
          * Usually happens with exclusive mode.
          * RetroArch will try selecting a format. */
         size_t i, j;
         bool preferred_formats[2];
         preferred_formats[0] = (format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);
         preferred_formats[1] = (format->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE);
         RARCH_WARN("[WASAPI]: Requested format not supported, and Windows could not suggest one. RetroArch will do so.\n");
         for (i = 0; i < ARRAY_SIZE(preferred_formats); ++i)
         {
            static const unsigned preferred_rates[] = { 48000, 44100, 96000, 192000, 32000 };
            for (j = 0; j < ARRAY_SIZE(preferred_rates); ++j)
            {
               HRESULT format_check_hr;
               WAVEFORMATEXTENSIBLE possible_format;
               wasapi_set_format(&possible_format, preferred_formats[i], preferred_rates[j], channels);
               format_check_hr = _IAudioClient_IsFormatSupported(client, mode, (const WAVEFORMATEX *) &possible_format, NULL);
               if (SUCCEEDED(format_check_hr))
               {
                  *format = possible_format;
                  result  = true;
                  RARCH_DBG("[WASAPI]: RetroArch suggests a format of (%s, %u-channel, %uHz).\n",
                        wave_format_name(format),
                        format->Format.nChannels,
                        format->Format.nSamplesPerSec);
                  goto done;
               }
            }
         }
         RARCH_ERR("[WASAPI]: Failed to select client format: No suitable format available.\n");
         break;
      }
      default:
         /* Something else went wrong. */
         RARCH_ERR("[WASAPI]: Failed to select client format: %s.\n", hresult_name(hr));
         break;
   }
done:
   /* IAudioClient::IsFormatSupported allocates a format object. */
   if (suggested_format)
      CoTaskMemFree(suggested_format);

   return result;
}

static IAudioClient *wasapi_init_client_ex(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
{
   WAVEFORMATEXTENSIBLE wf;
   IAudioClient *client           = NULL;
   REFERENCE_TIME minimum_period  = 0;
   REFERENCE_TIME buffer_duration = 0;
   UINT32 buffer_length           = 0;
   HRESULT hr                     = _IMMDevice_Activate(device,
         IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: IMMDevice::Activate failed: %s.\n", hresult_name(hr));
      return NULL;
   }

   hr = _IAudioClient_GetDevicePeriod(client, NULL, &minimum_period);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get minimum device period of exclusive client: %s.\n", hresult_name(hr));
      goto error;
   }

   /* Buffer_duration is in 100ns units. */
   buffer_duration = latency * 10000.0;
   if (buffer_duration < minimum_period)
      buffer_duration = minimum_period;

   wasapi_set_format(&wf, *float_fmt, *rate, channels);
   RARCH_DBG("[WASAPI]: Requesting exclusive %u-bit %u-channel client with %s samples at %uHz %ums.\n",
         wf.Format.wBitsPerSample,
         wf.Format.nChannels,
         wave_format_name(&wf),
         wf.Format.nSamplesPerSec,
         latency);

   if (!wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_EXCLUSIVE, channels))
   {
      RARCH_ERR("[WASAPI]: Failed to select a suitable device format.\n");
      goto error;
   }

   hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
         AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
         buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);

   if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
   {
      RARCH_WARN("[WASAPI]: Unaligned buffer size: %s.\n", hresult_name(hr));
      hr = _IAudioClient_GetBufferSize(client, &buffer_length);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: Failed to get buffer size of client: %s.\n", hresult_name(hr));
         goto error;
      }

      IFACE_RELEASE(client);
      hr = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)&client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: IMMDevice::Activate failed: %s.\n", hresult_name(hr));
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
      hr = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)&client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n", hresult_name(hr));
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
      RARCH_ERR("[WASAPI]: IAudioClient::Initialize failed: %s.\n", hresult_name(hr));
      goto error;
   }

   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM;
   *rate      = wf.Format.nSamplesPerSec;

   return client;

error:
   IFACE_RELEASE(client);

   return NULL;
}

static IAudioClient *wasapi_init_client_sh(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
{
   WAVEFORMATEXTENSIBLE wf;
   IAudioClient *client           = NULL;
   settings_t *settings           = config_get_ptr();
   unsigned sh_buffer_length      = settings->uints.audio_wasapi_sh_buffer_length;
   REFERENCE_TIME default_period  = 0;
   REFERENCE_TIME buffer_duration = 0;
   HRESULT hr                     = _IMMDevice_Activate(device,
         IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: IMMDevice::Activate failed: %s.\n", hresult_name(hr));
      return NULL;
   }

   hr = _IAudioClient_GetDevicePeriod(client, &default_period, NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get default device period of shared client: %s.\n", hresult_name(hr));
      goto error;
   }

   /* Use audio latency setting for buffer size if allowed */
   if (     (sh_buffer_length < WASAPI_SH_BUFFER_DEVICE_PERIOD)
         || (sh_buffer_length > WASAPI_SH_BUFFER_CLIENT_BUFFER))
   {
      /* Buffer_duration is in 100ns units. */
      buffer_duration = latency * 10000.0;
      if (buffer_duration < default_period)
         buffer_duration = default_period;
   }

   wasapi_set_format(&wf, *float_fmt, *rate, channels);
   RARCH_DBG("[WASAPI]: Requesting shared %u-bit %u-channel client with %s samples at %uHz %ums.\n",
         wf.Format.wBitsPerSample,
         wf.Format.nChannels,
         wave_format_name(&wf),
         wf.Format.nSamplesPerSec,
         latency);

   if (!wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_SHARED, channels))
   {
      RARCH_ERR("[WASAPI]: Failed to select a suitable device format.\n");
      goto error;
   }

   hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
         buffer_duration, 0, (WAVEFORMATEX*)&wf, NULL);

   if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
   {
      IFACE_RELEASE(client);
      hr = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)&client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: IMMDevice::Activate failed: %s.\n", hresult_name(hr));
         return NULL;
      }

      hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            buffer_duration, 0, (WAVEFORMATEX*)&wf, NULL);
   }

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: IAudioClient::Initialize failed: %s.\n", hresult_name(hr));
      goto error;
   }

   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM;
   *rate      = wf.Format.nSamplesPerSec;

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

   if (id)
      RARCH_DBG("[WASAPI]: Initializing %s device \"%s\"..\n", data_flow_name, id);
   else
      RARCH_DBG("[WASAPI]: Initializing default %s device..\n", data_flow_name);

#ifdef __cplusplus
   hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         IID_IMMDeviceEnumerator, (void **)&enumerator);
#else
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
#endif
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to create device enumerator: %s.\n", hresult_name(hr));
      goto error;
   }

   if (id)
   {
      /* If a specific device was requested... */
      int32_t idx_found        = -1;
      struct string_list *list = (struct string_list*)mmdevice_list_new(NULL, data_flow);

      if (!list)
      {
         RARCH_ERR("[WASAPI]: Failed to allocate %s device list.\n", data_flow_name);
         goto error;
      }

      if (list->elems)
      {
         /* If any devices were found... */
         unsigned d;
         for (d = 0; d < list->size; d++)
         {
            if (string_is_equal(id, list->elems[d].data))
            {
               RARCH_DBG("[WASAPI]: Found device #%d: \"%s\".\n", d, list->elems[d].data);
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
         RARCH_ERR("[WASAPI]: Failed to enumerate audio endpoints: %s.\n", hresult_name(hr));
         goto error;
      }

      hr = _IMMDeviceCollection_GetCount(collection, &dev_count);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: Failed to count IMMDevices: %s.\n", hresult_name(hr));
         goto error;
      }

      for (i = 0; i < dev_count; ++i)
      {
         hr = _IMMDeviceCollection_Item(collection, i, &device);
         if (FAILED(hr))
         {
            RARCH_ERR("[WASAPI]: Failed to get IMMDevice #%d: %s.\n", i, hresult_name(hr));
            goto error;
         }

         if (i == (UINT32)idx_found)
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
         RARCH_ERR("[WASAPI]: Failed to get default audio endpoint: %s.\n", hresult_name(hr));
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
      RARCH_WARN("[WASAPI]: Failed to initialize %s device \"%s\".\n", data_flow_name, id);
   else
      RARCH_ERR("[WASAPI]: Failed to initialize default %s device.\n", data_flow_name);

   return NULL;
}

IAudioClient *wasapi_init_client(IMMDevice *device, bool *exclusive,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels)
{
   HRESULT hr;
   IAudioClient *client;
   float latency_res;
   REFERENCE_TIME device_period     = 0;
   REFERENCE_TIME device_period_min = 0;
   REFERENCE_TIME stream_latency    = 0;
   UINT32 buffer_length             = 0;

   if (*exclusive)
   {
      client = wasapi_init_client_ex(device, float_fmt, rate, latency, channels);
      if (!client)
      {
         RARCH_WARN("[WASAPI]: Failed to initialize exclusive client, attempting shared client.\n");
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
         RARCH_WARN("[WASAPI]: Failed to initialize shared client, attempting exclusive client.\n");
         client = wasapi_init_client_ex(device, float_fmt, rate, latency, channels);
         if (client)
            *exclusive = true;
      }
   }

   if (!client)
      return NULL;

   /* Remaining calls are for logging purposes. */

   hr = _IAudioClient_GetDevicePeriod(client, &device_period, &device_period_min);
   if (SUCCEEDED(hr))
   {
      RARCH_DBG("[WASAPI]: Default device period is %.1fms.\n", (float)device_period * 100 / 1e6);
      RARCH_DBG("[WASAPI]: Minimum device period is %.1fms.\n", (float)device_period_min * 100 / 1e6);
   }
   else
      RARCH_WARN("[WASAPI]: IAudioClient::GetDevicePeriod failed: %s.\n", hresult_name(hr));

   if (!*exclusive)
   {
      hr = _IAudioClient_GetStreamLatency(client, &stream_latency);
      if (SUCCEEDED(hr))
         RARCH_DBG("[WASAPI]: Shared stream latency is %.1fms.\n", (float)stream_latency * 100 / 1e6);
      else
         RARCH_WARN("[WASAPI]: IAudioClient::GetStreamLatency failed: %s.\n", hresult_name(hr));
   }

   hr = _IAudioClient_GetBufferSize(client, &buffer_length);
   if (SUCCEEDED(hr))
   {
      size_t num_samples = buffer_length * channels;
      size_t num_bytes = num_samples * (*float_fmt ? sizeof(float) : sizeof(int16_t));
      RARCH_DBG("[WASAPI]: Endpoint buffer size is %u frames (%u samples, %u bytes, %.1f ms).\n",
            buffer_length, num_samples, num_bytes, (float)buffer_length * 1000.0 / *rate);
   }
   else
      RARCH_WARN("[WASAPI]: IAudioClient::GetBufferSize failed: %s.\n", hresult_name(hr));

   if (*exclusive)
      latency_res = (float)buffer_length * 1000.0 / (*rate);
   else
   {
      settings_t *settings      = config_get_ptr();
      unsigned sh_buffer_length = settings->uints.audio_wasapi_sh_buffer_length;

      switch (sh_buffer_length)
      {
         case WASAPI_SH_BUFFER_AUDIO_LATENCY:
         case WASAPI_SH_BUFFER_CLIENT_BUFFER:
            latency_res = (float)buffer_length * 1000.0 / (*rate);
            break;
         case WASAPI_SH_BUFFER_DEVICE_PERIOD:
            latency_res = (float)(stream_latency + device_period) / 10000.0;
            break;
         default:
            latency_res = (float)sh_buffer_length * 1000.0 / (*rate);
            break;
      }
   }

   RARCH_LOG("[WASAPI]: Client initialized (%s, %s, %uHz, %.1fms).\n",
         *exclusive ? "exclusive" : "shared",
         *float_fmt ? "FLOAT" : "PCM",
         *rate, latency_res);

   return client;
}
