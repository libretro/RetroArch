/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include "joypad_connection.h"

#include "../input_defines.h"

struct hidpad_snesusb_data
{
   struct pad_connection* connection;
   uint8_t data[64];
   uint32_t slot;
   uint32_t buttons;
};

static void* hidpad_snesusb_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_snesusb_data* device    = (struct hidpad_snesusb_data*)
      calloc(1, sizeof(struct hidpad_snesusb_data));

   if (!device)
      return NULL;

   if (!connection)
   {
      free(device);
      return NULL;
   }

   device->connection   = connection;
   device->slot         = slot;

   return device;
}

static void hidpad_snesusb_deinit(void *data)
{
   struct hidpad_snesusb_data *device = (struct hidpad_snesusb_data*)data;

   if (device)
      free(device);
}

static void hidpad_snesusb_get_buttons(void *data, input_bits_t *state)
{
	struct hidpad_snesusb_data *device = (struct hidpad_snesusb_data*)data;
	if (device)
   {
		BITS_COPY16_PTR(state, device->buttons);
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_snesusb_get_axis(void *data, unsigned axis)
{
   int val;
   struct hidpad_snesusb_data *device = (struct hidpad_snesusb_data*)data;

   if (!device || axis >= 2)
      return 0;

   val = device->data[1 + axis];
   val = (val << 8) - 0x8000;

   return (abs(val) > 0x1000) ? val : 0;
}

static void hidpad_snesusb_packet_handler(void *data, uint8_t *packet, uint16_t size)
{
   uint32_t i, pressed_keys;
   static const uint32_t button_mapping[16] =
   {
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_R,
      NO_BTN,
      NO_BTN,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_START,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_Y
   };
   struct hidpad_snesusb_data *device = (struct hidpad_snesusb_data*)data;

   if (!device)
      return;

   memcpy(device->data, packet, size);

   device->buttons = 0;

   pressed_keys  = device->data[7] | (device->data[6] << 8);

   for (i = 0; i < 16; i ++)
      if (button_mapping[i] != NO_BTN)
         device->buttons |= (pressed_keys & (1 << i)) ? (1 << button_mapping[i]) : 0;
}

static void hidpad_snesusb_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
	(void)data;
	(void)effect;
   (void)strength;
}

const char * hidpad_snesusb_get_name(void *data)
{
	(void)data;
	/* For now we return a single static name */
	return "Generic SNES USB Controller";
}

pad_connection_interface_t pad_connection_snesusb = {
   hidpad_snesusb_init,
   hidpad_snesusb_deinit,
   hidpad_snesusb_packet_handler,
   hidpad_snesusb_set_rumble,
   hidpad_snesusb_get_buttons,
   hidpad_snesusb_get_axis,
   hidpad_snesusb_get_name,
};
