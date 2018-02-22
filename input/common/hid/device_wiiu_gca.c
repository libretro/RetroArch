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

#ifdef WII
static uint8_t activation_packet[] = { 0x01, 0x13 };
#else
static uint8_t activation_packet[] = { 0x13 };
#endif

#define GCA_PORT_INITIALIZING 0x00
#define GCA_PORT_EMPTY        0x04
#define GCA_PORT_CONNECTED    0x14

typedef struct wiiu_gca_instance {
  hid_driver_instance_t *driver;
  uint8_t device_state[37];
  joypad_connection_t *pads[4];
} wiiu_gca_instance_t;

static void update_pad_state(wiiu_gca_instance_t *instance);
static joypad_connection_t *register_pad(wiiu_gca_instance_t *instance);

extern pad_connection_interface_t wiiu_gca_pad_connection;

static void *wiiu_gca_init(hid_driver_instance_t *driver)
{
  wiiu_gca_instance_t *instance = calloc(1, sizeof(wiiu_gca_instance_t));
  memset(instance, 0, sizeof(wiiu_gca_instance_t));
  instance->driver = driver;

  driver->hid_driver->send_control(driver->hid_data, activation_packet, sizeof(activation_packet));
  driver->hid_driver->read(driver->hid_data, instance->device_state, sizeof(instance->device_state));

  return instance;
}

static void wiiu_gca_free(void *data) {
  wiiu_gca_instance_t *instance = (wiiu_gca_instance_t *)data;
  if(instance) {
    free(instance);
  }
}

static void wiiu_gca_handle_packet(void *data, uint8_t *buffer, size_t size)
{
  wiiu_gca_instance_t *instance = (wiiu_gca_instance_t *)data;
  if(!instance)
    return;

  if(size > sizeof(instance->device_state))
    return;

  memcpy(instance->device_state, buffer, size);
  update_pad_state(instance);
}

static void update_pad_state(wiiu_gca_instance_t *instance)
{
  int i, pad;

  /* process each pad */
  for(i = 1; i < 37; i += 9)
  {
    pad = i / 9;
    switch(instance->device_state[i])
    {
       case GCA_PORT_INITIALIZING:
       case GCA_PORT_EMPTY:
          if(instance->pads[pad] != NULL)
          {
            /* TODO: free pad */
            instance->pads[pad] = NULL;
          }
          break;
       case GCA_PORT_CONNECTED:
         if(instance->pads[pad] == NULL)
         {
           instance->pads[pad] = register_pad(instance);
         }
    }
  }
}

static joypad_connection_t *register_pad(wiiu_gca_instance_t *instance) {
  int slot;
  joypad_connection_t *result;

  slot = pad_connection_find_vacant_pad(instance->driver->pad_connection_list);
  if(slot < 0)
    return NULL;

  result = &(instance->driver->pad_connection_list[slot]);
  result->iface = &wiiu_gca_pad_connection;
  result->data = result->iface->init(instance, slot, instance->driver->hid_driver);
  result->connected = true;
  input_pad_connect(slot, instance->driver->pad_driver);

  return result;
}

static bool wiiu_gca_detect(uint16_t vendor_id, uint16_t product_id) {
  return vendor_id == VID_NINTENDO && product_id == PID_NINTENDO_GCA;
}

hid_device_t wiiu_gca_hid_device = {
  wiiu_gca_init,
  wiiu_gca_free,
  wiiu_gca_handle_packet,
  wiiu_gca_detect,
  "Wii U Gamecube Adapter"
};

pad_connection_interface_t wiiu_gca_pad_connection = {
/*
   wiiu_gca_pad_init,
   wiiu_gca_pad_deinit,
   wiiu_gca_packet_handler,
   wiiu_gca_set_rumble,
   wiiu_gca_get_buttons,
   wiiu_gca_get_axis,
   wiiu_gca_get_name
*/
};
