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
#include <retro_environment.h>
#include <retro_miscellaneous.h>

#include "joypad_connection.h"
#include "../input_defines.h"
#include "../../driver.h"
#include "../common/sony_pad_output.h"
#include "../common/sony_pad_motion.h"
#include "../common/sony_pad_touch.h"
#include "../common/pad_motion.h"

/* The DualShock 4 and its USB wireless adaptor.
 *
 * Over USB, and over Bluetooth until it is woken, its report is 0x01;
 * woken over Bluetooth - by a read of its feature report 0x05, as SDL
 * does - it is 0x11, the same two bytes further on. Over USB, feature
 * report 0x02 is read the same way. A Bluetooth stack's header is
 * skipped where there is one.
 *
 * Rumble and the light bar - the player's colour, once - go as one
 * output report built by sony_pad_output.h, from the poll, when they
 * change; nothing is sent behind a stack's header.
 *
 * A pad that passes for one says what it has in feature report 0x03,
 * as SDL reads it, and is sent its rumble and light bar together.
 *
 * The input report after its ID: the sticks' four bytes, then
 * - byte 4: the hat in the low four bits, then square, cross, circle
 *   and triangle;
 * - byte 5: L1, R1, L2, R2, share, options, L3, R3, from bit 0;
 * - byte 6: PS in bit 0, the touchpad's click in bit 1;
 * - bytes 7 and 8: L2 and R2's travel;
 * - from byte 34, not in Bluetooth's short report, the touchpad's two
 *   fingers (sony_pad_touch.h). */
#define PS4_REPORT_LEN 9

enum hidpad_ps4_make
{
   PS4_SONY = 0,
   PS4_THIRD_PARTY,
   /* Razer's: no answer to 0x03, but rumble */
   PS4_RAZER,
   /* Victrix's: hangs if sent the output report */
   PS4_NO_OUTPUT
};

struct hidpad_ps4_data
{
   struct pad_connection* connection;
   hid_driver_t *driver;
   uint32_t slot;
   input_pad_view_t view; /* the RetroPad's buttons and the sticks */
   uint8_t motors[2];   /* strong, weak: what the core asked last */
   int player;          /* told: from 1, -1 for none; 0 not yet */
   bool bluetooth;      /* the driver says so, or report 0x11 does */
   bool stack;          /* behind a Bluetooth stack's header */
   bool lights_sent;
   bool motors_due;
   /* motion: in the full reports, after the triggers */
   sony_pad_motion_t motion;
   uint8_t motion_want; /* pad_motion.h's bits */
   bool motion_seen;
   bool third;          /* not Sony's */
   bool effects;        /* it takes the output report */
   bool sensors;        /* it has motion */
   /* the pad's own scales, against the nominal: 1 for Sony's */
   float accel_mul;
   float gyro_mul;
};

/* One output report: the motors, and the first time the light bar. */
static void hidpad_ps4_send(struct hidpad_ps4_data *device)
{
   uint8_t r[SONY_PAD_REPORT_MAX];
   int player = device->player ? device->player
      : (int)(device->slot % 4) + 1;
   size_t len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DS4,
         device->bluetooth, false, device->motors[0], device->motors[1],
         device->lights_sent && !device->third ? 0 : player);

   /* and the light bar's blink, off */
   if (device->third && !device->bluetooth)
      r[1] |= 0x04;
   if (     len && device->effects
         && device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, len);
   device->lights_sent = true;
   device->motors_due  = false;
}

/* What a pad that passes for one has, from its feature report 0x03:
 * 48 bytes, 0x27 in byte 2; in byte 4 the sensors (0x02), light bar
 * (0x04) and rumble (0x08); from 10 its gyroscope's and accelerometer's
 * scales, each a numerator and a denominator. */
static void hidpad_ps4_third_party(struct hidpad_ps4_data *device,
      enum hidpad_ps4_make make)
{
   uint8_t f[64];
   unsigned num, den;

   memset(f, 0, sizeof(f));
   device->third   = true;
   device->effects = (make == PS4_RAZER);
   device->sensors = false;
   if (     !device->driver || !device->driver->get_report
         ||  device->driver->get_report(device->connection,
               HID_REPORT_FEATURE, 0x03, f, 48) != 48
         ||  f[2] != 0x27)
      return;
   device->sensors = (f[4] & 0x02) != 0;
   device->effects = (f[4] & 0x0c) && make != PS4_NO_OUTPUT;
   num = f[10] | (f[11] << 8);
   den = f[12] | (f[13] << 8);
   if (num && den)
      device->gyro_mul  = (float)num / den * SONY_PAD_GYRO_PER_DPS;
   num = f[14] | (f[15] << 8);
   den = f[16] | (f[17] << 8);
   if (num && den)
      device->accel_mul = (float)num / den * SONY_PAD_ACCEL_PER_G;
}

static void* hidpad_ps4_start(void *data, uint32_t slot,
      hid_driver_t *driver, enum hidpad_ps4_make make)
{
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_ps4_data* device    = (struct hidpad_ps4_data*)
      calloc(1, sizeof(struct hidpad_ps4_data));

   if (!device)
      return NULL;

   if (!connection)
   {
      free(device);
      return NULL;
   }

   device->connection = connection;
   device->driver     = driver;
   device->slot       = slot;
   device->bluetooth  = driver && driver->is_bluetooth
      && driver->is_bluetooth(connection);
   device->effects    = true;
   device->sensors    = true;
   device->accel_mul  = 1.0f;
   device->gyro_mul   = 1.0f;
   /* Sony's: the calibration's feature report read, which wakes the
    * full report over Bluetooth; the others are wired, as SDL has it */
   /* the others' touchpads are not known to be read the same */
   device->view.touchpad = (make == PS4_SONY);
   if (make != PS4_SONY)
      hidpad_ps4_third_party(device, make);
   else if (driver && driver->get_report)
   {
      uint8_t feature[64];
      memset(feature, 0, sizeof(feature));
      driver->get_report(connection, HID_REPORT_FEATURE,
            device->bluetooth ? 0x05 : 0x02, feature, 41);
   }

   return device;
}

static void *hidpad_ps4_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   return hidpad_ps4_start(data, slot, driver, PS4_SONY);
}

static void *hidpad_ps4_third_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return hidpad_ps4_start(data, slot, driver, PS4_THIRD_PARTY);
}

static void *hidpad_ps4_razer_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return hidpad_ps4_start(data, slot, driver, PS4_RAZER);
}

static void *hidpad_ps4_no_output_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return hidpad_ps4_start(data, slot, driver, PS4_NO_OUTPUT);
}

static void hidpad_ps4_deinit(void *data)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;

   if (device)
      free(device);
}

/* The hat's eight ways and its rest (8), as RetroPad directions. */
static const uint8_t hidpad_ps4_hat[16] = {
   1 << 0,              /* up         */
   (1 << 0) | (1 << 3), /* up, right  */
   1 << 3,              /* right      */
   (1 << 3) | (1 << 1), /* right, down*/
   1 << 1,              /* down       */
   (1 << 1) | (1 << 2), /* down, left */
   1 << 2,              /* left       */
   (1 << 2) | (1 << 0), /* left, up   */
   0, 0, 0, 0, 0, 0, 0, 0
};

/* The report's buttons as the RetroPad's, once, when it comes. */
static uint32_t hidpad_ps4_decode(const uint8_t *r)
{
   uint32_t b   = 0;
   unsigned hat = hidpad_ps4_hat[r[4] & 0x0f];
   if (hat & 1)         b |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (hat & 2)         b |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (hat & 4)         b |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (hat & 8)         b |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   if (r[4] & 0x10)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;      /* square   */
   if (r[4] & 0x20)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_B;      /* cross    */
   if (r[4] & 0x40)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_A;      /* circle   */
   if (r[4] & 0x80)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_X;      /* triangle */
   if (r[5] & 0x01)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
   if (r[5] & 0x02)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
   if (r[5] & 0x04)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_L2;
   if (r[5] & 0x08)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_R2;
   if (r[5] & 0x10)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT; /* share    */
   if (r[5] & 0x20)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_START;  /* options  */
   if (r[5] & 0x40)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_L3;
   if (r[5] & 0x80)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_R3;
   return b;
}

static void hidpad_ps4_get_buttons(void *data, input_bits_t* state)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;

   if (!device)
      return;
   *state = device->view.buttons;
}

static int16_t hidpad_ps4_get_axis(void *data, unsigned axis)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;

   if (!device || axis >= INPUT_PAD_VIEW_AXES)
      return 0;
   return device->view.axes[axis];
}

static void hidpad_ps4_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   const uint8_t *r;
   unsigned i, rest;

   if (!device || !packet)
      return;

   /* a Bluetooth stack's DATA | INPUT header */
   if (size && packet[0] == 0xa1)
   {
      packet++;
      size--;
      device->stack = true;
   }
   /* read where it lies: the buttons decoded and the sticks kept, not
    * the report */
   if (size >= 1 + PS4_REPORT_LEN && packet[0] == 0x01)
      r = packet + 1;
   else if (size >= 3 + PS4_REPORT_LEN && packet[0] == 0x11)
   {
      r = packet + 3;
      device->bluetooth = true;
   }
   else
      return;
   for (i = 0; i < 4; i++)
   {
      int val = ((int)r[i] << 8) - 0x8000;
      device->view.axes[i] = (int16_t)((abs(val) > 0x1000) ? val : 0);
   }
   for (i = 0; i < 2; i++)
      device->view.axes[4 + i] = (int16_t)((r[7 + i] << 7) | (r[7 + i] >> 1));
   pad_view_set_buttons(&device->view, hidpad_ps4_decode(r),
         (r[6] & 0x01) != 0);
   /* a timestamp and the temperature, then the gyroscope and the
    * accelerometer; not in Bluetooth's short report */
   rest = size - (unsigned)(r - packet);
   if (device->view.touchpad && rest >= 42)
      sony_pad_touch(&device->view, r + 34, SONY_PAD_TOUCH_H_DS4,
            (r[6] & 0x02) != 0);
   else
      device->view.touch_down = 0;
   if (device->motion_want && device->sensors && rest >= 24)
   {
      for (i = 0; i < 3; i++)
      {
         device->motion.gyro[i]  = sony_pad_le16(r + 12 + 2 * i);
         device->motion.accel[i] = sony_pad_le16(r + 18 + 2 * i);
      }
      device->motion_seen = true;
   }

   if (!device->stack && (!device->lights_sent || device->motors_due))
      hidpad_ps4_send(device);
}

static void hidpad_ps4_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   unsigned idx  = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;
   uint8_t motor = (uint8_t)(strength >> 8);

   if (device && device->motors[idx] != motor)
   {
      device->motors[idx] = motor;
      device->motors_due  = true;
   }
}

static void hidpad_ps4_set_player(void *data, unsigned player)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   if (device)
   {
      device->player      = player ? (int)player : -1;
      device->lights_sent = false;
   }
}

static int32_t hidpad_ps4_button(void *data, uint16_t joykey)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static bool hidpad_ps4_set_sensor(void *data,
      enum retro_sensor_action action)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   if (     !device || !device->sensors
         || !pad_motion_want(&device->motion_want, action))
      return false;
   if (!device->motion_want)
      device->motion_seen = false;
   return true;
}

/* at the pad's nominal scale, as winraw reads it */
static bool hidpad_ps4_sensor(void *data, unsigned id, float *value)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   if (!device || !device->motion_seen)
      return false;
   if (id <= RETRO_SENSOR_ACCELEROMETER_Z
         ? !(device->motion_want & PAD_MOTION_ACCEL)
         : !(device->motion_want & PAD_MOTION_GYRO))
      return false;
   if (!sony_pad_motion_value(&device->motion, id, value))
      return false;
   *value *= id <= RETRO_SENSOR_ACCELEROMETER_Z
      ? device->accel_mul : device->gyro_mul;
   return true;
}

static const input_pad_view_t *hidpad_ps4_view(void *data)
{
   return &((struct hidpad_ps4_data*)data)->view;
}

pad_connection_interface_t pad_connection_ps4 = {
   hidpad_ps4_init,
   hidpad_ps4_deinit,
   hidpad_ps4_packet_handler,
   hidpad_ps4_set_rumble,
   hidpad_ps4_get_buttons,
   hidpad_ps4_get_axis,
   NULL, /* get_name */
   hidpad_ps4_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_ps4_set_player,
   hidpad_ps4_view,
   NULL,  /* idle */
   hidpad_ps4_set_sensor,
   hidpad_ps4_sensor
};

/* the pads that pass for one */
pad_connection_interface_t pad_connection_ps4_third_party = {
   hidpad_ps4_third_init,
   hidpad_ps4_deinit,
   hidpad_ps4_packet_handler,
   hidpad_ps4_set_rumble,
   hidpad_ps4_get_buttons,
   hidpad_ps4_get_axis,
   NULL, /* get_name */
   hidpad_ps4_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_ps4_set_player,
   hidpad_ps4_view,
   NULL,  /* idle */
   hidpad_ps4_set_sensor,
   hidpad_ps4_sensor
};
pad_connection_interface_t pad_connection_ps4_razer = {
   hidpad_ps4_razer_init,
   hidpad_ps4_deinit,
   hidpad_ps4_packet_handler,
   hidpad_ps4_set_rumble,
   hidpad_ps4_get_buttons,
   hidpad_ps4_get_axis,
   NULL, /* get_name */
   hidpad_ps4_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_ps4_set_player,
   hidpad_ps4_view,
   NULL,  /* idle */
   hidpad_ps4_set_sensor,
   hidpad_ps4_sensor
};
pad_connection_interface_t pad_connection_ps4_no_output = {
   hidpad_ps4_no_output_init,
   hidpad_ps4_deinit,
   hidpad_ps4_packet_handler,
   hidpad_ps4_set_rumble,
   hidpad_ps4_get_buttons,
   hidpad_ps4_get_axis,
   NULL, /* get_name */
   hidpad_ps4_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_ps4_set_player,
   hidpad_ps4_view,
   NULL,  /* idle */
   hidpad_ps4_set_sensor,
   hidpad_ps4_sensor
};
