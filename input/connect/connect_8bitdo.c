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

/* The 8BitDo SN30 Pro in its D-input mode (B + Start), on USB and
 * Bluetooth.
 *
 * Its buttons keep the numbers its report gives them - A 0, B 1, X 3,
 * Y 4, L 6, R 7, L2 8, R2 9, Select 10, Start 11, Home 12, L3 13,
 * R3 14 - and the D-pad is hat 0, as the hid profiles for it have
 * them; Home also opens the menu. The sticks are axes 0, 1, 4 and 5,
 * as those profiles have them too, and L2 and R2's travel axes 2
 * and 3.
 *
 * Its firmware from 2.05 has a mode its feature report 0x06 starts,
 * which has rumble: output report 0x05, the large motor then the
 * small. Older firmware sends a 9-byte report with no report ID, and
 * no rumble. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>

#include "joypad_connection.h"
#include "../input_defines.h"

#define SN30_OLD_REPORT_LEN 9   /* firmware before 2.05: no report ID */
#define SN30_REPORT_LEN     10  /* report 0x01, 0x03 or 0x04 */

struct hidpad_8bitdo_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   input_pad_view_t view;
   uint8_t motors[2];   /* strong, weak: what the core asked last */
   bool rumble;         /* the mode with rumble is on */
   bool behind_stack;   /* a Bluetooth stack's header: nothing is sent */
   bool motors_due;
};

static void *hidpad_8bitdo_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_8bitdo_data *device;

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_8bitdo_data*)
            calloc(1, sizeof(struct hidpad_8bitdo_data))))
      return NULL;

   device->connection = connection;
   device->driver     = driver;
   if (driver && driver->get_report)
   {
      uint8_t feature[64];
      memset(feature, 0, sizeof(feature));
      device->rumble = driver->get_report(connection, HID_REPORT_FEATURE,
            0x06, feature, sizeof(feature)) > 0;
   }
   return device;
}

static void hidpad_8bitdo_deinit(void *data)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   if (device)
      free(device);
}

/* The hat's eight ways from up, clockwise: up 1, down 2, left 4,
 * right 8. Anything else is its rest. */
static const uint8_t hidpad_8bitdo_hat[8] = {
   1, 1 | 8, 8, 8 | 2, 2, 2 | 4, 4, 4 | 1
};

/* 0x7f is the rest; 0 and 0xff the ends */
static int16_t hidpad_8bitdo_stick(uint8_t b)
{
   int v = (int)b - 0x7f;
   return (int16_t)(v < 0 ? v * 32768 / 127 : v * 32767 / 128);
}

static int16_t hidpad_8bitdo_trigger(uint8_t t)
{
   return (int16_t)((t << 7) | (t >> 1));
}

static void hidpad_8bitdo_send(struct hidpad_8bitdo_data *device)
{
   uint8_t r[5];
   r[0] = 0x05;
   r[1] = device->motors[0];
   r[2] = device->motors[1];
   r[3] = 0;
   r[4] = 0;
   if (device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, sizeof(r));
   device->motors_due = false;
}

static void hidpad_8bitdo_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   const uint8_t *r                  = packet;
   unsigned len                      = size;
   const uint8_t *buttons;
   uint8_t hat;

   if (!device || !r)
      return;

   /* a Bluetooth stack's DATA | INPUT header; never the old report,
    * whose first byte is buttons */
   if (     len > SN30_REPORT_LEN && r[0] == 0xa1
         && (r[1] == 0x01 || r[1] == 0x03 || r[1] == 0x04))
   {
      r++;
      len--;
      device->behind_stack = true;
   }

   if (len == SN30_OLD_REPORT_LEN)
   {
      buttons              = r;
      hat                  = r[2];
      device->view.axes[0] = hidpad_8bitdo_stick(r[3]);
      device->view.axes[1] = hidpad_8bitdo_stick(r[4]);
      device->view.axes[4] = hidpad_8bitdo_stick(r[5]);
      device->view.axes[5] = hidpad_8bitdo_stick(r[6]);
      device->view.axes[2] = (r[1] & 0x01) ? 0x7fff : 0;
      device->view.axes[3] = (r[1] & 0x02) ? 0x7fff : 0;
   }
   else if (len >= SN30_REPORT_LEN
         && (r[0] == 0x01 || r[0] == 0x03 || r[0] == 0x04))
   {
      buttons              = r + 8;
      hat                  = r[1];
      device->view.axes[0] = hidpad_8bitdo_stick(r[2]);
      device->view.axes[1] = hidpad_8bitdo_stick(r[3]);
      device->view.axes[4] = hidpad_8bitdo_stick(r[4]);
      device->view.axes[5] = hidpad_8bitdo_stick(r[5]);
      device->view.axes[2] = hidpad_8bitdo_trigger(r[7]);
      device->view.axes[3] = hidpad_8bitdo_trigger(r[6]);
   }
   else
      return;

   device->view.hats = hat < 8 ? hidpad_8bitdo_hat[hat] : 0;
   pad_view_set_buttons(&device->view,
         (uint32_t)buttons[0] | ((uint32_t)buttons[1] << 8),
         (buttons[1] & 0x10) != 0);

   if (device->motors_due && device->rumble && !device->behind_stack)
      hidpad_8bitdo_send(device);
}

static void hidpad_8bitdo_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;
   uint8_t motor = (uint8_t)(strength >> 8);

   if (device && device->motors[idx] != motor)
   {
      device->motors[idx] = motor;
      device->motors_due  = true;
   }
}

static void hidpad_8bitdo_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidpad_8bitdo_get_axis(void *data, unsigned axis)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   if (!device || axis >= INPUT_PAD_VIEW_AXES)
      return 0;
   return device->view.axes[axis];
}

static const char *hidpad_8bitdo_get_name(void *data)
{
   return "8Bitdo SN30 Pro";
}

/* The hat too, for a driver that asks the handler for it */
static int32_t hidpad_8bitdo_button(void *data, uint16_t joykey)
{
   struct hidpad_8bitdo_data *device = (struct hidpad_8bitdo_data*)data;
   unsigned hat_dir;

   if (!device)
      return 0;
   if ((hat_dir = GET_HAT_DIR(joykey)))
   {
      if (GET_HAT(joykey) != 0)
         return 0;
      switch (hat_dir)
      {
         case HAT_UP_MASK:    return (device->view.hats & 1) != 0;
         case HAT_DOWN_MASK:  return (device->view.hats & 2) != 0;
         case HAT_LEFT_MASK:  return (device->view.hats & 4) != 0;
         case HAT_RIGHT_MASK: return (device->view.hats & 8) != 0;
      }
      return 0;
   }
   if (joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_8bitdo_view(void *data)
{
   return &((struct hidpad_8bitdo_data*)data)->view;
}

pad_connection_interface_t pad_connection_8bitdo_sn30_pro = {
   hidpad_8bitdo_init,
   hidpad_8bitdo_deinit,
   hidpad_8bitdo_packet_handler,
   hidpad_8bitdo_set_rumble,
   hidpad_8bitdo_get_buttons,
   hidpad_8bitdo_get_axis,
   hidpad_8bitdo_get_name,
   hidpad_8bitdo_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_8bitdo_view
};
