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

#include "apple/common/rarch_wrapper.h"
#include "apple/common/hidpad/hidpad.h"
#include "btdynamic.h"
#include "btpad.h"
#include "btpad_queue.h"

// Private interface
enum btpad_state { BTPAD_EMPTY, BTPAD_CONNECTING, BTPAD_CONNECTED };

struct hidpad_connection
{
   uint32_t slot;

   struct hidpad_interface* interface;
   void* hidpad;

   enum btpad_state state;

   bool has_address;
   bd_addr_t address;

   uint16_t handle;
   uint16_t channels[2]; //0: Control, 1: Interrupt
};

static struct hidpad_connection g_connected_pads[MAX_PADS];

void hidpad_send_control(struct hidpad_connection* connection, uint8_t* data, size_t size)
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
static int32_t btpad_find_slot_for(uint16_t handle, bd_addr_t address)
{
   for (int i = 0; i < MAX_PADS; i ++)
   {
      if (!g_connected_pads[i].handle && !g_connected_pads[i].has_address)
         continue;

      if (handle && g_connected_pads[i].handle && handle != g_connected_pads[i].handle)
         continue;

      if (address && g_connected_pads[i].has_address && (BD_ADDR_CMP(address, g_connected_pads[i].address)))
         continue;

      return i;
   }

   return -1;
}

static int32_t btpad_find_slot_with_state(enum btpad_state state)
{
   for (int i = 0; i < MAX_PADS; i ++)
      if (g_connected_pads[i].state == state)
         return i;

   return -1;
}

static void btpad_disconnect_pad(uint32_t slot)
{
   if (slot > MAX_PADS)
      return;

   if (g_connected_pads[slot].interface && g_connected_pads[slot].hidpad)
   {
      ios_add_log_message("BTpad: Disconnecting slot %d", slot);
      g_connected_pads[slot].interface->disconnect(g_connected_pads[slot].hidpad);
   }

   if (g_connected_pads[slot].handle)
      btpad_queue_hci_disconnect(g_connected_pads[slot].handle, 0x15);

   memset(&g_connected_pads[slot], 0, sizeof(struct hidpad_connection));
}

static void btpad_disconnect_all_pads()
{
   for (int i = 0; i < MAX_PADS; i ++)
      btpad_disconnect_pad(i);
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
            if (packet[2] == HCI_STATE_WORKING)
            {
               btpad_queue_reset();

               btpad_queue_hci_read_bd_addr();
               bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_CONTROL, 672);  // TODO: Where did I get 672 for mtu?
               bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_INTERRUPT, 672);
               btpad_queue_hci_inquiry(HCI_INQUIRY_LAP, 3, 1);
               
               btpad_queue_run(1);
            }
            else if(packet[2] > HCI_STATE_WORKING)
            {
               btpad_disconnect_all_pads();
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
               if (!packet[5]) ios_add_log_message("BTpad: Local address is %s", bd_addr_to_str_ptr(event_addr));
               else            ios_add_log_message("BTpad: Failed to get local address (Status: %02X)", packet[5]);               
            }
         }
         break;

         case HCI_EVENT_INQUIRY_RESULT:
         {
            if (packet[2])
            {
               bt_flip_addr_ptr(event_addr, &packet[3]);

               const int32_t slot = btpad_find_slot_with_state(BTPAD_EMPTY);
               if (slot >= 0)
               {
                  ios_add_log_message("BTpad: Inquiry found device (Slot %d)", slot);

                  memcpy(g_connected_pads[slot].address, event_addr, sizeof(bd_addr_t));

                  g_connected_pads[slot].has_address = true;
                  g_connected_pads[slot].state = BTPAD_CONNECTING;
                  g_connected_pads[slot].slot = slot;

                  bt_send_cmd_ptr(l2cap_create_channel_ptr, g_connected_pads[slot].address, PSM_HID_CONTROL);
                  bt_send_cmd_ptr(l2cap_create_channel_ptr, g_connected_pads[slot].address, PSM_HID_INTERRUPT);
               }
            }
         }
         break;

         case HCI_EVENT_INQUIRY_COMPLETE:
         {
            // TODO: Check performance and battery effect of this

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

            const int32_t slot = btpad_find_slot_for(handle, event_addr);

            if (!packet[2])
            {
               if (slot < 0)
               {
                  ios_add_log_message("BTpad: Got L2CAP 'Channel Opened' event for unrecognized device");
                  break;
               }

               ios_add_log_message("BTpad: L2CAP channel opened: (Slot: %d, PSM: %02X)", slot, psm);
               g_connected_pads[slot].handle = handle;
            
               if (psm == PSM_HID_CONTROL)
                  g_connected_pads[slot].channels[0] = channel_id;
               else if (psm == PSM_HID_INTERRUPT)
                  g_connected_pads[slot].channels[1] = channel_id;
               else
                  ios_add_log_message("BTpad: Got unknown L2CAP PSM, ignoring (Slot: %d, PSM: %02X)", slot, psm);

               if (g_connected_pads[slot].channels[0] && g_connected_pads[slot].channels[1])
               {
                  ios_add_log_message("BTpad: Got both L2CAP channels, requesting name (Slot: %d)", slot);
                  btpad_queue_hci_remote_name_request(g_connected_pads[slot].address, 0, 0, 0);
               }
            }
            else
               ios_add_log_message("BTpad: Got failed L2CAP 'Channel Opened' event (Slot %d, PSM: %02X, Status: %02X)", -1, psm, packet[2]);
         }
         break;

         case L2CAP_EVENT_INCOMING_CONNECTION:
         {
            bt_flip_addr_ptr(event_addr, &packet[2]);
            const uint16_t handle = READ_BT_16(packet, 8);
            const uint32_t psm = READ_BT_16(packet, 10);
            const uint32_t channel_id = READ_BT_16(packet, 12);
      
            int32_t slot = btpad_find_slot_for(handle, event_addr);
            if (slot < 0)
            {
               slot = btpad_find_slot_with_state(BTPAD_EMPTY);

               if (slot >= 0)
               {
                  ios_add_log_message("BTpad: Got new incoming connection (Slot: %d)", slot);

                  memcpy(g_connected_pads[slot].address, event_addr, sizeof(bd_addr_t));

                  g_connected_pads[slot].has_address = true;
                  g_connected_pads[slot].handle = handle;
                  g_connected_pads[slot].state = BTPAD_CONNECTING;
                  g_connected_pads[slot].slot = slot;
               }
               else break;
            }

            ios_add_log_message("BTpad: Incoming L2CAP connection (Slot: %d, PSM: %02X)", slot, psm);
            bt_send_cmd_ptr(l2cap_accept_connection_ptr, channel_id);
         }
         break;

         case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
         {
            bt_flip_addr_ptr(event_addr, &packet[3]);

            const int32_t slot = btpad_find_slot_for(0, event_addr);
            if (slot < 0)
            {
               ios_add_log_message("BTpad: Got unexpected remote name, ignoring");
               break;
            }

            ios_add_log_message("BTpad: Got %.200s (Slot: %d)", (char*)&packet[9], slot);
            if (strncmp((char*)&packet[9], "PLAYSTATION(R)3 Controller", 26) == 0)
               g_connected_pads[slot].interface = &hidpad_ps3;
            else if (strncmp((char*)&packet[9], "Nintendo RVL-CNT-01", 19) == 0)
               g_connected_pads[slot].interface = &hidpad_wii;

            if (g_connected_pads[slot].interface)
            {
               g_connected_pads[slot].hidpad = g_connected_pads[slot].interface->connect(&g_connected_pads[slot], slot);
               g_connected_pads[slot].state = BTPAD_CONNECTED;
            }
         }
         break;

         case HCI_EVENT_PIN_CODE_REQUEST:
         {
            ios_add_log_message("BTpad: Sending WiiMote PIN");

            bt_flip_addr_ptr(event_addr, &packet[2]);
            btpad_queue_hci_pin_code_request_reply(event_addr, &packet[2]);
         }
         break;

         case HCI_EVENT_DISCONNECTION_COMPLETE:
         {
            const uint32_t handle = READ_BT_16(packet, 3);

            if (!packet[2])
            {
               const int32_t slot = btpad_find_slot_for(handle, 0);
               if (slot >= 0)
               {
                  g_connected_pads[slot].handle = 0;
                  btpad_disconnect_pad(slot);

                  ios_add_log_message("BTpad: Device disconnected (Slot: %d)", slot);
               }
            }
            else
               ios_add_log_message("BTpad: Got failed 'Disconnection Complete' event (Status: %02X)", packet[2]);
         }
         break;

         case L2CAP_EVENT_SERVICE_REGISTERED:
         {
            if (!packet[2])
               ios_add_log_message("BTpad: Got failed 'Service Registered' event (PSM: %02X, Status: %02X)", READ_BT_16(packet, 3), packet[2]);
         }
         break;
      }
   }
   else if (packet_type == L2CAP_DATA_PACKET)
   {
      for (int i = 0; i < MAX_PADS; i ++)
      {
         struct hidpad_connection* connection = &g_connected_pads[i];
   
         if (connection->hidpad && connection->interface && (connection->channels[0] == channel || connection->channels[1] == channel))
            connection->interface->packet_handler(connection->hidpad, packet, size);
      }
   }
}
