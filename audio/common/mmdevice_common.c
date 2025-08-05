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

#include <encodings/utf.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "mmdevice_common.h"
#include "mmdevice_common_inline.h"

#include "../../verbosity.h"

static const char* mmdevice_data_flow_name(unsigned data_flow)
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

const char *mmdevice_hresult_name(int hr)
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

size_t mmdevice_samplerate(void *data)
{
   HRESULT hr;
   PWAVEFORMATEX devfmt_props;
   PROPVARIANT prop_var;
   IMMDevice *device          = (IMMDevice*)data;
   IPropertyStore *prop_store = NULL;
   DWORD result               = 0;

   if (!device)
      return 0;

   hr = _IMMDevice_OpenPropertyStore(device,
         STGM_READ, &prop_store);

   if (FAILED(hr))
      return 0;

   PropVariantInit(&prop_var);
   hr = _IPropertyStore_GetValue(prop_store,
         PKEY_AudioEngine_DeviceFormat, &prop_var);
   if (SUCCEEDED(hr))
   {
      devfmt_props = (PWAVEFORMATEX)prop_var.blob.pBlobData;
      result       = devfmt_props->nSamplesPerSec;
   }

   PropVariantClear(&prop_var);
   if (prop_store)
   {
#ifdef __cplusplus
      prop_store->Release();
#else
      prop_store->lpVtbl->Release(prop_store);
#endif
      prop_store = NULL;
   }
   return (size_t)result;
}

char *mmdevice_name(void *data)
{
   HRESULT hr;
   PROPVARIANT prop_var;
   IMMDevice *device          = (IMMDevice*)data;
   IPropertyStore *prop_store = NULL;
   char* result               = NULL;

   if (!device)
      return NULL;

   hr = _IMMDevice_OpenPropertyStore(device,
         STGM_READ, &prop_store);

   if (FAILED(hr))
      return NULL;

   PropVariantInit(&prop_var);
   hr = _IPropertyStore_GetValue(prop_store,
         PKEY_Device_FriendlyName, &prop_var);
   if (SUCCEEDED(hr))
      result = utf16_to_utf8_string_alloc(prop_var.pwszVal);

   PropVariantClear(&prop_var);
   if (prop_store)
   {
#ifdef __cplusplus
      prop_store->Release();
#else
      prop_store->lpVtbl->Release(prop_store);
#endif
      prop_store = NULL;
   }
   return result;
}

void *mmdevice_handle(int id, unsigned data_flow)
{
   HRESULT hr;
   IMMDeviceEnumerator *enumerator = NULL;
   IMMDevice *device               = NULL;
   IMMDeviceCollection *collection = NULL;
#ifdef __cplusplus
   hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         IID_IMMDeviceEnumerator, (void **)&enumerator);
#else
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
#endif
   if (FAILED(hr))
      return NULL;
   hr = _IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,
         data_flow, DEVICE_STATE_ACTIVE, &collection);
   if (FAILED(hr))
   {
      RARCH_ERR("[MMDevice] Failed to enumerate audio endpoints: %s.\n", mmdevice_hresult_name(hr));
      goto error;
   }

   hr = _IMMDeviceCollection_Item(collection, id, &device);
   if (FAILED(hr))
   {
      RARCH_ERR("[MMDevice] Failed to get IMMDevice #%d: %s.\n", id, mmdevice_hresult_name(hr));
      goto error;
   }

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

   return NULL;
}

size_t mmdevice_get_samplerate(int id)
{
   IMMDevice *device = (IMMDevice*)mmdevice_handle(id, 0 /* eRender */);
   if (device)
   {
      size_t _len = mmdevice_samplerate(device);
#ifdef __cplusplus
      device->Release();
#else
      device->lpVtbl->Release(device);
#endif
      return _len;
   }
   return 0;
}

void *mmdevice_init_device(const char *id, unsigned data_flow)
{
   HRESULT hr;
   UINT32 dev_count, i;
   IMMDeviceEnumerator *enumerator = NULL;
   IMMDevice *device               = NULL;
   IMMDeviceCollection *collection = NULL;
   const char *data_flow_name      = mmdevice_data_flow_name(data_flow);

   if (id)
      RARCH_DBG("[MMDevice] Initializing %s device \"%s\"...\n", data_flow_name, id);
   else
      RARCH_DBG("[MMDevice] Initializing default %s device...\n", data_flow_name);

#ifdef __cplusplus
   hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         IID_IMMDeviceEnumerator, (void **)&enumerator);
#else
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
#endif
   if (FAILED(hr))
   {
      RARCH_ERR("[MMDevice] Failed to create device enumerator: %s.\n", mmdevice_hresult_name(hr));
      goto error;
   }

   if (id)
   {
      /* If a specific device was requested... */
      int32_t idx_found        = -1;
      struct string_list *list = (struct string_list*)mmdevice_list_new(NULL, data_flow);

      if (!list)
      {
         RARCH_ERR("[MMDevice] Failed to allocate %s device list.\n", data_flow_name);
         goto error;
      }

      if (list->elems)
      {
         size_t d;
         /* If any devices were found... */
         for (d = 0; d < list->size; d++)
         {
            if (string_is_equal(id, list->elems[d].data))
            {
               RARCH_DBG("[MMDevice] Found device #%d: \"%s\".\n", d,
                     list->elems[d].data);
               idx_found = d;
               break;
            }
         }

         /* Index was not found yet based on name string,
          * just assume id is a one-character number index. */
         if (idx_found == -1 && isdigit(id[0]))
         {
            idx_found = strtoul(id, NULL, 0);
            RARCH_LOG("[MMDevice] Fallback, %s device index is a single number index instead: %u.\n",
                  data_flow_name, idx_found);
         }
      }
      string_list_free(list);

      if (idx_found == -1)
         idx_found = 0;

      hr = _IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,
            data_flow, DEVICE_STATE_ACTIVE, &collection);
      if (FAILED(hr))
      {
         RARCH_ERR("[MMDevice] Failed to enumerate audio endpoints: %s.\n", mmdevice_hresult_name(hr));
         goto error;
      }

      hr = _IMMDeviceCollection_GetCount(collection, &dev_count);
      if (FAILED(hr))
      {
         RARCH_ERR("[MMDevice] Failed to count IMMDevices: %s.\n", mmdevice_hresult_name(hr));
         goto error;
      }

      for (i = 0; i < dev_count; ++i)
      {
         hr = _IMMDeviceCollection_Item(collection, i, &device);
         if (FAILED(hr))
         {
            RARCH_ERR("[MMDevice] Failed to get IMMDevice #%d: %s.\n", i, mmdevice_hresult_name(hr));
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
         RARCH_ERR("[MMDevice] Failed to get default audio endpoint: %s.\n", mmdevice_hresult_name(hr));
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
      RARCH_WARN("[MMDevice] Failed to initialize %s device \"%s\".\n", data_flow_name, id);
   else
      RARCH_ERR("[MMDevice] Failed to initialize default %s device.\n", data_flow_name);

   return NULL;
}

void *mmdevice_list_new(const void *u, unsigned data_flow)
{
   HRESULT hr;
   UINT i;
   union string_list_elem_attr attr;
   IMMDeviceEnumerator *enumerator = NULL;
   IMMDeviceCollection *collection = NULL;
   UINT dev_count                  = 0;
   IMMDevice *device               = NULL;
   LPWSTR dev_id_wstr              = NULL;
   bool br                         = false;
   char *dev_id_str                = NULL;
   char *dev_name_str              = NULL;
   struct string_list *sl          = string_list_new();

   if (!sl)
      return NULL;

   attr.i = 0;
#ifdef __cplusplus
   hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         IID_IMMDeviceEnumerator, (void **)&enumerator);
#else
   hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
         &IID_IMMDeviceEnumerator, (void **)&enumerator);
#endif
   if (FAILED(hr))
      goto error;

   hr = _IMMDeviceEnumerator_EnumAudioEndpoints(enumerator,
         data_flow, DEVICE_STATE_ACTIVE, &collection);
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

      hr = _IMMDevice_GetId(device, &dev_id_wstr);
      if (FAILED(hr))
         goto error;

      if (!(dev_id_str = utf16_to_utf8_string_alloc(dev_id_wstr)))
         goto error;

      if (!(dev_name_str = mmdevice_name(device)))
         goto error;

      br = string_list_append(sl, dev_name_str, attr);
      if (!br)
         goto error;
      if (dev_id_str)
         sl->elems[sl->size-1].userdata = dev_id_str;

      if (dev_id_wstr)
         CoTaskMemFree(dev_id_wstr);
      if (dev_name_str)
         free(dev_name_str);
      dev_name_str = NULL;
      dev_id_wstr  = NULL;
      if (device)
      {
#ifdef __cplusplus
         device->Release();
#else
         device->lpVtbl->Release(device);
#endif
         device = NULL;
      }
   }

   if (collection)
   {
#ifdef __cplusplus
      collection->Release();
#else
      collection->lpVtbl->Release(collection);
#endif
      collection = NULL;
   }
   if (enumerator)
   {
#ifdef __cplusplus
      enumerator->Release();
#else
      enumerator->lpVtbl->Release(enumerator);
#endif
      enumerator = NULL;
   }

   return sl;

error:
   if (dev_id_str)
      free(dev_id_str);
   if (dev_name_str)
      free(dev_name_str);
   dev_id_str   = NULL;
   dev_name_str = NULL;
   if (dev_id_wstr)
      CoTaskMemFree(dev_id_wstr);
   dev_id_wstr = NULL;
   if (device)
   {
#ifdef __cplusplus
      device->Release();
#else
      device->lpVtbl->Release(device);
#endif
      device = NULL;
   }
   if (collection)
   {
#ifdef __cplusplus
      collection->Release();
#else
      collection->lpVtbl->Release(collection);
#endif
      collection = NULL;
   }
   if (enumerator)
   {
#ifdef __cplusplus
      enumerator->Release();
#else
      enumerator->lpVtbl->Release(enumerator);
#endif
      enumerator = NULL;
   }
   if (sl)
      string_list_free(sl);

   return NULL;
}
