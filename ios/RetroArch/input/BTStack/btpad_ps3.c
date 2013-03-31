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

enum btpad_ps3_state { BTPAD_PS3_INITIALIZING, BTPAD_PS3_PENDING, BTPAD_PS3_WANT_NAME, BTPAD_PS3_CONNECTED };

struct btpad_ps3_data
{
   enum btpad_ps3_state state;

   uint8_t data[512];
   
   bool have_address;
   bd_addr_t address;
   uint32_t handle;
   uint32_t channels[2];
   
   bool have_led;
};

static void* btpad_ps3_connect()
{
   struct btpad_ps3_data* device = malloc(sizeof(struct btpad_ps3_data));
   memset(device, 0, sizeof(*device));

   ios_add_log_message("BTstack PS3: Registering HID INTERRUPT service");
   bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_INTERRUPT, 672);
   device->state = BTPAD_PS3_INITIALIZING;
   
   return device;
}

static void btpad_ps3_disconnect(struct btpad_ps3_data* device)
{
   ios_add_log_message("BTstack PS3: Disconnecting.");

   bt_send_cmd_ptr(hci_disconnect_ptr, device->handle, 0x15);
   free(device);
}

static void btpad_ps3_setleds(struct btpad_ps3_data* device, unsigned leds)
{
   static uint8_t report_buffer[] = {
      0x52, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0xff, 0x27, 0x10, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
   };
   
   report_buffer[11] = (leds & 0xF) << 1;
   bt_send_l2cap_ptr(device->channels[0], report_buffer, sizeof(report_buffer));
}

static uint32_t btpad_ps3_get_buttons(struct btpad_ps3_data* device)
{
   return (device->state == BTPAD_PS3_CONNECTED) ? device->data[3] | (device->data[4] << 8) | ((device->data[5] & 1) << 16): 0;
}

static int16_t btpad_ps3_get_axis(struct btpad_ps3_data* device, unsigned axis)
{
   if (axis < 4)
   {
      int val = device->data[7 + axis];
      val = (val << 8) - 0x8000;
      return (abs(val) > 0x1000) ? val : 0;
   }

   return 0;
}

static void btpad_ps3_packet_handler(struct btpad_ps3_data* device, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   if (packet_type == HCI_EVENT_PACKET)
   {
      if (device->state == BTPAD_PS3_INITIALIZING)
      {
         if (packet[0] == L2CAP_EVENT_SERVICE_REGISTERED)
         {
            uint32_t psm = READ_BT_16(packet, 3);
            if (psm == PSM_HID_INTERRUPT)
            {
               ios_add_log_message("BTstack PS3: HID INTERRUPT service registered");
               ios_add_log_message("BTstack PS3: Registering HID CONTROL service");
               bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_CONTROL, 672);
            }
            else if(psm == PSM_HID_CONTROL)
            {
               ios_add_log_message("BTstack PS3: HID CONTROL service registered");
               ios_add_log_message("BTstack PS3: Waiting for connection");
               device->state = BTPAD_PS3_PENDING;
            }
         }
      }
      else if(device->state == BTPAD_PS3_PENDING)
      {
         if (packet[0] == L2CAP_EVENT_INCOMING_CONNECTION)
         {
            const uint32_t psm = READ_BT_16(packet, 10);
            const unsigned interrupt = (psm == PSM_HID_INTERRUPT) ? 1 : 0;
      
            ios_add_log_message("BTstack PS3: Incoming L2CAP connection for PSM: %02X", psm);

            bd_addr_t address;
            bt_flip_addr_ptr(address, &packet[2]);
         
            if (!device->have_address || BD_ADDR_CMP(device->address, address) == 0)
            {
               device->have_address = true;
         
               bt_flip_addr_ptr(device->address, &packet[2]);
               device->handle = READ_BT_16(packet, 8);
               device->channels[interrupt] = READ_BT_16(packet, 12);
            
               bt_send_cmd_ptr(l2cap_accept_connection_ptr, device->channels[interrupt]);
               ios_add_log_message("BTstack PS3: Connection accepted");
            
               if (device->channels[0] && device->channels[1])
               {
                  ios_add_log_message("BTstack PS3: Got both channels, requesting name");
                  bt_send_cmd_ptr(hci_remote_name_request_ptr, device->address, 0, 0, 0);
                  device->state = BTPAD_PS3_WANT_NAME;
               }
            }
            else
               ios_add_log_message("BTstack PS3: Connection unexpected; ignoring");
         }
      }
      else if(device->state == BTPAD_PS3_WANT_NAME)
      {
         if (packet[0] == HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE)
         {
            bd_addr_t event_addr;
            bt_flip_addr_ptr(event_addr, &packet[3]);
               
            if (BD_ADDR_CMP(event_addr, device->address) == 0)
            {
               if (strncmp((char*)&packet[9], "PLAYSTATION(R)3 Controller", 26) == 0)
               {
                  ios_add_log_message("BTstack PS3: Got 'PLAYSTATION(R)3 Controller'; Sending startup packets");
                  
                  // Special packet to tell PS3 controller to send reports
                  uint8_t data[] = {0x53, 0xF4, 0x42, 0x03, 0x00, 0x00};
                  bt_send_l2cap_ptr(device->channels[0], data, 6);

                  btpad_ps3_setleds(device, 1);
               
                  device->state = BTPAD_PS3_CONNECTED;
               }
               else
               {
                  ios_add_log_message("BTstack PS3: Got %200s; Will keep looking", (char*)&packet[9]);
                  device->have_address = 0;
                  device->handle = 0;
                  device->channels[0] = 0;
                  device->channels[1] = 0;
                  device->state = BTPAD_PS3_PENDING;
               }
            }
         }
      }
   }
   else if (packet_type == L2CAP_DATA_PACKET && packet[0] == 0xA1)
   {
      if (!device->have_led)
      {
         btpad_ps3_setleds(device, 1);
         device->have_led = true;
      }
   
      memcpy(device->data, packet, size);
   }
}

struct btpad_interface btpad_ps3 =
{
   (void*)&btpad_ps3_connect,
   (void*)&btpad_ps3_disconnect,
   (void*)&btpad_ps3_setleds,
   (void*)&btpad_ps3_get_buttons,
   (void*)&btpad_ps3_get_axis,
   (void*)&btpad_ps3_packet_handler
};
