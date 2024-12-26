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

#include "mmdevice_common.h"
#include "mmdevice_common_inline.h"

char* mmdevice_name(IMMDevice *device)
{
   HRESULT hr;
   IPropertyStore *prop_store = NULL;
   PROPVARIANT prop_var;
   bool prop_var_init         = false;
   char* result               = NULL;

   if (!device)
      return NULL;

   hr = _IMMDevice_OpenPropertyStore(device, STGM_READ, &prop_store);

   if (FAILED(hr))
      return NULL;

   PropVariantInit(&prop_var);
   prop_var_init = true;
   hr = _IPropertyStore_GetValue(prop_store, PKEY_Device_FriendlyName, &prop_var);
   if (FAILED(hr))
      goto done;

   result = utf16_to_utf8_string_alloc(prop_var.pwszVal);

done:
   if (prop_var_init)
      PropVariantClear(&prop_var);

   IFACE_RELEASE(prop_store);

   return result;
}

void *mmdevice_list_new(const void *u, EDataFlow data_flow)
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
      IFACE_RELEASE(device);
   }

   IFACE_RELEASE(collection);
   IFACE_RELEASE(enumerator);

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
   IFACE_RELEASE(device);
   IFACE_RELEASE(collection);
   IFACE_RELEASE(enumerator);
   if (sl)
      string_list_free(sl);

   return NULL;
}
