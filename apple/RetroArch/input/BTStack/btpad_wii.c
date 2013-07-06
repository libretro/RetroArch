/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include "boolean.h"
#include "../../rarch_wrapper.h"

#include "btdynamic.h"
#include "btpad.h"
#include "wiimote.h"

static void* btpad_wii_connect(const btpad_connection_t* connection)
{
   struct wiimote_t* device = malloc(sizeof(struct wiimote_t));
   memset(device, 0, sizeof(struct wiimote_t));

   memcpy(device->addr, connection->address, BD_ADDR_LEN);

   device->unid = connection->slot;
   device->wiiMoteConHandle = connection->handle;
   device->c_source_cid = connection->channels[0];
   device->i_source_cid = connection->channels[1];
   device->state = WIIMOTE_STATE_CONNECTED;
   device->exp.type = EXP_NONE;

   wiimote_handshake(device, -1, NULL, -1);
   
   return device;
}

static void btpad_wii_disconnect(struct wiimote_t* device)
{
}

static uint32_t btpad_wii_get_buttons(struct wiimote_t* device)
{
   return device->btns | (device->exp.classic.btns << 16);
}

static int16_t btpad_wii_get_axis(struct wiimote_t* device, unsigned axis)
{
/* TODO
   if (device->.exp.type == EXP_CLASSIC)
   {
      switch (axis)
      {
         case 0: return device->wiimote.exp.classic.ljs.rx * 0x7FFF;
         case 1: return device->wiimote.exp.classic.ljs.ry * 0x7FFF;
         case 2: return device->wiimote.exp.classic.rjs.rx * 0x7FFF;
         case 3: return device->wiimote.exp.classic.rjs.ry * 0x7FFF;
         default: return 0;
      }
   }
*/
   
   return 0;
}

static void btpad_wii_packet_handler(struct wiimote_t* device, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   if(packet_type == L2CAP_DATA_PACKET)
   {
      byte* msg = packet + 2;
         
      switch (packet[1])
      {
         case WM_RPT_BTN:
         {
            wiimote_pressed_buttons(device, msg);
            break;
         }

         case WM_RPT_READ:
         {
            wiimote_pressed_buttons(device, msg);
            wiimote_handshake(device, WM_RPT_READ, msg + 5, ((msg[2] & 0xF0) >> 4) + 1);
            break;
         }

         case WM_RPT_CTRL_STATUS:
         {
            wiimote_pressed_buttons(device, msg);
            wiimote_handshake(device,WM_RPT_CTRL_STATUS,msg,-1);
            break;
         }

         case WM_RPT_BTN_EXP:
         {
            wiimote_pressed_buttons(device, msg);
            wiimote_handle_expansion(device, msg+2);
            break;
         }
      }

      g_current_input_data.pad_buttons[device->unid] = btpad_wii_get_buttons(device);
      for (int i = 0; i < 4; i ++)
         g_current_input_data.pad_axis[device->unid][i] = btpad_wii_get_axis(device, i);
   }
}

struct btpad_interface btpad_wii =
{
   (void*)&btpad_wii_connect,
   (void*)&btpad_wii_disconnect,
   (void*)&btpad_wii_get_buttons,
   (void*)&btpad_wii_get_axis,
   (void*)&btpad_wii_packet_handler
};
