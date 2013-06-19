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

#include "btdynamic.h"
#include "btpad.h"
#include "btpad_queue.h"

// NOTE: It seems that it is not needed to wait for the l2cap commands; TODO: Confirm
// #define WAIT_FOR_L2CAP

struct btpad_queue_command
{
   const hci_cmd_t* command;

   union
   {
      struct
      {
         uint8_t on;
      }  btstack_set_power_mode;
   
      struct
      {
         uint16_t handle;
         uint8_t reason;
      }  hci_disconnect;

      struct
      {
         uint32_t lap;
         uint8_t length;
         uint8_t num_responses;
      }  hci_inquiry;

      struct
      {
         bd_addr_t bd_addr;
         uint8_t page_scan_repetition_mode;
         uint8_t reserved;
         uint16_t clock_offset;
      }  hci_remote_name_request;

      struct // For wiimote only
      {
         bd_addr_t bd_addr;
         bd_addr_t pin;
      }  hci_pin_code_request_reply;

#ifdef WAIT_FOR_L2CAP
      struct
      {
         uint16_t psm;
         uint16_t mtu;
      }  l2cap_register_service;

      struct
      {
         bd_addr_t bd_addr;
         uint16_t psm;
      }  l2cap_create_channel;

      struct
      {
         uint16_t cid;
      }  l2cap_accept_connection;

      struct
      {
         uint16_t cid;
         uint8_t reason;
      }  l2cap_decline_connection;
#endif
   };
};

struct btpad_queue_command commands[64];
static uint32_t insert_position;
static uint32_t read_position;
static uint32_t can_run;

#define INCPOS(POS) { POS##_position = (POS##_position + 1) % 64; }

void btpad_queue_reset()
{
   insert_position = 0;
   read_position = 0;
   can_run = 1;
}

void btpad_queue_run(uint32_t count)
{
   can_run = count;

   btpad_queue_process();
}

void btpad_queue_process()
{
   for (; can_run && (insert_position != read_position); can_run --)
   {
      struct btpad_queue_command* cmd = &commands[read_position];

           if (cmd->command == btstack_set_power_mode_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->btstack_set_power_mode.on);
      else if (cmd->command == hci_read_bd_addr_ptr)
         bt_send_cmd_ptr(cmd->command);
      else if (cmd->command == hci_disconnect_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->hci_disconnect.handle, cmd->hci_disconnect.reason);
      else if (cmd->command == hci_inquiry_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->hci_inquiry.lap, cmd->hci_inquiry.length, cmd->hci_inquiry.num_responses);
      else if (cmd->command == hci_remote_name_request_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->hci_remote_name_request.bd_addr, cmd->hci_remote_name_request.page_scan_repetition_mode,
                         cmd->hci_remote_name_request.reserved, cmd->hci_remote_name_request.clock_offset);
      else if (cmd->command == hci_pin_code_request_reply_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->hci_pin_code_request_reply.bd_addr, 6, cmd->hci_pin_code_request_reply.pin);
#ifdef WAIT_FOR_L2CAP
      else if (cmd->command == l2cap_register_service_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->l2cap_register_service.psm, cmd->l2cap_register_service.mtu);
      else if (cmd->command == l2cap_create_channel_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->l2cap_create_channel.bd_addr, cmd->l2cap_create_channel.psm);
      else if (cmd->command == l2cap_accept_connection_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->l2cap_accept_connection.cid);
      else if (cmd->command == l2cap_decline_connection_ptr)
         bt_send_cmd_ptr(cmd->command, cmd->l2cap_decline_connection.cid, cmd->l2cap_decline_connection.reason);
#endif

      INCPOS(read);
   }
}

void btpad_queue_btstack_set_power_mode(uint8_t on)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = btstack_set_power_mode_ptr;
   cmd->btstack_set_power_mode.on = on;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_hci_read_bd_addr()
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = hci_read_bd_addr_ptr;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_hci_disconnect(uint16_t handle, uint8_t reason)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = hci_disconnect_ptr;
   cmd->hci_disconnect.handle = handle;
   cmd->hci_disconnect.reason = reason;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_hci_inquiry(uint32_t lap, uint8_t length, uint8_t num_responses)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = hci_inquiry_ptr;
   cmd->hci_inquiry.lap = lap;
   cmd->hci_inquiry.length = length;
   cmd->hci_inquiry.num_responses = num_responses;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_hci_remote_name_request(bd_addr_t bd_addr, uint8_t page_scan_repetition_mode, uint8_t reserved, uint16_t clock_offset)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = hci_remote_name_request_ptr;
   memcpy(cmd->hci_remote_name_request.bd_addr, bd_addr, sizeof(bd_addr_t));
   cmd->hci_remote_name_request.page_scan_repetition_mode = page_scan_repetition_mode;
   cmd->hci_remote_name_request.reserved = reserved;
   cmd->hci_remote_name_request.clock_offset = clock_offset;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_hci_pin_code_request_reply(bd_addr_t bd_addr, bd_addr_t pin)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = hci_pin_code_request_reply_ptr;
   memcpy(cmd->hci_pin_code_request_reply.bd_addr, bd_addr, sizeof(bd_addr_t));
   memcpy(cmd->hci_pin_code_request_reply.pin, pin, sizeof(bd_addr_t));

   INCPOS(insert);
   btpad_queue_process();
}

// NOTE: It seems the l2cap commands don't need to wait
#ifdef WAIT_FOR_L2CAP
void btpad_queue_l2cap_register_service(uint16_t psm, uint16_t mtu)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = l2cap_register_service_ptr;
   cmd->l2cap_register_service.psm = psm;
   cmd->l2cap_register_service.mtu = mtu;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_l2cap_create_channel(bd_addr_t bd_addr, uint16_t psm)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = l2cap_create_channel_ptr;
   memcpy(cmd->l2cap_create_channel.bd_addr, bd_addr, sizeof(bd_addr_t));
   cmd->l2cap_create_channel.psm = psm;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_l2cap_accept_connection(uint16_t cid)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = l2cap_accept_connection_ptr;
   cmd->l2cap_accept_connection.cid = cid;

   INCPOS(insert);
   btpad_queue_process();
}

void btpad_queue_l2cap_decline_connection(uint16_t cid, uint8_t reason)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   cmd->command = l2cap_decline_connection_ptr;
   cmd->l2cap_decline_connection.cid = cid;
   cmd->l2cap_decline_connection.reason = reason;

   INCPOS(insert);
   btpad_queue_process();
}
#else
void btpad_queue_l2cap_register_service(uint16_t psm, uint16_t mtu)
{
   bt_send_cmd_ptr(l2cap_register_service_ptr, psm, mtu);
}

void btpad_queue_l2cap_create_channel(bd_addr_t bd_addr, uint16_t psm)
{
   bt_send_cmd_ptr(l2cap_create_channel_ptr, bd_addr, psm);
}

void btpad_queue_l2cap_accept_connection(uint16_t cid)
{
   bt_send_cmd_ptr(l2cap_accept_connection_ptr, cid);
}

void btpad_queue_l2cap_decline_connection(uint16_t cid, uint8_t reason)
{
   bt_send_cmd_ptr(l2cap_decline_connection_ptr, cid, reason);
}
#endif