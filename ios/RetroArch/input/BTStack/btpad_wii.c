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

enum btpad_wii_state { BTPAD_WII_WAITING, BTPAD_WII_PENDING, BTPAD_WII_WANT_NAME, BTPAD_WII_CONNECTING, BTPAD_WII_CONNECTED };

struct btpad_wii_data
{
   enum btpad_wii_state state;
   struct wiimote_t wiimote;
   
   bd_addr_t address;
   uint32_t page_scan_repetition_mode;
   uint32_t class;
   uint32_t clock_offset;
   
};

static void* btpad_wii_connect()
{
   struct btpad_wii_data* device = malloc(sizeof(struct btpad_wii_data));
   memset(device, 0, sizeof(struct btpad_wii_data));

   ios_add_log_message("BTstack Wii: Waiting for connection");
   bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 0);

   device->state = BTPAD_WII_WAITING;
   
   return device;
}

static void btpad_wii_disconnect(struct btpad_wii_data* device)
{
   ios_add_log_message("BTstack Wii: Disconnecting.");

   bt_send_cmd_ptr(hci_disconnect_ptr, device->wiimote.wiiMoteConHandle, 0x15);
   free(device);
}

static void btpad_wii_setleds(struct btpad_wii_data* device, unsigned leds)
{
}

static uint32_t btpad_wii_get_buttons(struct btpad_wii_data* device)
{
   return (device->state == BTPAD_WII_CONNECTED) ? device->wiimote.btns | (device->wiimote.exp.classic.btns << 16) : 0;
}

static void btpad_wii_packet_handler(struct btpad_wii_data* device, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   if (packet_type == HCI_EVENT_PACKET && packet[0] == HCI_EVENT_PIN_CODE_REQUEST)
   {
      ios_add_log_message("BTstack Wii: Sending PIN");
    
      bd_addr_t event_addr;
      bt_flip_addr_ptr(event_addr, &packet[2]);
      bt_send_cmd_ptr(hci_pin_code_request_reply_ptr, event_addr, 6, &packet[2]);
   }

   if (packet_type == HCI_EVENT_PACKET)
   {
      if (device->state == BTPAD_WII_WAITING)
      {
         switch (packet[0])
         {
            case HCI_EVENT_INQUIRY_RESULT:
            {
               if (packet[2])
               {
                  ios_add_log_message("BTstack Wii: Inquiry found device");
                  device->state = BTPAD_WII_PENDING;
         
                  bt_flip_addr_ptr(device->address, &packet[3]);
                  device->page_scan_repetition_mode =   packet [3 + packet[2] * (6)];
                  device->class = READ_BT_24(packet, 3 + packet[2] * (6+1+1+1));
                  device->clock_offset =   READ_BT_16(packet, 3 + packet[2] * (6+1+1+1+3)) & 0x7fff;
               }

               break;
            }
      
            case HCI_EVENT_INQUIRY_COMPLETE:
            {
               bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 0);
               break;
            }
         }
      }
      else if(device->state == BTPAD_WII_PENDING)
      {
         if (packet[0] == HCI_EVENT_INQUIRY_COMPLETE)
         {
            ios_add_log_message("BTstack Wii: Got inquiry complete; requesting name\n");
            device->state = BTPAD_WII_WANT_NAME;

            bt_send_cmd_ptr(hci_remote_name_request_ptr, device->address, device->page_scan_repetition_mode,
                            0, device->clock_offset | 0x8000);
         }
      }
      else if(device->state == BTPAD_WII_WANT_NAME)
      {
         if (packet[0] == HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE)
         {
            bd_addr_t event_addr;
            bt_flip_addr_ptr(event_addr, &packet[3]);
               
            if (BD_ADDR_CMP(event_addr, device->address) == 0)
            {
               if (strncmp((char*)&packet[9], "Nintendo RVL-CNT-01", 19) == 0)
               {
                  ios_add_log_message("BTstack Wii: Got Nintendo RVL-CNT-01; Connecting");
                  device->state = BTPAD_WII_CONNECTING;
      
                  bt_send_cmd_ptr(l2cap_create_channel_ptr, device->address, PSM_HID_CONTROL);
               }
               else
               {
                  ios_add_log_message("BTstack Wii: Unrecognized device %s; Will keep looking", (char*)&packet[9]);
                  memset(device, 0, sizeof(*device));
                  
                  device->state = BTPAD_WII_WAITING;
                  bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 0);
               }
            }
            else
               ios_add_log_message("BTstack Wii: Connection unexpected; ignoring");
         }
      }
      else if(device->state == BTPAD_WII_CONNECTING)
      {
         if (packet[0] == L2CAP_EVENT_CHANNEL_OPENED)
         {
            bd_addr_t event_addr;
            bt_flip_addr_ptr(event_addr, &packet[3]);
            if (BD_ADDR_CMP(device->address, event_addr) != 0)
            {
               ios_add_log_message("BTstack Wii: Incoming L2CAP connection not recognized; ignoring");
               return;
            }
         
            const uint16_t psm = READ_BT_16(packet, 11);
            
            if (packet[2] == 0)
            {
               ios_add_log_message("BTstack: WiiMote L2CAP channel opened: %02X\n", psm);
            
               if (psm == PSM_HID_CONTROL)
               {
                  ios_add_log_message("BTstack Wii: Got HID CONTROL channel; Opening INTERRUPT");
                  bt_send_cmd_ptr(l2cap_create_channel_ptr, device->address, PSM_HID_INTERRUPT);

                  memset(&device->wiimote, 0, sizeof(struct wiimote_t));
                  device->wiimote.c_source_cid = READ_BT_16(packet, 13);
                  device->wiimote.wiiMoteConHandle = READ_BT_16(packet, 9);
                  memcpy(&device->wiimote.addr, &device->address, BD_ADDR_LEN);
                  device->wiimote.exp.type = EXP_NONE;
               }
               else if (psm == PSM_HID_INTERRUPT)
               {
                  ios_add_log_message("BTstack Wii: Got HID INTERRUPT channel; Connected");
                  device->state = BTPAD_WII_CONNECTED;

                  device->wiimote.i_source_cid = READ_BT_16(packet, 13);
                  device->wiimote.state = WIIMOTE_STATE_CONNECTED;
                  wiimote_handshake(&device->wiimote, -1, NULL, -1);
               }
            }
            else
            {
               ios_add_log_message("BTstack Wii: Failed to open WiiMote L2CAP channel for PSM: %02X", psm);
            }
         }
      }
   }
   else if(packet_type == L2CAP_DATA_PACKET)
   {
      byte* msg = packet + 2;
         
      switch (packet[1])
      {
         case WM_RPT_BTN:
         {
            wiimote_pressed_buttons(&device->wiimote, msg);
            break;
         }

         case WM_RPT_READ:
         {
            wiimote_pressed_buttons(&device->wiimote, msg);

            byte len = ((msg[2] & 0xF0) >> 4) + 1;
            byte *data = (msg + 5);

            wiimote_handshake(&device->wiimote, WM_RPT_READ, data, len);
            return;
         }

         case WM_RPT_CTRL_STATUS:
         {
            wiimote_pressed_buttons(&device->wiimote, msg);
            wiimote_handshake(&device->wiimote,WM_RPT_CTRL_STATUS,msg,-1);

            return;
         }

         case WM_RPT_BTN_EXP:
         {
            wiimote_pressed_buttons(&device->wiimote, msg);
            wiimote_handle_expansion(&device->wiimote, msg+2);
            break;
         }
      }
   }
}

struct btpad_interface btpad_wii =
{
   (void*)&btpad_wii_connect,
   (void*)&btpad_wii_disconnect,
   (void*)&btpad_wii_setleds,
   (void*)&btpad_wii_get_buttons,
   (void*)&btpad_wii_packet_handler
};
