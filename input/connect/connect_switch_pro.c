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

/* The Nintendo Switch Pro Controller.
 *
 * On USB it says nothing until it is greeted: 0x80 0x02, 0x80 0x03
 * (a faster link to its own chip), 0x80 0x02 again, each answered with
 * 0x81, then 0x80 0x04 starts its reports. Pads that pass for one -
 * 8BitDo's among them - do not answer 0x80 0x03, so a step not answered
 * goes on after a while, by the clock: such a pad sends nothing to count
 * the while by. Reports that stop over USB are started again. Over
 * Bluetooth it sends the short report 0x3F until it is told to send the
 * full one.
 *
 * Commands are output report 0x01 - a packet number, the rumble, then
 * the command - answered by input report 0x21, which also carries the
 * pad's state. One goes at a time, the next when it is answered, from
 * the packet handler; one not answered is sent again after a while of
 * reports. Rumble alone is report 0x10.
 *
 * Full report 0x30 (and 0x21): from byte 3, the buttons - Y X B A,
 * -, -, R, ZR; then -, +, R3, L3, Home, Capture; then down, up, right,
 * left, -, -, L, ZL - then the sticks, 12 bits an axis, up positive.
 * Each stick's centre and reach are in the pad's flash.
 *
 * Motion: report 0x30 has three samples of the accelerometer and the
 * gyroscope from byte 13, the newest first, once subcommand 0x40 has
 * turned them on - which it is while a core asks for them. Their
 * calibration is in the flash too; they are given on the axes SDL
 * gives them, as the DualShock 4's are (sony_pad_motion.h). */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>

#include "joypad_connection.h"
#include "../input_defines.h"
#include "../../verbosity.h"
#include "../common/pad_motion.h"

#define SWITCH_PRO_FULL_LEN      12
#define SWITCH_PRO_SIMPLE_LEN    12
#define SWITCH_PRO_COMMAND_LEN   49
#define SWITCH_PRO_USB_LEN       64
#define SWITCH_PRO_RUMBLE_LEN    10

/* reports without the answer before a command is sent again, and how
 * often it is */
#define SWITCH_PRO_RETRY_AFTER   32
#define SWITCH_PRO_TRIES         4
/* how long a pad may be quiet before a step goes on: SDL's wait */
#define SWITCH_PRO_QUIET_US      100000
/* reports between rumble reports, and between refreshes of one held */
#define SWITCH_PRO_RUMBLE_GAP    2
#define SWITCH_PRO_RUMBLE_HOLD   6

#define SWITCH_PRO_USB_HELLO     0  /* 0x80 0x02 sent */
#define SWITCH_PRO_USB_BAUD      1  /* 0x80 0x03 sent */
#define SWITCH_PRO_USB_HELLO2    2  /* 0x80 0x02 sent again */
#define SWITCH_PRO_COMMANDS      3
#define SWITCH_PRO_READY         4

/* the commands, in order */
#define SWITCH_PRO_CMD_MODE      0
#define SWITCH_PRO_CMD_RUMBLE    1
#define SWITCH_PRO_CMD_LIGHTS    2
#define SWITCH_PRO_CMD_FACTORY   3
#define SWITCH_PRO_CMD_USER      4
#define SWITCH_PRO_CMD_IMU_CAL   5
#define SWITCH_PRO_CMD_IMU_USER  6
#define SWITCH_PRO_CMD_COUNT     7
/* sent on its own, its answer not waited for */
#define SWITCH_PRO_CMD_IMU       7

#define SWITCH_PRO_FACTORY_CAL   0x603d
#define SWITCH_PRO_FACTORY_LEN   18
#define SWITCH_PRO_USER_CAL      0x8010
#define SWITCH_PRO_USER_LEN      22
#define SWITCH_PRO_IMU_CAL       0x6020
#define SWITCH_PRO_IMU_CAL_LEN   24
#define SWITCH_PRO_IMU_USER      0x8026
#define SWITCH_PRO_IMU_USER_LEN  20
#define SWITCH_PRO_IMU_AT        13   /* the newest sample in report 0x30 */

struct hidpad_switch_pro_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   uint32_t slot;
   /* the RetroPad's buttons, then Home and Capture as 16 and 17; the
    * sticks as 0 to 3 */
   input_pad_view_t view;
   /* the sticks' calibration, by axis: centre, and reach either way */
   int16_t centre[4];
   int16_t reach_up[4];
   int16_t reach_down[4];
   uint16_t motors[2];  /* strong, weak: what the core asked last */
   uint8_t rumble[8];   /* the motors as the pad takes them */
   uint8_t state;
   uint8_t command;
   uint8_t tries;
   uint8_t number;      /* the output's packet number */
   uint8_t since_sent;  /* reports since output last went */
   bool heard;          /* a report has come: the first is logged */
   bool quiet_rest;     /* at rest with no amplitude: see the N64 pad's */
   /* motion: the newest sample as the pad sends it - accelerometer X Y
    * Z, then gyroscope X Y Z - and its calibration: counts to g and to
    * radians a second, and the gyroscope's reading at rest */
   int16_t imu[6];
   float accel_scale[3];
   float gyro_scale[3];
   int16_t gyro_rest[3];
   int16_t accel_sens[3]; /* the factory's, for the user's origins */
   int16_t gyro_sens[3];
   bool imu_factory;      /* the factory's calibration was read */
   uint8_t imu_want;    /* bit 0 the accelerometer, 1 the gyroscope */
   bool imu_on;         /* as the pad was last told */
   bool imu_seen;       /* a report has had a sample */
   retro_time_t quiet_since; /* the first poll without a report; 0: one came */
   int player;          /* told: from 1, -1 for none; 0 not yet */
   bool lights_due;     /* told another once its commands are done */
   bool bluetooth;
   bool stack;          /* behind a Bluetooth stack's header */
   bool motors_due;
};

static void hidpad_switch_pro_write(struct hidpad_switch_pro_data *device,
      uint8_t *r, size_t len)
{
   if (device->driver && device->driver->send_control && !device->stack)
      device->driver->send_control(device->connection, r, len);
   device->since_sent = 0;
}

static void hidpad_switch_pro_usb_command(
      struct hidpad_switch_pro_data *device, uint8_t command)
{
   uint8_t r[SWITCH_PRO_USB_LEN];
   memset(r, 0, sizeof(r));
   r[0] = 0x80;
   r[1] = command;
   hidpad_switch_pro_write(device, r, sizeof(r));
}

/* The player's lights as the console lights them: one for player 1,
 * four for 4, then patterns to 8; its slot's until it is told. */
static uint8_t hidpad_switch_pro_lights(struct hidpad_switch_pro_data *device)
{
   static const uint8_t pattern[8] = {
      0x01, 0x03, 0x07, 0x0f, 0x09, 0x05, 0x0d, 0x06 };
   int player = device->player ? device->player : (int)device->slot + 1;
   device->lights_due = false;
   return player > 0 ? pattern[(player - 1) % 8] : 0;
}

static void hidpad_switch_pro_send_command(
      struct hidpad_switch_pro_data *device)
{
   uint8_t r[SWITCH_PRO_COMMAND_LEN];
   unsigned address = 0, len = 0;

   memset(r, 0, sizeof(r));
   r[0]  = 0x01;
   r[1]  = device->number++ & 0x0f;
   memcpy(r + 2, device->rumble, sizeof(device->rumble));
   switch (device->command)
   {
      case SWITCH_PRO_CMD_MODE:
         r[10] = 0x03;
         r[11] = 0x30;
         break;
      case SWITCH_PRO_CMD_RUMBLE:
         r[10] = 0x48;
         r[11] = 0x01;
         break;
      case SWITCH_PRO_CMD_LIGHTS:
         r[10] = 0x30;
         r[11] = hidpad_switch_pro_lights(device);
         break;
      case SWITCH_PRO_CMD_FACTORY:
         address = SWITCH_PRO_FACTORY_CAL;
         len     = SWITCH_PRO_FACTORY_LEN;
         break;
      case SWITCH_PRO_CMD_USER:
         address = SWITCH_PRO_USER_CAL;
         len     = SWITCH_PRO_USER_LEN;
         break;
      case SWITCH_PRO_CMD_IMU_CAL:
         address = SWITCH_PRO_IMU_CAL;
         len     = SWITCH_PRO_IMU_CAL_LEN;
         break;
      case SWITCH_PRO_CMD_IMU_USER:
         address = SWITCH_PRO_IMU_USER;
         len     = SWITCH_PRO_IMU_USER_LEN;
         break;
      case SWITCH_PRO_CMD_IMU:
         r[10] = 0x40;
         r[11] = device->imu_want ? 1 : 0;
         device->imu_on = device->imu_want != 0;
         break;
   }
   if (len)
   {
      r[10] = 0x10;    /* read the flash */
      r[11] = (uint8_t)(address & 0xff);
      r[12] = (uint8_t)(address >> 8);
      r[15] = (uint8_t)len;
   }
   hidpad_switch_pro_write(device, r, sizeof(r));
}

/* The full report is asked for on USB too, as SDL does: a pad that
 * passes for one may speak the Bluetooth protocol over its cable, and
 * send nothing until it is asked. */
static void hidpad_switch_pro_commands(struct hidpad_switch_pro_data *device)
{
   device->state   = SWITCH_PRO_COMMANDS;
   device->command = SWITCH_PRO_CMD_MODE;
   device->tries   = 1;
   hidpad_switch_pro_send_command(device);
}

/* The greeting done, answered or not: 0x80 0x04 starts the reports,
 * and the commands follow. */
static void hidpad_switch_pro_start(struct hidpad_switch_pro_data *device,
      bool answered)
{
   RARCH_DBG("[Switch Pro] Starting the reports%s.\n",
         answered ? "" : ", the greeting unanswered");
   hidpad_switch_pro_usb_command(device, 0x04);
   hidpad_switch_pro_commands(device);
}

static void hidpad_switch_pro_hello2(struct hidpad_switch_pro_data *device)
{
   device->state = SWITCH_PRO_USB_HELLO2;
   device->tries = 1;
   hidpad_switch_pro_usb_command(device, 0x02);
}

static void hidpad_switch_pro_next_command(
      struct hidpad_switch_pro_data *device)
{
   device->tries = 1;
   if (++device->command >= SWITCH_PRO_CMD_COUNT)
   {
      device->state = SWITCH_PRO_READY;
      RARCH_DBG("[Switch Pro] Ready.\n");
   }
   else
      hidpad_switch_pro_send_command(device);
}

/* Until its flash is read, a stick as most are, and the sensors at
 * their nominal scale, as SDL takes them. */
static void hidpad_switch_pro_default_calibration(
      struct hidpad_switch_pro_data *device)
{
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      device->centre[i]     = 2048;
      device->reach_up[i]   = 1000;
      device->reach_down[i] = 1000;
   }
   for (i = 0; i < 3; i++)
   {
      device->accel_scale[i] = 1.0f / 4096.0f;
      device->gyro_scale[i]  = 3.14159265358979323846f / 180.0f / 14.2842f;
      device->gyro_rest[i]   = 0;
   }
}

static int16_t hidpad_switch_pro_le16(const uint8_t *p)
{
   return (int16_t)(uint16_t)(p[0] | (p[1] << 8));
}

/* The sensors' calibration, as SDL reads it: the factory's origins and
 * sensitivities, the user's origins over them where the user
 * calibrated (@user, marked 0xB2 0xA1 first). */
static void hidpad_switch_pro_imu_scale(
      struct hidpad_switch_pro_data *device, unsigned i,
      int accel_origin, int gyro_origin)
{
   int accel_sens = device->accel_sens[i];
   int gyro_sens  = device->gyro_sens[i];
   if (accel_sens != accel_origin)
      device->accel_scale[i] = 4.0f / (float)(accel_sens - accel_origin);
   if (gyro_sens != gyro_origin)
      device->gyro_scale[i]  = 936.0f / (float)(gyro_sens - gyro_origin)
         * 3.14159265358979323846f / 180.0f;
   device->gyro_rest[i] = (int16_t)gyro_origin;
}

static void hidpad_switch_pro_imu_calibrate(
      struct hidpad_switch_pro_data *device, const uint8_t *p, bool user)
{
   unsigned i;
   if (user)
   {
      /* the user's origins, over the factory's sensitivities */
      if (p[0] != 0xb2 || p[1] != 0xa1 || !device->imu_factory)
         return;
      for (i = 0; i < 3; i++)
         hidpad_switch_pro_imu_scale(device, i,
               hidpad_switch_pro_le16(p + 2 + 2 * i),
               hidpad_switch_pro_le16(p + 14 + 2 * i));
      return;
   }
   for (i = 0; i < 3; i++)
   {
      device->accel_sens[i] = hidpad_switch_pro_le16(p + 6 + 2 * i);
      device->gyro_sens[i]  = hidpad_switch_pro_le16(p + 18 + 2 * i);
      hidpad_switch_pro_imu_scale(device, i,
            hidpad_switch_pro_le16(p + 2 * i),
            hidpad_switch_pro_le16(p + 12 + 2 * i));
   }
   device->imu_factory = true;
}

static void hidpad_switch_pro_encode(uint8_t *p, uint16_t strength,
      bool quiet_rest);

static void *hidpad_switch_pro_open(void *data, uint32_t slot,
      hid_driver_t *driver, bool quiet_rest)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_switch_pro_data *device;

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_switch_pro_data*)
            calloc(1, sizeof(struct hidpad_switch_pro_data))))
      return NULL;

   device->connection = connection;
   device->driver     = driver;
   device->slot       = slot;
   device->bluetooth  = driver && driver->is_bluetooth
      && driver->is_bluetooth(connection);
   device->quiet_rest = quiet_rest;
   hidpad_switch_pro_encode(device->rumble,     0, quiet_rest);
   hidpad_switch_pro_encode(device->rumble + 4, 0, quiet_rest);
   hidpad_switch_pro_default_calibration(device);

   /* Where the driver cannot say, the greeting goes anyway: on
    * Bluetooth it is not answered, and the short report tells. */
   if (device->bluetooth)
      hidpad_switch_pro_commands(device);
   else
   {
      device->state = SWITCH_PRO_USB_HELLO;
      device->tries = 1;
      hidpad_switch_pro_usb_command(device, 0x02);
   }
   return device;
}

static void *hidpad_switch_pro_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return hidpad_switch_pro_open(data, slot, driver, false);
}

/* The Switch Online N64 controller - and 8BitDo's 64 pad, which passes
 * for it - rumbles at the usual rest, as SDL found: its rest has no
 * amplitude. */
static void *hidpad_switch_pro_n64_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return hidpad_switch_pro_open(data, slot, driver, true);
}

static void hidpad_switch_pro_deinit(void *data)
{
   struct hidpad_switch_pro_data *device = (struct hidpad_switch_pro_data*)data;
   if (device)
      free(device);
}

/* Three bytes holding two 12-bit values. */
static void hidpad_switch_pro_pair(const uint8_t *p, int *a, int *b)
{
   *a = p[0] | ((p[1] & 0x0f) << 8);
   *b = (p[1] >> 4) | (p[2] << 4);
}

/* One stick's nine bytes of flash: the left's are reach up, centre,
 * reach down; the right's centre, reach down, reach up. Unwritten
 * flash reads 0xFFF. */
static void hidpad_switch_pro_calibrate(struct hidpad_switch_pro_data *device,
      unsigned stick, const uint8_t *p)
{
   int up[2], centre[2], down[2];
   unsigned i;

   if (stick == 0)
   {
      hidpad_switch_pro_pair(p,     &up[0],     &up[1]);
      hidpad_switch_pro_pair(p + 3, &centre[0], &centre[1]);
      hidpad_switch_pro_pair(p + 6, &down[0],   &down[1]);
   }
   else
   {
      hidpad_switch_pro_pair(p,     &centre[0], &centre[1]);
      hidpad_switch_pro_pair(p + 3, &down[0],   &down[1]);
      hidpad_switch_pro_pair(p + 6, &up[0],     &up[1]);
   }
   for (i = 0; i < 2; i++)
   {
      unsigned axis = stick * 2 + i;
      if (centre[i] == 0xfff)
         centre[i] = 2048;
      if (up[i] == 0xfff)
         up[i]     = centre[i] * 7 / 10;
      if (down[i] == 0xfff)
         down[i]   = centre[i] * 7 / 10;
      /* the flash's reach is past what most sticks make: seven tenths
       * of it is taken, and widened when a stick goes further */
      device->centre[axis]     = (int16_t)centre[i];
      device->reach_up[axis]   = (int16_t)MAX(up[i] * 7 / 10, 1);
      device->reach_down[axis] = (int16_t)MAX(down[i] * 7 / 10, 1);
   }
}

/* The answer to a flash read: its address and length, then the bytes. */
static void hidpad_switch_pro_flash(struct hidpad_switch_pro_data *device,
      const uint8_t *r, unsigned len)
{
   unsigned address = r[15] | (r[16] << 8) | (r[17] << 16)
      | ((unsigned)r[18] << 24);
   const uint8_t *p = r + 20;

   if (address == SWITCH_PRO_FACTORY_CAL
         && len >= 20 + SWITCH_PRO_FACTORY_LEN)
   {
      hidpad_switch_pro_calibrate(device, 0, p);
      hidpad_switch_pro_calibrate(device, 1, p + 9);
   }
   else if (address == SWITCH_PRO_USER_CAL
         && len >= 20 + SWITCH_PRO_USER_LEN)
   {
      /* a stick the user calibrated is marked so */
      if (p[0] == 0xb2 && p[1] == 0xa1)
         hidpad_switch_pro_calibrate(device, 0, p + 2);
      if (p[11] == 0xb2 && p[12] == 0xa1)
         hidpad_switch_pro_calibrate(device, 1, p + 13);
   }
   else if (address == SWITCH_PRO_IMU_CAL
         && len >= 20 + SWITCH_PRO_IMU_CAL_LEN)
      hidpad_switch_pro_imu_calibrate(device, p, false);
   else if (address == SWITCH_PRO_IMU_USER
         && len >= 20 + SWITCH_PRO_IMU_USER_LEN)
      hidpad_switch_pro_imu_calibrate(device, p, true);
}

static int16_t hidpad_switch_pro_scale(struct hidpad_switch_pro_data *device,
      unsigned axis, int raw, bool invert)
{
   int32_t v = raw - device->centre[axis];
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

static void hidpad_switch_pro_full(struct hidpad_switch_pro_data *device,
      const uint8_t *r)
{
   uint32_t b = 0;
   int x, y;

   if (r[3] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
   if (r[3] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
   if (r[3] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
   if (r[3] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
   if (r[3] & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
   if (r[3] & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R2;
   if (r[4] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
   if (r[4] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
   if (r[4] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R3;
   if (r[4] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L3;
   if (r[5] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (r[5] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (r[5] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   if (r[5] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (r[5] & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
   if (r[5] & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L2;
   b |= (uint32_t)((r[4] >> 4) & 0x03) << 16;    /* Home, Capture */

   hidpad_switch_pro_pair(r + 6, &x, &y);
   device->view.axes[0] = hidpad_switch_pro_scale(device, 0, x, false);
   device->view.axes[1] = hidpad_switch_pro_scale(device, 1, y, true);
   hidpad_switch_pro_pair(r + 9, &x, &y);
   device->view.axes[2] = hidpad_switch_pro_scale(device, 2, x, false);
   device->view.axes[3] = hidpad_switch_pro_scale(device, 3, y, true);
   pad_view_set_buttons(&device->view, b, (r[4] & 0x10) != 0);
}

/* The hat's eight ways from up, clockwise, and its rest (8): up 1,
 * down 2, left 4, right 8. */
static const uint8_t hidpad_switch_pro_hat[16] = {
   1, 1 | 8, 8, 8 | 2, 2, 2 | 4, 4, 4 | 1,
   0, 0, 0, 0, 0, 0, 0, 0
};

/* Bluetooth's first report: B A Y X L R ZL ZR; -, +, L3, R3, Home,
 * Capture; the hat; the sticks, 16 bits an axis, down positive, in
 * about half their range. */
static void hidpad_switch_pro_simple(struct hidpad_switch_pro_data *device,
      const uint8_t *r)
{
   uint32_t b   = 0;
   unsigned hat = hidpad_switch_pro_hat[r[3] & 0x0f];
   unsigned i;

   if (r[1] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
   if (r[1] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
   if (r[1] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
   if (r[1] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
   if (r[1] & 0x10) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
   if (r[1] & 0x20) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
   if (r[1] & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L2;
   if (r[1] & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R2;
   if (r[2] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
   if (r[2] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
   if (r[2] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L3;
   if (r[2] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R3;
   if (hat & 1)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (hat & 2)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (hat & 4)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (hat & 8)     b |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   b |= (uint32_t)((r[2] >> 4) & 0x03) << 16;

   for (i = 0; i < 4; i++)
   {
      int32_t v = ((int32_t)(r[4 + i * 2] | (r[5 + i * 2] << 8)) - 0x8000) * 2;
      device->view.axes[i] = (int16_t)MAX(-32767, MIN(32767, v));
   }
   pad_view_set_buttons(&device->view, b, (r[2] & 0x10) != 0);
}

/* The strength at which each of the pad's amplitudes, 1 to 99, ends;
 * from 0 to 100 they climb about evenly in loudness. */
static const uint16_t hidpad_switch_pro_amplitude[99] = {
     514,   775,   921,  1096,  1303,  1550,  1843,  2192,  2606,  3100,
    3686,  4383,  5213,  6199,  7372,  7698,  8039,  8395,  8767,  9155,
    9560,  9984, 10426, 10887, 11369, 11873, 12398, 12947, 13520, 14119,
   14744, 15067, 15397, 15734, 16079, 16431, 16790, 17158, 17534, 17918,
   18310, 18711, 19121, 19540, 19967, 20405, 20851, 21308, 21775, 22251,
   22739, 23236, 23745, 24265, 24797, 25340, 25894, 26462, 27041, 27633,
   28238, 28856, 29488, 30134, 30794, 31468, 32157, 32861, 33581, 34316,
   35068, 35836, 36620, 37422, 38242, 39079, 39935, 40809, 41703, 42616,
   43549, 44503, 45477, 46473, 47491, 48531, 49593, 50679, 51789, 52923,
   54082, 55266, 56476, 57713, 58977, 60268, 61588, 62936, 64315
};

/* One actuator's four bytes: 320 Hz and 160 Hz, both at the strength
 * asked; at rest, the pad's own neutral. */
static void hidpad_switch_pro_encode(uint8_t *p, uint16_t strength,
      bool quiet_rest)
{
   unsigned amp = 1;

   if (!strength)
   {
      p[0] = 0x00;
      p[1] = quiet_rest ? 0x00 : 0x01;
      p[2] = quiet_rest ? 0x01 : 0x40;
      p[3] = 0x40;
      return;
   }
   while (amp < 100 && strength > hidpad_switch_pro_amplitude[amp - 1])
      amp++;
   p[0] = 0x74;
   p[1] = (uint8_t)(amp * 2);
   p[2] = (uint8_t)(0x3d | ((amp & 1) << 7));
   p[3] = (uint8_t)(0x40 + (amp >> 1));
}

static void hidpad_switch_pro_send_rumble(
      struct hidpad_switch_pro_data *device)
{
   uint8_t r[SWITCH_PRO_RUMBLE_LEN];
   r[0] = 0x10;
   r[1] = device->number++ & 0x0f;
   memcpy(r + 2, device->rumble, sizeof(device->rumble));
   hidpad_switch_pro_write(device, r, sizeof(r));
   device->motors_due = false;
}

/* The USB greeting's answers. */
static void hidpad_switch_pro_usb_reply(struct hidpad_switch_pro_data *device,
      uint8_t command)
{
   switch (device->state)
   {
      case SWITCH_PRO_USB_HELLO:
         if (command != 0x02)
            return;
         device->state = SWITCH_PRO_USB_BAUD;
         hidpad_switch_pro_usb_command(device, 0x03);
         break;
      case SWITCH_PRO_USB_BAUD:
         if (command != 0x03)
            return;
         hidpad_switch_pro_hello2(device);
         return;
      case SWITCH_PRO_USB_HELLO2:
         if (command != 0x02)
            return;
         /* not answered: the reports start */
         hidpad_switch_pro_start(device, true);
         return;
      default:
         return;
   }
   device->tries = 1;
}

static void hidpad_switch_pro_command_reply(
      struct hidpad_switch_pro_data *device, const uint8_t *r, unsigned len)
{
   static const uint8_t ids[SWITCH_PRO_CMD_COUNT] = {
      0x03, 0x48, 0x30, 0x10, 0x10, 0x10, 0x10 };

   if (     device->state != SWITCH_PRO_COMMANDS
         || len < 15
         || r[14] != ids[device->command])
      return;
   if (r[14] == 0x10)
   {
      if (len < 20)
         return;
      hidpad_switch_pro_flash(device, r, len);
   }
   hidpad_switch_pro_next_command(device);
}

/* A command not answered after a while goes again, a few times; then
 * the next. The faster link is not asked twice: a pad that passes for
 * one does not answer it. The greeting given up on, the reports are
 * started anyway. */
static void hidpad_switch_pro_retry(struct hidpad_switch_pro_data *device)
{
   if (     device->state == SWITCH_PRO_READY
         || device->since_sent < SWITCH_PRO_RETRY_AFTER)
      return;
   if (device->state == SWITCH_PRO_USB_BAUD)
   {
      RARCH_DBG("[Switch Pro] Faster link not answered: going on without it.\n");
      hidpad_switch_pro_hello2(device);
      return;
   }
   if (device->tries++ >= SWITCH_PRO_TRIES)
   {
      if (device->state == SWITCH_PRO_COMMANDS)
         hidpad_switch_pro_next_command(device);
      else
         hidpad_switch_pro_start(device, false);
      return;
   }
   switch (device->state)
   {
      case SWITCH_PRO_USB_HELLO:
      case SWITCH_PRO_USB_HELLO2:
         hidpad_switch_pro_usb_command(device, 0x02);
         break;
      default:
         hidpad_switch_pro_send_command(device);
         break;
   }
}

static void hidpad_switch_pro_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   const uint8_t *r = packet;
   unsigned len     = size;

   if (!device || !r)
      return;

   /* a Bluetooth stack's DATA | INPUT header: read, nothing sent */
   if (len && r[0] == 0xa1)
   {
      r++;
      len--;
      device->stack = true;
   }
   if (!len)
      return;
   device->quiet_since = 0;
   if (!device->heard)
   {
      device->heard = true;
      RARCH_DBG("[Switch Pro] First report: 0x%02x, %u bytes%s.\n",
            r[0], len, device->stack ? ", behind a stack's header" : "");
   }

   switch (r[0])
   {
      case 0x81:
         if (len >= 2)
            hidpad_switch_pro_usb_reply(device, r[1]);
         return;
      case 0x3f:
         if (len < SWITCH_PRO_SIMPLE_LEN)
            return;
         hidpad_switch_pro_simple(device, r);
         if (device->state < SWITCH_PRO_COMMANDS)
         {
            device->bluetooth = true;
            hidpad_switch_pro_commands(device);
            return;
         }
         break;
      case 0x21:
         if (len < SWITCH_PRO_FULL_LEN)
            return;
         hidpad_switch_pro_full(device, r);
         hidpad_switch_pro_command_reply(device, r, len);
         break;
      case 0x30:
      case 0x31:
      case 0x32:
      case 0x33:
         if (len < SWITCH_PRO_FULL_LEN)
            return;
         hidpad_switch_pro_full(device, r);
         /* the newest motion sample, where the pad sends them: an
          * accelerometer all nought has none */
         if (     r[0] == 0x30 && device->imu_want
               && len >= SWITCH_PRO_IMU_AT + 12)
         {
            unsigned i;
            for (i = 0; i < 6; i++)
               device->imu[i] = hidpad_switch_pro_le16(
                     r + SWITCH_PRO_IMU_AT + 2 * i);
            if (device->imu[0] || device->imu[1] || device->imu[2])
               device->imu_seen = true;
         }
         break;
      default:
         return;
   }

   if (device->since_sent < 0xff)
      device->since_sent++;
   hidpad_switch_pro_retry(device);

   /* lights told after the commands, and the sensors turned on or off
    * when a core changes its mind, each as a command of its own; their
    * answers are not waited for */
   if (     device->state == SWITCH_PRO_READY
         && device->lights_due
         && device->since_sent >= SWITCH_PRO_RUMBLE_GAP)
   {
      device->command = SWITCH_PRO_CMD_LIGHTS;
      hidpad_switch_pro_send_command(device);
      return;
   }
   if (     device->state == SWITCH_PRO_READY
         && device->imu_on != (device->imu_want != 0)
         && device->since_sent >= SWITCH_PRO_RUMBLE_GAP)
   {
      device->command = SWITCH_PRO_CMD_IMU;
      hidpad_switch_pro_send_command(device);
      return;
   }

   /* rumble rides in the commands until they are done; then on its
    * own, kept up while it is held */
   if (     device->state == SWITCH_PRO_READY
         && device->since_sent >= SWITCH_PRO_RUMBLE_GAP
         && (device->motors_due
            || (     (device->motors[0] || device->motors[1])
                  && device->since_sent >= SWITCH_PRO_RUMBLE_HOLD)))
      hidpad_switch_pro_send_rumble(device);
}

/* A poll with no report. Quiet for a while, a step not answered goes
 * on as it would after a while of reports; once ready, reports that
 * stopped on USB are started again, as SDL does - Steam can stop them. */
static void hidpad_switch_pro_idle(void *data)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   retro_time_t now;

   if (!device)
      return;
   now = cpu_features_get_time_usec();
   if (!device->quiet_since)
   {
      device->quiet_since = now;
      return;
   }
   if (now - device->quiet_since < SWITCH_PRO_QUIET_US)
      return;
   device->quiet_since = now;
   if (device->state == SWITCH_PRO_READY)
   {
      if (!device->bluetooth && !device->stack)
         hidpad_switch_pro_usb_command(device, 0x04);
      return;
   }
   device->since_sent = SWITCH_PRO_RETRY_AFTER;
   hidpad_switch_pro_retry(device);
}

static void hidpad_switch_pro_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (device && device->motors[idx] != strength)
   {
      device->motors[idx] = strength;
      /* the strong motor is the left hand's, the weak the right's */
      hidpad_switch_pro_encode(device->rumble + idx * 4, strength,
            device->quiet_rest);
      device->motors_due  = true;
   }
}

/* A core asks for the accelerometer or the gyroscope, or is done with
 * it; the pad is told at the next report. */
static bool hidpad_switch_pro_set_sensor(void *data,
      enum retro_sensor_action action)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   if (!device || !pad_motion_want(&device->imu_want, action))
      return false;
   if (!device->imu_want)
      device->imu_seen = false;
   return true;
}

/* One value: the pad's axes turned into SDL's as SDL turns them, in g
 * and radians a second, then into libretro's (pad_motion.h). */
static bool hidpad_switch_pro_sensor(void *data, unsigned id, float *value)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   float accel[3], gyro[3];

   if (!device || !device->imu_seen)
      return false;
   accel[0] = -(device->accel_scale[1] * device->imu[1]);
   accel[1] =   device->accel_scale[2] * device->imu[2];
   accel[2] = -(device->accel_scale[0] * device->imu[0]);
   gyro[0]  = -(device->gyro_scale[1] * (device->imu[4] - device->gyro_rest[1]));
   gyro[1]  =   device->gyro_scale[2] * (device->imu[5] - device->gyro_rest[2]);
   gyro[2]  = -(device->gyro_scale[0] * (device->imu[3] - device->gyro_rest[0]));
   return pad_motion_value(accel, gyro, device->imu_want, id, value);
}

static void hidpad_switch_pro_set_player(void *data, unsigned player)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   if (device)
   {
      device->player     = player ? (int)player : -1;
      device->lights_due = true;
   }
}

static void hidpad_switch_pro_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;

   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

/* The sticks as 0 to 3; ZL and ZR have no travel. */
static int16_t hidpad_switch_pro_get_axis(void *data, unsigned axis)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   if (!device || axis >= 4)
      return 0;
   return device->view.axes[axis];
}

static const char *hidpad_switch_pro_get_name(void *data)
{
   return "Pro Controller";
}

static int32_t hidpad_switch_pro_button(void *data, uint16_t joykey)
{
   struct hidpad_switch_pro_data *device =
      (struct hidpad_switch_pro_data*)data;
   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_switch_pro_view(void *data)
{
   return &((struct hidpad_switch_pro_data*)data)->view;
}

pad_connection_interface_t pad_connection_switch_pro = {
   hidpad_switch_pro_init,
   hidpad_switch_pro_deinit,
   hidpad_switch_pro_packet_handler,
   hidpad_switch_pro_set_rumble,
   hidpad_switch_pro_get_buttons,
   hidpad_switch_pro_get_axis,
   hidpad_switch_pro_get_name,
   hidpad_switch_pro_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_switch_pro_set_player,
   hidpad_switch_pro_view,
   hidpad_switch_pro_idle,
   hidpad_switch_pro_set_sensor,
   hidpad_switch_pro_sensor
};

pad_connection_interface_t pad_connection_switch_pro_n64 = {
   hidpad_switch_pro_n64_init,
   hidpad_switch_pro_deinit,
   hidpad_switch_pro_packet_handler,
   hidpad_switch_pro_set_rumble,
   hidpad_switch_pro_get_buttons,
   hidpad_switch_pro_get_axis,
   hidpad_switch_pro_get_name,
   hidpad_switch_pro_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_switch_pro_set_player,
   hidpad_switch_pro_view,
   hidpad_switch_pro_idle,
   hidpad_switch_pro_set_sensor,
   hidpad_switch_pro_sensor
};
