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

struct hidpad_ps4_hori_mini_data
{
   struct pad_connection* connection;
   uint32_t slot;
   uint32_t buttons;
   uint8_t axis[4];
   input_pad_view_t view;  /* what the getters answer */
};

static void* hidpad_ps4_hori_mini_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   struct pad_connection *connection        = (struct pad_connection*)data;
   struct hidpad_ps4_hori_mini_data *device = 
	   (struct hidpad_ps4_hori_mini_data*)
	   calloc(1, sizeof(struct hidpad_ps4_hori_mini_data));

   if (!device)
      return NULL;

   if (!connection)
   {
      free(device);
      return NULL;
   }

   device->connection = connection;
   device->slot       = slot;

   return device;
}

static void hidpad_ps4_hori_mini_deinit(void *data)
{
   struct hidpad_ps4_hori_mini_data *device = 
      (struct hidpad_ps4_hori_mini_data*)data;

   if (device)
      free(device);
}

static void hidpad_ps4_hori_mini_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_ps4_hori_mini_data *device = (struct hidpad_ps4_hori_mini_data*)data;
   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_ps4_hori_mini_get_axis(void *data, unsigned axis)
{
   struct hidpad_ps4_hori_mini_data *device = (struct hidpad_ps4_hori_mini_data*)data;
   if (!device || axis >= INPUT_PAD_VIEW_AXES)
      return 0;
   return device->view.axes[axis];
}

static int16_t hidpad_ps4_hori_mini_axis_of(void *data, unsigned axis)
{
   int val;
   struct hidpad_ps4_hori_mini_data *device = 
      (struct hidpad_ps4_hori_mini_data*)data;

   if (!device || axis >= 4)
      return 0;

   val = (device->axis[axis] << 8) - 0x8000;

   if (abs(val) > 0x1000)
      return val;
   return 0;
}

static void hidpad_ps4_hori_mini_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   uint8_t dpad = 0;
   uint32_t i, pressed_keys;
   static const uint32_t button_mapping[15] =
   {
      RETRO_DEVICE_ID_JOYPAD_Y,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L2,
      RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_L3,
      RETRO_DEVICE_ID_JOYPAD_R3,
      16,
      17,
      18
   };
   struct hidpad_ps4_hori_mini_data *device = 
      (struct hidpad_ps4_hori_mini_data*)data;

   if (!device || size < 8)
      return;

   memcpy(device->axis, packet + 1, sizeof(device->axis));

   device->buttons = 0;

   dpad            = packet[5] & 0xF;

   switch(dpad)
   {
      case 0:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
         break;
      case 1:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         break;
      case 2:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         break;
      case 3:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      case 4:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      case 5:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
         break;
      case 6:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
         break;
      case 7:
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
         device->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
         break;
   }

   pressed_keys           = (
                          (packet[5] & 0xF0) >> 4)
                        | (packet[6] << 4)
                        | (packet[7] << 12);

   for (i = 0; i < 15; i++)
      device->buttons |= (pressed_keys & (1 << i)) ?
         (1 << button_mapping[i]) : 0;
   /* the view, which the getters and the frontend read */
   pad_view_set_buttons(&device->view, device->buttons, false);
   {
      unsigned a;
      for (a = 0; a < 4; a++)
         device->view.axes[a] = hidpad_ps4_hori_mini_axis_of(device, a);
   }
}

static void hidpad_ps4_hori_mini_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength) { }

/* For now we return a single static name */
const char * hidpad_ps4_hori_mini_get_name(void *data)
{
   return "HORI mini wired PS4";
}

static int32_t hidpad_ps4_hori_mini_button(void *data, uint16_t joykey)
{
   struct hidpad_ps4_hori_mini_data *pad = 
      (struct hidpad_ps4_hori_mini_data*)data;
   if (!pad || joykey > 31)
      return 0;
   return BIT256_GET(pad->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_ps4_hori_mini_view(void *data)
{
   return &((struct hidpad_ps4_hori_mini_data*)data)->view;
}

pad_connection_interface_t pad_connection_ps4_hori_mini = {
   hidpad_ps4_hori_mini_init,
   hidpad_ps4_hori_mini_deinit,
   hidpad_ps4_hori_mini_packet_handler,
   hidpad_ps4_hori_mini_set_rumble,
   hidpad_ps4_hori_mini_get_buttons,
   hidpad_ps4_hori_mini_get_axis,
   hidpad_ps4_hori_mini_get_name,
   hidpad_ps4_hori_mini_button, /* button */
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_ps4_hori_mini_view
};
