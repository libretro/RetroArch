/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../boolean.h"
#include "wiimote.h"

static void* hidpad_wii_connect(void *data, uint32_t slot)
{
   struct pad_connection *connection = (struct pad_connection*)data;
   struct wiimote_t *device = (struct wiimote_t*)
      calloc(1, sizeof(struct wiimote_t));

   if (!device || !connection)
      return NULL;

   device->connection = connection;
   device->unid = slot;
   device->state = WIIMOTE_STATE_CONNECTED;
   device->exp.type = EXP_NONE;

   wiimote_handshake(device, -1, NULL, -1);

   return device;
}

static void hidpad_wii_disconnect(void *data)
{
   struct wiimote_t* device = (struct wiimote_t*)data;

   if (device)
      free(device);
}

static int16_t hidpad_wii_get_axis(void *data, unsigned axis)
{
   struct wiimote_t* device = (struct wiimote_t*)data;

   if (device && device->exp.type == EXP_CLASSIC)
   {
      switch (axis)
      {
         case 0:
            return device->exp.cc.classic.ljs.x.value * 0x7FFF;
         case 1:
            return device->exp.cc.classic.ljs.y.value * 0x7FFF;
         case 2:
            return device->exp.cc.classic.rjs.x.value * 0x7FFF;
         case 3:
            return device->exp.cc.classic.rjs.y.value * 0x7FFF;
      }
   }

   return 0;
}

static uint32_t hidpad_wii_get_buttons(void *data)
{
   struct wiimote_t* device = (struct wiimote_t*)data;
   if (device)
      return  device->btns | (device->exp.cc.classic.btns << 16);
   return 0;
}

static void hidpad_wii_packet_handler(void *data,
      uint8_t *packet, uint16_t size)
{
   struct wiimote_t* device = (struct wiimote_t*)data;
   byte* msg = packet + 2;

   if (!device)
      return;

   switch (packet[1])
   {
      case WM_RPT_BTN:
         wiimote_pressed_buttons(device, msg);
         break;
      case WM_RPT_READ:
         wiimote_pressed_buttons(device, msg);
         wiimote_handshake(device, WM_RPT_READ, msg + 5,
               ((msg[2] & 0xF0) >> 4) + 1);
         break;
      case WM_RPT_CTRL_STATUS:
         wiimote_pressed_buttons(device, msg);
         wiimote_handshake(device,WM_RPT_CTRL_STATUS,msg,-1);
         break;
      case WM_RPT_BTN_EXP:
         wiimote_pressed_buttons(device, msg);
         wiimote_handle_expansion(device, msg+2);
         break;
   }
}

static void hidpad_wii_set_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength)
{
   /* TODO */
   (void)data;
   (void)effect;
   (void)strength;
}

pad_connection_interface_t apple_pad_wii = {
   hidpad_wii_connect,
   hidpad_wii_disconnect,
   hidpad_wii_packet_handler,
   hidpad_wii_set_rumble,
   hidpad_wii_get_buttons,
   hidpad_wii_get_axis,
};
