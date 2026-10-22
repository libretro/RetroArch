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
#include "../../driver.h"

/* report 0x34, its ID first: the sticks from 3, the buttons from 0x0B
 * to 0x0D */
#define WIIUPRO_REPORT_ID  0x34
#define WIIUPRO_REPORT_MIN 0x0E

struct wiiupro
{
   int32_t hatvalue[4];
};

struct hidpad_wiiupro_data
{
   struct pad_connection* connection;
   hid_driver_t *driver;
   struct wiiupro data;
   input_pad_view_t view;  /* the buttons, which the getters answer */
   uint32_t slot;
   bool ids_first;      /* the driver wants no HIDP header */
   bool have_led;
   uint16_t motors[2];
   struct
   {
      int32_t hatvalue_calib[4];
      uint16_t calib_round;
   } calib;
};

static void hidpad_wiiupro_send_control(struct hidpad_wiiupro_data* device)
{
   /* 0x12 = Set data report; 0x34 = All buttons and analogs; after the
    * HIDP header where the driver takes one */
   static uint8_t report_buffer[4] = { 0xA2, 0x12, 0x00, 0x34 };
   if (device->ids_first)
      device->driver->send_control(device->connection,
            report_buffer + 1, sizeof(report_buffer) - 1);
   else
      device->driver->send_control(device->connection,
            report_buffer, sizeof(report_buffer));
}

static void* hidpad_wiiupro_init(void *data,
      uint32_t slot, hid_driver_t *driver)
{
   struct pad_connection* connection  = (struct pad_connection*)data;
   struct hidpad_wiiupro_data* device = (struct hidpad_wiiupro_data*)
      calloc(1, sizeof(struct hidpad_wiiupro_data));

   if (!device)
      return NULL;

   if (!connection)
   {
      free(device);
      return NULL;
   }

   device->connection        = connection;
   device->slot              = slot;
   device->driver            = driver;
   device->ids_first         = PAD_CONNECTION_IDS_FIRST(driver);

   device->calib.calib_round = 0;
   /* Without this, the digital buttons won't be reported. */
   hidpad_wiiupro_send_control(device);

   return device;
}

static void hidpad_wiiupro_deinit(void *data)
{
   struct hidpad_wiiupro_data *device = (struct hidpad_wiiupro_data*)data;

   if (device)
      free(device);
}

static void hidpad_wiiupro_get_buttons(void *data, input_bits_t *state)
{
   struct hidpad_wiiupro_data *device = (struct hidpad_wiiupro_data*)data;
   if (device)
      *state = device->view.buttons;
}

static int16_t hidpad_wiiupro_get_axis(void *data, unsigned axis)
{
#if 0
   struct hidpad_wiiupro_data *device = (struct hidpad_wiiupro_data*)data;
   struct wiiupro *rpt = device ? (struct wiiupro*)&device->data : NULL;

   if (device && (axis < 4))
   {
      int val = rpt ? rpt->hatvalue[axis] : 0;
      val = (val << 8) - 0x8000;
      return (abs(val) > 0x1000) ? val : 0;
   }
#endif

   return 0;
}

static void hidpad_wiiupro_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct hidpad_wiiupro_data *device = (struct hidpad_wiiupro_data*)data;

   if (!device || !packet)
      return;
   /* a Bluetooth stack's DATA | INPUT header */
   if (size && packet[0] == 0xa1)
   {
      packet++;
      size--;
   }
   if (size < WIIUPRO_REPORT_MIN || packet[0] != WIIUPRO_REPORT_ID)
      return;

#if 0
   if (!device->have_led)
   {
      hidpad_wiiupro_send_control(device);
      device->have_led      = true;
   }
#endif

   /* the buttons are low when held; read where they lie, the report
    * left as it was given */
   {
      uint8_t c       = (uint8_t)~packet[0x0B];
      uint8_t d       = (uint8_t)~packet[0x0C];
      uint8_t e       = (uint8_t)~packet[0x0D];
      uint32_t b      = 0;
      if (d & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
      if (d & 0x10) b |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
      if (d & 0x20) b |= 1u << RETRO_DEVICE_ID_JOYPAD_Y;
      if (d & 0x08) b |= 1u << RETRO_DEVICE_ID_JOYPAD_X;
      if (c & 0x20) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L;
      if (c & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R;
      if (d & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L2;
      if (d & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R2;
      if (c & 0x10) b |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
      if (c & 0x04) b |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
      if (e & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_L3;
      if (e & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_R3;
      if (d & 0x02) b |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
      if (c & 0x80) b |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
      if (d & 0x01) b |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
      if (c & 0x40) b |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
      pad_view_set_buttons(&device->view, b, (c & 0x08) != 0);
   }

   if (device->calib.calib_round < 5)
   {
       device->calib.hatvalue_calib[0] = (packet[3] |  (packet[3 + 1] << 8));
       device->calib.hatvalue_calib[1] = (packet[7] |  (packet[7 + 1] << 8));
       device->calib.hatvalue_calib[2] = (packet[5] |  (packet[5 + 1] << 8));
       device->calib.hatvalue_calib[3] = (packet[9] |  (packet[9 + 1] << 8));

       device->calib.calib_round++;
   }
   else
   {
       device->data.hatvalue[0] = (packet[3] |  (packet[3 + 1] << 8))
          - device->calib.hatvalue_calib[0];
       device->data.hatvalue[1] = (packet[7] |  (packet[7 + 1] << 8))
          - device->calib.hatvalue_calib[1];
       device->data.hatvalue[2] = (packet[5] |  (packet[5 + 1] << 8))
          - device->calib.hatvalue_calib[2];
       device->data.hatvalue[3] = (packet[9] |  (packet[9 + 1] << 8))
          - device->calib.hatvalue_calib[3];
   }
}

/* TODO/FIXME */
static void hidpad_wiiupro_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength) { }

static int32_t hidpad_wiiupro_button(void *data, uint16_t joykey)
{
   struct hidpad_wiiupro_data *device = (struct hidpad_wiiupro_data*)data;
   if (!device || joykey >= 256)
      return 0;
   return BIT256_GET(device->view.buttons, joykey) ? 1 : 0;
}

static const input_pad_view_t *hidpad_wiiupro_view(void *data)
{
   return &((struct hidpad_wiiupro_data*)data)->view;
}

pad_connection_interface_t pad_connection_wiiupro = {
   hidpad_wiiupro_init,
   hidpad_wiiupro_deinit,
   hidpad_wiiupro_packet_handler,
   hidpad_wiiupro_set_rumble,
   hidpad_wiiupro_get_buttons,
   hidpad_wiiupro_get_axis,
   NULL, /* get_name */
   hidpad_wiiupro_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidpad_wiiupro_view
};
