/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

/* The Nintendo Switch 2 Pro Controller, on USB.
 *
 * It sends nothing until it is woken through its second interface,
 * which is its HID driver's to do (winusb_hid.c). Then a 64-byte
 * report every few milliseconds: from byte 5, the buttons - Y X B A,
 * -, -, R, ZR; then -, +, R3, L3, Home, Capture, C; then down, up,
 * right, left, -, -, L, ZL; then the grips, right and left - and from
 * byte 11 the sticks, 12 bits an axis, up positive. From byte 0x2b a
 * sensor clock, and from 0x31 the accelerometer and gyroscope, x y z each.
 *
 * Rumble is output report 0x02: a sequence number, then each hand's
 * actuator, a high and a low band, kept up while it lasts. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "joypad_connection.h"
#include "../common/pad_motion.h"
#include "../input_defines.h"

#define SWITCH2_PRO_REPORT_LEN   64
/* reports between rumble reports while it is held: about 12 ms */
#define SWITCH2_PRO_RUMBLE_HOLD  3
/* past this the actuators are only louder, and worn */
#define SWITCH2_PRO_RUMBLE_MAX   29000
/* sensor clock samples timed before the gyroscope's range is known */
#define SWITCH2_PRO_CLOCK_SAMPLES 100

struct hidpad_switch2_pro_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   uint32_t slot;
   /* the RetroPad's buttons, then Home, Capture, C and the right and
    * left grips as 16 to 20; the sticks as 0 to 3 */
   input_pad_view_t view;
   /* the sticks' reach either way, widened as they go further */
   int16_t reach_up[4];
   int16_t reach_down[4];
   uint16_t motors[2];  /* strong, weak: what the core asked last */
   uint8_t sequence;
   uint8_t since_sent;  /* reports since rumble last went */
   bool motors_due;
   int16_t imu[6];      /* accel x y z, gyro x y z, as sent */
   uint32_t clock_first;
   uint8_t clock_count;
   uint8_t imu_want;    /* PAD_MOTION_* a core asked for */
   bool imu_seen;
   float gyro_range;    /* full scale, radians a second */
};

static void *hidpad_switch2_pro_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_switch2_pro_data *device;
   unsigned i;

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_switch2_pro_data*)
            calloc(1, sizeof(struct hidpad_switch2_pro_data))))
      return NULL;

   device->connection = connection;
   device->driver     = driver;
   device->slot       = slot;
   /* most sticks reach further; the first push that far widens it */
   for (i = 0; i < 4; i++)
   {
      device->reach_up[i]   = 1200;
      device->reach_down[i] = 1200;
   }
   return device;
}

static void hidpad_switch2_pro_deinit(void *data)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;
   if (device)
      free(device);
}

static int16_t hidpad_switch2_pro_scale(
      struct hidpad_switch2_pro_data *device, unsigned axis, int raw,
      bool invert)
{
   int32_t v = raw - 2048;
   int32_t out;

   if (v >= 0)
   {
      if (v > device->reach_up[axis])
         device->reach_up[axis] = (int16_t)v;
      out = v * 32767 / device->reach_up[axis];
   }
   else
   {
      if (-v > device->reach_down[axis])
         device->reach_down[axis] = (int16_t)-v;
      out = v * 32767 / device->reach_down[axis];
   }
   return (int16_t)(invert ? -out : out);
}

static void hidpad_switch2_pro_decode(struct hidpad_switch2_pro_data *device,
      const uint8_t *r)
{
   uint32_t b = 0;

   if (r[5] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
   if (r[5] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
   if (r[5] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
   if (r[5] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
   if (r[5] & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
   if (r[5] & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R2;
   if (r[6] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
   if (r[6] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
   if (r[6] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R3;
   if (r[6] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L3;
   if (r[7] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (r[7] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (r[7] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   if (r[7] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (r[7] & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
   if (r[7] & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L2;
   b |= (uint32_t)((r[6] >> 4) & 0x07) << 16;   /* Home, Capture, C */
   b |= (uint32_t)(r[8] & 0x03) << 19;          /* the grips */

   device->view.axes[0] = hidpad_switch2_pro_scale(device, 0,
         r[11] | ((r[12] & 0x0f) << 8), false);
   device->view.axes[1] = hidpad_switch2_pro_scale(device, 1,
         (r[12] >> 4) | (r[13] << 4), true);
   device->view.axes[2] = hidpad_switch2_pro_scale(device, 2,
         r[14] | ((r[15] & 0x0f) << 8), false);
   device->view.axes[3] = hidpad_switch2_pro_scale(device, 3,
         (r[15] >> 4) | (r[16] << 4), true);
   pad_view_set_buttons(&device->view, b, (r[6] & 0x10) != 0);
}

/* As SDL: the sensor clock ticks microseconds on most pads, and the
 * gyroscope then reaches 34.8 rad/s; on the others it does not, and
 * the gyroscope reaches 40. */
static void hidpad_switch2_pro_motion(struct hidpad_switch2_pro_data *device,
      const uint8_t *r)
{
   unsigned i;
   uint32_t clock = (uint32_t)r[0x2b] | ((uint32_t)r[0x2c] << 8)
      | ((uint32_t)r[0x2d] << 16) | ((uint32_t)r[0x2e] << 24);

   if (!clock)
      return;
   if (device->gyro_range == 0.0f)
   {
      device->clock_count++;
      if (device->clock_count >= 5 && !device->clock_first)
      {
         device->clock_first = clock;
         device->clock_count = 0;
      }
      else if (device->clock_count == SWITCH2_PRO_CLOCK_SAMPLES)
      {
         uint64_t coeff = (uint64_t)(uint32_t)(clock - device->clock_first)
               * 1000 / (SWITCH2_PRO_CLOCK_SAMPLES * 4);
         if ((coeff + 100000) / 200000 == 5)
            device->gyro_range = 34.8f;
         else if (coeff)
            device->gyro_range = 40.0f;
         device->clock_first = 0;
         device->clock_count = 0;
      }
      return;
   }
   for (i = 0; i < 6; i++)
      device->imu[i] = (int16_t)(r[0x31 + i * 2] | (r[0x32 + i * 2] << 8));
   device->imu_seen = true;
}

/* Both hands' actuators alike: the weak motor the high band at about
 * 320 Hz, the strong the low at about 160 Hz. */
static void hidpad_switch2_pro_send_rumble(
      struct hidpad_switch2_pro_data *device)
{
   static const unsigned high_freq = 0x187;
   static const unsigned low_freq  = 0x112;
   uint8_t r[SWITCH2_PRO_REPORT_LEN];
   unsigned high = (unsigned)((uint32_t)device->motors[1]
         * SWITCH2_PRO_RUMBLE_MAX / 0xffff);
   unsigned low  = (unsigned)((uint32_t)device->motors[0]
         * SWITCH2_PRO_RUMBLE_MAX / 0xffff);

   memset(r, 0, sizeof(r));
   r[0] = 0x02;
   r[1] = (uint8_t)(0x50 | (device->sequence++ & 0x0f));
   r[2] = (uint8_t)(high_freq & 0xff);
   r[3] = (uint8_t)(((high >> 4) & 0xfc) | ((high_freq >> 8) & 0x03));
   r[4] = (uint8_t)((high >> 12) | (low_freq << 4));
   r[5] = (uint8_t)((low & 0xc0) | ((low_freq >> 4) & 0x3f));
   r[6] = (uint8_t)(low >> 8);
   memmove(r + 0x11, r + 1, 6);

   if (device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, sizeof(r));
   device->since_sent = 0;
   device->motors_due = false;
}

static void hidpad_switch2_pro_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;

   if (!device || !packet || size < SWITCH2_PRO_REPORT_LEN)
      return;
   hidpad_switch2_pro_decode(device, packet);
   hidpad_switch2_pro_motion(device, packet);

   if (device->since_sent < 0xff)
      device->since_sent++;
   if (     device->since_sent >= SWITCH2_PRO_RUMBLE_HOLD
         && (device->motors_due || device->motors[0] || device->motors[1]))
      hidpad_switch2_pro_send_rumble(device);
}

static void hidpad_switch2_pro_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (device && device->motors[idx] != strength)
   {
      device->motors[idx] = strength;
      device->motors_due  = true;
   }
}

static void hidpad_switch2_pro_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;

   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

/* The sticks as 0 to 3; ZL and ZR have no travel. */
static int16_t hidpad_switch2_pro_get_axis(void *data, unsigned axis)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;
   if (!device || axis >= 4)
      return 0;
   return device->view.axes[axis];
}

static const char *hidpad_switch2_pro_get_name(void *data)
{
   return "Nintendo Switch 2 Pro Controller";
}

static int32_t hidpad_switch2_pro_button(void *data, uint16_t joykey)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;
   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_switch2_pro_view(void *data)
{
   return &((struct hidpad_switch2_pro_data*)data)->view;
}

/* The pad always sends its sensors; only what a core sees changes. */
static bool hidpad_switch2_pro_set_sensor(void *data,
      enum retro_sensor_action action)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;
   return device && pad_motion_want(&device->imu_want, action);
}

/* SDL's axes, as SDL turns the pad's, without the gyroscope's flash
 * biases, which only the bulk endpoint reads. */
static bool hidpad_switch2_pro_sensor(void *data, unsigned id, float *value)
{
   struct hidpad_switch2_pro_data *device =
      (struct hidpad_switch2_pro_data*)data;
   float accel[3], gyro[3];
   float a, g;

   if (!device || !device->imu_seen)
      return false;
   a        = 8.0f / 32767.0f;
   g        = device->gyro_range / 32767.0f;
   accel[0] =   a * device->imu[0];
   accel[1] =   a * device->imu[2];
   accel[2] = -(a * device->imu[1]);
   gyro[0]  =   g * device->imu[3];
   gyro[1]  =   g * device->imu[5];
   gyro[2]  = -(g * device->imu[4]);
   return pad_motion_value(accel, gyro, device->imu_want, id, value);
}

pad_connection_interface_t pad_connection_switch2_pro = {
   hidpad_switch2_pro_init,
   hidpad_switch2_pro_deinit,
   hidpad_switch2_pro_packet_handler,
   hidpad_switch2_pro_set_rumble,
   hidpad_switch2_pro_get_buttons,
   hidpad_switch2_pro_get_axis,
   hidpad_switch2_pro_get_name,
   hidpad_switch2_pro_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_switch2_pro_view,
   NULL,  /* idle */
   hidpad_switch2_pro_set_sensor,
   hidpad_switch2_pro_sensor
};
