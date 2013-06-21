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
#include "btpad_queue.h"
#include "wiimote.h"

static btpad_connection_t btpad_connection[MAX_PADS];
static struct btpad_interface* btpad_iface[MAX_PADS];
static void* btpad_device[MAX_PADS];

static int32_t btpad_find_slot_for(uint16_t handle, bd_addr_t address)
{
   for (int i = 0; i < MAX_PADS; i ++)
   {
      if (!btpad_connection[i].handle && !btpad_connection[i].has_address)
         continue;

      if (handle && btpad_connection[i].handle && handle != btpad_connection[i].handle)
         continue;

      if (address && btpad_connection[i].has_address && (BD_ADDR_CMP(address, btpad_connection[i].address)))
         continue;

      return i;
   }

   return -1;
}

static int32_t btpad_find_slot_with_state(enum btpad_state state)
{
   for (int i = 0; i < MAX_PADS; i ++)
      if (btpad_connection[i].state == state)
         return i;

   return -1;
}

// MAIN THREAD ONLY
uint32_t btpad_get_buttons(uint32_t slot)
{
   if (slot < MAX_PADS && btpad_device[slot] && btpad_iface[slot])
      return btpad_iface[slot]->get_buttons(btpad_device[slot]);

   return 0;
}

int16_t btpad_get_axis(uint32_t slot, unsigned axis)
{
   if (slot < MAX_PADS && btpad_device[slot] && btpad_iface[slot])
      return btpad_iface[slot]->get_axis(btpad_device[slot], axis);

   return 0;
}

static void btpad_disconnect_pad(uint32_t slot)
{
   if (slot > MAX_PADS)
      return;

   if (btpad_iface[slot] && btpad_device[slot])
   {
      ios_add_log_message("BTpad: Disconnecting slot %d", slot);
   
      btpad_iface[slot]->disconnect(btpad_device[slot]);
      btpad_device[slot] = 0;
      btpad_iface[slot] = 0;
   }

   if (btpad_connection[slot].handle)
      btpad_queue_hci_disconnect(btpad_connection[slot].handle, 0x15);

   memset(&btpad_connection[slot], 0, sizeof(btpad_connection_t));
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
            else if(packet[2] > HCI_STATE_WORKING && btpad_iface && btpad_device)
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

         case L2CAP_EVENT_SERVICE_REGISTERED:
         {
            uint32_t psm = READ_BT_16(packet, 3);

            if (!packet[2] && psm == PSM_HID_INTERRUPT)
               ios_add_log_message("BTpad: HID INTERRUPT service registered");
            else if (!packet[2] && psm == PSM_HID_CONTROL)
               ios_add_log_message("BTpad: HID CONTROL service registered");
            else if (!packet[2])
               ios_add_log_message("BTpad: Unknown service registered (PSM: %02X)", psm);
            else
               ios_add_log_message("BTpad: Got failed 'Service Registered' event (PSM: %02X, Status: %02X)", psm, packet[2]);
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

                  memcpy(btpad_connection[slot].address, event_addr, sizeof(bd_addr_t));

                  btpad_connection[slot].has_address = true;
                  btpad_connection[slot].state = BTPAD_CONNECTING;
                  btpad_connection[slot].slot = slot;

                  bt_send_cmd_ptr(l2cap_create_channel_ptr, btpad_connection[slot].address, PSM_HID_CONTROL);
                  bt_send_cmd_ptr(l2cap_create_channel_ptr, btpad_connection[slot].address, PSM_HID_INTERRUPT);
               }
            }
         }
         break;

         case HCI_EVENT_INQUIRY_COMPLETE:
         {
            // TODO: Check performance and battery effect of this
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
               btpad_connection[slot].handle = handle;
            
               if (psm == PSM_HID_CONTROL)
                  btpad_connection[slot].channels[0] = channel_id;
               else if (psm == PSM_HID_INTERRUPT)
                  btpad_connection[slot].channels[1] = channel_id;
               else
                  ios_add_log_message("BTpad: Got unknown L2CAP PSM, ignoring (Slot: %d, PSM: %02X)", slot, psm);

               if (btpad_connection[slot].channels[0] && btpad_connection[slot].channels[1])
               {
                  ios_add_log_message("BTpad: Got both L2CAP channels, requesting name (Slot: %d)", slot);
                  btpad_queue_hci_remote_name_request(btpad_connection[slot].address, 0, 0, 0);
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

                  memcpy(btpad_connection[slot].address, event_addr, sizeof(bd_addr_t));

                  btpad_connection[slot].has_address = true;
                  btpad_connection[slot].handle = handle;
                  btpad_connection[slot].state = BTPAD_CONNECTING;
                  btpad_connection[slot].slot = slot;
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
               btpad_iface[slot] = &btpad_ps3;
            else if (strncmp((char*)&packet[9], "Nintendo RVL-CNT-01", 19) == 0)
               btpad_iface[slot] = &btpad_wii;

            if (btpad_iface[slot])
            {
               btpad_device[slot] = btpad_iface[slot]->connect(&btpad_connection[slot]);
               btpad_connection[slot].state = BTPAD_CONNECTED;
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
                  btpad_connection[slot].handle = 0;
                  btpad_disconnect_pad(slot);

                  ios_add_log_message("BTpad: Device disconnected (Slot: %d)", slot);
               }
            }
            else
               ios_add_log_message("BTpad: Got failed 'Disconnection Complete' event (Status: %02X)", packet[2]);
         }
         break;
      }
   }

   for (int i = 0; i < MAX_PADS; i ++)
      if (btpad_device[i] && btpad_iface[i] && (btpad_connection[i].channels[0] == channel || btpad_connection[i].channels[1] == channel))
         btpad_iface[i]->packet_handler(btpad_device[i], packet_type, channel, packet, size);
}
