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

// I take back everything I ever said about bad bluetooth stacks, this shit is hard.

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>

#include "apple/common/rarch_wrapper.h"
#include "btdynamic.h"
#include "btpad.h"
#include "btpad_queue.h"

// Private interface
enum btpad_state { BTPAD_EMPTY, BTPAD_CONNECTING, BTPAD_CONNECTED };

struct apple_pad_connection
{
   uint32_t slot;

   enum btpad_state state;

   bool has_address;
   bd_addr_t address;

   uint16_t handle;
   uint16_t channels[2]; //0: Control, 1: Interrupt
};

static struct apple_pad_connection g_connections[MAX_PLAYERS];

void apple_pad_send_control(struct apple_pad_connection* connection, uint8_t* data, size_t size)
{
   bt_send_l2cap_ptr(connection->channels[0], data, size);
}


static bool inquiry_off;
static bool inquiry_running;

// External interface (MAIN THREAD ONLY)
void btpad_set_inquiry_state(bool on)
{
   inquiry_off = !on;

   if (!inquiry_off && !inquiry_running)
      btpad_queue_hci_inquiry(HCI_INQUIRY_LAP, 3, 1);      
}

// Internal interface:
static struct apple_pad_connection* btpad_find_empty_connection()
{
   for (int i = 0; i != MAX_PLAYERS; i ++)
      if (g_connections[i].state == BTPAD_EMPTY)
         return &g_connections[i];
   
   return 0;
}

static struct apple_pad_connection* btpad_find_connection_for(uint16_t handle, bd_addr_t address)
{
   for (int i = 0; i < MAX_PLAYERS; i ++)
   {
      if (!g_connections[i].handle && !g_connections[i].has_address)
         continue;

      if (handle && g_connections[i].handle && handle != g_connections[i].handle)
         continue;

      if (address && g_connections[i].has_address && (BD_ADDR_CMP(address, g_connections[i].address)))
         continue;

      return &g_connections[i];
   }

   return 0;
}

static void btpad_close_connection(struct apple_pad_connection* connection)
{
   if (connection->handle)
      btpad_queue_hci_disconnect(connection->handle, 0x15);

   memset(connection, 0, sizeof(struct apple_pad_connection));
}

static void btpad_close_all_connections()
{
   for (int i = 0; i < MAX_PLAYERS; i ++)
      btpad_close_connection(&g_connections[i]);
}

void btpad_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   bd_addr_t event_addr;

   if (packet_type == HCI_EVENT_PACKET)
   {
      switch (packet[0])
      {
         case BTSTACK_EVENT_STATE:
         {
            RARCH_LOG("BTstack: HCI State %d\n", packet[2]);
         
            switch (packet[2])
            {                  
               case HCI_STATE_WORKING:
                  btpad_queue_reset();

                  btpad_queue_hci_read_bd_addr();
                  bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_CONTROL, 672);  // TODO: Where did I get 672 for mtu?
                  bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_INTERRUPT, 672);
                  btpad_queue_hci_inquiry(HCI_INQUIRY_LAP, 3, 1);
               
                  btpad_queue_run(1);
                  break;
                  
               case HCI_STATE_HALTING:
                  btpad_close_all_connections();
                  CFRunLoopStop(CFRunLoopGetCurrent());
                  break;                  
            }
         }
         break;

         case HCI_EVENT_COMMAND_STATUS:
         {
            btpad_queue_run(packet[3]);
         }
         break;

         case HCI_EVENT_COMMAND_COMPLETE:
         {
            btpad_queue_run(packet[2]);

            if (COMMAND_COMPLETE_EVENT(packet, (*hci_read_bd_addr_ptr)))
            {
               bt_flip_addr_ptr(event_addr, &packet[6]);
               if (!packet[5]) RARCH_LOG("BTpad: Local address is %s\n", bd_addr_to_str_ptr(event_addr));
               else            RARCH_LOG("BTpad: Failed to get local address (Status: %02X)\n", packet[5]);               
            }
         }
         break;

         case HCI_EVENT_INQUIRY_RESULT:
         {
            if (packet[2])
            {
               bt_flip_addr_ptr(event_addr, &packet[3]);

               struct apple_pad_connection* connection = btpad_find_empty_connection();
               if (connection)
               {
                  RARCH_LOG("BTpad: Inquiry found device\n");
                  memset(connection, 0, sizeof(struct apple_pad_connection));

                  memcpy(connection->address, event_addr, sizeof(bd_addr_t));
                  connection->has_address = true;
                  connection->state = BTPAD_CONNECTING;

                  bt_send_cmd_ptr(l2cap_create_channel_ptr, connection->address, PSM_HID_CONTROL);
                  bt_send_cmd_ptr(l2cap_create_channel_ptr, connection->address, PSM_HID_INTERRUPT);
               }
            }
         }
         break;

         case HCI_EVENT_INQUIRY_COMPLETE:
         {
            // This must be turned off during gameplay as it causes a ton of lag
            inquiry_running = !inquiry_off;

            if (inquiry_running)
               btpad_queue_hci_inquiry(HCI_INQUIRY_LAP, 3, 1);
         }
         break;

         case L2CAP_EVENT_CHANNEL_OPENED:
         {
            bt_flip_addr_ptr(event_addr, &packet[3]);
            const uint16_t handle = READ_BT_16(packet, 9);
            const uint16_t psm = READ_BT_16(packet, 11);
            const uint16_t channel_id = READ_BT_16(packet, 13);

            struct apple_pad_connection* connection = btpad_find_connection_for(handle, event_addr);

            if (!packet[2])
            {
               if (!connection)
               {
                  RARCH_LOG("BTpad: Got L2CAP 'Channel Opened' event for unrecognized device\n");
                  break;
               }

               RARCH_LOG("BTpad: L2CAP channel opened: (PSM: %02X)\n", psm);
               connection->handle = handle;
            
               if (psm == PSM_HID_CONTROL)
                  connection->channels[0] = channel_id;
               else if (psm == PSM_HID_INTERRUPT)
                  connection->channels[1] = channel_id;
               else
                  RARCH_LOG("BTpad: Got unknown L2CAP PSM, ignoring (PSM: %02X)\n", psm);

               if (connection->channels[0] && connection->channels[1])
               {
                  RARCH_LOG("BTpad: Got both L2CAP channels, requesting name\n");
                  btpad_queue_hci_remote_name_request(connection->address, 0, 0, 0);
               }
            }
            else
               RARCH_LOG("BTpad: Got failed L2CAP 'Channel Opened' event (PSM: %02X, Status: %02X)\n", psm, packet[2]);
         }
         break;

         case L2CAP_EVENT_INCOMING_CONNECTION:
         {
            bt_flip_addr_ptr(event_addr, &packet[2]);
            const uint16_t handle = READ_BT_16(packet, 8);
            const uint32_t psm = READ_BT_16(packet, 10);
            const uint32_t channel_id = READ_BT_16(packet, 12);
      
            struct apple_pad_connection* connection = btpad_find_connection_for(handle, event_addr);

            if (!connection)
            {
               connection = btpad_find_empty_connection();
               if (connection)
               {
                  RARCH_LOG("BTpad: Got new incoming connection\n");

                  memset(connection, 0, sizeof(struct apple_pad_connection));

                  memcpy(connection->address, event_addr, sizeof(bd_addr_t));
                  connection->has_address = true;
                  connection->handle = handle;
                  connection->state = BTPAD_CONNECTING;
               }
               else break;
            }

            RARCH_LOG("BTpad: Incoming L2CAP connection (PSM: %02X)\n", psm);
            bt_send_cmd_ptr(l2cap_accept_connection_ptr, channel_id);
         }
         break;

         case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
         {
            bt_flip_addr_ptr(event_addr, &packet[3]);
            
            struct apple_pad_connection* connection = btpad_find_connection_for(0, event_addr);

            if (!connection)
            {
               RARCH_LOG("BTpad: Got unexpected remote name, ignoring\n");
               break;
            }

            RARCH_LOG("BTpad: Got %.200s\n", (char*)&packet[9]);
            
            connection->slot = apple_joypad_connect((char*)packet + 9, connection);
            connection->state = BTPAD_CONNECTED;
         }
         break;

         case HCI_EVENT_PIN_CODE_REQUEST:
         {
            RARCH_LOG("BTpad: Sending WiiMote PIN\n");

            bt_flip_addr_ptr(event_addr, &packet[2]);
            btpad_queue_hci_pin_code_request_reply(event_addr, &packet[2]);
         }
         break;

         case HCI_EVENT_DISCONNECTION_COMPLETE:
         {
            const uint32_t handle = READ_BT_16(packet, 3);

            if (!packet[2])
            {
               struct apple_pad_connection* connection = btpad_find_connection_for(handle, 0);
               if (connection)
               {
                  connection->handle = 0;
               
                  apple_joypad_disconnect(connection->slot);
                  btpad_close_connection(connection);
               }
            }
            else
               RARCH_LOG("BTpad: Got failed 'Disconnection Complete' event (Status: %02X)\n", packet[2]);
         }
         break;

         case L2CAP_EVENT_SERVICE_REGISTERED:
         {
            if (packet[2])
               RARCH_LOG("BTpad: Got failed 'Service Registered' event (PSM: %02X, Status: %02X)\n", READ_BT_16(packet, 3), packet[2]);
         }
         break;
      }
   }
   else if (packet_type == L2CAP_DATA_PACKET)
   {
      for (int i = 0; i < MAX_PLAYERS; i ++)
      {
         struct apple_pad_connection* connection = &g_connections[i];
   
         if (connection->state == BTPAD_CONNECTED && (connection->channels[0] == channel || connection->channels[1] == channel))
            apple_joypad_packet(connection->slot, packet, size);
      }
   }
}
