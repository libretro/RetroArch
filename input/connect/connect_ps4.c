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

/* The input report after its ID: the sticks' four bytes, then
 * - byte 4: the hat in the low four bits, then square, cross, circle
 *   and triangle;
 * - byte 5: L1, R1, L2, R2, share, options, L3, R3, from bit 0;
 * - byte 6: PS in bit 0, the touchpad's click in bit 1;
 * - bytes 7 and 8: L2 and R2's travel. */
#define PS4_REPORT_LEN 9

struct hidpad_ps4_data
{
   struct pad_connection* connection;
   hid_driver_t *driver;
   uint32_t slot;
   input_pad_view_t view; /* the RetroPad's buttons and the sticks */
   uint16_t motors[2];
   bool have_led;
};

static void hidpad_ps4_send_control(struct hidpad_ps4_data* device)
{
   /* TODO: Can this be modified to turn off motion tracking? */
   static uint8_t report_buffer[79] = {
      0x52, 0x11, 0xB0, 0x00, 0x0F
   };

#if 0
   uint8_t rgb[4][3] = { { 0xFF, 0, 0 }, { 0, 0xFF, 0 }, { 0, 0, 0xFF }, { 0xFF, 0xFF, 0xFF } };
   report_buffer[ 9] = rgb[(device->slot % 4)][0];
   report_buffer[10] = rgb[(device->slot % 4)][1];
   report_buffer[11] = rgb[(device->slot % 4)][2];
#endif

   device->driver->send_control(device->connection,
         report_buffer, sizeof(report_buffer));
}

static void* hidpad_ps4_init(void *data, uint32_t slot, hid_driver_t *driver)
{
#if 0
   uint8_t magic_data[0x25];
#endif
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

#if 0
   /* TODO - unsure of this */
   /* This is needed to get full input packet over bluetooth. */
   device->driver->send_control(device->connection, magic_data, 0x2);
   (void)magic_data;
#endif

   /* Without this, the digital buttons won't be reported. */
   hidpad_ps4_send_control(device);

   return device;
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
   unsigned i;

   if (!device)
      return;

#if 0
   if (!device->have_led)
   {
      hidpad_ps4_send_control(device);
      device->have_led = true;
   }
#endif

   /* read where it lies: the buttons decoded and the sticks kept, not
    * the report */
   if (size < 1 + PS4_REPORT_LEN)
      return;
   for (i = 0; i < 4; i++)
   {
      int val = ((int)packet[1 + i] << 8) - 0x8000;
      device->view.axes[i] = (int16_t)((abs(val) > 0x1000) ? val : 0);
   }
   pad_view_set_buttons(&device->view, hidpad_ps4_decode(packet + 1),
         (packet[1 + 6] & 0x01) != 0);
}

static void hidpad_ps4_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   /* TODO */
#if 0
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   unsigned idx = (effect == RETRO_RUMBLE_STRONG) ? 0 : 1;

   if (device && (device->motors[idx] != strength))
   {
      device->motors[idx] = strength;
      hidpad_ps4_send_control(device);
   }
#endif
}

static int32_t hidpad_ps4_button(void *data, uint16_t joykey)
{
   struct hidpad_ps4_data *device = (struct hidpad_ps4_data*)data;
   if (!device || joykey > 31)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
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
   NULL,  /* set_player */
   hidpad_ps4_view
};
