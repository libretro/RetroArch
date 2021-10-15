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

#define SIXAXIS_REPORT_0xF2_SIZE 17
#define SIXAXIS_REPORT_0xF5_SIZE 8

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
   uint8_t led_state[4];
} ds3_instance_t;

struct __attribute__((__packed__)) sixaxis_led {
    uint8_t time_enabled; /* the total time the led is active (0xff means forever) */
    uint8_t duty_length;  /* how long a cycle is in deciseconds (0 means "really fast") */
    uint8_t enabled;
    uint8_t duty_off; /* % of duty_length the led is off (0xff means 100%) */
    uint8_t duty_on;  /* % of duty_length the led is on (0xff mean 100%) */
};

struct __attribute__((__packed__)) sixaxis_rumble {
    uint8_t padding;
    uint8_t right_duration; /* Right motor duration (0xff means forever) */
    uint8_t right_motor_on; /* Right (small) motor on/off, only supports values of 0 or 1 (off/on) */
    uint8_t left_duration;    /* Left motor duration (0xff means forever) */
    uint8_t left_motor_force; /* left (large) motor, supports force values from 0 to 255 */
};

struct __attribute__((__packed__)) sixaxis_output_report {
    uint8_t report_id;
    struct sixaxis_rumble rumble;
    uint8_t padding[4];
    uint8_t leds_bitmap; /* bitmap of enabled LEDs: LED_1 = 0x02, LED_2 = 0x04, ... */
    struct sixaxis_led led[4];    /* LEDx at (4 - x) */
    struct sixaxis_led _reserved; /* LED5, not actually soldered */
};

union sixaxis_output_report_01 {
    struct sixaxis_output_report data;
    uint8_t buf[36];
};

static const union sixaxis_output_report_01 default_report = {
    .buf = {
        0x01,
        0x01, 0xff, 0x00, 0xff, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0xff, 0x27, 0x10, 0x00, 0x32,
        0x00, 0x00, 0x00, 0x00, 0x00
    }
};

/* forward declarations */
static void set_leds_from_id(ds3_instance_t *instance);
static int ds3_set_operational(ds3_instance_t *instance);
static int ds3_send_output_report(ds3_instance_t *instance);
static void ds3_update_pad_state(ds3_instance_t *instance);
static void ds3_update_analog_state(ds3_instance_t *instance);

static void *ds3_init(void *handle, uint32_t slot, hid_driver_t *driver) {
   ds3_instance_t *instance = (ds3_instance_t *)malloc(sizeof(ds3_instance_t));
   int ret;
   if(!instance) {
      return NULL;
   }

   instance->handle = handle;
   instance->hid_driver = driver;
   instance->slot = slot;
   set_leds_from_id(instance);

   if((ret = ds3_set_operational(instance)) < 0) {
      RARCH_LOG("Failed to set operational mode\n");
      goto error;
   }
   if((ret = ds3_send_output_report(instance)) < 0) {
      RARCH_LOG("Failed to send output report\n");
      goto error;
   } 

   return instance;
error:
   free(instance);
   return NULL;
}

static void ds3_deinit(void *device_data) {
   if(device_data) {
      free(device_data);
   }
}

static void ds3_packet_handler(void *device_data, uint8_t *packet, uint16_t size) {
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   static long packet_count = 0;

   if(!device)
      return;

   if (!device->led_set)
   {
      ds3_send_output_report(device);
      device->led_set = true;
   }

   if (size > sizeof(device->data))
   {
      RARCH_ERR("[ds3]: Expecting packet to be %ld but was %d\n",
         (long)sizeof(device->data), size);
      return;
   }
   packet_count++;

#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
   packet++;
   size -= 2;
#endif

   memcpy(device->data, packet, size);
   ds3_update_pad_state(device);
   ds3_update_analog_state(device);
}

static void ds3_set_rumble(void *device_data, enum retro_rumble_effect effect, uint16_t strength) {

}

static void ds3_get_buttons(void *device_data, input_bits_t *state) {
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   if (device)
   {
      /* copy 32 bits : needed for PS button? */
      BITS_COPY32_PTR(state, device->buttons);
   }
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ds3_get_axis(void *device_data, unsigned axis) {
   union joyaxis {
      uint32_t encoded;
      int16_t axis[2];
   } joyaxis;
   axis_data axis_data = {0};
   ds3_instance_t *device = (ds3_instance_t *)device_data;

   joyaxis.encoded = axis;
   gamepad_read_axis_data(axis, &axis_data);

   if (!device || axis_data.axis >= 4)
      return 0;

   if(joyaxis.axis[0] < 0 || joyaxis.axis[1] < 0) {
      return gamepad_get_axis_value(device->analog_state, &axis_data);
   } else {
      return gamepad_get_axis_value_raw(device->analog_state, &axis_data, false);
   }
}

static const char *ds3_get_name(void *device_data) {
   return "PLAYSTATION(R)3 Controller";
}

static int32_t ds3_button(void *device_data, uint16_t joykey) {
   ds3_instance_t *device = (ds3_instance_t *)device_data;

   if (!device || joykey > 31)
      return 0;
   return device->buttons & (1 << joykey);
}

static void set_leds_from_id(ds3_instance_t *instance)
{
   /* for pads 0-3, we just light up the appropriate LED. */
   /* for higher pads, we sum up the numbers on the LEDs  */
   /* themselves, so e.g. pad 5 is 4 + 1, pad 6 is 4 + 2, */
   /* and so on. We max out at 10 because 4+3+2+1 = 10    */
   static const uint8_t sixaxis_leds[10][4] = {
            { 0x01, 0x00, 0x00, 0x00 },
            { 0x00, 0x01, 0x00, 0x00 },
            { 0x00, 0x00, 0x01, 0x00 },
            { 0x00, 0x00, 0x00, 0x01 },
            { 0x01, 0x00, 0x00, 0x01 },
            { 0x00, 0x01, 0x00, 0x01 },
            { 0x00, 0x00, 0x01, 0x01 },
            { 0x01, 0x00, 0x01, 0x01 },
            { 0x00, 0x01, 0x01, 0x01 },
            { 0x01, 0x01, 0x01, 0x01 }
   };

   int id = instance->slot;

   if (id < 0)
      return;

   id %= 10;
   memcpy(instance->led_state, sixaxis_leds[id], sizeof(sixaxis_leds[id]));
}

static int ds3_set_operational(ds3_instance_t *instance) {
   const int buf_size = SIXAXIS_REPORT_0xF2_SIZE;
   uint8_t *buf = (uint8_t *)malloc(buf_size);
   int ret;

   if(!buf) {
      return -1;
   }

   ret = instance->hid_driver->get_report(instance->handle, HID_REPORT_FEATURE, 0xf2, buf, SIXAXIS_REPORT_0xF2_SIZE);
   if(ret < 0) {
      RARCH_LOG("Failed to set operational mode step 1\n");
      goto out;
   }

   ret = instance->hid_driver->get_report(instance->handle, HID_REPORT_FEATURE, 0xf5, buf, SIXAXIS_REPORT_0xF5_SIZE);
   if(ret < 0) {
      RARCH_LOG("Failed to set operational mode step 2\n");
      goto out;
   }

   ret = instance->hid_driver->set_report(instance->handle, HID_REPORT_OUTPUT, buf[0], buf, 1);
   if(ret < 0) {
      RARCH_LOG("Failed to set operational mode step 3, ignoring\n");
      ret = 0;
   }
out:
   free(buf);
   return ret;
}

static int ds3_send_output_report(ds3_instance_t *instance) {
   struct sixaxis_output_report report = {0};
   int n;

   /* Initialize the report with default values */
   memcpy(&report, &default_report, sizeof(struct sixaxis_output_report));
   report.leds_bitmap |= instance->led_state[0] << 1;
   report.leds_bitmap |= instance->led_state[1] << 2;
   report.leds_bitmap |= instance->led_state[2] << 3;
   report.leds_bitmap |= instance->led_state[3] << 4;

   return instance->hid_driver->set_report(instance->handle, HID_REPORT_OUTPUT, report.report_id, (uint8_t *)&report, sizeof(report));
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
      axis         = (pad_axis % 2) ? 0 : 1;
      stick        = pad_axis / 2;
      interpolated = instance->data[6 + pad_axis];

      /* libretro requires "up" to be negative, so we invert the y axis */
      interpolated = (axis) ?
         ((interpolated - 128) * 256) :
         ((interpolated - 128) * -256);
      instance->analog_state[stick][axis] = interpolated;
   }
}

pad_connection_interface_t pad_connection_ps3 = {
   ds3_init,
   ds3_deinit,
   ds3_packet_handler,
   ds3_set_rumble,
   ds3_get_buttons,
   ds3_get_axis,
   ds3_get_name,
   ds3_button,
   false,
};
