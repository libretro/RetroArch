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
#include "../common/hid/hid_device_driver.h"

struct hidpad_ps3_data
{
   struct pad_connection* connection;
   hid_driver_t *driver;
   uint8_t data[512];
   uint32_t slot;
   uint32_t buttons;
   bool have_led;
   uint16_t motors[2];
};

/*
 * TODO: give these more meaningful names.
 */

#define DS3_ACTIVATION_REPORT_ID 0xf4
#define DS3_RUMBLE_REPORT_ID     0x01

static void hidpad_ps3_send_control(struct hidpad_ps3_data* device)
{
   /* TODO: Can this be modified to turn off motion tracking? */
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

   /* Turn on the appropriate LED */
   report_buffer[11] = 1 << ((device->slot % 4) + 1);
   /* Set rumble state */
   report_buffer[4]  = device->motors[1] >> 8;
   report_buffer[6]  = device->motors[0] >> 8;
#ifdef HAVE_WIIUSB_HID
   report_buffer[1]  = 0x03; /* send control message type */
   device->driver->send_control(device->connection, &report_buffer[1], sizeof(report_buffer)-1);
#elif defined(WIIU)
   device->driver->set_report(device->connection,
                              HID_REPORT_OUTPUT,
                              DS3_RUMBLE_REPORT_ID,
                              report_buffer+2,
                              sizeof(report_buffer) - (2*sizeof(uint8_t)));
#else
   device->driver->send_control(device->connection, report_buffer, sizeof(report_buffer));
#endif
}

static void* hidpad_ps3_init(void *data, uint32_t slot, hid_driver_t *driver)
{
#if defined(HAVE_WIIUSB_HID) || defined(WIIU)
   /* Special command to enable Sixaxis, first byte defines the message type */
   static uint8_t magic_data[]       = {0x02, 0x42, 0x0c, 0x00, 0x00};
#elif defined(IOS)
   /* Magic packet to start reports. */
   static uint8_t magic_data[]       = {0x53, 0xF4, 0x42, 0x03, 0x00, 0x00};
#endif
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_ps3_data* device    = (struct hidpad_ps3_data*)
      calloc(1, sizeof(struct hidpad_ps3_data));

   if (!device)
      return NULL;

   if (!connection)
   {
      free(device);
      return NULL;
   }

   device->connection = connection;
   device->slot       = slot;
   device->driver     = driver;

#if defined(IOS) || defined(HAVE_WIIUSB_HID)
   device->driver->send_control(device->connection, magic_data, sizeof(magic_data));
#endif

#ifdef WIIU
   device->driver->set_protocol(device->connection, 1);
   hidpad_ps3_send_control(device);
   device->driver->set_report(device->connection,
                              HID_REPORT_FEATURE,
                              DS3_ACTIVATION_REPORT_ID,
                              magic_data+1,
                              (sizeof(magic_data) - sizeof(uint8_t)));
#endif

#ifndef HAVE_WIIUSB_HID
   /* Without this, the digital buttons won't be reported. */
   hidpad_ps3_send_control(device);
#endif
   return device;
}

static void hidpad_ps3_deinit(void *data)
{
   struct hidpad_ps3_data *device = (struct hidpad_ps3_data*)data;

   if (device)
      free(device);
}

static void hidpad_ps3_get_buttons(void *data, input_bits_t *state)
{
	struct hidpad_ps3_data *device = (struct hidpad_ps3_data*)data;
	if ( device )
	{
		/* copy 32 bits : needed for PS button? */
		BITS_COPY32_PTR(state, device->buttons);
	}
	else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_ps3_get_axis(void *data, unsigned axis)
{
   int val;
   struct hidpad_ps3_data *device = (struct hidpad_ps3_data*)data;

   if (!device || axis >= 4)
      return 0;

   val = device->data[7 + axis];
   val = (val << 8) - 0x8000;

   return (abs(val) > 0x1000) ? val : 0;
}

static void hidpad_ps3_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   uint32_t i, pressed_keys;
   static const uint32_t button_mapping[17] =
   {
      RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_L3,
      RETRO_DEVICE_ID_JOYPAD_R3,
      RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_UP,
      RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_LEFT,
      RETRO_DEVICE_ID_JOYPAD_L2,
      RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B,
      RETRO_DEVICE_ID_JOYPAD_Y,
      16 /* PS Button */
   };
   struct hidpad_ps3_data *device = (struct hidpad_ps3_data*)data;

   if (!device)
      return;

   if (!device->have_led)
   {
      hidpad_ps3_send_control(device);
      device->have_led = true;
   }

   memcpy(device->data, packet, size);

   device->buttons     = 0;

   pressed_keys        = device->data[3] | (device->data[4] << 8) |
      ((device->data[5] & 1) << 16);

   for (i = 0; i < 17; i ++)
      device->buttons |= (pressed_keys & (1 << i)) ?
         (1 << button_mapping[i]) : 0;
}

static void hidpad_ps3_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_ps3_data *device = (struct hidpad_ps3_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (!device)
      return;
   if (device->motors[idx] == strength)
      return;

   device->motors[idx] = strength;
   hidpad_ps3_send_control(device);
}

const char * hidpad_ps3_get_name(void *data)
{
	(void)data;
	/* For now we return a single static name */
	return "PLAYSTATION(R)3 Controller";
}

pad_connection_interface_t pad_connection_ps3 = {
   hidpad_ps3_init,
   hidpad_ps3_deinit,
   hidpad_ps3_packet_handler,
   hidpad_ps3_set_rumble,
   hidpad_ps3_get_buttons,
   hidpad_ps3_get_axis,
   hidpad_ps3_get_name,
};
