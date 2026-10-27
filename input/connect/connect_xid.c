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

/* An original Xbox pad, and its adapters: an XID device (USB interface
 * class 0x58), not a HID one. Its 20-byte report has the D-pad, Start,
 * Back and the stick clicks as bits, the six face buttons and the two
 * triggers as a pressure each, 0 to 255, and the sticks as 16 bits.
 *
 * The RetroPad's buttons are the bits the reads answer, a face button
 * or trigger pressed past a third of its travel; the pressures are axes
 * 4 to 11 beside the sticks' 0 to 3, which the profile binds, so a core
 * that reads a button's pressure has it. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include "joypad_connection.h"
#include "../input_defines.h"

#define XID_REPORT_LEN 20
/* a face button or trigger counts as pressed past this */
#define XID_PRESS      0x55

struct hidpad_xid_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   uint32_t slot;
   uint8_t want[2];        /* the motors as the core asks */
   uint8_t motors[2];      /* as last sent */
   bool motors_due;
   input_pad_view_t view;  /* what the getters answer */
};

static void *hidpad_xid_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_xid_data *device;

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_xid_data*)calloc(1, sizeof(*device))))
      return NULL;
   device->connection = connection;
   device->driver     = driver;
   device->slot       = slot;
   return device;
}

static void hidpad_xid_deinit(void *data)
{
   free(data);
}

static void hidpad_xid_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_xid_data *device = (struct hidpad_xid_data*)data;
   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_xid_get_axis(void *data, unsigned axis)
{
   struct hidpad_xid_data *device = (struct hidpad_xid_data*)data;
   if (!device || axis >= INPUT_PAD_VIEW_AXES)
      return 0;
   return device->view.axes[axis];
}

/* a stick's 16 bits, little-endian; @flip for Y, which is up positive */
static INLINE int16_t hidpad_xid_stick(const uint8_t *p, bool flip)
{
   int32_t v = (int16_t)(uint16_t)(p[0] | (p[1] << 8));
   if (flip)
      v = -1 - v;   /* -32768..32767 onto 32767..-32768, no overflow */
   return (int16_t)v;
}

/* The motors are output report 0: the left, the heavy one, at byte 3,
 * the right at 5, each 0 to 255. Sent from the packet handler, when
 * one changes. */
static void hidpad_xid_send_rumble(struct hidpad_xid_data *device)
{
   uint8_t r[6];
   device->motors_due = false;
   if (     device->want[0] == device->motors[0]
         && device->want[1] == device->motors[1])
      return;
   device->motors[0] = device->want[0];
   device->motors[1] = device->want[1];
   r[0] = 0x00;
   r[1] = sizeof(r);
   r[2] = 0x00;
   r[3] = device->motors[0];
   r[4] = 0x00;
   r[5] = device->motors[1];
   if (device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, sizeof(r));
}

static void hidpad_xid_packet_handler(void *data, uint8_t *packet,
      uint16_t size)
{
   /* the report's pressure bytes 4 to 11, and the RetroPad's button for
    * each: A, B, X, Y, Black, White, the left and the right trigger */
   static const uint8_t pressed[8] = {
      RETRO_DEVICE_ID_JOYPAD_B, RETRO_DEVICE_ID_JOYPAD_A,
      RETRO_DEVICE_ID_JOYPAD_Y, RETRO_DEVICE_ID_JOYPAD_X,
      RETRO_DEVICE_ID_JOYPAD_R, RETRO_DEVICE_ID_JOYPAD_L,
      RETRO_DEVICE_ID_JOYPAD_L2, RETRO_DEVICE_ID_JOYPAD_R2
   };
   /* byte 2's bits, from bit 0 */
   static const uint8_t bits[8] = {
      RETRO_DEVICE_ID_JOYPAD_UP, RETRO_DEVICE_ID_JOYPAD_DOWN,
      RETRO_DEVICE_ID_JOYPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_RIGHT,
      RETRO_DEVICE_ID_JOYPAD_START, RETRO_DEVICE_ID_JOYPAD_SELECT,
      RETRO_DEVICE_ID_JOYPAD_L3, RETRO_DEVICE_ID_JOYPAD_R3
   };
   struct hidpad_xid_data *device = (struct hidpad_xid_data*)data;
   uint32_t buttons               = 0;
   unsigned i;

   /* the input report: 0, then its length */
   if (!device || size < XID_REPORT_LEN || packet[0] != 0x00
         || packet[1] != XID_REPORT_LEN)
      return;

   for (i = 0; i < 8; i++)
   {
      uint8_t p = packet[4 + i];
      if (packet[2] & (1u << i))
         buttons |= 1u << bits[i];
      if (p > XID_PRESS)
         buttons |= 1u << pressed[i];
      /* 0..255 onto 0..32767 */
      device->view.axes[4 + i] = (int16_t)((p << 7) | (p >> 1));
   }
   pad_view_set_buttons(&device->view, buttons, false);
   device->view.axes[0] = hidpad_xid_stick(packet + 12, false);
   device->view.axes[1] = hidpad_xid_stick(packet + 14, true);
   device->view.axes[2] = hidpad_xid_stick(packet + 16, false);
   device->view.axes[3] = hidpad_xid_stick(packet + 18, true);

   if (device->motors_due)
      hidpad_xid_send_rumble(device);
}

static void hidpad_xid_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_xid_data *device = (struct hidpad_xid_data*)data;
   if (!device)
      return;
   device->want[effect == RETRO_RUMBLE_STRONG ? 0 : 1] =
      (uint8_t)(strength >> 8);
   device->motors_due = true;
}

static const char *hidpad_xid_get_name(void *data)
{
   return "Original Xbox Controller";
}

static int32_t hidpad_xid_button(void *data, uint16_t joykey)
{
   struct hidpad_xid_data *device = (struct hidpad_xid_data*)data;
   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_xid_view(void *data)
{
   return &((struct hidpad_xid_data*)data)->view;
}

pad_connection_interface_t pad_connection_xid = {
   hidpad_xid_init,
   hidpad_xid_deinit,
   hidpad_xid_packet_handler,
   hidpad_xid_set_rumble,
   hidpad_xid_get_buttons,
   hidpad_xid_get_axis,
   hidpad_xid_get_name,
   hidpad_xid_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_xid_view
};
