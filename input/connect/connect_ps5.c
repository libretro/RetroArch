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
 *
 * Output (rumble, the player lights) goes as USB's report 0x02 only:
 * Bluetooth's needs a checksum and its framing differs between the
 * Bluetooth backends. */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "joypad_connection.h"
#include "../input_defines.h"

#define PS5_USB_REPORT_LEN    64
#define PS5_BT_SHORT_LEN      10
#define PS5_BT_FULL_LEN       12 /* what is read of report 0x31 */
#define PS5_USB_OUTPUT_LEN    48

/* bytes of the USB output report, its ID at 0 */
#define PS5_OUT_FLAGS0        1  /* 0x01 rumble, 0x02 rumble not haptics */
#define PS5_OUT_FLAGS1        2  /* 0x04 lightbar, 0x10 player lights */
#define PS5_OUT_MOTOR_WEAK    3
#define PS5_OUT_MOTOR_STRONG  4
#define PS5_OUT_FLAGS2        39 /* 0x02 lightbar setup */
#define PS5_OUT_LIGHT_SETUP   42 /* 0x02: the start-up glow fades out */
#define PS5_OUT_PLAYER_LIGHTS 44
#define PS5_OUT_LIGHTBAR      45 /* red, green, blue */

struct hidpad_ps5_data
{
   struct pad_connection *connection;
   hid_driver_t *driver;
   uint32_t slot;
   uint32_t buttons;    /* the RetroPad's, then PS, touchpad and mute
                           as 16, 17 and 18 */
   uint8_t sticks[4];
   uint8_t triggers[2];
   uint8_t motors[2];   /* strong, weak: what the core asked last */
   bool ps;
   bool usb;            /* the reports come over USB: output can go */
   bool lights_sent;
   bool motors_due;
};

static void *hidpad_ps5_init(void *data, uint32_t slot, hid_driver_t *driver)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct hidpad_ps5_data *device;

   if (!connection)
      return NULL;
   if (!(device = (struct hidpad_ps5_data*)
            calloc(1, sizeof(struct hidpad_ps5_data))))
      return NULL;

   device->connection = connection;
   device->driver     = driver;
   device->slot       = slot;
   device->sticks[0]  = device->sticks[1] = 0x80;
   device->sticks[2]  = device->sticks[3] = 0x80;
   return device;
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
   return r;
}

/* One USB output report: the rumble, and the first time the player's
 * lights. Sent from the poll, so a core changing its rumble every frame
 * costs one report a poll at most. */
static void hidpad_ps5_send(struct hidpad_ps5_data *device)
{
   static const uint8_t player_lights[4] = { 0x04, 0x0a, 0x15, 0x1b };
   static const uint8_t lightbar[4][3]   = {
      { 0x00, 0x00, 0x40 }, { 0x40, 0x00, 0x00 },
      { 0x00, 0x40, 0x00 }, { 0x20, 0x00, 0x20 } };
   uint8_t r[PS5_USB_OUTPUT_LEN];
   unsigned player = device->slot & 3;

   memset(r, 0, sizeof(r));
   r[0]                    = 0x02;
   r[PS5_OUT_FLAGS0]       = 0x01 | 0x02;
   r[PS5_OUT_MOTOR_STRONG] = device->motors[0];
   r[PS5_OUT_MOTOR_WEAK]   = device->motors[1];
   if (!device->lights_sent)
   {
      r[PS5_OUT_FLAGS1]        = 0x04 | 0x10;
      r[PS5_OUT_FLAGS2]        = 0x02;
      r[PS5_OUT_LIGHT_SETUP]   = 0x02;
      r[PS5_OUT_PLAYER_LIGHTS] = player_lights[player];
      memcpy(&r[PS5_OUT_LIGHTBAR], lightbar[player], 3);
   }
   if (device->driver && device->driver->send_control)
      device->driver->send_control(device->connection, r, sizeof(r));
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

   if (!device || !r)
      return;

   /* a Bluetooth stack's DATA | INPUT header */
   if (len && r[0] == 0xa1)
   {
      r++;
      len--;
   }
   if (!len)
      return;

   if (r[0] == 0x01 && len >= PS5_USB_REPORT_LEN)
   {
      triggers    = r + 5;
      buttons     = r + 8;
      device->usb = true;
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
   }
   else
      return;

   memcpy(device->sticks, r + 1, sizeof(device->sticks));
   memcpy(device->triggers, triggers, sizeof(device->triggers));
   device->buttons = hidpad_ps5_decode(buttons);
   device->ps      = (buttons[2] & 0x01) != 0;

   if (device->usb && (!device->lights_sent || device->motors_due))
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

static void hidpad_ps5_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;

   BIT256_CLEAR_ALL_PTR(state);
   if (!device)
      return;
   BITS_COPY32_PTR(state, device->buttons);
   if (device->ps)
      BIT256_SET_PTR(state, RARCH_MENU_TOGGLE);
}

/* The sticks as 0 to 3, L2 and R2's travel as 4 and 5. No dead zone
 * here: the frontend's applies. */
static int16_t hidpad_ps5_get_axis(void *data, unsigned axis)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;

   if (!device)
      return 0;
   if (axis < 4)
      return (int16_t)(((int)device->sticks[axis] << 8) - 0x8000);
   if (axis < 6)
   {
      unsigned t = device->triggers[axis - 4];
      return (int16_t)((t << 7) | (t >> 1));
   }
   return 0;
}

static const char *hidpad_ps5_get_name(void *data)
{
   (void)data;
   return "DualSense Wireless Controller";
}

static int32_t hidpad_ps5_button(void *data, uint16_t joykey)
{
   struct hidpad_ps5_data *device = (struct hidpad_ps5_data*)data;
   if (!device || joykey > 31)
      return 0;
   return (device->buttons >> joykey) & 1;
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
   false
};
