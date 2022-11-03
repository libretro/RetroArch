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

struct hidpad_kade_data
{
   struct pad_connection* connection;
   uint32_t slot;
   uint32_t buttons;
   uint8_t data[64];
};

static void* hidpad_kade_init(void* data, uint32_t slot, hid_driver_t* driver)
{
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_kade_data* device = (struct hidpad_kade_data*)
      calloc(1, sizeof(struct hidpad_kade_data));

   if (!device)
      return NULL;

   if (!connection)
   {
      free(device);
      return NULL;
   }

   device->connection = connection;
   device->slot = slot;

   return device;
}

static void hidpad_kade_deinit(void* data)
{
   struct hidpad_kade_data* device = (struct hidpad_kade_data*)data;

   if (device)
      free(device);
}

static void hidpad_kade_get_buttons(void* data, input_bits_t* state)
{
   struct hidpad_kade_data* device = (struct hidpad_kade_data*)data;
   if (device)
   {
      BITS_COPY16_PTR(state, device->buttons);
   }
   else
      BIT256_CLEAR_ALL_PTR(state);
}

/*This is being implemented for use with an arcade stick, so there are no axis values on this control, only a hat values .
If your device with kade firmware uses analog controls this function must be implemented.*/
static int16_t hidpad_kade_get_axis(void* data, unsigned axis)
{
   return 0;
}

#define KADE_H_GET(a)   (a & 0x0F) /*HAT MASK = 0x0F */
#define KADE_H_LEFT(a)  (a == 0x05) || (a == 0x06) || (a == 0x07)
#define KADE_H_RIGHT(a) (a == 0x01) || (a == 0x02) || (a == 0x03)
#define KADE_H_UP(a)    (a == 0x07) || (a == 0x00) || (a == 0x01)
#define KADE_H_DOWN(a)  (a == 0x03) || (a == 0x04) || (a == 0x05)

static void hidpad_kade_packet_handler(void* data, uint8_t* packet, uint16_t size)
{
   uint32_t i, pressed_keys;
   int16_t hat_value;
   static const uint32_t button_mapping[16] =
   {
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_START,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L2,
      RETRO_DEVICE_ID_JOYPAD_R2
   };
   struct hidpad_kade_data* device = (struct hidpad_kade_data*)data;

   if (!device)
      return;

   memcpy(device->data, packet, size);

   device->buttons = 0;

   pressed_keys = device->data[1] | (device->data[0] << 8);

   for (i = 0; i < 16; i++)
      if (button_mapping[i] != NO_BTN)
         device->buttons |= (pressed_keys & (1 << i)) ? (1 << button_mapping[i]) : 0;

   hat_value = KADE_H_GET(device->data[2]);
   device->buttons |= KADE_H_LEFT(hat_value) ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   device->buttons |= KADE_H_RIGHT(hat_value) ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   device->buttons |= KADE_H_UP(hat_value) ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   device->buttons |= KADE_H_DOWN(hat_value) ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
}

static void hidpad_kade_set_rumble(void* data,
   enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)effect;
   (void)strength;
}

const char* hidpad_kade_get_name(void* data)
{
   (void)data;
   /* For now we return a single static name */
   return "KADE: Kick Ass Dynamic Encoder";
}

static int32_t hidpad_kade_button(void* data, uint16_t joykey)
{
   struct hidpad_kade_data* pad = (struct hidpad_kade_data*)data;
   if (!pad || joykey > 31)
      return 0;
   return pad->buttons & (1 << joykey);
}

pad_connection_interface_t pad_connection_kade = {
   hidpad_kade_init,
   hidpad_kade_deinit,
   hidpad_kade_packet_handler,
   hidpad_kade_set_rumble,
   hidpad_kade_get_buttons,
   hidpad_kade_get_axis,
   hidpad_kade_get_name,
   hidpad_kade_button,
   false
};
