/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "boolean.h"
#include "apple/common/apple_input.h"

struct hidpad_ps3_data
{
   struct apple_pad_connection* connection;

   uint8_t data[512];
  
   uint32_t slot;
   bool have_led;

   uint16_t motors[2];
};

static void hidpad_ps3_send_control(struct hidpad_ps3_data* device)
{
   // TODO: Can this be modified to turn off motion tracking?
   static uint8_t report_buffer[] = {
      0x52, 0x01,
      0x00, 0xFF, 0x00, 0xFF, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
   };
   
   report_buffer[11] = 1 << ((device->slot % 4) + 1);
   report_buffer[4] = device->motors[1] >> 8;
   report_buffer[6] = device->motors[0] >> 8;
   apple_pad_send_control(device->connection, report_buffer, sizeof(report_buffer));
}

static void* hidpad_ps3_connect(struct apple_pad_connection* connection, uint32_t slot)
{
   struct hidpad_ps3_data* device = calloc(1, sizeof(struct hidpad_ps3_data));
   device->connection = connection;  
   device->slot = slot;
   
   // Magic packet to start reports
#ifdef IOS
   static uint8_t data[] = {0x53, 0xF4, 0x42, 0x03, 0x00, 0x00};
   apple_pad_send_control(device->connection, data, 6);
#endif

   // Without this the digital buttons won't be reported
   hidpad_ps3_send_control(device);

   return device;
}

static void hidpad_ps3_disconnect(struct hidpad_ps3_data* device)
{
   free(device);
}

static uint32_t hidpad_ps3_get_buttons(struct hidpad_ps3_data* device)
{
   #define KEY(X) RETRO_DEVICE_ID_JOYPAD_##X
   static const uint32_t button_mapping[17] =
   {
      KEY(SELECT), KEY(L3),    KEY(R3),   KEY(START),
      KEY(UP),     KEY(RIGHT), KEY(DOWN), KEY(LEFT),
      KEY(L2),     KEY(R2),    KEY(L),    KEY(R),
      KEY(X),      KEY(A),     KEY(B),    KEY(Y),
      16 //< PS Button
   };
   #undef KEY

   const uint32_t pressed_keys = device->data[3] | (device->data[4] << 8) | ((device->data[5] & 1) << 16);
   uint32_t result = 0;

   for (int i = 0; i < 17; i ++)
      result |= (pressed_keys & (1 << i)) ? (1 << button_mapping[i]) : 0;

   return result;
}

static int16_t hidpad_ps3_get_axis(struct hidpad_ps3_data* device, unsigned axis)
{
   if (axis < 4)
   {
      int val = device->data[7 + axis];
      val = (val << 8) - 0x8000;
      return (abs(val) > 0x1000) ? val : 0;
   }

   return 0;
}

static void hidpad_ps3_packet_handler(struct hidpad_ps3_data* device, uint8_t *packet, uint16_t size)
{
   if (!device->have_led)
   {
      hidpad_ps3_send_control(device);
      device->have_led = true;
   }

   memcpy(device->data, packet, size);

   g_current_input_data.pad_buttons[device->slot] = hidpad_ps3_get_buttons(device);
   for (int i = 0; i < 4; i ++)
      g_current_input_data.pad_axis[device->slot][i] = hidpad_ps3_get_axis(device, i);
}

static void hidpad_ps3_set_rumble(struct hidpad_ps3_data* device, enum retro_rumble_effect effect, uint16_t strength)
{
   unsigned index = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (device->motors[index] != strength)
   {
      device->motors[index] = strength;
      hidpad_ps3_send_control(device);
   }
}

struct apple_pad_interface apple_pad_ps3 =
{
   (void*)&hidpad_ps3_connect,
   (void*)&hidpad_ps3_disconnect,
   (void*)&hidpad_ps3_packet_handler,
   (void*)&hidpad_ps3_set_rumble
};
