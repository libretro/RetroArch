/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>

#include "btdynamic.h"
#include "btpad.h"
#include "../input/connect/joypad_connection.h"

extern joypad_connection_t *slots;

/* Private interface. */
enum btpad_state
{
   BTPAD_EMPTY,
   BTPAD_CONNECTING,
   BTPAD_CONNECTED
};

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

      /* For wiimote only.
       * TODO - should we repurpose this so
       * that it's for more than just Wiimote?
       * */
      struct
      {
         bd_addr_t bd_addr;
         bd_addr_t pin;
      }  hci_pin_code_request_reply;
   };
};

struct pad_connection
{
   uint32_t slot;

   enum btpad_state state;

   bool has_address;
   bd_addr_t address;

   uint16_t handle;

   /* 0: Control, 1: Interrupt */
   uint16_t channels[2];
};

static bool inquiry_off;
static bool inquiry_running;
static struct pad_connection g_connections[MAX_USERS];

struct btpad_queue_command commands[64];
static uint32_t insert_position;
static uint32_t read_position;
static uint32_t can_run;

#define INCPOS(POS) { POS##_position = (POS##_position + 1) % 64; }

static void btpad_queue_process_cmd(struct btpad_queue_command *cmd)
{
    if (!cmd)
        return;
    
    if (cmd->command == btstack_set_power_mode_ptr)
        bt_send_cmd_ptr(
                        cmd->command,
                        cmd->btstack_set_power_mode.on);
    else if (cmd->command == hci_read_bd_addr_ptr)
        bt_send_cmd_ptr(cmd->command);
    else if (cmd->command == hci_disconnect_ptr)
        bt_send_cmd_ptr(
                        cmd->command,
                        cmd->hci_disconnect.handle,
                        cmd->hci_disconnect.reason);
    else if (cmd->command == hci_inquiry_ptr)
        bt_send_cmd_ptr(
                        cmd->command,
                        cmd->hci_inquiry.lap,
                        cmd->hci_inquiry.length,
                        cmd->hci_inquiry.num_responses);
    else if (cmd->command == hci_remote_name_request_ptr)
        bt_send_cmd_ptr(
                        cmd->command,
                        cmd->hci_remote_name_request.bd_addr,
                        cmd->hci_remote_name_request.page_scan_repetition_mode,
                        cmd->hci_remote_name_request.reserved,
                        cmd->hci_remote_name_request.clock_offset);
    
    else if (cmd->command == hci_pin_code_request_reply_ptr)
        bt_send_cmd_ptr(
                        cmd->command,
                        cmd->hci_pin_code_request_reply.bd_addr,
                        6,
                        cmd->hci_pin_code_request_reply.pin);
}

static void btpad_queue_process(void)
{
   for (; can_run && (insert_position != read_position); can_run --)
   {
      struct btpad_queue_command* cmd = &commands[read_position];
      btpad_queue_process_cmd(cmd);
      INCPOS(read);
   }
}

static void btpad_queue_reset(void)
{
   insert_position = 0;
   read_position   = 0;
   can_run         = 1;
}

static void btpad_queue_run(uint32_t count)
{
   can_run = count;

   btpad_queue_process();
}

static void btpad_queue_btstack_set_power_mode(uint8_t on)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   if (!cmd)
      return;

   cmd->command                   = btstack_set_power_mode_ptr;
   cmd->btstack_set_power_mode.on = on;

   INCPOS(insert);
   btpad_queue_process();
}

static void btpad_queue_hci_read_bd_addr(void)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   if (!cmd)
      return;

   cmd->command = hci_read_bd_addr_ptr;

   INCPOS(insert);
   btpad_queue_process();
}

static void btpad_queue_hci_disconnect(uint16_t handle, uint8_t reason)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   if (!cmd)
      return;

   cmd->command               = hci_disconnect_ptr;
   cmd->hci_disconnect.handle = handle;
   cmd->hci_disconnect.reason = reason;

   INCPOS(insert);
   btpad_queue_process();
}

static void btpad_queue_hci_inquiry(uint32_t lap,
      uint8_t length, uint8_t num_responses)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   if (!cmd)
      return;

   cmd->command                   = hci_inquiry_ptr;
   cmd->hci_inquiry.lap           = lap;
   cmd->hci_inquiry.length        = length;
   cmd->hci_inquiry.num_responses = num_responses;

   INCPOS(insert);
   btpad_queue_process();
}

static void btpad_queue_hci_remote_name_request(bd_addr_t bd_addr,
      uint8_t page_scan_repetition_mode,
      uint8_t reserved, uint16_t clock_offset)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   if (!cmd)
      return;

   cmd->command = hci_remote_name_request_ptr;
   memcpy(cmd->hci_remote_name_request.bd_addr, bd_addr, sizeof(bd_addr_t));
   cmd->hci_remote_name_request.page_scan_repetition_mode = 
      page_scan_repetition_mode;
   cmd->hci_remote_name_request.reserved = reserved;
   cmd->hci_remote_name_request.clock_offset = clock_offset;

   INCPOS(insert);
   btpad_queue_process();
}

static void btpad_queue_hci_pin_code_request_reply(
      bd_addr_t bd_addr, bd_addr_t pin)
{
   struct btpad_queue_command* cmd = &commands[insert_position];

   if (!cmd)
      return;

   cmd->command = hci_pin_code_request_reply_ptr;
   memcpy(cmd->hci_pin_code_request_reply.bd_addr, bd_addr, sizeof(bd_addr_t));
   memcpy(cmd->hci_pin_code_request_reply.pin, pin, sizeof(bd_addr_t));

   INCPOS(insert);
   btpad_queue_process();
}

static void btpad_connection_send_control(void *data,
      uint8_t* data_buf, size_t size)
{
   struct pad_connection *connection = (struct pad_connection*)data;

   if (connection)
      bt_send_l2cap_ptr(connection->channels[0], data_buf, size);
}

void btpad_set_inquiry_state(bool on)
{
   inquiry_off = !on;

   if (!inquiry_off && !inquiry_running)
      btpad_queue_hci_inquiry(HCI_INQUIRY_LAP, 3, 1);      
}

/* Internal interface. */
static struct pad_connection* btpad_find_empty_connection(void)
{
   unsigned i;
   for (i = 0; i < MAX_USERS; i++)
   {
      if (g_connections[i].state == BTPAD_EMPTY)
         return &g_connections[i];
   }

   return 0;
}

static struct pad_connection* btpad_find_connection_for(
      uint16_t handle, bd_addr_t address)
{
   unsigned i;
   for (i = 0; i < MAX_USERS; i++)
   {
      if (!g_connections[i].handle && !g_connections[i].has_address)
         continue;

      if (handle && g_connections[i].handle
            && handle != g_connections[i].handle)
         continue;

      if (address && g_connections[i].has_address
            && (BD_ADDR_CMP(address, g_connections[i].address)))
         continue;

      return &g_connections[i];
   }

   return 0;
}

static void btpad_close_connection(struct pad_connection* connection)
{
   if (!connection)
      return;

   if (connection->handle)
      btpad_queue_hci_disconnect(connection->handle, 0x15);

   memset(connection, 0, sizeof(struct pad_connection));
}

static void btpad_close_all_connections(void)
{
   int i;
   for (i = 0; i < MAX_USERS; i ++)
      btpad_close_connection(&g_connections[i]);
   /* TODO/FIXME - create platform-agnostic solution for this
    * and figure out why/if this is needed. */
   CFRunLoopStop(CFRunLoopGetCurrent());
}

void btpad_packet_handler(uint8_t packet_type,
      uint16_t channel, uint8_t *packet, uint16_t size)
{
   int i;
   bd_addr_t event_addr;

   switch (packet_type)
   {
      case L2CAP_DATA_PACKET:
         for (i = 0; i < MAX_USERS; i ++)
         {
            struct pad_connection* connection = 
               (struct pad_connection*)&g_connections[i];

            if (connection && connection->state == BTPAD_CONNECTED
                  && (connection->channels[0] == channel || 
                     connection->channels[1] == channel))
               pad_connection_packet(&slots[connection->slot], connection->slot, packet, size);
         }
         break;
      case HCI_EVENT_PACKET:
         switch (packet[0])
         {
            case BTSTACK_EVENT_STATE:
               {
                  RARCH_LOG("[BTstack]: HCI State %d.\n", packet[2]);

                  switch (packet[2])
                  {                  
                     case HCI_STATE_WORKING:
                        btpad_queue_reset();

                        btpad_queue_hci_read_bd_addr();
                        /* TODO: Where did I get 672 for MTU? */
                        bt_send_cmd_ptr(l2cap_register_service_ptr,
                              PSM_HID_CONTROL, 672);  
                        bt_send_cmd_ptr(l2cap_register_service_ptr,
                              PSM_HID_INTERRUPT, 672);
                        btpad_queue_hci_inquiry(HCI_INQUIRY_LAP, 3, 1);

                        btpad_queue_run(1);
                        break;

                     case HCI_STATE_HALTING:
                        btpad_close_all_connections();
                        break;                  
                  }
               }
               break;

            case HCI_EVENT_COMMAND_STATUS:
               btpad_queue_run(packet[3]);
               break;

            case HCI_EVENT_COMMAND_COMPLETE:
               {
                  btpad_queue_run(packet[2]);

                  if (COMMAND_COMPLETE_EVENT(packet, (*hci_read_bd_addr_ptr)))
                  {
                     bt_flip_addr_ptr(event_addr, &packet[6]);
                     if (!packet[5])
                        RARCH_LOG("[BTpad]: Local address is %s.\n",
                              bd_addr_to_str_ptr(event_addr));
                     else
                        RARCH_LOG("[BTpad]: Failed to get local address (Status: %02X).\n",
                              packet[5]);
                  }
               }
               break;

            case HCI_EVENT_INQUIRY_RESULT:
               {
                  if (packet[2])
                  {
                     bt_flip_addr_ptr(event_addr, &packet[3]);

                     struct pad_connection* connection = 
                        (struct pad_connection*)btpad_find_empty_connection();

                     if (!connection)
                        return;

                     RARCH_LOG("[BTpad]: Inquiry found device\n");
                     memset(connection, 0, sizeof(struct pad_connection));

                     memcpy(connection->address, event_addr, sizeof(bd_addr_t));
                     connection->has_address = true;
                     connection->state = BTPAD_CONNECTING;

                     bt_send_cmd_ptr(l2cap_create_channel_ptr, connection->address, PSM_HID_CONTROL);
                     bt_send_cmd_ptr(l2cap_create_channel_ptr, connection->address, PSM_HID_INTERRUPT);
                  }
               }
               break;

            case HCI_EVENT_INQUIRY_COMPLETE:
               {
                  /* This must be turned off during gameplay 
                   * as it causes a ton of lag. */
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

                  struct pad_connection* connection = 
                     (struct pad_connection*)btpad_find_connection_for(
                           handle, event_addr);

                  if (!packet[2])
                  {
                     if (!connection)
                     {
                        RARCH_LOG("[BTpad]: Got L2CAP 'Channel Opened' event for unrecognized device.\n");
                        break;
                     }

                     RARCH_LOG("[BTpad]: L2CAP channel opened: (PSM: %02X)\n", psm);
                     connection->handle = handle;

                     if (psm == PSM_HID_CONTROL)
                        connection->channels[0] = channel_id;
                     else if (psm == PSM_HID_INTERRUPT)
                        connection->channels[1] = channel_id;
                     else
                        RARCH_LOG("[BTpad]: Got unknown L2CAP PSM, ignoring (PSM: %02X).\n", psm);

                     if (connection->channels[0]
                           && connection->channels[1])
                     {
                        RARCH_LOG("[BTpad]: Got both L2CAP channels, requesting name.\n");
                        btpad_queue_hci_remote_name_request(
                              connection->address, 0, 0, 0);
                     }
                  }
                  else
                     RARCH_LOG("[BTpad]: Got failed L2CAP 'Channel Opened' event (PSM: %02X, Status: %02X).\n", psm, packet[2]);
               }
               break;

            case L2CAP_EVENT_INCOMING_CONNECTION:
               {
                  bt_flip_addr_ptr(event_addr, &packet[2]);
                  const uint16_t handle     = READ_BT_16(packet, 8);
                  const uint32_t psm        = READ_BT_16(packet, 10);
                  const uint32_t channel_id = READ_BT_16(packet, 12);

                  struct pad_connection* connection = 
                     (struct pad_connection*)btpad_find_connection_for(
                           handle, event_addr);

                  if (!connection)
                  {
                     connection = btpad_find_empty_connection();
                     if (!connection)
                        break;

                     RARCH_LOG("[BTpad]: Got new incoming connection\n");

                     memset(connection, 0,
                           sizeof(struct pad_connection));

                     memcpy(connection->address, event_addr,
                           sizeof(bd_addr_t));
                     connection->has_address = true;
                     connection->handle = handle;
                     connection->state = BTPAD_CONNECTING;
                  }

                  RARCH_LOG("[BTpad]: Incoming L2CAP connection (PSM: %02X).\n",
                        psm);
                  bt_send_cmd_ptr(l2cap_accept_connection_ptr, channel_id);
               }
               break;

            case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
               {
                  bt_flip_addr_ptr(event_addr, &packet[3]);

                  struct pad_connection* connection = 
                     (struct pad_connection*)btpad_find_connection_for(
                           0, event_addr);

                  if (!connection)
                  {
                     RARCH_LOG("[BTpad]: Got unexpected remote name, ignoring.\n");
                     break;
                  }

                  RARCH_LOG("[BTpad]: Got %.200s.\n", (char*)&packet[9]);

                  connection->slot  = pad_connection_pad_init(&slots[connection->slot],
                        (char*)packet + 9, connection, &btpad_connection_send_control);
                  connection->state = BTPAD_CONNECTED;
               }
               break;

            case HCI_EVENT_PIN_CODE_REQUEST:
               RARCH_LOG("[BTpad]: Sending Wiimote PIN.\n");

               bt_flip_addr_ptr(event_addr, &packet[2]);
               btpad_queue_hci_pin_code_request_reply(event_addr, &packet[2]);
               break;

            case HCI_EVENT_DISCONNECTION_COMPLETE:
               {
                  const uint32_t handle = READ_BT_16(packet, 3);

                  if (!packet[2])
                  {
                     struct pad_connection* connection = 
                        (struct pad_connection*)btpad_find_connection_for(
                              handle, 0);

                     if (connection)
                     {
                        connection->handle = 0;

                        pad_connection_pad_deinit(&slots[connection->slot], connection->slot);
                        btpad_close_connection(connection);
                     }
                  }
                  else
                     RARCH_LOG("[BTpad]: Got failed 'Disconnection Complete' event (Status: %02X).\n", packet[2]);
               }
               break;

            case L2CAP_EVENT_SERVICE_REGISTERED:
               if (packet[2])
                  RARCH_LOG("[BTpad]: Got failed 'Service Registered' event (PSM: %02X, Status: %02X).\n",
                        READ_BT_16(packet, 3), packet[2]);
               break;
         }
         break;
   }
}
