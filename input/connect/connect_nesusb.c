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

struct hidpad_nesusb_data
{
   struct pad_connection* connection;
   uint32_t slot;
   uint32_t buttons;
   uint8_t axis[2];
   input_pad_view_t view;  /* what the getters answer */
};

static void* hidpad_nesusb_init(void *data,
      uint32_t slot, hid_driver_t *driver)
{
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_nesusb_data* device = (struct hidpad_nesusb_data*)
      calloc(1, sizeof(struct hidpad_nesusb_data));

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

static void hidpad_nesusb_deinit(void *data)
{
   struct hidpad_nesusb_data *device = (struct hidpad_nesusb_data*)data;

   if (device)
      free(device);
}

static void hidpad_nesusb_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_nesusb_data *device = (struct hidpad_nesusb_data*)data;
   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_nesusb_get_axis(void *data, unsigned axis)
{
   struct hidpad_nesusb_data *device = (struct hidpad_nesusb_data*)data;
   if (!device || axis >= INPUT_PAD_VIEW_AXES)
      return 0;
   return device->view.axes[axis];
}

static int16_t hidpad_nesusb_axis_of(void *data, unsigned axis)
{
   int val;
   struct hidpad_nesusb_data *device = (struct hidpad_nesusb_data*)data;

   if (!device || axis >= 2)
      return 0;

   val = device->axis[axis];
   val = (val << 8) - 0x8000;

   if (abs(val) > 0x1000)
      return val;
   return 0;
}

static void hidpad_nesusb_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   uint32_t i, pressed_keys;
   static const uint32_t button_mapping[17] =
   {
      RETRO_DEVICE_ID_JOYPAD_L2,
      RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_START,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      NO_BTN,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_X,
   };
   struct hidpad_nesusb_data *device = (struct hidpad_nesusb_data*)data;

   if (!device || size < 7)
      return;

   device->axis[0] = packet[3];
   device->axis[1] = packet[4];

   device->buttons = 0;
   pressed_keys    = packet[6] | (packet[5] << 8);

   for (i = 0; i < 16; i ++)
      if (button_mapping[i] != NO_BTN)
         device->buttons |= (pressed_keys & (1 << i)) ? (1 << button_mapping[i]) : 0;
   /* the view, which the getters and the frontend read */
   pad_view_set_buttons(&device->view, device->buttons & 0xffff, false);
   {
      unsigned a;
      for (a = 0; a < 2; a++)
         device->view.axes[a] = hidpad_nesusb_axis_of(device, a);
   }
}

static void hidpad_nesusb_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength) { }

/* As the pad calls itself: the chip's name, the same on an NES pad, an
 * SNES pad or a Saturn pad */
const char * hidpad_nesusb_get_name(void *data)
{
   return "USB Gamepad";
}

static int32_t hidpad_nesusb_button(void *data, uint16_t joykey)
{
   struct hidpad_nesusb_data *pad = (struct hidpad_nesusb_data*)data;
   if (!pad || joykey > 31)
      return 0;
   return BIT256_GET(pad->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_nesusb_view(void *data)
{
   return &((struct hidpad_nesusb_data*)data)->view;
}

pad_connection_interface_t pad_connection_nesusb = {
   hidpad_nesusb_init,
   hidpad_nesusb_deinit,
   hidpad_nesusb_packet_handler,
   hidpad_nesusb_set_rumble,
   hidpad_nesusb_get_buttons,
   hidpad_nesusb_get_axis,
   hidpad_nesusb_get_name,
   hidpad_nesusb_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_nesusb_view
};
