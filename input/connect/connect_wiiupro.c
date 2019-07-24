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
#include "../common/hid/hid_device_driver.h"

struct wiiupro_buttons
{
   bool a;
   bool b;
   bool x;
   bool y;
   bool l;
   bool r;
   bool zl;
   bool zr;
   bool minus;
   bool plus;
   bool l3;
   bool r3;
   bool home;

   /* D-Pad */
   bool left;
   bool right;
   bool up;
   bool down;
}__attribute__((packed));

struct wiiupro
{
   int32_t hatvalue[4];
   struct wiiupro_buttons btn;
};


struct hidpad_wiiupro_data
{
   struct pad_connection* connection;
   hid_driver_t *driver;
   struct wiiupro data;
   uint32_t slot;
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
   /* 0x12 = Set data report; 0x34 = All buttons and analogs */
   static uint8_t report_buffer[4] = { 0xA2, 0x12, 0x00, 0x34 };
   device->driver->send_control(device->connection,
         report_buffer, sizeof(report_buffer));
}

static void* hidpad_wiiupro_init(void *data,
      uint32_t slot, hid_driver_t *driver)
{
   struct pad_connection* connection = (struct pad_connection*)data;
   struct hidpad_wiiupro_data* device    = (struct hidpad_wiiupro_data*)
      calloc(1, sizeof(struct hidpad_wiiupro_data));

   if (!device)
      goto error;

   if (!connection)
      goto error;

   device->connection = connection;
   device->slot       = slot;
   device->driver     = driver;

   device->calib.calib_round = 0;
   /* Without this, the digital buttons won't be reported. */
   hidpad_wiiupro_send_control(device);

   return device;

error:
   if (device)
      free(device);
   return NULL;
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
   struct wiiupro                *rpt = device ?
      (struct wiiupro*)&device->data : NULL;

   if (!device || !rpt)
      return;

   BIT256_CLEAR_ALL_PTR(state);

   if (rpt->btn.r3)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_R3);
   if (rpt->btn.l3)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_L3);
   if (rpt->btn.plus)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_START);
   if ( rpt->btn.minus)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_SELECT);
   if ( rpt->btn.zr)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_R2);
   if ( rpt->btn.zl)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_L2);
   if ( rpt->btn.r)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_R);
   if ( rpt->btn.l)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_L);
   if ( rpt->btn.x)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_X);
   if ( rpt->btn.a)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_A);
   if ( rpt->btn.b)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_B);
   if ( rpt->btn.y)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_Y);
   if ( rpt->btn.left)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_LEFT);
   if ( rpt->btn.down)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_DOWN);
   if ( rpt->btn.right)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   if ( rpt->btn.up)
      BIT256_SET_PTR(state, RETRO_DEVICE_ID_JOYPAD_UP);
   if ( rpt->btn.home)
      BIT256_SET_PTR(state, RARCH_MENU_TOGGLE);
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

   if (!device)
      return;

#if 0
   if (!device->have_led)
   {
      hidpad_wiiupro_send_control(device);
      device->have_led = true;
   }
#endif

   packet[0x0C] ^= 0xFF;
   packet[0x0D] ^= 0xFF;
   packet[0x0E] ^= 0xFF;

   memset(&device->data, 0, sizeof(struct wiiupro));

   device->data.btn.b       = (packet[0x0D] & 0x40) ? 1 : 0;
   device->data.btn.a       = (packet[0x0D] & 0x10) ? 1 : 0;
   device->data.btn.y       = (packet[0x0D] & 0x20) ? 1 : 0;
   device->data.btn.x       = (packet[0x0D] & 0x08) ? 1 : 0;
   device->data.btn.l       = (packet[0x0C] & 0x20) ? 1 : 0;
   device->data.btn.r       = (packet[0x0C] & 0x02) ? 1 : 0;
   device->data.btn.zl      = (packet[0x0D] & 0x80) ? 1 : 0;
   device->data.btn.zr      = (packet[0x0D] & 0x04) ? 1 : 0;
   device->data.btn.minus   = (packet[0x0C] & 0x10) ? 1 : 0;
   device->data.btn.plus    = (packet[0x0C] & 0x04) ? 1 : 0;
   device->data.btn.l3      = (packet[0x0E] & 0x02) ? 1 : 0;
   device->data.btn.r3      = (packet[0x0E] & 0x01) ? 1 : 0;

   device->data.btn.left    = (packet[0x0D] & 0x02) ? 1 : 0;
   device->data.btn.right   = (packet[0x0C] & 0x80) ? 1 : 0;
   device->data.btn.up      = (packet[0x0D] & 0x01) ? 1 : 0;
   device->data.btn.down    = (packet[0x0C] & 0x40) ? 1 : 0;

   device->data.btn.home    = (packet[0x0C] & 0x8)  ? 1 : 0;

   if(device->calib.calib_round < 5)
   {
       device->calib.hatvalue_calib[0] = (packet[4] |  (packet[4 + 1] << 8));
       device->calib.hatvalue_calib[1] = (packet[8] |  (packet[8 + 1] << 8));
       device->calib.hatvalue_calib[2] = (packet[6] |  (packet[6 + 1] << 8));
       device->calib.hatvalue_calib[3] = (packet[10] | (packet[10 + 1] << 8));

       device->calib.calib_round++;
   }
   else
   {
       device->data.hatvalue[0] = (packet[4] |  (packet[4 + 1] << 8))
          - device->calib.hatvalue_calib[0];
       device->data.hatvalue[1] = (packet[8] |  (packet[8 + 1] << 8))
          - device->calib.hatvalue_calib[1];
       device->data.hatvalue[2] = (packet[6] |  (packet[6 + 1] << 8))
          - device->calib.hatvalue_calib[2];
       device->data.hatvalue[3] = (packet[10] | (packet[10 + 1] << 8))
          - device->calib.hatvalue_calib[3];
   }
}

static void hidpad_wiiupro_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   /* TODO */
}

pad_connection_interface_t pad_connection_wiiupro = {
   hidpad_wiiupro_init,
   hidpad_wiiupro_deinit,
   hidpad_wiiupro_packet_handler,
   hidpad_wiiupro_set_rumble,
   hidpad_wiiupro_get_buttons,
   hidpad_wiiupro_get_axis,
   NULL,
};
