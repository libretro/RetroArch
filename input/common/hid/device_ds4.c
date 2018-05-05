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

extern pad_connection_interface_t ds4_pad_connection;

typedef struct ds4_instance {
   void *handle;
   joypad_connection_t *pad;
   int slot;
   uint32_t buttons;
   uint16_t motors[2];
   uint8_t data[64];
} ds4_instance_t;

/**
 * I'm leaving this code in here for posterity, and because maybe it can
 * be used on other platforms. But using the DS4 on the Wii U directly is
 * impossible because it doesn't generate a HID event. Which makes me think
 * it's not a HID device at all--at least, not over USB.
 *
 * I imagine it might be useful in Bluetooth mode, though.
 */
static void *ds4_init(void *handle)
{
   ds4_instance_t *instance;
   instance = (ds4_instance_t *)calloc(1, sizeof(ds4_instance_t));
   if(!instance)
      goto error;

   memset(instance, 0, sizeof(ds4_instance_t));
   instance->handle = handle;
   instance->pad = hid_pad_register(instance, &ds4_pad_connection);
   if(!instance->pad)
      goto error;

   RARCH_LOG("[ds4]: init complete.\n");
   return instance;

   error:
      RARCH_ERR("[ds4]: init failed.\n");
      if(instance)
         free(instance);

      return NULL;
}

static void ds4_free(void *data)
{
   ds4_instance_t *instance = (ds4_instance_t *)data;

   if(instance) {
      hid_pad_deregister(instance->pad);
      free(instance);
   }
}

static void ds4_handle_packet(void *data, uint8_t *buffer, size_t size)
{
   ds4_instance_t *instance = (ds4_instance_t *)data;

   if(instance && instance->pad)
      instance->pad->iface->packet_handler(instance->pad->data, buffer, size);
}

static bool ds4_detect(uint16_t vendor_id, uint16_t product_id)
{
  return vendor_id == VID_SONY && product_id == PID_SONY_DS4;
}

hid_device_t ds4_hid_device = {
  ds4_init,
  ds4_free,
  ds4_handle_packet,
  ds4_detect,
  "Sony DualShock 4"
};

static void *ds4_pad_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   ds4_instance_t *instance = (ds4_instance_t *)data;

   if(!instance)
      return NULL;

   instance->slot = slot;
   return instance;
}

static void ds4_pad_deinit(void *data)
{
}

static void ds4_get_buttons(void *data, input_bits_t *state)
{
   ds4_instance_t *instance = (ds4_instance_t *)data;
   if(!instance)
      return;

   /* TODO: get buttons */
}

static void ds4_packet_handler(void *data, uint8_t *packet, uint16_t size)
{
   ds4_instance_t *instance = (ds4_instance_t *)data;
   if(!instance)
      return;

   RARCH_LOG_BUFFER(packet, size);
}

static void ds4_set_rumble(void *data, enum retro_rumble_effect effect, uint16_t strength)
{
}

static int16_t ds4_get_axis(void *data, unsigned axis)
{
   return 0;
}

static const char *ds4_get_name(void *data)
{
   return "Sony DualShock 4";
}

static bool ds4_button(void *data, uint16_t joykey)
{
  return false;
}

pad_connection_interface_t ds4_pad_connection = {
   ds4_pad_init,
   ds4_pad_deinit,
   ds4_packet_handler,
   ds4_set_rumble,
   ds4_get_buttons,
   ds4_get_axis,
   ds4_get_name,
   ds4_button
};
