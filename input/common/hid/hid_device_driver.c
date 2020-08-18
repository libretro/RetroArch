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

#include "hid_device_driver.h"

hid_driver_instance_t hid_instance = {0};

hid_device_t *hid_device_list[]    = 
{
   &wiiu_gca_hid_device,
   &ds3_hid_device,
#if 0
   &ds4_hid_device,
#endif
   NULL /* must be last entry in list */
};

hid_device_t *hid_device_driver_lookup(
      uint16_t vendor_id, uint16_t product_id)
{
   int i = 0;

   for (i = 0; hid_device_list[i] != NULL; i++)
   {
      if (hid_device_list[i]->detect(vendor_id, product_id))
         return hid_device_list[i];
   }

   return NULL;
}

joypad_connection_t *hid_pad_register(
      void *pad_handle, pad_connection_interface_t *iface)
{
   int slot;
   joypad_connection_t *result;

   if (!pad_handle)
      return NULL;

   slot = pad_connection_find_vacant_pad(hid_instance.pad_list);

   if (slot < 0)
   {
      RARCH_ERR("[hid]: failed to find a vacant pad.\n");
      return NULL;
   }

   result            = &(hid_instance.pad_list[slot]);
   result->iface     = iface;
   result->data      = iface->init(pad_handle, slot, hid_instance.os_driver);
   result->connected = true;
   input_pad_connect(slot, hid_instance.pad_driver);

   return result;
}

void hid_pad_deregister(joypad_connection_t *pad)
{
   if (!pad)
      return;

   if (pad->data)
   {
      pad->iface->deinit(pad->data);
      pad->data   = NULL;
   }

   pad->iface     = NULL;
   pad->connected = false;
}

static bool init_pad_list(hid_driver_instance_t *instance, unsigned slots)
{
   if (!instance || slots > MAX_USERS)
      return false;

   if (instance->pad_list)
      return true;

   /* Initializing pad list.. */
   instance->pad_list = pad_connection_init(slots);
   if (!instance->pad_list)
      return false;

   instance->max_slot = slots;

   return true;
}

/**
 * Fill in instance with data from initialized hid subsystem.
 *
 * @argument instance the hid_driver_instance_t struct to fill in
 * @argument hid_driver the HID driver to initialize
 * @argument pad_driver the gamepad driver to handle HID pads 
 * detected by the HID driver.
 *
 * @returns true if init is successful, false otherwise.
 */
bool hid_init(hid_driver_instance_t *instance,
              hid_driver_t *hid_driver,
              input_device_driver_t *pad_driver,
              unsigned slots)
{
   void *os_driver_data = NULL;
   if (!instance || !hid_driver || !pad_driver || slots > MAX_USERS)
      return false;

   /* Initializing HID subsystem driver */
   os_driver_data = hid_driver->init();
   if (!os_driver_data)
      return false;

   if (!init_pad_list(instance, slots))
   {
      hid_driver->free(os_driver_data);
      return false;
   }

   instance->os_driver_data = os_driver_data;
   instance->os_driver      = hid_driver;
   instance->pad_driver     = pad_driver;

   return true;
}

/**
 * Tear down the HID system set up by hid_init()
 *
 * @argument instance the hid_driver_instance_t to tear down.
 */
void hid_deinit(hid_driver_instance_t *instance)
{
   if (!instance)
      return;

   if (instance->os_driver && instance->os_driver_data)
      instance->os_driver->free(instance->os_driver_data);

   /* Destroying pad data.. */
   pad_connection_destroy(instance->pad_list);

   /* Wiping instance data.. */
   memset(instance, 0, sizeof(hid_driver_instance_t));
}
