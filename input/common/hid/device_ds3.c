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

#define DS3_ACTIVATION_REPORT_ID 0xf4
#define DS3_RUMBLE_REPORT_ID     0x01

typedef struct ds3_instance {
   void *handle;
   joypad_connection_t *pad;
} ds3_instance_t;

static uint8_t activation_packet[] = {
  0x42, 0x0c, 0x00, 0x00
};

extern pad_connection_interface_t ds3_pad_connection;

static void *ds3_init(void *handle)
{
   ds3_instance_t *instance;

   instance = (ds3_instance_t *)calloc(1, sizeof(ds3_instance_t));
   if(!instance)
     goto error;

   instance->handle = handle;

/* TODO: do whatever is needed so that the read loop doesn't bomb out */

   instance->pad = hid_pad_register(instance, &ds3_pad_connection);
   if(!instance->pad)
      goto error;

   return instance;

   error:
      if(instance)
         free(instance);
      return NULL;
}

static void ds3_free(void *data)
{
   ds3_instance_t *instance = (ds3_instance_t *)data;

   if(instance)
      free(instance);
}

static void ds3_handle_packet(void *data, uint8_t *buffer, size_t size)
{
   ds3_instance_t *instance = (ds3_instance_t *)data;
}

static bool ds3_detect(uint16_t vendor_id, uint16_t product_id)
{
   return vendor_id == VID_SONY && product_id == PID_SONY_DS3;
}

hid_device_t ds3_hid_device = {
   ds3_init,
   ds3_free,
   ds3_handle_packet,
   ds3_detect,
   "Sony DualShock 3"
};

/**
 * pad interface implementation
 */

static void *ds3_pad_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   return data;
}

static void ds3_pad_deinit(void *data)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
}

static void ds3_get_buttons(void *data, retro_bits_t *state)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
}

static void ds3_packet_handler(void *data, uint8_t *packet, uint16_t size)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
}

static void ds3_set_rumble(void *data, enum retro_rumble_effect effect, uint16_t strength)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
}

static int16_t ds3_get_axis(void *data, unsigned axis)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   return 0;
}

static const char *ds3_get_name(void *data)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   return "Sony DualShock 3";
}

static bool ds3_button(void *data, uint16_t joykey)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   return false;
}

pad_connection_interface_t ds3_pad_connection = {
   ds3_pad_init,
   ds3_pad_deinit,
   ds3_packet_handler,
   ds3_set_rumble,
   ds3_get_buttons,
   ds3_get_axis,
   ds3_get_name,
   ds3_button
};
