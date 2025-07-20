/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2023-2025 - Jesse Talavera-Greenberg
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
#include "../common/wasapi.h"

#include "../audio_driver.h"
#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"
#endif
#include "../../verbosity.h"
#include "../../configuration.h"

/* Max time to wait before continuing */
#define WASAPI_TIMEOUT 256

enum wasapi_flags
{
   WASAPI_FLG_EXCLUSIVE = (1 << 0),
   WASAPI_FLG_NONBLOCK  = (1 << 1),
   WASAPI_FLG_RUNNING   = (1 << 2)
};

typedef struct
{
   HANDLE write_event;
   IMMDevice          *device;
   IAudioClient       *client;
   IAudioRenderClient *renderer;
   fifo_buffer_t      *buffer;
   size_t engine_buffer_size;
   unsigned char frame_size;          /* 4 or 8 only */
   uint8_t flags;
} wasapi_t;

static const char *hresult_name(HRESULT hr)
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

static const char* wasapi_error(DWORD error)
{
   static char s[256];
   FormatMessage(
           FORMAT_MESSAGE_IGNORE_INSERTS
         | FORMAT_MESSAGE_FROM_SYSTEM,
         NULL, error,
         MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
         s, sizeof(s) - 1, NULL);
   return s;
}

static const char* wasapi_data_flow_name(unsigned data_flow)
{
   switch (data_flow)
   {
      case 0:
         return "eRender";
      case 1:
         return "eCapture";
      case 2:
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
         RARCH_DBG("[WASAPI] Windows suggests a format of (%s, %u-channel, %uHz).\n",
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
            RARCH_ERR("[WASAPI] Windows suggested a format, but RetroArch can't use it.\n");
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
         RARCH_WARN("[WASAPI] Requested format not supported, and Windows could not suggest one. RetroArch will do so.\n");
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
                  RARCH_DBG("[WASAPI] RetroArch suggests a format of (%s, %u-channel, %uHz).\n",
                        wave_format_name(format),
                        format->Format.nChannels,
                        format->Format.nSamplesPerSec);
                  goto done;
               }
            }
         }
         RARCH_ERR("[WASAPI] Failed to select client format: No suitable format available.\n");
         break;
      }
      default:
         /* Something else went wrong. */
         RARCH_ERR("[WASAPI] Failed to select client format: %s.\n", hresult_name(hr));
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
      RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n", hresult_name(hr));
      return NULL;
   }

   hr = _IAudioClient_GetDevicePeriod(client, NULL, &minimum_period);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get minimum device period of exclusive client: %s.\n", hresult_name(hr));
      goto error;
   }

   /* Buffer_duration is in 100ns units. */
   buffer_duration = latency * 10000.0;
   if (buffer_duration < minimum_period)
      buffer_duration = minimum_period;

   wasapi_set_format(&wf, *float_fmt, *rate, channels);
   RARCH_DBG("[WASAPI] Requesting exclusive %u-bit %u-channel client with %s samples at %uHz %ums.\n",
         wf.Format.wBitsPerSample,
         wf.Format.nChannels,
         wave_format_name(&wf),
         wf.Format.nSamplesPerSec,
         latency);

   if (!wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_EXCLUSIVE, channels))
   {
      RARCH_ERR("[WASAPI] Failed to select a suitable device format.\n");
      goto error;
   }

   hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
         AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
         buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);

   if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
   {
      RARCH_WARN("[WASAPI] Unaligned buffer size: %s.\n", hresult_name(hr));
      hr = _IAudioClient_GetBufferSize(client, &buffer_length);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to get buffer size of client: %s.\n", hresult_name(hr));
         goto error;
      }

      if (client)
#ifdef __cplusplus
         client->Release();
#else
         client->lpVtbl->Release(client);
#endif
      client = NULL;
      hr     = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)&client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n", hresult_name(hr));
         return NULL;
      }

      buffer_duration = 10000.0 * 1000.0 / (*rate) * buffer_length + 0.5;
      hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            buffer_duration, buffer_duration, (WAVEFORMATEX*)&wf, NULL);
   }
   if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
   {
      if (client)
#ifdef __cplusplus
         client->Release();
#else
         client->lpVtbl->Release(client);
#endif
      client = NULL;
      hr     = _IMMDevice_Activate(device,
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
      RARCH_ERR("[WASAPI] IAudioClient::Initialize failed: %s.\n", hresult_name(hr));
      goto error;
   }

   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM;
   *rate      = wf.Format.nSamplesPerSec;

   return client;

error:
   if (client)
#ifdef __cplusplus
      client->Release();
#else
      client->lpVtbl->Release(client);
#endif
   client = NULL;

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
      RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n", hresult_name(hr));
      return NULL;
   }

   hr = _IAudioClient_GetDevicePeriod(client, &default_period, NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get default device period of shared client: %s.\n", hresult_name(hr));
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
   RARCH_DBG("[WASAPI] Requesting shared %u-bit %u-channel client with %s samples at %uHz %ums.\n",
         wf.Format.wBitsPerSample,
         wf.Format.nChannels,
         wave_format_name(&wf),
         wf.Format.nSamplesPerSec,
         latency);

   if (!wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_SHARED, channels))
   {
      RARCH_ERR("[WASAPI] Failed to select a suitable device format.\n");
      goto error;
   }

   hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
         buffer_duration, 0, (WAVEFORMATEX*)&wf, NULL);

   if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
   {
      if (client)
#ifdef __cplusplus
         client->Release();
#else
         client->lpVtbl->Release(client);
#endif
      client = NULL;
      hr     = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)&client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n", hresult_name(hr));
         return NULL;
      }

      hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            buffer_duration, 0, (WAVEFORMATEX*)&wf, NULL);
   }

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] IAudioClient::Initialize failed: %s.\n", hresult_name(hr));
      goto error;
   }

   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM;
   *rate      = wf.Format.nSamplesPerSec;

   return client;

error:
   if (client)
#ifdef __cplusplus
      client->Release();
#else
      client->lpVtbl->Release(client);
#endif
   client = NULL;

   return NULL;
}

static IMMDevice *wasapi_init_device(const char *id, unsigned data_flow)
{
   HRESULT hr;
   UINT32 dev_count, i;
   IMMDeviceEnumerator *enumerator = NULL;
   IMMDevice *device               = NULL;
   IMMDeviceCollection *collection = NULL;
   const char *data_flow_name      = wasapi_data_flow_name(data_flow);

   if (id)
      RARCH_DBG("[WASAPI] Initializing %s device \"%s\"...\n", data_flow_name, id);
   else
      RARCH_DBG("[WASAPI] Initializing default %s device...\n", data_flow_name);

#ifdef __cplusplus
   hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         IID_IMMDeviceEnumerator, (void **)&enumerator);
#else
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
#endif
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to create device enumerator: %s.\n", hresult_name(hr));
      goto error;
   }

   if (id)
   {
      /* If a specific device was requested... */
      int32_t idx_found        = -1;
      struct string_list *list = (struct string_list*)mmdevice_list_new(NULL, data_flow);

      if (!list)
      {
         RARCH_ERR("[WASAPI] Failed to allocate %s device list.\n", data_flow_name);
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
               RARCH_DBG("[WASAPI] Found device #%d: \"%s\".\n", d, list->elems[d].data);
               idx_found = d;
               break;
            }
         }

         /* Index was not found yet based on name string,
          * just assume id is a one-character number index. */
         if (idx_found == -1 && isdigit(id[0]))
         {
            idx_found = strtoul(id, NULL, 0);
            RARCH_LOG("[WASAPI] Fallback, %s device index is a single number index instead: %u.\n", data_flow_name, idx_found);
         }
      }
      string_list_free(list);

      if (idx_found == -1)
         idx_found = 0;

      hr = _IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,
            data_flow, DEVICE_STATE_ACTIVE, &collection);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to enumerate audio endpoints: %s.\n", hresult_name(hr));
         goto error;
      }

      hr = _IMMDeviceCollection_GetCount(collection, &dev_count);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to count IMMDevices: %s.\n", hresult_name(hr));
         goto error;
      }

      for (i = 0; i < dev_count; ++i)
      {
         hr = _IMMDeviceCollection_Item(collection, i, &device);
         if (FAILED(hr))
         {
            RARCH_ERR("[WASAPI] Failed to get IMMDevice #%d: %s.\n", i, hresult_name(hr));
            goto error;
         }

         if (i == (UINT32)idx_found)
            break;

         if (device)
#ifdef __cplusplus
            device->Release();
#else
            device->lpVtbl->Release(device);
#endif
          device = NULL;
      }
   }
   else
   {
      hr = _IMMDeviceEnumerator_GetDefaultAudioEndpoint(
            enumerator, data_flow, eConsole, &device);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to get default audio endpoint: %s.\n", hresult_name(hr));
         goto error;
      }
   }

   if (!device)
      goto error;

   if (collection)
#ifdef __cplusplus
      collection->Release();
#else
      collection->lpVtbl->Release(collection);
#endif
   if (enumerator)
#ifdef __cplusplus
      enumerator->Release();
#else
      enumerator->lpVtbl->Release(enumerator);
#endif
   collection = NULL;
   enumerator = NULL;

   return device;

error:
   if (collection)
#ifdef __cplusplus
      collection->Release();
#else
      collection->lpVtbl->Release(collection);
#endif
   if (enumerator)
#ifdef __cplusplus
      enumerator->Release();
#else
      enumerator->lpVtbl->Release(enumerator);
#endif
   collection = NULL;
   enumerator = NULL;

   if (id)
      RARCH_WARN("[WASAPI] Failed to initialize %s device \"%s\".\n", data_flow_name, id);
   else
      RARCH_ERR("[WASAPI] Failed to initialize default %s device.\n", data_flow_name);

   return NULL;
}

static IAudioClient *wasapi_init_client(IMMDevice *device, bool *exclusive,
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

   /* Remaining calls are for logging purposes. */

   hr = _IAudioClient_GetDevicePeriod(client, &device_period, &device_period_min);
   if (SUCCEEDED(hr))
   {
      RARCH_DBG("[WASAPI] Default device period is %.1fms.\n", (float)device_period * 100 / 1e6);
      RARCH_DBG("[WASAPI] Minimum device period is %.1fms.\n", (float)device_period_min * 100 / 1e6);
   }
   else
      RARCH_WARN("[WASAPI] IAudioClient::GetDevicePeriod failed: %s.\n", hresult_name(hr));

   if (!*exclusive)
   {
      hr = _IAudioClient_GetStreamLatency(client, &stream_latency);
      if (SUCCEEDED(hr))
         RARCH_DBG("[WASAPI] Shared stream latency is %.1fms.\n", (float)stream_latency * 100 / 1e6);
      else
         RARCH_WARN("[WASAPI] IAudioClient::GetStreamLatency failed: %s.\n", hresult_name(hr));
   }

   hr = _IAudioClient_GetBufferSize(client, &buffer_length);
   if (SUCCEEDED(hr))
   {
      size_t num_samples = buffer_length * channels;
      size_t num_bytes = num_samples * (*float_fmt ? sizeof(float) : sizeof(int16_t));
      RARCH_DBG("[WASAPI] Endpoint buffer size is %u frames (%u samples, %u bytes, %.1f ms).\n",
            buffer_length, num_samples, num_bytes, (float)buffer_length * 1000.0 / *rate);
   }
   else
      RARCH_WARN("[WASAPI] IAudioClient::GetBufferSize failed: %s.\n", hresult_name(hr));

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

   RARCH_LOG("[WASAPI] Client initialized (%s, %s, %uHz, %.1fms).\n",
         *exclusive ? "exclusive" : "shared",
         *float_fmt ? "FLOAT" : "PCM",
         *rate, latency_res);

   return client;
}

#ifdef HAVE_MICROPHONE
typedef struct
{
   HANDLE              read_event;
   IMMDevice           *device;
   char                *device_name;
   IAudioClient        *client;
   IAudioCaptureClient *capture;

   /**
    * The buffer in which samples from the microphone will be read and stored
    * until the frontend fetches them.
    */
   fifo_buffer_t       *buffer;

   /**
    * The size of an audio frame, in bytes.
    * Mic input is in one channel with either 16-bit ints or 32-bit floats,
    * so this will be 2 or 4.
    */
   size_t frame_size;
   size_t engine_buffer_size;
   bool exclusive;
   bool running;
} wasapi_microphone_handle_t;

typedef struct wasapi_microphone
{
   bool nonblock;
} wasapi_microphone_t;

static void wasapi_microphone_close_mic(void *driver_context, void *mic_context)
{
   DWORD ir;
   HANDLE write_event;
   wasapi_microphone_t     *wasapi = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;

   if (!wasapi || !mic)
      return;

   write_event = mic->read_event;

   if (mic->capture)
#ifdef __cplusplus
      mic->capture->Release();
#else
      mic->capture->lpVtbl->Release(mic->capture);
#endif
   mic->capture = NULL;
   if (mic->client)
   {
      _IAudioClient_Stop(mic->client);
#ifdef __cplusplus
      mic->client->Release();
#else
      mic->client->lpVtbl->Release(mic->client);
#endif
   }
   if (mic->device)
   {
#ifdef __cplusplus
      mic->device->Release();
#else
      mic->device->lpVtbl->Release(mic->device);
#endif
   }
   mic->client = NULL;
   mic->device = NULL;
   if (mic->buffer)
      fifo_free(mic->buffer);
   if (mic->device_name)
      free(mic->device_name);
   free(mic);

   ir = WaitForSingleObject(write_event, 20);
   if (ir == WAIT_FAILED)
   {
      RARCH_ERR("[WASAPI mic] WaitForSingleObject failed: %s.\n", wasapi_error(GetLastError()));
   }

   /* If event isn't signaled log and leak */
   if (ir == WAIT_OBJECT_0)
      CloseHandle(write_event);
}

static void *wasapi_microphone_init(void)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)calloc(1, sizeof(wasapi_microphone_t));
   if (!wasapi)
   {
      RARCH_ERR("[WASAPI mic] Failed to allocate microphone driver context.\n");
      return NULL;
   }
   wasapi->nonblock = !config_get_ptr()->bools.audio_sync;
   RARCH_DBG("[WASAPI mic] Initialized microphone driver context.\n");
   return wasapi;
}

static void wasapi_microphone_free(void *driver_context)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;
   if (wasapi)
      free(wasapi);
}

/**
 * Flushes microphone's most recent input to the provided context's FIFO queue.
 * WASAPI requires that fetched input be consumed in its entirety,
 * so the returned value may be less than the queue's size
 * if the next packet won't fit in it.
 * @param microphone Pointer to the microphone context.
 * @return The number of bytes in the queue after fetching input,
 * or -1 if there was an error.
 */
static int wasapi_microphone_fetch_fifo(wasapi_microphone_handle_t *mic)
{
   UINT32 next_packet_size = 0;
   /* Shared-mode capture streams split their input buffer into multiple packets,
    * while exclusive-mode capture streams just use the one.
    *
    * The following loop will run at least once;
    * for exclusive-mode streams, that's all that we'll need.
    */

   do
   {
      BYTE *mic_input           = NULL;
      UINT32 frames_read        = 0;
      UINT32 bytes_read         = 0;
      DWORD buffer_status_flags = 0;
      HRESULT hr = _IAudioCaptureClient_GetBuffer(mic->capture,
            &mic_input, &frames_read, &buffer_status_flags, NULL, NULL);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to get capture device \"%s\"'s buffer: %s.\n",
            mic->device_name, hresult_name(hr));
         return -1;
      }
      bytes_read = frames_read * mic->frame_size;

      /* If the queue has room for the packets we just got,
       * then enqueue the bytes directly from the mic's buffer */
      if (FIFO_WRITE_AVAIL(mic->buffer) >= bytes_read && bytes_read > 0)
         fifo_write(mic->buffer, mic_input, bytes_read);
      else /* Not enough space for new frames, so we can't consume this packet right now */
         frames_read = 0;
      /* If there's insufficient room in the queue, then we can't read the packet.
       * In that case, we leave the packet for next time. */

      hr = _IAudioCaptureClient_ReleaseBuffer(mic->capture, frames_read);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to release capture device \"%s\"'s buffer after consuming %u frames: %s.\n",
            mic->device_name, frames_read, hresult_name(hr));
         return -1;
      }

      /* If this is a shared-mode stream and
       * we didn't run out of room in the sample queue... */
      if (!mic->exclusive && frames_read > 0)
      {
         hr = _IAudioCaptureClient_GetNextPacketSize(mic->capture, &next_packet_size);
         /* Get the number of frames that the mic has for us. */
         if (FAILED(hr))
         {
            RARCH_ERR("[WASAPI] Failed to get capture device \"%s\"'s next packet size: %s.\n",
                      mic->device_name, hresult_name(hr));
            return -1;
         }
      }
      /* Exclusive-mode streams only deliver one packet at a time, though it's bigger. */
      else
         next_packet_size = 0;
   }
   while (next_packet_size != 0);

   return FIFO_READ_AVAIL(mic->buffer);
}

/**
 * Blocks until the provided microphone's capture event is signalled.
 *
 * @param microphone The microphone to wait on.
 * @param timeout The amount of time to wait, in milliseconds.
 * @return \c true if the event was signalled,
 * \c false if it timed out or there was an error.
 */
static bool wasapi_microphone_wait_for_capture_event(wasapi_microphone_handle_t *mic, DWORD timeout)
{
   /*...then let's wait for the mic to tell us that samples are ready. */
   switch (WaitForSingleObject(mic->read_event, timeout))
   {
      case WAIT_OBJECT_0:
         /* Okay, there's data available. */
         return true;
      case WAIT_TIMEOUT:
         /* Time out; there's nothing here for us. */
         RARCH_ERR("[WASAPI] Failed to wait for capture device \"%s\" event: Timeout after %ums.\n", mic->device_name, timeout);
         break;
      default:
         RARCH_ERR("[WASAPI] Failed to wait for capture device \"%s\" event: %s.\n", mic->device_name, wasapi_error(GetLastError()));
         break;
   }
   return false;
}

/**
 * Reads samples from a microphone,
 * fetching more from it if necessary.
 * Works for exclusive and shared-mode streams.
 *
 * @param microphone Pointer to the context of the microphone
 * from which samples will be read.
 * @param buffer The buffer in which the fetched samples will be stored.
 * @param buffer_size The size of buffer, in bytes.
 * @param timeout Timeout for new samples, in milliseconds.
 * 0 means that this function won't wait for new samples,
 * \c INFINITE means that this function will wait indefinitely.
 * @return The number of samples that were retrieved,
 * or -1 if there was an error (including timeout).
 */
static int wasapi_microphone_read_buffered(
   wasapi_microphone_handle_t *mic, void *s, size_t len,
   DWORD timeout)
{
   int bytes_read      = 0; /* Number of bytes sent to the core */
   int bytes_available = FIFO_READ_AVAIL(mic->buffer);

   /* If we don't have any queued samples to give to the core... */
   if (!bytes_available)
   {
      /* If we couldn't wait for the microphone to signal a capture event... */
      if (!wasapi_microphone_wait_for_capture_event(mic, timeout))
         return -1;

      bytes_available = wasapi_microphone_fetch_fifo(mic);
      /* If we couldn't fetch samples from the microphone... */
      if (bytes_available < 0)
         return -1;
   }

   /* Now that we have samples available, let's give them to the core */

   bytes_read = MIN((int)len, bytes_available);
   fifo_read(mic->buffer, s, bytes_read);
   /* Read data from the sample queue and store it in the provided buffer */
   return bytes_read;
}

static int wasapi_microphone_read(void *driver_context, void *mic_context, void *s, size_t len)
{
   int bytes_read = 0;
   wasapi_microphone_t     *wasapi = (wasapi_microphone_t *)driver_context;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;

   if (!wasapi || !mic || !s)
      return -1;

   /* If microphones shouldn't block... */
   if (wasapi->nonblock)
      return wasapi_microphone_read_buffered(mic, s, len, 0);

   if (mic->exclusive)
   {
      int read;
      for (read = -1; (size_t)bytes_read < len; bytes_read += read)
      {
         read = wasapi_microphone_read_buffered(mic,
               (char *)s   + bytes_read,
               len         - bytes_read,
               INFINITE);
         if (read == -1)
            return -1;
      }
   }
   else
   {
      int read;
      for (read = -1; (size_t)bytes_read < len; bytes_read += read)
      {
         read = wasapi_microphone_read_buffered(mic,
               (char *)s   + bytes_read,
               len         - bytes_read,
               INFINITE);
         if (read == -1)
            return -1;
      }
   }

   return bytes_read;
}

static void wasapi_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;
   wasapi->nonblock = nonblock;
}

static void *wasapi_microphone_open_mic(void *driver_context, const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   HRESULT hr;
   settings_t *settings            = config_get_ptr();
   DWORD flags                     = 0;
   UINT32 frame_count              = 0;
   REFERENCE_TIME dev_period       = 0;
   BYTE *dest                      = NULL;
   bool float_format               = settings->bools.microphone_wasapi_float_format;
   bool exclusive_mode             = settings->bools.microphone_wasapi_exclusive_mode;
   unsigned sh_buffer_length       = settings->uints.microphone_wasapi_sh_buffer_length;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)calloc(
         1, sizeof(wasapi_microphone_handle_t));

   if (!mic)
      return NULL;

   mic->exclusive         = exclusive_mode;
   mic->device            = wasapi_init_device(device, 1 /* eCapture */);

   /* If we requested a particular capture device, but couldn't open it... */
   if (device && !mic->device)
   {
      RARCH_WARN("[WASAPI] Failed to open requested capture device \"%s\", attempting to open default device.\n", device);
      mic->device = wasapi_init_device(NULL, 1 /* eCapture */);
   }

   if (!mic->device)
   {
      RARCH_ERR("[WASAPI] Failed to open capture device.\n");
      goto error;
   }

   if (!(mic->device_name = mmdevice_name(mic->device)))
   {
      RARCH_ERR("[WASAPI] Failed to get friendly name of capture device.\n");
      goto error;
   }

   mic->client = wasapi_init_client(mic->device,
      &mic->exclusive, &float_format, &rate, latency, 1);
   if (!mic->client)
   {
      RARCH_ERR("[WASAPI] Failed to open client for capture device \"%s\".\n", mic->device_name);
      goto error;
   }

   hr = _IAudioClient_GetBufferSize(mic->client, &frame_count);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get buffer size of IAudioClient for capture device \"%s\": %s.\n",
          mic->device_name, hresult_name(hr));
      goto error;
   }

   mic->frame_size         = float_format ? sizeof(float) : sizeof(int16_t);
   mic->engine_buffer_size = frame_count * mic->frame_size;

   /* If this mic should be used *exclusively* by RetroArch... */
   if (mic->exclusive)
   {
      mic->buffer = fifo_new(mic->engine_buffer_size);
      if (!mic->buffer)
      {
         RARCH_ERR("[WASAPI] Failed to initialize FIFO queue for capture device.\n");
         goto error;
      }

      RARCH_LOG("[WASAPI] Intermediate exclusive-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                frame_count, (double)frame_count * 1000.0 / rate, mic->engine_buffer_size);
   }
   else
   {
      /* If the user selected the "default" shared buffer length... */
      if (sh_buffer_length <= 0)
      {
         hr = _IAudioClient_GetDevicePeriod(mic->client, &dev_period, NULL);
         if (FAILED(hr))
            goto error;

         sh_buffer_length = (dev_period * rate / 10000000) * 2;
         /* Default buffer seems to be too small, resulting in slowdown.
          * Doubling it seems to work okay. Dunno why. */
      }

      mic->buffer = fifo_new(sh_buffer_length * mic->frame_size);
      if (!mic->buffer)
         goto error;

      RARCH_LOG("[WASAPI] Intermediate shared-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                sh_buffer_length, (double)sh_buffer_length * 1000.0 / rate, sh_buffer_length * mic->frame_size);
   }

   if (!(mic->read_event = CreateEventA(NULL, FALSE, FALSE, NULL)))
   {
      RARCH_ERR("[WASAPI] Failed to allocate capture device's event handle.\n");
      goto error;
   }

   hr = _IAudioClient_SetEventHandle(mic->client, mic->read_event);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to set capture device's event handle: %s.\n", hresult_name(hr));
      goto error;
   }

   hr = _IAudioClient_GetService(mic->client,
         IID_IAudioCaptureClient, (void**)&mic->capture);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get capture device's IAudioCaptureClient service: %s.\n", hresult_name(hr));
      goto error;
   }

   /* Get and release the buffer, just to ensure that we can. */
   hr = _IAudioCaptureClient_GetBuffer(mic->capture, &dest, &frame_count, &flags, NULL, NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get capture client buffer: %s.\n", hresult_name(hr));
      goto error;
   }

   hr = _IAudioCaptureClient_ReleaseBuffer(mic->capture, 0);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to release capture client buffer: %s.\n", hresult_name(hr));
      goto error;
   }

   /* The rate was (possibly) modified when we initialized the client */
   if (new_rate)
      *new_rate = rate;
   return mic;

error:
   if (mic->capture)
#ifdef __cplusplus
      mic->capture->Release();
#else
      mic->capture->lpVtbl->Release(mic->capture);
#endif
   mic->capture = NULL;
   if (mic->client)
   {
#ifdef __cplusplus
      mic->client->Release();
#else
      mic->client->lpVtbl->Release(mic->client);
#endif
   }
   if (mic->device)
   {
#ifdef __cplusplus
      mic->device->Release();
#else
      mic->device->lpVtbl->Release(mic->device);
#endif
   }
   mic->client = NULL;
   mic->device = NULL;
   if (mic->read_event)
      CloseHandle(mic->read_event);
   if (mic->buffer)
      fifo_free(mic->buffer);
   if (mic->device_name)
      free(mic->device_name);
   free(mic);
   return NULL;
}

static bool wasapi_microphone_start_mic(void *driver_context, void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;
   HRESULT hr;
   if (!mic)
      return false;
   hr = _IAudioClient_Start(mic->client);

   /* Starting an already-active microphone is not an error */
   if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED)
      mic->running = true;
   else
   {
      RARCH_ERR("[WASAPI mic] Failed to start capture device \"%s\"'s IAudioClient: %s.\n",
         mic->device_name, hresult_name(hr));
      mic->running = false;
   }
   return mic->running;
}

static bool wasapi_microphone_stop_mic(void *driver_context, void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;
   HRESULT hr;
   if (!mic)
      return false;
   hr = _IAudioClient_Stop(mic->client);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI mic] Failed to stop capture device \"%s\"'s IAudioClient: %s.\n",
         mic->device_name, hresult_name(hr));
      return false;
   }
   RARCH_LOG("[WASAPI mic] Stopped capture device \"%s\".\n", mic->device_name);
   mic->running = false;
   return true;
}

static bool wasapi_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t *)mic_context;
   return mic && mic->running;
}

static struct string_list *wasapi_microphone_device_list_new(const void *driver_context)
{
   return mmdevice_list_new(driver_context, 1 /* eCapture */);
}

static void wasapi_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
   struct string_list *sl = (struct string_list*)devices;
   if (sl)
      string_list_free(sl);
}

static bool wasapi_microphone_use_float(const void *driver_context, const void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t *)mic_context;
   return (mic && (mic->frame_size == sizeof(float)));
}

microphone_driver_t microphone_wasapi = {
      wasapi_microphone_init,
      wasapi_microphone_free,
      wasapi_microphone_read,
      wasapi_microphone_set_nonblock_state,
      "wasapi",
      wasapi_microphone_device_list_new,
      wasapi_microphone_device_list_free,
      wasapi_microphone_open_mic,
      wasapi_microphone_close_mic,
      wasapi_microphone_mic_alive,
      wasapi_microphone_start_mic,
      wasapi_microphone_stop_mic,
      wasapi_microphone_use_float
};
#endif

static void *wasapi_init(const char *dev_id, unsigned rate, unsigned latency,
      unsigned u1, unsigned *new_rate)
{
   HRESULT hr;
   UINT32 frame_count        = 0;
   REFERENCE_TIME dev_period = 0;
   BYTE *dest                = NULL;
   settings_t *settings      = config_get_ptr();
   bool float_format         = settings->bools.audio_wasapi_float_format;
   bool exclusive_mode       = settings->bools.audio_wasapi_exclusive_mode;
   bool audio_sync           = settings->bools.audio_sync;
   unsigned sh_buffer_length = settings->uints.audio_wasapi_sh_buffer_length;
   wasapi_t *w               = (wasapi_t*)calloc(1, sizeof(wasapi_t));

   if (!w)
      return NULL;

   w->device                 = wasapi_init_device(dev_id, 0 /* eRender */);
   if (!w->device && dev_id)
      w->device = wasapi_init_device(NULL, 0 /* eRender */);
   if (!w->device)
      goto error;

   if (!(w->client = wasapi_init_client(w->device,
         &exclusive_mode, &float_format, &rate, latency, 2)))
      goto error;
   if (exclusive_mode)
      w->flags              |= WASAPI_FLG_EXCLUSIVE;

   hr = _IAudioClient_GetBufferSize(w->client, &frame_count);
   if (FAILED(hr))
      goto error;

   w->frame_size             = float_format ? 8 : 4;
   w->engine_buffer_size     = frame_count * w->frame_size;

   if ((w->flags & WASAPI_FLG_EXCLUSIVE) > 0)
   {
      if (!(w->buffer = fifo_new(w->engine_buffer_size)))
         goto error;
   }
   else
   {
      switch (sh_buffer_length)
      {
         case WASAPI_SH_BUFFER_AUDIO_LATENCY:
         case WASAPI_SH_BUFFER_CLIENT_BUFFER:
            sh_buffer_length = frame_count;
            break;
         case WASAPI_SH_BUFFER_DEVICE_PERIOD:
            hr = _IAudioClient_GetDevicePeriod(w->client, &dev_period, NULL);

            if (FAILED(hr))
               goto error;

            sh_buffer_length = dev_period * rate / 10000000;
            break;
         default:
            break;
      }

      if (!(w->buffer = fifo_new(sh_buffer_length * w->frame_size)))
         goto error;
   }

   if (!(w->write_event = CreateEventA(NULL, FALSE, FALSE, NULL)))
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

   w->flags    |=   WASAPI_FLG_RUNNING;
   if (audio_sync)
      w->flags &= ~(WASAPI_FLG_NONBLOCK);
   else
      w->flags |=  (WASAPI_FLG_NONBLOCK);

   if (new_rate)
      *new_rate = rate;

   return w;

error:
   if (w->renderer)
#ifdef __cplusplus
      w->renderer->Release();
#else
      w->renderer->lpVtbl->Release(w->renderer);
#endif
   w->renderer = NULL;
   if (w->client)
   {
#ifdef __cplusplus
      w->client->Release();
#else
      w->client->lpVtbl->Release(w->client);
#endif
   }
   if (w->device)
   {
#ifdef __cplusplus
      w->device->Release();
#else
      w->device->lpVtbl->Release(w->device);
#endif
   }
   w->client = NULL;
   w->device = NULL;
   if (w->write_event)
      CloseHandle(w->write_event);
   if (w->buffer)
      fifo_free(w->buffer);
   free(w);

   return NULL;
}

static ssize_t wasapi_write(void *wh, const void *data, size_t len)
{
   size_t written = 0;
   wasapi_t *w    = (wasapi_t*)wh;
   uint8_t flg    = w->flags;

   if (!((flg & WASAPI_FLG_RUNNING) > 0))
      return -1;

   if ((flg & WASAPI_FLG_EXCLUSIVE) > 0)
   {
      if ((flg & WASAPI_FLG_NONBLOCK) > 0)
      {
         size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
         if (!write_avail)
         {
            UINT32 frame_count;
            BYTE *dest         = NULL;
            if (WaitForSingleObject(w->write_event, 0) != WAIT_OBJECT_0)
               return 0;
            frame_count        = w->engine_buffer_size / w->frame_size;
            if (FAILED(_IAudioRenderClient_GetBuffer(
                        w->renderer, frame_count, &dest)))
               return -1;
            fifo_read(w->buffer, dest, w->engine_buffer_size);
            if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                        w->renderer, frame_count, 0)))
               return -1;
            write_avail = w->engine_buffer_size;
         }
         written = (len < write_avail) ? len : write_avail;
         fifo_write(w->buffer, data, written);
      }
      else
      {
         ssize_t ir;
         for (ir = -1; written < len; written += ir)
         {
            const void *_data  = (char*)data + written;
            size_t __len       = len - written;
            size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
            if (!write_avail)
            {
               BYTE *dest         = NULL;
               if (!WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) != WAIT_OBJECT_0)
               {
                  UINT32 frame_count = w->engine_buffer_size / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  fifo_read(w->buffer, dest, w->engine_buffer_size);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
                  write_avail = w->engine_buffer_size;
               }
            }
            ir = (__len < write_avail) ? __len : write_avail;
            fifo_write(w->buffer, _data, ir);
         }
      }
   }
   else
   {
      if ((flg & WASAPI_FLG_NONBLOCK) > 0)
      {
         size_t write_avail = 0;
         UINT32 padding     = 0;
         if (w->buffer)
         {
            if (!FIFO_WRITE_AVAIL(w->buffer))
            {
               size_t read_avail = 0;
               if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
                  return -1;
               read_avail  = FIFO_READ_AVAIL(w->buffer);
               write_avail = w->engine_buffer_size - padding * w->frame_size;
               written     = read_avail < write_avail ? read_avail : write_avail;
               if (written)
               {
                  BYTE *dest         = NULL;
                  UINT32 frame_count = written / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  fifo_read(w->buffer, dest, written);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
               }
            }
            write_avail = FIFO_WRITE_AVAIL(w->buffer);
            written     = len < write_avail ? len : write_avail;
            if (written)
               fifo_write(w->buffer, data, written);
         }
         else
         {
            if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
               return -1;
            if (!(write_avail = w->engine_buffer_size - padding * w->frame_size))
               return 0;
            written = (len < write_avail) ? len : write_avail;
            if (written)
            {
               BYTE *dest         = NULL;
               UINT32 frame_count = written / w->frame_size;
               if (FAILED(_IAudioRenderClient_GetBuffer(
                           w->renderer, frame_count, &dest)))
                  return -1;
               memcpy(dest, data, written);
               if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                           w->renderer, frame_count, 0)))
                  return -1;
            }
         }
      }
      else if (w->buffer)
      {
         ssize_t ir;
         for (; written < len; written += ir)
         {
            const void *_data  = (char*)data + written;
            size_t _len        = len - written;
            size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
            UINT32 padding     = 0;
            if (!write_avail)
            {
               size_t read_avail = 0;
               if (!(WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) == WAIT_OBJECT_0))
                  return -1;
               if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
                  return -1;
               read_avail  = FIFO_READ_AVAIL(w->buffer);
               write_avail = w->engine_buffer_size - padding * w->frame_size;
               ir          = read_avail < write_avail ? read_avail : write_avail;
               if (ir)
               {
                  BYTE *dest         = NULL;
                  UINT32 frame_count = ir / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  fifo_read(w->buffer, dest, ir);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
               }
            }
            write_avail = FIFO_WRITE_AVAIL(w->buffer);
            ir          = (_len < write_avail) ? _len : write_avail;
            if (ir)
               fifo_write(w->buffer, _data, ir);
         }
      }
      else
      {
         ssize_t ir;
         for (; written < len; written += ir)
         {
            const void *_data  = (char*)data + written;
            size_t _len        = len - written;
            size_t write_avail = 0;
            UINT32 padding     = 0;
            if (!(WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) == WAIT_OBJECT_0))
               return -1;
            if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
               return -1;
            if (!(write_avail = w->engine_buffer_size - padding * w->frame_size))
               ir = 0;
            else
            {
               ir = (_len < write_avail) ? _len : write_avail;
               if (ir)
               {
                  BYTE *dest         = NULL;
                  UINT32 frame_count = ir / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  memcpy(dest, _data, ir);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
               }
            }
         }
      }
   }

   return written;
}

static bool wasapi_stop(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (FAILED(_IAudioClient_Stop(w->client)))
      return (!(w->flags & WASAPI_FLG_RUNNING));

   w->flags  &= ~(WASAPI_FLG_RUNNING);

   return true;
}

static bool wasapi_start(void *wh, bool u)
{
   wasapi_t *w = (wasapi_t*)wh;
   HRESULT  hr = _IAudioClient_Start(w->client);
   if (hr != AUDCLNT_E_NOT_STOPPED)
   {
      if (FAILED(hr))
         return ((w->flags & WASAPI_FLG_RUNNING) > 0);
      w->flags  |= (WASAPI_FLG_RUNNING);
   }
   return true;
}

static bool wasapi_alive(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   return ((w->flags & WASAPI_FLG_RUNNING) > 0);
}

static void wasapi_set_nonblock_state(void *wh, bool nonblock)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (nonblock)
      w->flags |=  WASAPI_FLG_NONBLOCK;
   else
      w->flags &= ~WASAPI_FLG_NONBLOCK;
}

static void wasapi_free(void *wh)
{
   DWORD ir;
   wasapi_t *w        = (wasapi_t*)wh;
   HANDLE write_event = w->write_event;

   if (w->renderer)
#ifdef __cplusplus
      w->renderer->Release();
#else
      w->renderer->lpVtbl->Release(w->renderer);
#endif
   w->renderer = NULL;
   if (w->client)
   {
      _IAudioClient_Stop(w->client);
#ifdef __cplusplus
      w->client->Release();
#else
      w->client->lpVtbl->Release(w->client);
#endif
   }
   if (w->device)
   {
#ifdef __cplusplus
      w->device->Release();
#else
      w->device->lpVtbl->Release(w->device);
#endif
   }
   w->client = NULL;
   w->device = NULL;
   if (w->buffer)
      fifo_free(w->buffer);
   free(w);

   ir = WaitForSingleObject(write_event, 20);
   if (ir == WAIT_FAILED)
      RARCH_ERR("[WASAPI] WaitForSingleObject failed with error %d.\n", GetLastError());

   if (!(ir == WAIT_OBJECT_0))
      return;

   CloseHandle(write_event);
}

static bool wasapi_use_float(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   return (w->frame_size == 8);
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

   if (w->flags & WASAPI_FLG_EXCLUSIVE && w->buffer)
      return FIFO_WRITE_AVAIL(w->buffer);
   if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
      return 0;
   if (w->buffer) /* Exaggerate available size for best results.. */
      return FIFO_WRITE_AVAIL(w->buffer) + padding;
   return w->engine_buffer_size - padding * w->frame_size;
}

static size_t wasapi_buffer_size(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (w->buffer)
      return w->buffer->size;

   return w->engine_buffer_size;
}

static void *wasapi_device_list_new(void *u)
{
   return mmdevice_list_new(u, 0 /* eRender */);
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
