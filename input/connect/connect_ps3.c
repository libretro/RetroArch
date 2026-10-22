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
#include "../../verbosity.h"
#include "../common/pad_motion.h"

#define DS3_ACTIVATION_REPORT_ID 0xf4

#define SIXAXIS_REPORT_0xF2_SIZE 17
#define SIXAXIS_REPORT_0xF5_SIZE 8

typedef struct ds3_instance
{
   hid_driver_t *driver;
   void *handle;
   int slot;
   int player;     /* told: from 1, -1 for none; 0 not yet */
   bool led_set;
   bool motors_due;
   bool shanwan;        /* ShanWan's: no output report at init */
   uint8_t motion_want; /* pad_motion.h's bits: the accelerometer alone */
   uint16_t accel[3];   /* as the report has them, 511 at rest */
   /* the RetroPad's buttons and PS as 16; the sticks as 0 to 3, the
    * pressures 4 to 15 */
   input_pad_view_t view;
   uint8_t motors[2]; /* strong, weak: what the core asked last */
} ds3_instance_t;

struct __attribute__((__packed__)) sixaxis_led
{
    uint8_t time_enabled; /* the total time the led is active (0xff means forever) */
    uint8_t duty_length;  /* how long a cycle is in deciseconds (0 means "really fast") */
    uint8_t enabled;
    uint8_t duty_off; /* % of duty_length the led is off (0xff means 100%) */
    uint8_t duty_on;  /* % of duty_length the led is on (0xff mean 100%) */
};

struct __attribute__((__packed__)) sixaxis_rumble
{
    uint8_t right_duration; /* Right motor duration (0xff means forever) */
    uint8_t right_motor_on; /* Right (small) motor on/off, only supports values of 0 or 1 (off/on) */
    uint8_t left_duration;    /* Left motor duration (0xff means forever) */
    uint8_t left_motor_force; /* left (large) motor, supports force values from 0 to 255 */
};

struct __attribute__((__packed__)) sixaxis_output_report
{
    uint8_t report_id;
    uint8_t padding1;
    struct sixaxis_rumble rumble;
    uint8_t padding2[4];
    uint8_t leds_bitmap; /* bitmap of enabled LEDs: LED_1 = 0x02, LED_2 = 0x04, ... */
    struct sixaxis_led led[4];    /* LEDx at (4 - x) */
    struct sixaxis_led _reserved; /* LED5, not actually soldered */
    uint8_t unknown[13];
};

struct __attribute__((__packed__)) sixaxis_activation_report
{
   uint8_t report_id;
   uint8_t knock[4];
};

union sixaxis_activation_report_f4
{
   uint8_t buf[5];
   struct sixaxis_activation_report data;
};

union sixaxis_output_report_01
{
    uint8_t buf[49];
    struct sixaxis_output_report data;
};

static int ds3_set_operational(ds3_instance_t *instance)
{
   static const union sixaxis_activation_report_f4 ds3_activation_packet = {
      { 0xf4, 0x42, 0x0c, 0x00, 0x00 }
   };
   int ret_f2, ret_f4;
   uint8_t usb_packet[64] = { 0x00 };

   if (     !instance->driver->set_report
         || !instance->driver->get_report)
   {
      RARCH_ERR("[Sixaxis] This HID implementation does not support the Sony Sixaxis controller:\n");
      if (!instance->driver->set_report)
         RARCH_ERR("[Sixaxis] set_report implementation is missing.\n");
      if (!instance->driver->get_report)
         RARCH_ERR("[Sixaxis] get_report implementation is missing.\n");
      return -1;
   }

   /* On USB the pad starts when 0xF2 is read; over Bluetooth, when
    * 0xF4 is set. A pad on USB refuses 0xF4, so either one is enough. */
   ret_f2 = instance->driver->get_report(
         instance->handle,
         HID_REPORT_FEATURE,
         0xf2,
         (uint8_t*)&usb_packet,
         SIXAXIS_REPORT_0xF2_SIZE);
   ret_f4 = instance->driver->set_report(instance->handle,
         HID_REPORT_FEATURE,
         ds3_activation_packet.data.report_id,
         (uint8_t*)ds3_activation_packet.buf,
         sizeof(ds3_activation_packet.buf));
   if (ret_f2 < 0 && ret_f4 < 0)
   {
      RARCH_LOG("[Sixaxis] Neither 0xf2 could be read nor 0xf4 set.\n");
      return -1;
   }

   if (instance->driver->get_report(
         instance->handle,
         HID_REPORT_FEATURE,
         0xf5,
         (uint8_t*)&usb_packet,
         SIXAXIS_REPORT_0xF5_SIZE) < 0)
      RARCH_DBG("[Sixaxis] Feature report 0xf5 could not be read.\n");

   return 0;
}

static uint8_t ds3_get_leds(unsigned pad_number)
{
   switch(pad_number)
   {
      case 1:
      case 2:
      case 3:
      case 4:
         return 1 << pad_number; 
      case 5:
         return (1 << 1) | (1 << 4);
      case 6:
         return (1 << 2) | (1 << 4);
      case 7:
         return (1 << 3) | (1 << 4);
      case 8:
         return (1 << 3) | (1 << 1) | (1 << 4);
      case 9:
         return (1 << 2) | (1 << 3) | (1 << 4);
      case 10:
      default:
         break;
   }

   return (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
}

static int ds3_send_output_report(ds3_instance_t *instance)
{
   static const union sixaxis_output_report_01 default_report = 
   {
      {
         0x01, /* report ID */
         0x00, /* padding */
         0xff, 0x00, /* right rumble */
         0xff, 0x00, /* left rumble */
         0x00, 0x00, 0x00, 0x00, /* gyro */
         0x00, /* LED bitmap */
         0xff, 0x27, 0x10, 0x00, 0x32, /* LED 1 config */
         0xff, 0x27, 0x10, 0x00, 0x32, /* LED 2 config */
         0xff, 0x27, 0x10, 0x00, 0x32, /* LED 3 config */
         0xff, 0x27, 0x10, 0x00, 0x32, /* LED 4 config */
         0x00, 0x00, 0x00, 0x00, 0x00, /* LED 5 config (unusable/unsoldered) */
         0x00, 0x00, 0x00, 0x00, 0x00, /* unknown */
         0x00, 0x00, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00
      }
   };
   struct sixaxis_output_report report = {0};
   uint8_t *packet                     = (uint8_t *)&report;

   /* Initialize the report with default values */
   memcpy(&report, &default_report, sizeof(struct sixaxis_output_report));
   /* the small motor is only on or off */
   report.rumble.right_motor_on   = instance->motors[1] ? 1 : 0;
   report.rumble.left_motor_force = instance->motors[0];
   report.leds_bitmap = instance->player < 0 ? 0
      : ds3_get_leds(instance->player ? (unsigned)instance->player
            : (unsigned)instance->slot + 1);

   return instance->driver->set_report(instance->handle, HID_REPORT_OUTPUT, report.report_id, packet, sizeof(report));
}

static void ds3_update_pad_state(ds3_instance_t *instance,
      const uint8_t *report)
{
   uint32_t i, pressed_keys;
   uint32_t buttons = 0;

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

   pressed_keys          = 
          report[2]
      |  (report[3]         << 8)
      | ((report[4] & 0x01) << 16);

   for (i = 0; i < 17; i++)
      buttons |= (pressed_keys & (1 << i)) ?
         (1 << button_mapping[i]) : 0;
   pad_view_set_buttons(&instance->view, buttons, false);
}

static void ds3_update_analog_state(ds3_instance_t *instance,
      const uint8_t *report)
{
   int pad_axis;

   /* The pad's 0 is left and up, as libretro's negative is: no axis is
    * turned round. */
   for (pad_axis = 0; pad_axis < 4; pad_axis++)
      instance->view.axes[pad_axis] =
         (int16_t)(report[6 + pad_axis] * 257 - 32768);
}


/* A ShanWan PS2-to-PS3 converter's 7-byte report, as SDL reads it:
 * the right stick, the left, the hat and triangle, circle, cross and
 * square, then L1, R1, L2, R2, select, start, L3 and R3. Its buttons
 * have no pressure: a press is all of it. */
static void ds3_mini_report(ds3_instance_t *instance, const uint8_t *r)
{
   static const uint8_t hat[16] = {
      1, 1 | 8, 8, 8 | 2, 2, 2 | 4, 4, 4 | 1,
      0, 0, 0, 0, 0, 0, 0, 0 };
   static const uint32_t face[4] = {
      RETRO_DEVICE_ID_JOYPAD_X, RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_Y };
   static const uint32_t rest[8] = {
      RETRO_DEVICE_ID_JOYPAD_L,      RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_L2,     RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_SELECT, RETRO_DEVICE_ID_JOYPAD_START,
      RETRO_DEVICE_ID_JOYPAD_L3,     RETRO_DEVICE_ID_JOYPAD_R3 };
   /* the pressure axes' buttons, in ds3_pressure_byte's order */
   static const uint32_t pressed[12] = {
      RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2,
      RETRO_DEVICE_ID_JOYPAD_B,  RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_Y,  RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_L,  RETRO_DEVICE_ID_JOYPAD_R,
      RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT };
   uint32_t b = 0;
   unsigned i, h = hat[r[4] & 0x0f];

   if (h & 1) b |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (h & 2) b |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (h & 4) b |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (h & 8) b |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   for (i = 0; i < 4; i++)
      if (r[4] & (0x10 << i))
         b |= 1u << face[i];
   for (i = 0; i < 8; i++)
      if (r[5] & (1 << i))
         b |= 1u << rest[i];
   pad_view_set_buttons(&instance->view, b, false);

   instance->view.axes[0] = (int16_t)(r[2] * 257 - 32768);
   instance->view.axes[1] = (int16_t)(r[3] * 257 - 32768);
   instance->view.axes[2] = (int16_t)(r[0] * 257 - 32768);
   instance->view.axes[3] = (int16_t)(r[1] * 257 - 32768);
   for (i = 0; i < 12; i++)
      instance->view.axes[4 + i] = (b & (1u << pressed[i])) ? 0x7fff : 0;
}

static void *ds3_start(void *handle, uint32_t slot, hid_driver_t *driver,
      bool shanwan)
{
   ds3_instance_t *instance = (ds3_instance_t *)
      calloc(1, sizeof(ds3_instance_t));
   if (!instance)
      return NULL;

   instance->handle     = handle;
   instance->driver     = driver;
   instance->slot       = slot;
   instance->shanwan    = shanwan;

   if (instance->driver->set_protocol)
      instance->driver->set_protocol(instance->handle, 1);

   /* not to ShanWan's: as SDL found, it can rumble them non-stop */
   if (!shanwan && ds3_send_output_report(instance) < 0)
   {
      RARCH_LOG("[Sixaxis] Failed to send output report.\n");
      goto error;
   }

   if (ds3_set_operational(instance) < 0)
   {
      RARCH_LOG("[Sixaxis] Failed to set operational mode.\n");
      goto error;
   }

   return instance;
error:
   free(instance);
   return NULL;
}

static void *ds3_init(void *handle, uint32_t slot, hid_driver_t *driver)
{
   return ds3_start(handle, slot, driver, false);
}

static void *ds3_shanwan_init(void *handle, uint32_t slot,
      hid_driver_t *driver)
{
   return ds3_start(handle, slot, driver, true);
}

static void ds3_deinit(void *device_data)
{
   if (device_data)
      free(device_data);
}

/* The input report's bytes with the pressure of L2 and R2, then
 * cross, circle, square, triangle, L1, R1, up, down, left and right:
 * axes 4 to 15, as SDL gives them. */
static const uint8_t ds3_pressure_byte[12] = {
   18, 19, 24, 23, 25, 22, 20, 21, 14, 16, 17, 15
};

static void ds3_packet_handler(void *device_data,
      uint8_t *packet, uint16_t size)
{
   ds3_instance_t *device   = (ds3_instance_t *)device_data;

   if (!device)
      return;

   /* lights and motors go in one report, from here: a core changing
    * its rumble every frame costs one report a packet at most */
   if (!device->led_set || device->motors_due)
   {
      ds3_send_output_report(device);
      device->led_set    = true;
      device->motors_due = false;
   }

   /* the input report, whole; nothing else is the pad's state. It is
    * the backend's, read where it lies: what the getters answer from
    * is taken out of it here, not the report kept. */
   if (size == 7)
   {
      ds3_mini_report(device, packet);
      return;
   }
   /* 0xFF where its buttons begin: not a state, as SDL found */
   if (size < 49 || packet[0] != 0x01 || packet[1] == 0xff)
      return;

   ds3_update_pad_state(device, packet);
   ds3_update_analog_state(device, packet);
   {
      unsigned i;
      for (i = 0; i < 12; i++)
      {
         unsigned b = packet[ds3_pressure_byte[i]];
         device->view.axes[4 + i] = (int16_t)((b << 7) | (b >> 1));
      }
      /* the accelerometer, big-endian, X then Y then Z from 41 */
      for (i = 0; i < 3; i++)
         device->accel[i] = (uint16_t)((packet[41 + 2 * i] << 8)
               | packet[42 + 2 * i]);
   }
}

/* The accelerometer, as SDL reads it: 113 counts a g about 511, on
 * SDL's axes; the pad's one-axis gyroscope is not given. */
static bool ds3_set_sensor(void *device_data, enum retro_sensor_action action)
{
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   if (     !device
         || action == RETRO_SENSOR_GYROSCOPE_ENABLE
         || !pad_motion_want(&device->motion_want, action))
      return false;
   return true;
}

static bool ds3_sensor(void *device_data, unsigned id, float *value)
{
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   float accel[3];
   if (!device || !device->accel[0])
      return false;
   accel[0] =  ((float)device->accel[0] - 511.0f) / 113.0f;
   accel[1] = -((float)device->accel[2] - 511.0f) / 113.0f;
   accel[2] = -((float)device->accel[1] - 511.0f) / 113.0f;
   return pad_motion_value(accel, NULL, device->motion_want, id, value);
}

static void ds3_set_rumble(void *device_data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   unsigned idx           = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;
   uint8_t motor          = (uint8_t)(strength >> 8);

   if (device && device->motors[idx] != motor)
   {
      device->motors[idx] = motor;
      device->motors_due  = true;
   }
}

/* Its lights go with the next report the pad sends. */
static void ds3_set_player(void *device_data, unsigned player)
{
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   if (device)
   {
      device->player  = player ? (int)player : -1;
      device->led_set = false;
   }
}

static void ds3_get_buttons(void *device_data, input_bits_t *state)
{
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ds3_get_axis(void *device_data, unsigned axis)
{
   ds3_instance_t *device = (ds3_instance_t *)device_data;
   int16_t val;
   if (!device)
      return 0;
   if (axis < INPUT_PAD_VIEW_AXES)
      return device->view.axes[axis];
   /* a stick's direction, bound: what wiiu_hid's state() asks */
   if (AXIS_NEG_GET(axis) < 4)
      return ((val = device->view.axes[AXIS_NEG_GET(axis)]) < 0) ? val : 0;
   if (AXIS_POS_GET(axis) < 4)
      return ((val = device->view.axes[AXIS_POS_GET(axis)]) > 0) ? val : 0;
   return 0;
}

static const char *ds3_get_name(void *device_data)
{
   return "PLAYSTATION(R)3 Controller";
}

static int32_t ds3_button(void *device_data, uint16_t joykey)
{
   ds3_instance_t *device = (ds3_instance_t *)device_data;

   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *ds3_view(void *device_data)
{
   return &((ds3_instance_t *)device_data)->view;
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
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   ds3_set_player,
   ds3_view,
   NULL,  /* idle */
   ds3_set_sensor,
   ds3_sensor
};

pad_connection_interface_t pad_connection_ps3_shanwan = {
   ds3_shanwan_init,
   ds3_deinit,
   ds3_packet_handler,
   ds3_set_rumble,
   ds3_get_buttons,
   ds3_get_axis,
   ds3_get_name,
   ds3_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   ds3_set_player,
   ds3_view,
   NULL,  /* idle */
   ds3_set_sensor,
   ds3_sensor
};