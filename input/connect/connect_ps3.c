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

struct sixaxis_led {
    uint8_t time_enabled; /* the total time the led is active (0xff means forever) */
    uint8_t duty_length;  /* how long a cycle is in deciseconds (0 means "really fast") */
    uint8_t enabled;
    uint8_t duty_off; /* % of duty_length the led is off (0xff means 100%) */
    uint8_t duty_on;  /* % of duty_length the led is on (0xff mean 100%) */
} __packed;

struct sixaxis_rumble {
    uint8_t padding;
    uint8_t right_duration; /* Right motor duration (0xff means forever) */
    uint8_t right_motor_on; /* Right (small) motor on/off, only supports values of 0 or 1 (off/on) */
    uint8_t left_duration;    /* Left motor duration (0xff means forever) */
    uint8_t left_motor_force; /* left (large) motor, supports force values from 0 to 255 */
} __packed;

struct sixaxis_output_report {
    uint8_t report_id;
    struct sixaxis_rumble rumble;
    uint8_t padding[4];
    uint8_t leds_bitmap; /* bitmap of enabled LEDs: LED_1 = 0x02, LED_2 = 0x04, ... */
    struct sixaxis_led led[4];    /* LEDx at (4 - x) */
    struct sixaxis_led _reserved; /* LED5, not actually soldered */
} __packed;

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

static void *ds3_init(void *handle, uint32_t slot, hid_driver_t *driver) {
   ds3_instance_t *instance = (ds3_instance_t *)malloc(sizeof(ds3_instance_t));
   if(!instance) {
      return NULL;
   }

   instance->handle = handle;
   instance->hid_driver = driver;
   instance->slot = slot;
   set_leds_from_id(instance);


}

static void ds3_deinit(void *device_data) {

}

static void ds3_packet_handler(void *device_data, uint8_t *packet, uint16_t size) {

}

static void ds3_set_rumble(void *device_data, enum retro_rumble_effect effect, uint16_t strength) {

}

static void ds3_get_buttons(void *device_data, input_bits_t *state) {

}

static int16_t ds3_get_axis(void *device_data, unsigned axis) {
   return 0;
}

static const char *ds3_get_name(void *device_data) {
   return "PLAYSTATION(R)3 Controller";
}

static int32_t ds3_button(void *device_data, uint16_t joykey) {
   return 0;
}

static void set_leds_from_id(ds3_instance_t *instance)
{
   /* for pads 0-3, we just light up the appropriate LED. */
   /* for higher pads, we sum up the numbers on the LEDs  */
   /* themselves, so e.g. pad 5 is 4 + 1, pad 6 is 4 + 2, */
   /* and so on. We max out at 10 because 4+3+2+1 = 10    */
   static const u8 sixaxis_leds[10][4] = {
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

   if(!buf) {
      return -1;
   }

   instance->hid_driver->send_control(instance->handle,);
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
