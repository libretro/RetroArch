/*  RetroArch - A frontend for libretro.
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

/* GameCube adapters in their PC mode on DragonRise's id - EVORETRO's
 * and Mayflash's - as SDL reads them. The first port with a pad is
 * the one read.
 *
 * Older firmware sends ten bytes, the port first; from firmware 7 it
 * sends nine, a device for each port, and has rumble with the second
 * plug in: output report {0, 0xff, 0xff} on, zeros off, kept up every
 * 16 ms while on. From the port on: X A B Y, L, R, -, Z; then -,
 * Start, -, -, up, right, down, left; the sticks, the C stick the
 * other way round on the older firmware; L and R's travel.
 *
 * The buttons are given as the Wii U adapter's handler gives them. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <features/features_cpu.h>

#include "joypad_connection.h"
#include "../input_defines.h"

#define GCPC_STICK_LOW   (128 - 88)
#define GCPC_STICK_HIGH  (128 + 88)
#define GCPC_TRIGGER_LOW 40
/* how long rumble lasts unless sent again, as SDL keeps it up */
#define GCPC_RUMBLE_USEC 16000

struct hidpad_gamecube_pc_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   /* the sticks as 0 to 3, L and R's travel as 4 and 5 */
   input_pad_view_t view;
   retro_time_t rumble_sent;
   uint8_t low[6];
   uint8_t high[6];
   uint8_t port;        /* the port read, from 1; 0 until one is seen */
   bool firmware_7;     /* nine-byte reports: rumble */
   bool seen;
   bool motors[2];      /* strong, weak: on as the core asked */
   bool rumble;
   bool rumble_on;      /* as last sent */
};

static void *hidpad_gamecube_pc_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_gamecube_pc_data *device;

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_gamecube_pc_data*)
            calloc(1, sizeof(struct hidpad_gamecube_pc_data))))
      return NULL;
   device->connection = connection;
   device->driver     = driver;
   memset(device->low,  GCPC_STICK_LOW,  sizeof(device->low));
   memset(device->high, GCPC_STICK_HIGH, sizeof(device->high));
   device->low[4]     = GCPC_TRIGGER_LOW;
   device->low[5]     = GCPC_TRIGGER_LOW;
   return device;
}

static void hidpad_gamecube_pc_deinit(void *data)
{
   struct hidpad_gamecube_pc_data *device =
      (struct hidpad_gamecube_pc_data*)data;
   if (device)
      free(device);
}

/* @v within the reach seen so far, widened to take it in */
static int32_t hidpad_gamecube_pc_reach(
      struct hidpad_gamecube_pc_data *device, unsigned i, uint8_t v)
{
   if (v < device->low[i])
      device->low[i]  = v;
   if (v > device->high[i])
      device->high[i] = v;
   return (int32_t)(v - device->low[i]) * 65535
      / (device->high[i] - device->low[i]);
}

static void hidpad_gamecube_pc_decode(
      struct hidpad_gamecube_pc_data *device, const uint8_t *r,
      bool c_inverted)
{
   uint8_t raw[4];
   uint32_t b = 0;
   unsigned i;

   if (r[0] & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
   if (r[0] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
   if (r[0] & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
   if (r[0] & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
   if (r[0] & 0x10) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
   if (r[0] & 0x20) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
   if (r[0] & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT; /* Z */
   if (r[1] & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
   if (r[1] & 0x10) b |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (r[1] & 0x20) b |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   if (r[1] & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (r[1] & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   pad_view_set_buttons(&device->view, b, false);

   /* each stick right and up high, as the Wii U adapter's */
   raw[0] = r[2];
   raw[1] = r[3];
   raw[2] = c_inverted ? (uint8_t)(0xff - r[5]) : r[5];
   raw[3] = c_inverted ? (uint8_t)(0xff - r[4]) : r[4];
   for (i = 0; i < 4; i++)
   {
      int32_t v = hidpad_gamecube_pc_reach(device, i, raw[i]) - 32768;
      if (i & 1)
         v = v <= -32768 ? 32767 : -v;
      device->view.axes[i] = (int16_t)v;
   }
   for (i = 4; i < 6; i++)
      device->view.axes[i] = (int16_t)(hidpad_gamecube_pc_reach(device, i,
               r[2 + i]) >> 1);
}

/* Not on macOS, where iohidmanager sends every report as 0x01. */
static void hidpad_gamecube_pc_send_rumble(
      struct hidpad_gamecube_pc_data *device, retro_time_t now)
{
#ifndef __APPLE__
   uint8_t r[3];
   r[0] = 0x00;
   r[1] = r[2] = device->rumble ? 0xff : 0x00;
   if (device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, sizeof(r));
#endif
   device->rumble_on   = device->rumble;
   device->rumble_sent = now;
}

static void hidpad_gamecube_pc_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_gamecube_pc_data *device =
      (struct hidpad_gamecube_pc_data*)data;

   if (!device || !packet)
      return;
   if (size == 10)
   {
      if (!packet[0])
         return;
      if (!device->port)
         device->port = packet[0];
      if (packet[0] != device->port)
         return;
      hidpad_gamecube_pc_decode(device, packet + 1, true);
   }
   else if (size == 9)
   {
      if (!device->seen)
         device->firmware_7 = true;
      hidpad_gamecube_pc_decode(device, packet, false);
   }
   else
      return;
   device->seen = true;

   if (device->firmware_7 && (device->rumble || device->rumble_on))
   {
      retro_time_t now = cpu_features_get_time_usec();
      if (     device->rumble != device->rumble_on
            || now - device->rumble_sent >= GCPC_RUMBLE_USEC)
         hidpad_gamecube_pc_send_rumble(device, now);
   }
}

static void hidpad_gamecube_pc_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_gamecube_pc_data *device =
      (struct hidpad_gamecube_pc_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;
   if (!device)
      return;
   device->motors[idx] = (strength != 0);
   device->rumble      = device->motors[0] || device->motors[1];
}

static void hidpad_gamecube_pc_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_gamecube_pc_data *device =
      (struct hidpad_gamecube_pc_data*)data;
   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_gamecube_pc_get_axis(void *data, unsigned axis)
{
   struct hidpad_gamecube_pc_data *device =
      (struct hidpad_gamecube_pc_data*)data;
   if (!device || axis >= 6)
      return 0;
   return device->view.axes[axis];
}

static const char *hidpad_gamecube_pc_get_name(void *data)
{
   return "GameCube Controller Adapter";
}

static int32_t hidpad_gamecube_pc_button(void *data, uint16_t joykey)
{
   struct hidpad_gamecube_pc_data *device =
      (struct hidpad_gamecube_pc_data*)data;
   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_gamecube_pc_view(void *data)
{
   return &((struct hidpad_gamecube_pc_data*)data)->view;
}

pad_connection_interface_t pad_connection_gamecube_pc = {
   hidpad_gamecube_pc_init,
   hidpad_gamecube_pc_deinit,
   hidpad_gamecube_pc_packet_handler,
   hidpad_gamecube_pc_set_rumble,
   hidpad_gamecube_pc_get_buttons,
   hidpad_gamecube_pc_get_axis,
   hidpad_gamecube_pc_get_name,
   hidpad_gamecube_pc_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_gamecube_pc_view,
   NULL,  /* idle */
   NULL,  /* set_sensor */
   NULL   /* sensor */
};
