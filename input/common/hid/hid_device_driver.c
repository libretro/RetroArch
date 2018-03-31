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

hid_device_t *hid_device_list[] = {
  &wiiu_gca_hid_device,
  &ds3_hid_device,
  &ds4_hid_device,
  NULL /* must be last entry in list */
};

hid_device_t *hid_device_driver_lookup(uint16_t vendor_id, uint16_t product_id) {
  int i = 0;

  for(i = 0; hid_device_list[i] != NULL; i++) {
    if(hid_device_list[i]->detect(vendor_id, product_id))
      return hid_device_list[i];
  }

  return NULL;
}

joypad_connection_t *hid_pad_register(void *pad_handle, pad_connection_interface_t *iface)
{
   int slot;
   joypad_connection_t *result;

   if(!pad_handle)
      return NULL;

   slot = pad_connection_find_vacant_pad(hid_instance.pad_list);
   if(slot < 0)
      return NULL;

   result = &(hid_instance.pad_list[slot]);
   result->iface = iface;
   result->data = iface->init(pad_handle, slot, hid_instance.os_driver);
   result->connected = true;
   input_pad_connect(slot, hid_instance.pad_driver);

   return result;
}

/**
 * Fill in instance with data from initialized hid subsystem.
 *
 * @argument instance the hid_driver_instance_t struct to fill in
 * @argument hid_driver the HID driver to initialize
 * @argument pad_driver the gamepad driver to handle HID pads detected by the HID driver.
 *
 * @returns true if init is successful, false otherwise.
 */
bool hid_init(hid_driver_instance_t *instance,
              hid_driver_t *hid_driver,
              input_device_driver_t *pad_driver,
              unsigned slots)
{
   RARCH_LOG("[hid]: initializing instance with %d pad slots\n", slots);
   if(!instance || !hid_driver || !pad_driver || slots > MAX_USERS)
      return false;

   RARCH_LOG("[hid]: initializing HID subsystem driver...\n");
   instance->os_driver_data = hid_driver->init(instance);
   if(!instance->os_driver_data)
      return false;

   RARCH_LOG("[hid]: initializing pad list...\n");
   instance->pad_list = pad_connection_init(slots);
   if(!instance->pad_list)
   {
      hid_driver->free(instance->os_driver_data);
      instance->os_driver_data = NULL;
      return false;
   }

   instance->max_slot = slots;
   instance->os_driver = hid_driver;
   instance->pad_driver = pad_driver;

   RARCH_LOG("[hid]: instance initialization complete.\n");

   return true;
}

/**
 * Tear down the HID system set up by hid_init()
 *
 * @argument instance the hid_driver_instance_t to tear down.
 */
void hid_deinit(hid_driver_instance_t *instance)
{
   if(!instance)
      return;

   RARCH_LOG("[hid]: destroying instance\n");

   if(instance->os_driver && instance->os_driver_data)
   {
      RARCH_LOG("[hid]: tearing down HID subsystem driver...\n");
      instance->os_driver->free(instance->os_driver_data);
   }

   RARCH_LOG("[hid]: destroying pad data...\n");
   pad_connection_destroy(instance->pad_list);

   RARCH_LOG("[hid]: wiping instance data...\n");
   memset(instance, 0, sizeof(hid_driver_instance_t));
}

static void hid_device_log_buffer(uint8_t *data, uint32_t len)
{
#if 0
  int i, offset;
  int padding = len % 0x0F;
  uint8_t buf[16];

  RARCH_LOG("%d bytes read:\n", len);

  for(i = 0, offset = 0; i < len; i++)
  {
    buf[offset] = data[i];
    offset++;
    if(offset == 16)
    {
      offset = 0;
      RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
        buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
    }
  }

  if(padding)
  {
    for(i = padding; i < 16; i++)
      buf[i] = 0xff;

    RARCH_LOG("%02x%02x%02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
        buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]);
  }

  RARCH_LOG("=================================\n");
  #endif
}
