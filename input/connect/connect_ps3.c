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
#include "verbosity.h"

#define DS3_ACTIVATION_REPORT_ID 0xf4
#define DS3_RUMBLE_REPORT_ID     0x01

typedef struct ds3_instance
{
   hid_driver_t *hid_driver;
   void *handle;
   int slot;
   bool led_set;
   uint32_t buttons;
   int16_t analog_state[3][2];
   uint16_t motors[2];
   uint8_t data[64];
} ds3_instance_t;

static void ds3_update_pad_state(ds3_instance_t *instance);
static void ds3_update_analog_state(ds3_instance_t *instance);

static uint8_t ds3_activation_packet[] =
{
#if defined(IOS)
   0x53, 0xF4,
#elif defined(HAVE_WIIUSB_HID)
   0x02,
#endif
   0x42, 0x0c, 0x00, 0x00
};

#if defined(WIIU)
#define PACKET_OFFSET 2
#elif defined(HAVE_WIIUSB_HID)
#define PACKET_OFFSET 1
#else
#define PACKET_OFFSET 0
#endif

#define LED_OFFSET 11
#define MOTOR1_OFFSET 4
#define MOTOR2_OFFSET 6

static uint8_t ds3_control_packet[] = {
   0x52, 0x01,
   0x00, 0xff, 0x00, 0xff, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0xff, 0x27, 0x10, 0x00, 0x32,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00
};


static int32_t ds3_send_control_packet(
      void *data, uint32_t slot, hid_driver_t *driver)
{
   int32_t result = 0;
   uint8_t packet_buffer[64] = {0};
   memcpy(packet_buffer, ds3_control_packet, sizeof(ds3_control_packet));

   packet_buffer[LED_OFFSET] = 0;
   packet_buffer[LED_OFFSET] = 1 << ((slot % 4) + 1);
   packet_buffer[MOTOR1_OFFSET] = 0;
   packet_buffer[MOTOR2_OFFSET] = 0;

#if defined(HAVE_WIIUSB_HID)
   packet_buffer[1] = 0x03;
#endif

#if defined(WIIU)   
   result = driver->set_report(data, HID_REPORT_OUTPUT, DS3_RUMBLE_REPORT_ID, packet_buffer+PACKET_OFFSET, 64-PACKET_OFFSET);
#else
   driver->send_control(data, packet_buffer+PACKET_OFFSET, 64-PACKET_OFFSET);
#endif /* WIIU */
   return result;
}

static int32_t ds3_send_activation_packet(void *data,
      uint32_t slot, hid_driver_t *driver)
{
#ifdef WIIU
   return driver->set_report(data, HID_REPORT_FEATURE, DS3_ACTIVATION_REPORT_ID, ds3_activation_packet, sizeof(ds3_activation_packet));
#else
   driver->send_control(data, ds3_activation_packet, sizeof(ds3_activation_packet));
   return 0;
#endif
}

static void *ds3_pad_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   int errors               = 0;
   ds3_instance_t *instance = (ds3_instance_t *)calloc(1, sizeof(ds3_instance_t));

   driver->set_protocol(data, 1);

   if (ds3_send_control_packet(data, slot, driver) < 0)
      errors++;

   /* Sending activation packet.. */
   if (ds3_send_activation_packet(data, slot, driver) < 0)
      errors++;

   if (errors)
      goto error;

   instance->hid_driver = driver;
   instance->handle     = data;
   instance->slot       = slot;
   instance->led_set    = true;

   return instance;

error:
   free(instance);
   return NULL;
}

static void ds3_pad_deinit(void *data)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   if (pad)
      free(pad);
}

static void ds3_get_buttons(void *data, input_bits_t *state)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;

   if (pad)
   {
      BITS_COPY16_PTR(state, pad->buttons);

      if (pad->buttons & 0x10000)
         BIT256_SET_PTR(state, RARCH_MENU_TOGGLE);
   }
   else
   {
      BIT256_CLEAR_ALL_PTR(state);
   }
}

static void ds3_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   ds3_instance_t *instance = (ds3_instance_t *)data;
   if(!instance)
      return;

   if (!instance->led_set)
   {
      ds3_send_control_packet(instance->handle,
            instance->slot, instance->hid_driver);
      instance->led_set = true;
   }

   if (size > sizeof(instance->data))
   {
      RARCH_ERR("[ds3]: Expecting packet to be %ld but was %d\n",
         (long)sizeof(instance->data), size);
      return;
   }

   memcpy(instance->data, packet, size);
   ds3_update_pad_state(instance);
   ds3_update_analog_state(instance);
}

const char * ds3_get_name(void *data)
{
	(void)data;
	/* For now we return a single static name */
	return "PLAYSTATION(R)3 Controller";
}

static void ds3_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength) { }

static int16_t ds3_get_axis(void *data, unsigned axis)
{
   axis_data axis_data;
   ds3_instance_t *pad = (ds3_instance_t *)data;

   gamepad_read_axis_data(axis, &axis_data);

   if (!pad || axis_data.axis >= 4)
      return 0;

   return gamepad_get_axis_value(pad->analog_state, &axis_data);
}

static int32_t ds3_button(void *data, uint16_t joykey)
{
   ds3_instance_t *pad = (ds3_instance_t *)data;
   if (!pad || joykey > 31)
      return 0;
   return pad->buttons & (1 << joykey);
}

static void ds3_update_pad_state(ds3_instance_t *instance)
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
      16 /* PS button */
   };

   instance->buttons = 0;

   pressed_keys = instance->data[2]        | 
      (instance->data[3] << 8) |
      ((instance->data[4] & 0x01) << 16);

   for (i = 0; i < 17; i++)
      instance->buttons |= (pressed_keys & (1 << i)) ?
         (1 << button_mapping[i]) : 0;
}

static void ds3_update_analog_state(ds3_instance_t *instance)
{
   int pad_axis;
   int16_t interpolated;
   unsigned stick, axis;

   for (pad_axis = 0; pad_axis < 4; pad_axis++)
   {
      axis         = pad_axis % 2 ? 0 : 1;
      stick        = pad_axis / 2;
      interpolated = instance->data[6+pad_axis];
      instance->analog_state[stick][axis] = (interpolated - 128) * 256;
   }
}

pad_connection_interface_t pad_connection_ps3 = {
   ds3_pad_init,
   ds3_pad_deinit,
   ds3_packet_handler,
   ds3_set_rumble,
   ds3_get_buttons,
   ds3_get_axis,
   ds3_get_name,
   ds3_button,
   false,
};
