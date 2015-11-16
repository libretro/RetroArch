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

#include <stdio.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <string.h>
#ifdef __APPLE__
#include <CoreFoundation/CFRunLoop.h>
#endif

#include <rthreads/rthreads.h>
#include <dynamic/dylib.h>

#include "../input_hid_driver.h"
#define BUILDING_BTDYNAMIC
#include "btstack_hid.h"
#include "../connect/joypad_connection.h"

typedef struct btstack_hid
{
   void *empty;
} btstack_hid_t;

enum btpad_state
{
   BTPAD_EMPTY = 0,
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

struct btstack_hid_adapter
{
   uint32_t slot;

   enum btpad_state state;

   bool has_address;
   bd_addr_t address;

   uint16_t handle;

   /* 0: Control, 1: Interrupt */
   uint16_t channels[2];
};

#define GRAB(A) {#A, (void**)&A##_ptr}

static struct
{
   const char* name;
   void** target;
}  grabbers[] =
{
   GRAB(bt_open),
   GRAB(bt_close),
   GRAB(bt_flip_addr),
   GRAB(bd_addr_to_str),
   GRAB(bt_register_packet_handler),
   GRAB(bt_send_cmd),
   GRAB(bt_send_l2cap),
   GRAB(run_loop_init),
   GRAB(run_loop_execute),

   GRAB(btstack_set_power_mode),
   GRAB(hci_delete_stored_link_key),
   GRAB(hci_disconnect),
   GRAB(hci_read_bd_addr),
   GRAB(hci_inquiry),
   GRAB(hci_inquiry_cancel),
   GRAB(hci_pin_code_request_reply),
   GRAB(hci_pin_code_request_negative_reply),
   GRAB(hci_remote_name_request),
   GRAB(hci_remote_name_request_cancel),
   GRAB(hci_write_authentication_enable),
   GRAB(hci_write_inquiry_mode),
   GRAB(l2cap_create_channel),
   GRAB(l2cap_register_service),
   GRAB(l2cap_accept_connection),
   GRAB(l2cap_decline_connection),
   {0, 0}
};

extern void btpad_packet_handler(uint8_t packet_type,
      uint16_t channel, uint8_t *packet, uint16_t size);

static bool btstack_tested;
static bool btstack_loaded;

static sthread_t *btstack_thread;

#ifdef __APPLE__
static CFRunLoopSourceRef btstack_quit_source;
#endif

static void *btstack_get_handle(void)
{
   void *handle = dylib_load("/usr/lib/libBTstack.dylib");

   if (handle)
      return handle;

   return NULL;
}

bool btstack_try_load(void)
{
   unsigned i;
   void *handle   = NULL;

   if (btstack_tested)
      return btstack_loaded;

   btstack_tested = true;
   btstack_loaded = false;

   handle         = btstack_get_handle();

   if (!handle)
      return false;

   for (i = 0; grabbers[i].name; i ++)
   {
      *grabbers[i].target = dylib_proc(handle, grabbers[i].name);

      if (!*grabbers[i].target)
      {
         dylib_close(handle);
         return false;
      }
   }

   run_loop_init_ptr(RUN_LOOP_COCOA);
   bt_register_packet_handler_ptr(btpad_packet_handler);

   btstack_loaded = true;

   return true;
}

static void btstack_thread_stop(void *data)
{
   (void)data;
   bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_OFF);
}

static void btstack_thread_func(void* data)
{
   RARCH_LOG("[BTstack]: Thread started");

   if (bt_open_ptr())
      return;

#ifdef __APPLE__
   CFRunLoopSourceContext ctx = { 0, 0, 0, 0, 0, 0, 0, 0, 0, btstack_thread_stop };
   btstack_quit_source = CFRunLoopSourceCreate(0, 0, &ctx);
   CFRunLoopAddSource(CFRunLoopGetCurrent(), btstack_quit_source, kCFRunLoopCommonModes);
#endif

   RARCH_LOG("[BTstack]: Turning on...\n");
   bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_ON);

   RARCH_LOG("BTstack: Thread running...\n");
#ifdef __APPLE__
   CFRunLoopRun();
#endif

   RARCH_LOG("[BTstack]: Thread done.\n");

#ifdef __APPLE__
   CFRunLoopSourceInvalidate(btstack_quit_source);
   CFRelease(btstack_quit_source);
#endif
}

void btstack_set_poweron(bool on)
{
   if (!btstack_try_load())
      return;

   if (on && !btstack_thread)
      btstack_thread = sthread_create(btstack_thread_func, NULL);
   else if (!on && btstack_thread && btstack_quit_source)
   {
#ifdef __APPLE__
      CFRunLoopSourceSignal(btstack_quit_source);
#endif
      sthread_join(btstack_thread);
      btstack_thread = NULL;
   }
}

static bool inquiry_off;
static bool inquiry_running;
static struct btstack_hid_adapter g_connections[MAX_USERS];

struct btpad_queue_command commands[64];
static uint32_t insert_position;
static uint32_t read_position;
static uint32_t can_run;

static void btpad_increment_position(uint32_t *ptr)
{
   *ptr = (*ptr + 1) % 64;
}

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
   for (; can_run && (insert_position != read_position); can_run--)
   {
      struct btpad_queue_command* cmd = &commands[read_position];
      btpad_queue_process_cmd(cmd);
      btpad_increment_position(&read_position);
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

static void btpad_queue_btstack_set_power_mode(
      struct btpad_queue_command *cmd, uint8_t on)
{
   if (!cmd)
      return;

   cmd->command                   = btstack_set_power_mode_ptr;
   cmd->btstack_set_power_mode.on = on;

   btpad_increment_position(&insert_position);
   btpad_queue_process();
}

static void btpad_queue_hci_read_bd_addr(
      struct btpad_queue_command *cmd)
{
   if (!cmd)
      return;

   cmd->command = hci_read_bd_addr_ptr;

   btpad_increment_position(&insert_position);
   btpad_queue_process();
}

static void btpad_queue_hci_disconnect(
      struct btpad_queue_command *cmd,
      uint16_t handle, uint8_t reason)
{
   if (!cmd)
      return;

   cmd->command               = hci_disconnect_ptr;
   cmd->hci_disconnect.handle = handle;
   cmd->hci_disconnect.reason = reason;

   btpad_increment_position(&insert_position);
   btpad_queue_process();
}

static void btpad_queue_hci_inquiry(
      struct btpad_queue_command *cmd,
      uint32_t lap,
      uint8_t length, uint8_t num_responses)
{
   if (!cmd)
      return;

   cmd->command                   = hci_inquiry_ptr;
   cmd->hci_inquiry.lap           = lap;
   cmd->hci_inquiry.length        = length;
   cmd->hci_inquiry.num_responses = num_responses;

   btpad_increment_position(&insert_position);
   btpad_queue_process();
}

static void btpad_queue_hci_remote_name_request(
      struct btpad_queue_command *cmd,
      bd_addr_t bd_addr,
      uint8_t page_scan_repetition_mode,
      uint8_t reserved, uint16_t clock_offset)
{
   if (!cmd)
      return;

   cmd->command = hci_remote_name_request_ptr;
   memcpy(cmd->hci_remote_name_request.bd_addr, bd_addr, sizeof(bd_addr_t));
   cmd->hci_remote_name_request.page_scan_repetition_mode = 
      page_scan_repetition_mode;
   cmd->hci_remote_name_request.reserved     = reserved;
   cmd->hci_remote_name_request.clock_offset = clock_offset;

   btpad_increment_position(&insert_position);
   btpad_queue_process();
}

static void btpad_queue_hci_pin_code_request_reply(
      struct btpad_queue_command *cmd,
      bd_addr_t bd_addr, bd_addr_t pin)
{
   if (!cmd)
      return;

   cmd->command = hci_pin_code_request_reply_ptr;
   memcpy(cmd->hci_pin_code_request_reply.bd_addr, bd_addr, sizeof(bd_addr_t));
   memcpy(cmd->hci_pin_code_request_reply.pin, pin, sizeof(bd_addr_t));

   btpad_increment_position(&insert_position);
   btpad_queue_process();
}

static void btpad_connection_send_control(void *data,
      uint8_t* data_buf, size_t size)
{
   struct btstack_hid_adapter *connection = (struct btstack_hid_adapter*)data;

   if (connection)
      bt_send_l2cap_ptr(connection->channels[0], data_buf, size);
}

void btpad_set_inquiry_state(bool on)
{
   inquiry_off = !on;

   if (!inquiry_off && !inquiry_running)
      btpad_queue_hci_inquiry(&commands[insert_position],
            HCI_INQUIRY_LAP, 3, 1);
}

/* Internal interface. */
static struct btstack_hid_adapter *btpad_find_empty_connection(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (g_connections[i].state == BTPAD_EMPTY)
         return &g_connections[i];
   }

   return 0;
}

static struct btstack_hid_adapter *btpad_find_connection_for(
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

static void btpad_close_connection(struct btstack_hid_adapter* connection)
{
   if (!connection)
      return;

   if (connection->handle)
      btpad_queue_hci_disconnect(&commands[insert_position],
            connection->handle, 0x15);

   memset(connection, 0, sizeof(struct btstack_hid_adapter));
}

static void btpad_close_all_connections(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i ++)
      btpad_close_connection(&g_connections[i]);

#ifdef __APPLE__
   CFRunLoopStop(CFRunLoopGetCurrent());
#endif
}

void btpad_packet_handler(uint8_t packet_type,
      uint16_t channel, uint8_t *packet, uint16_t size)
{
   unsigned i;
   bd_addr_t event_addr;
   struct btpad_queue_command* cmd = &commands[insert_position];

   switch (packet_type)
   {
      case L2CAP_DATA_PACKET:
         for (i = 0; i < MAX_USERS; i ++)
         {
            struct btstack_hid_adapter *connection = &g_connections[i];

            if (!connection || connection->state != BTPAD_CONNECTED)
               continue;

            if (     connection->channels[0] == channel 
                  || connection->channels[1] == channel)
               pad_connection_packet(hid_driver_find_slot(connection->slot), connection->slot, packet, size);
         }
         break;
      case HCI_EVENT_PACKET:
         switch (packet[0])
         {
            case BTSTACK_EVENT_STATE:
               RARCH_LOG("[BTstack]: HCI State %d.\n", packet[2]);

               switch (packet[2])
               {                  
                  case HCI_STATE_WORKING:
                     btpad_queue_reset();
                     btpad_queue_hci_read_bd_addr(cmd);

                     /* TODO: Where did I get 672 for MTU? */

                     bt_send_cmd_ptr(l2cap_register_service_ptr,
                           PSM_HID_CONTROL, 672);  
                     bt_send_cmd_ptr(l2cap_register_service_ptr,
                           PSM_HID_INTERRUPT, 672);
                     btpad_queue_hci_inquiry(cmd, HCI_INQUIRY_LAP, 3, 1);

                     btpad_queue_run(1);
                     break;

                  case HCI_STATE_HALTING:
                     btpad_close_all_connections();
                     break;                  
               }
               break;

            case HCI_EVENT_COMMAND_STATUS:
               btpad_queue_run(packet[3]);
               break;

            case HCI_EVENT_COMMAND_COMPLETE:
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
               break;

            case HCI_EVENT_INQUIRY_RESULT:
               if (packet[2])
               {
                  struct btstack_hid_adapter* connection = NULL;

                  bt_flip_addr_ptr(event_addr, &packet[3]);

                  connection = btpad_find_empty_connection();

                  if (!connection)
                     return;

                  RARCH_LOG("[BTpad]: Inquiry found device\n");
                  memset(connection, 0, sizeof(struct btstack_hid_adapter));

                  memcpy(connection->address, event_addr, sizeof(bd_addr_t));
                  connection->has_address = true;
                  connection->state       = BTPAD_CONNECTING;

                  bt_send_cmd_ptr(l2cap_create_channel_ptr, connection->address, PSM_HID_CONTROL);
                  bt_send_cmd_ptr(l2cap_create_channel_ptr, connection->address, PSM_HID_INTERRUPT);
               }
               break;

            case HCI_EVENT_INQUIRY_COMPLETE:
               /* This must be turned off during gameplay 
                * as it causes a ton of lag. */
               inquiry_running = !inquiry_off;

               if (inquiry_running)
                  btpad_queue_hci_inquiry(cmd, HCI_INQUIRY_LAP, 3, 1);
               break;

            case L2CAP_EVENT_CHANNEL_OPENED:
               {
                  uint16_t handle, psm, channel_id;
                  struct btstack_hid_adapter *connection = NULL;

                  bt_flip_addr_ptr(event_addr, &packet[3]);

                  handle             = READ_BT_16(packet, 9);
                  psm                = READ_BT_16(packet, 11);
                  channel_id         = READ_BT_16(packet, 13);
                  connection         = btpad_find_connection_for(handle, event_addr);

                  if (!packet[2])
                  {
                     if (!connection)
                     {
                        RARCH_LOG("[BTpad]: Got L2CAP 'Channel Opened' event for unrecognized device.\n");
                        break;
                     }

                     RARCH_LOG("[BTpad]: L2CAP channel opened: (PSM: %02X)\n", psm);
                     connection->handle         = handle;

                     if (psm == PSM_HID_CONTROL)
                        connection->channels[0] = channel_id;
                     else if (psm == PSM_HID_INTERRUPT)
                        connection->channels[1] = channel_id;
                     else
                        RARCH_LOG("[BTpad]: Got unknown L2CAP PSM, ignoring (PSM: %02X).\n", psm);

                     if (connection->channels[0] && connection->channels[1])
                     {
                        RARCH_LOG("[BTpad]: Got both L2CAP channels, requesting name.\n");
                        btpad_queue_hci_remote_name_request(cmd, connection->address, 0, 0, 0);
                     }
                  }
                  else
                     RARCH_LOG("[BTpad]: Got failed L2CAP 'Channel Opened' event (PSM: %02X, Status: %02X).\n", psm, packet[2]);
               }
               break;

            case L2CAP_EVENT_INCOMING_CONNECTION:
               {
                  uint16_t handle, psm, channel_id;
                  struct btstack_hid_adapter* connection = NULL;

                  bt_flip_addr_ptr(event_addr, &packet[2]);

                  handle     = READ_BT_16(packet, 8);
                  psm        = READ_BT_16(packet, 10);
                  channel_id = READ_BT_16(packet, 12);

                  connection = btpad_find_connection_for(handle, event_addr);

                  if (!connection)
                  {
                     connection = btpad_find_empty_connection();
                     if (!connection)
                        break;

                     RARCH_LOG("[BTpad]: Got new incoming connection\n");

                     memset(connection, 0,
                           sizeof(struct btstack_hid_adapter));

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
                  struct btstack_hid_adapter *connection = NULL;

                  bt_flip_addr_ptr(event_addr, &packet[3]);

                  connection = btpad_find_connection_for(0, event_addr);

                  if (!connection)
                  {
                     RARCH_LOG("[BTpad]: Got unexpected remote name, ignoring.\n");
                     break;
                  }

                  RARCH_LOG("[BTpad]: Got %.200s.\n", (char*)&packet[9]);

                  connection->slot  = pad_connection_pad_init(hid_driver_find_slot(connection->slot),
                        (char*)packet + 9, 0, 0, connection, &btpad_connection_send_control);
                  connection->state = BTPAD_CONNECTED;
               }
               break;

            case HCI_EVENT_PIN_CODE_REQUEST:
               RARCH_LOG("[BTpad]: Sending Wiimote PIN.\n");

               bt_flip_addr_ptr(event_addr, &packet[2]);
               btpad_queue_hci_pin_code_request_reply(cmd, event_addr, &packet[2]);
               break;

            case HCI_EVENT_DISCONNECTION_COMPLETE:
               {
                  const uint32_t handle = READ_BT_16(packet, 3);

                  if (!packet[2])
                  {
                     struct btstack_hid_adapter* connection = btpad_find_connection_for(handle, 0);

                     if (connection)
                     {
                        connection->handle = 0;

                        pad_connection_pad_deinit(hid_driver_find_slot(connection->slot), connection->slot);
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


static bool btstack_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *btstack_hid_joypad_name(void *data, unsigned pad)
{
   /* TODO/FIXME - implement properly */
   if (pad >= MAX_USERS)
      return NULL;

   return NULL;
}

static uint64_t btstack_hid_joypad_get_buttons(void *data, unsigned port)
{
   btstack_hid_t        *hid   = (btstack_hid_t*)data;
   if (hid)
      return pad_connection_get_buttons(hid_driver_find_slot(port), port);
   return 0;
}

static bool btstack_hid_joypad_button(void *data, unsigned port, uint16_t joykey)
{
   uint64_t buttons          = btstack_hid_joypad_get_buttons(data, port);

   if (joykey == NO_BTN)
      return false;

   /* Check hat. */
   if (GET_HAT_DIR(joykey))
      return false;

   /* Check the button. */
   if ((port < MAX_USERS) && (joykey < 32))
      return ((buttons & (1 << joykey)) != 0);
   return false;
}

static bool btstack_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   btstack_hid_t        *hid   = (btstack_hid_t*)data;
   if (!hid)
      return false;
   return pad_connection_rumble(hid_driver_find_slot(pad), pad, effect, strength);
}

static int16_t btstack_hid_joypad_axis(void *data, unsigned port, uint32_t joyaxis)
{
   int16_t               val  = 0;

   if (joyaxis == AXIS_NONE)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      val += pad_connection_get_axis(hid_driver_find_slot(port), port, AXIS_NEG_GET(joyaxis));

      if (val >= 0)
         val = 0;
   }
   else if(AXIS_POS_GET(joyaxis) < 4)
   {
      val += pad_connection_get_axis(hid_driver_find_slot(port), port, AXIS_POS_GET(joyaxis));

      if (val <= 0)
         val = 0;
   }

   return val;
}

static void btstack_hid_free(void *data)
{
   btstack_hid_t *hid = (btstack_hid_t*)data;

   if (!hid)
      return;

   hid_driver_destroy_slots();

   if (hid)
      free(hid);
}

static void *btstack_hid_init(void)
{
   btstack_hid_t *hid = (btstack_hid_t*)calloc(1, sizeof(btstack_hid_t));

   if (!hid)
      goto error;

   if (!hid_driver_init_slots(MAX_USERS))
      goto error;

   return hid;

error:
   btstack_hid_free(hid);
   return NULL;
}


static void btstack_hid_poll(void *data)
{
   (void)data;
}

hid_driver_t btstack_hid = {
   btstack_hid_init,
   btstack_hid_joypad_query,
   btstack_hid_free,
   btstack_hid_joypad_button,
   btstack_hid_joypad_get_buttons,
   btstack_hid_joypad_axis,
   btstack_hid_poll,
   btstack_hid_joypad_rumble,
   btstack_hid_joypad_name,
   "btstack",
};
