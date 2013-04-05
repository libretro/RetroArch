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

// I take back everything I ever said about bad bluetooth stacks, this shit is hard.

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>

#include "../../rarch_wrapper.h"
#include "btdynamic.h"
#include "btpad.h"
#include "wiimote.h"

static btpad_connection_t btpad_connection;

static bool btpad_connection_test(uint16_t handle, bd_addr_t address)
{
   if (handle && btpad_connection.handle && handle != btpad_connection.handle)
      return false;
   btpad_connection.handle = handle ? handle : btpad_connection.handle;

   if (address && btpad_connection.has_address && (BD_ADDR_CMP(address, btpad_connection.address)))
      return false;

   if (address)
   {
      btpad_connection.has_address = true;
      memcpy(btpad_connection.address, address, sizeof(bd_addr_t));
   }

   return true;
}

static struct btpad_interface* btpad_iface;
static void* btpad_device;

// MAIN THREAD ONLY
uint32_t btpad_get_buttons()
{
   return (btpad_device && btpad_iface) ? btpad_iface->get_buttons(btpad_device) : 0;
}

int16_t btpad_get_axis(unsigned axis)
{
   return (btpad_device && btpad_iface) ? btpad_iface->get_axis(btpad_device, axis) : 0;
}

static void btpad_disconnect_pad()
{
   if (btpad_iface && btpad_device)
   {
      ios_add_log_message("BTpad: Disconnecting");
   
      btpad_iface->disconnect(btpad_device);
      btpad_device = 0;
      btpad_iface = 0;
   }

   if (btpad_connection.handle)
      bt_send_cmd_ptr(hci_disconnect_ptr, btpad_connection.handle, 0x15);

   memset(&btpad_connection, 0, sizeof(btpad_connection_t));
}

static void btpad_connect_pad()
{
   if (btpad_connection.state == BTPAD_CONNECTED)
      btpad_disconnect_pad();
   memset(&btpad_connection, 0, sizeof(btpad_connection_t));

   ios_add_log_message("BTpad: Registering HID INTERRUPT service");
   bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_INTERRUPT, 672);
}

void btpad_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   bd_addr_t event_addr;

   if (packet_type == HCI_EVENT_PACKET)
   {
      switch (packet[0])
      {
        case BTSTACK_EVENT_STATE:
            if (packet[2] == HCI_STATE_WORKING)
               btpad_connect_pad();
            else if(packet[2] > HCI_STATE_WORKING && btpad_iface && btpad_device)
               btpad_disconnect_pad();
            break;

         case L2CAP_EVENT_SERVICE_REGISTERED:
         {
            uint32_t psm = READ_BT_16(packet, 3);
            if (psm == PSM_HID_INTERRUPT)
            {
               ios_add_log_message("BTpad: HID INTERRUPT service registered");
               ios_add_log_message("BTpad: Registering HID CONTROL service");
               bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_CONTROL, 672);
            }
            else if(psm == PSM_HID_CONTROL)
            {
               ios_add_log_message("BTpad: HID CONTROL service registered");
               ios_add_log_message("BTpad: Starting inquiry");
               bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 1);
            }
         }
         break;

         case HCI_EVENT_INQUIRY_RESULT:
         {
            if (packet[2])
            {
               bt_flip_addr_ptr(event_addr, &packet[3]);
               if (btpad_connection_test(0, event_addr))
               {
                  btpad_connection.state = BTPAD_WANT_INQ_COMPLETE;

                  btpad_connection.page_scan_repetition_mode = packet [3 + packet[2] * (6)];
                  btpad_connection.class = READ_BT_24(packet, 3 + packet[2] * (6+1+1+1));
                  btpad_connection.clock_offset = READ_BT_16(packet, 3 + packet[2] * (6+1+1+1+3)) & 0x7fff;

                  ios_add_log_message("BTpad: Inquiry found device");
               }
            }
         }
         break;

         case HCI_EVENT_INQUIRY_COMPLETE:
         {
            if (btpad_connection.state == BTPAD_WANT_INQ_COMPLETE)
            {
               ios_add_log_message("BTpad: Got inquiry complete; connecting\n");
               bt_send_cmd_ptr(l2cap_create_channel_ptr, btpad_connection.address, PSM_HID_CONTROL);
            }
            else if(btpad_connection.state == BTPAD_EMPTY)
            {
               if (btpad_connection.laps < 40)
               {
                  btpad_connection.laps ++;
                  bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 1);
               }
               else
                  ios_add_log_message("BTpad: Did not find wiimote, will stop searching");
            }
         }
         break;

         case L2CAP_EVENT_CHANNEL_OPENED:
         {
            const uint8_t status = packet[2];
            bt_flip_addr_ptr(event_addr, &packet[3]);
            const uint16_t handle = READ_BT_16(packet, 9);
            const uint16_t psm = READ_BT_16(packet, 11);
            const uint16_t channel_id = READ_BT_16(packet, 13);

            if (!btpad_connection_test(handle, event_addr))
            {
               ios_add_log_message("BTpad: Incoming L2CAP connection not recognized; ignoring");
               break;
            }

            if (status == 0)
            {
               ios_add_log_message("BTpad: L2CAP channel opened for psm: %02X", psm);
            
               if (psm == PSM_HID_CONTROL)
               {
                  btpad_connection.channels[0] = channel_id;

                  ios_add_log_message("BTpad: Got HID CONTROL channel; Opening INTERRUPT");
                  bt_send_cmd_ptr(l2cap_create_channel_ptr, btpad_connection.address, PSM_HID_INTERRUPT);
               }
               else if (psm == PSM_HID_INTERRUPT)
               {
                  btpad_connection.channels[1] = channel_id;

                  ios_add_log_message("BTpad: Got HID INTERRUPT channel; Requesting name");
                  bt_send_cmd_ptr(hci_remote_name_request_ptr, btpad_connection.address, btpad_connection.page_scan_repetition_mode,
                                  0, btpad_connection.clock_offset | 0x8000);
               }
               else
                  ios_add_log_message("BTpad: Got unknown L2CAP channel, ignoring");
            }
            else
               ios_add_log_message("BTpad: Failed to open WiiMote L2CAP channel for PSM: %02X", psm);
         }
         break;

         case L2CAP_EVENT_INCOMING_CONNECTION:
         {
            bt_flip_addr_ptr(event_addr, &packet[2]);
            const uint16_t handle = READ_BT_16(packet, 8);
            const uint32_t psm = READ_BT_16(packet, 10);
            const uint32_t channel_id = READ_BT_16(packet, 12);

            const unsigned interrupt = (psm == PSM_HID_INTERRUPT) ? 1 : 0;
      
            ios_add_log_message("BTpad: Incoming L2CAP connection for PSM: %02X", psm);

            if (!btpad_connection_test(handle, event_addr))
            {
               ios_add_log_message("BTpad: Connection is for unregnized handle or address, denying");

               // TODO: Check error code
               bt_send_cmd_ptr(l2cap_decline_connection_ptr, 0x15);

               break;
            }

            btpad_connection.channels[interrupt] = channel_id;
            
            bt_send_cmd_ptr(l2cap_accept_connection_ptr, btpad_connection.channels[interrupt]);
            ios_add_log_message("BTpad: L2CAP Connection accepted");
            
            if (btpad_connection.channels[0] && btpad_connection.channels[1])
            {
               ios_add_log_message("BTpad: Got both L2CAP channels, requesting name");
               bt_send_cmd_ptr(hci_remote_name_request_ptr, btpad_connection.address, 0, 0, 0);
            }
         }
         break;

         case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
         {
            bt_flip_addr_ptr(event_addr, &packet[3]);

            if (!btpad_connection_test(0, event_addr))
            {
               ios_add_log_message("BTpad: Got unexpected remote name, ignoring");
               break;
            }

            ios_add_log_message("BTpad: Got %200s", (char*)&packet[9]);
            if (strncmp((char*)&packet[9], "PLAYSTATION(R)3 Controller", 26) == 0)
               btpad_iface = &btpad_ps3;
            else if (strncmp((char*)&packet[9], "Nintendo RVL-CNT-01", 19) == 0)
               btpad_iface = &btpad_wii;

            if (btpad_iface)
            {
               btpad_device = btpad_iface->connect(&btpad_connection);
               btpad_connection.state = BTPAD_CONNECTED;
            }

         }
         break;

         case HCI_EVENT_PIN_CODE_REQUEST:
         {
            ios_add_log_message("BTpad: Sending PIN");

            bt_flip_addr_ptr(event_addr, &packet[2]);
            bt_send_cmd_ptr(hci_pin_code_request_reply_ptr, event_addr, 6, &packet[2]);
         }
         break;
      }
   }

   if (btpad_device && btpad_iface)
      btpad_iface->packet_handler(btpad_device, packet_type, channel, packet, size);
}
