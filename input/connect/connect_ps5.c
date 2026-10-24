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

/* The DualSense and DualSense Edge.
 *
 * Input report 0x01 comes in two shapes: on USB, 64 bytes - the sticks,
 * L2 and R2's travel, a counter, then the buttons; over Bluetooth until
 * the pad is sent an output report, 10 bytes - the sticks, the buttons,
 * then the travel. Over Bluetooth after that it is report 0x31, which is
 * USB's layout one byte later. The buttons are the DualShock 4's:
 * - byte 0: the hat in the low four bits, then square, cross, circle
 *   and triangle;
 * - byte 1: L1, R1, L2, R2, create, options, L3, R3, from bit 0;
 * - byte 2: PS, the touchpad's click, mute.
 * After the motion, from byte 33 of the USB report, are the touchpad's
 * two fingers (sony_pad_touch.h); Bluetooth's short report has none.
 *
 * Output - rumble and the player lights - is the report raw input's
 * driver sends (input/common/sony_pad_output.h), over USB or
 * Bluetooth as the input came. Not behind a Bluetooth stack's own
 * header, whose framing of output is that stack's. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "joypad_connection.h"
#include "../input_defines.h"
#include "../common/sony_pad_output.h"
#include "../common/sony_pad_motion.h"
#include "../common/sony_pad_touch.h"
#include "../common/pad_motion.h"

#define PS5_USB_REPORT_LEN    64
#define PS5_BT_SHORT_LEN      10
#define PS5_BT_FULL_LEN       12 /* what is read of report 0x31 */

/* how the pad came: its output goes the same way */
#define PS5_LINK_NONE         0
#define PS5_LINK_USB          1
#define PS5_LINK_BT           2
#define PS5_LINK_STACK        3  /* behind a Bluetooth stack's header */

struct hidpad_ps5_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   uint32_t slot;
   /* the RetroPad's buttons, then PS, touchpad and mute as 16, 17 and
    * 18; the sticks as 0 to 3 and the triggers as 4 and 5 */
   input_pad_view_t view;
   uint8_t motors[2];   /* strong, weak: what the core asked last */
   uint8_t link;
   int player;          /* told: from 1, -1 for none; 0 not yet */
   bool edge;
   bool bluetooth;      /* the driver says so: its reports' lengths do not */
   bool vibration_v2;   /* the newer way of driving the motors */
   bool lights_sent;
   bool motors_due;
   /* motion: in the USB report and Bluetooth's full one, from 16 */
   sony_pad_motion_t motion;
   uint8_t motion_want; /* pad_motion.h's bits */
   bool motion_seen;
   bool effects;        /* it takes the output report */
   bool sensors;        /* it has motion */
};

/* A pad that passes for a DualSense says what it has in its feature
 * report 0x03, as SDL reads it: 48 bytes, 0x28 in byte 2, then in
 * byte 4 the sensors (0x02), light bar (0x04) and rumble (0x08), and
 * in byte 20 the player lights (0x80). One that does not answer is sent
 * nothing. */
static void hidpad_ps5_third_party(struct hidpad_ps5_data *device)
{
   uint8_t feature[64];
   memset(feature, 0, sizeof(feature));
   device->effects = false;
   device->sensors = false;
   if (     !device->driver || !device->driver->get_report
         ||  device->driver->get_report(device->connection,
               HID_REPORT_FEATURE, 0x03, feature, 48) != 48
         ||  feature[2] != 0x28)
      return;
   device->sensors = (feature[4] & 0x02) != 0;
   device->effects = (feature[4] & 0x0c) || (feature[20] & 0x80);
}

static void *hidpad_ps5_start(void *data, uint32_t slot,
      hid_driver_t *driver, bool edge, bool third)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_ps5_data *device;

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_ps5_data*)
            calloc(1, sizeof(struct hidpad_ps5_data))))
      return NULL;

   device->connection   = connection;
   device->driver       = driver;
   device->slot         = slot;
   device->edge         = edge;
   device->bluetooth    = driver && driver->is_bluetooth
      && driver->is_bluetooth(connection);
   /* the Edge drives its motors the newer way; a DualSense from
    * firmware 2.21, which its feature report 0x20 tells */
   device->vibration_v2 = edge;
   device->effects      = true;
   device->sensors      = true;
   /* the others' touchpads are not known to be read the same */
   device->view.touchpad = !third;
   if (third)
      hidpad_ps5_third_party(device);
   else if (!edge && driver && driver->get_report)
   {
      uint8_t feature[64];
      memset(feature, 0, sizeof(feature));
      if (driver->get_report(connection, HID_REPORT_FEATURE, 0x20,
               feature, sizeof(feature)) >= 46)
         device->vibration_v2 =
            (unsigned)(feature[44] | (feature[45] << 8)) > 0x0215;
   }
   return device;
}

static void *hidpad_ps5_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   return hidpad_ps5_start(data, slot, driver, false, false);
}

static void *hidpad_ps5_third_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return hidpad_ps5_start(data, slot, driver, false, true);
}

static void *hidpad_ps5_edge_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return hidpad_ps5_start(data, slot, driver, true, false);
}

static void hidpad_ps5_deinit(void *data)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   if (device)
      free(device);
}

/* The hat's eight ways and its rest (8): up 1, down 2, left 4, right 8. */
static const uint8_t hidpad_ps5_hat[16] = {
   1, 1 | 8, 8, 8 | 2, 2, 2 | 4, 4, 4 | 1,
   0, 0, 0, 0, 0, 0, 0, 0
};

static uint32_t hidpad_ps5_decode(const uint8_t *b)
{
   uint32_t r   = 0;
   unsigned hat = hidpad_ps5_hat[b[0] & 0x0f];
   if (hat & 1)     r |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
   if (hat & 2)     r |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
   if (hat & 4)     r |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
   if (hat & 8)     r |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
   if (b[0] & 0x10) r |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;      /* square   */
   if (b[0] & 0x20) r |= 1u << RETRO_DEVICE_ID_JOYPAD_B;      /* cross    */
   if (b[0] & 0x40) r |= 1u << RETRO_DEVICE_ID_JOYPAD_A;      /* circle   */
   if (b[0] & 0x80) r |= 1u << RETRO_DEVICE_ID_JOYPAD_X;      /* triangle */
   if (b[1] & 0x01) r |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
   if (b[1] & 0x02) r |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
   if (b[1] & 0x04) r |= 1u << RETRO_DEVICE_ID_JOYPAD_L2;
   if (b[1] & 0x08) r |= 1u << RETRO_DEVICE_ID_JOYPAD_R2;
   if (b[1] & 0x10) r |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT; /* create   */
   if (b[1] & 0x20) r |= 1u << RETRO_DEVICE_ID_JOYPAD_START;  /* options  */
   if (b[1] & 0x40) r |= 1u << RETRO_DEVICE_ID_JOYPAD_L3;
   if (b[1] & 0x80) r |= 1u << RETRO_DEVICE_ID_JOYPAD_R3;
   r |= (uint32_t)(b[2] & 0x07) << 16;                        /* PS, pad, mute */
   /* an Edge's Fn buttons, then its back buttons, left first */
   r |= (uint32_t)(b[2] & 0xf0) << 15;
   return r;
}

/* One output report: the rumble, and the first time the player's
 * lights. Sent from the poll, so a core changing its rumble every frame
 * costs one report a poll at most. */
static void hidpad_ps5_send(struct hidpad_ps5_data *device)
{
   uint8_t r[SONY_PAD_REPORT_MAX];
   size_t len = sony_pad_output_report(r, sizeof(r), SONY_PAD_DUALSENSE,
         device->link == PS5_LINK_BT, device->vibration_v2,
         device->motors[0], device->motors[1],
         device->lights_sent ? 0 : device->player ? device->player
         : (int)(device->slot % 4) + 1);

   if (len && device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, len);
   device->lights_sent = true;
   device->motors_due  = false;
}

static void hidpad_ps5_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   const uint8_t *r               = packet;
   const uint8_t *buttons, *triggers;
   unsigned len                   = size;
   unsigned motion_len            = 0; /* what of the report after r */
   unsigned i;
   uint8_t link                   = PS5_LINK_NONE;

   if (!device || !r)
      return;

   /* a Bluetooth stack's DATA | INPUT header */
   if (len && r[0] == 0xa1)
   {
      r++;
      len--;
      link = PS5_LINK_STACK;
   }
   if (!len)
      return;

   if (r[0] == 0x01 && len >= PS5_USB_REPORT_LEN && !device->bluetooth)
   {
      triggers    = r + 5;
      buttons     = r + 8;
      motion_len  = len;
      if (!link)
         link     = PS5_LINK_USB;
   }
   else if (r[0] == 0x01 && len >= PS5_BT_SHORT_LEN)
   {
      buttons     = r + 5;
      triggers    = r + 8;
   }
   else if (r[0] == 0x31 && len >= PS5_BT_FULL_LEN)
   {
      r++;        /* the report's sequence byte */
      triggers    = r + 5;
      buttons     = r + 8;
      motion_len  = len - 1;
   }
   else
      return;
   device->link = link ? link : PS5_LINK_BT;

   for (i = 0; i < 4; i++)
      device->view.axes[i]     = (int16_t)(((int)r[1 + i] << 8) - 0x8000);
   for (i = 0; i < 2; i++)
      device->view.axes[4 + i] = (int16_t)((triggers[i] << 7)
            | (triggers[i] >> 1));
   pad_view_set_buttons(&device->view, hidpad_ps5_decode(buttons),
         (buttons[2] & 0x01) != 0);
   if (device->view.touchpad && motion_len >= 41)
      sony_pad_touch(&device->view, r + 33, SONY_PAD_TOUCH_H_DS5,
            (buttons[2] & 0x02) != 0);
   else
      device->view.touch_down = 0;
   if (device->motion_want && device->sensors && motion_len >= 16 + 12)
   {
      for (i = 0; i < 3; i++)
      {
         device->motion.gyro[i]  = sony_pad_le16(r + 16 + 2 * i);
         device->motion.accel[i] = sony_pad_le16(r + 22 + 2 * i);
      }
      device->motion_seen = true;
   }

   if (     device->link != PS5_LINK_STACK && device->effects
         && (!device->lights_sent || device->motors_due))
      hidpad_ps5_send(device);
}

static void hidpad_ps5_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   unsigned idx                   = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;
   uint8_t motor                  = (uint8_t)(strength >> 8);

   if (device && device->motors[idx] != motor)
   {
      device->motors[idx] = motor;
      device->motors_due  = true;
   }
}

static void hidpad_ps5_set_player(void *data, unsigned player)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   if (device)
   {
      device->player      = player ? (int)player : -1;
      device->lights_sent = false;
   }
}

static void hidpad_ps5_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;

   if (device)
      *state = device->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

/* The sticks as 0 to 3, L2 and R2's travel as 4 and 5. No dead zone
 * here: the frontend's applies. */
static int16_t hidpad_ps5_get_axis(void *data, unsigned axis)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;

   if (!device || axis >= INPUT_PAD_VIEW_AXES)
      return 0;
   return device->view.axes[axis];
}

static const char *hidpad_ps5_get_name(void *data)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   return (device && device->edge)
      ? "DualSense Edge Wireless Controller"
      : "DualSense Wireless Controller";
}

static int32_t hidpad_ps5_button(void *data, uint16_t joykey)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static bool hidpad_ps5_set_sensor(void *data,
      enum retro_sensor_action action)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   if (!device || !pad_motion_want(&device->motion_want, action))
      return false;
   if (!device->motion_want)
      device->motion_seen = false;
   return true;
}

/* at the pads' nominal scale, as winraw reads them */
static bool hidpad_ps5_sensor(void *data, unsigned id, float *value)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   if (!device || !device->motion_seen)
      return false;
   if (id <= RETRO_SENSOR_ACCELEROMETER_Z
         ? !(device->motion_want & PAD_MOTION_ACCEL)
         : !(device->motion_want & PAD_MOTION_GYRO))
      return false;
   return sony_pad_motion_value(&device->motion, id, value);
}

static const input_pad_view_t *hidpad_ps5_view(void *data)
{
   return &((struct hidpad_ps5_data*)data)->view;
}

pad_connection_interface_t pad_connection_ps5 = {
   hidpad_ps5_init,
   hidpad_ps5_deinit,
   hidpad_ps5_packet_handler,
   hidpad_ps5_set_rumble,
   hidpad_ps5_get_buttons,
   hidpad_ps5_get_axis,
   hidpad_ps5_get_name,
   hidpad_ps5_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_ps5_set_player,
   hidpad_ps5_view,
   NULL,  /* idle */
   hidpad_ps5_set_sensor,
   hidpad_ps5_sensor
};

pad_connection_interface_t pad_connection_ps5_edge = {
   hidpad_ps5_edge_init,
   hidpad_ps5_deinit,
   hidpad_ps5_packet_handler,
   hidpad_ps5_set_rumble,
   hidpad_ps5_get_buttons,
   hidpad_ps5_get_axis,
   hidpad_ps5_get_name,
   hidpad_ps5_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_ps5_set_player,
   hidpad_ps5_view,
   NULL,  /* idle */
   hidpad_ps5_set_sensor,
   hidpad_ps5_sensor
};

/* the pads that pass for one */
pad_connection_interface_t pad_connection_ps5_third_party = {
   hidpad_ps5_third_init,
   hidpad_ps5_deinit,
   hidpad_ps5_packet_handler,
   hidpad_ps5_set_rumble,
   hidpad_ps5_get_buttons,
   hidpad_ps5_get_axis,
   hidpad_ps5_get_name,
   hidpad_ps5_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   hidpad_ps5_set_player,
   hidpad_ps5_view,
   NULL,  /* idle */
   hidpad_ps5_set_sensor,
   hidpad_ps5_sensor
};
