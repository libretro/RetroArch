/*
 * This file is part of MAME4iOS.
 *
 * Copyright (C) 2012 David Valdeita (Seleuco)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Seleuco
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>

#include "../../rarch_wrapper.h"
#include "btdynamic.h"
#include "btpad.h"
#include "wiimote.h"

enum btpad_state_t { BTPAD_NOT_INITIALIZED, BTPAD_NO_CONNECTION, BTPAD_INQUIRY_CONNECTION, BTPAD_CONNECTION, BTPAD_WANT_NAME, BTPAD_CONNECTED };
static enum btpad_state_t btpad_state = BTPAD_NOT_INITIALIZED;

static enum btpad_device_type_t device_type;
static bd_addr_t device_address;
static char device_name[256];

// Connection data
static uint32_t device_handle[2];
static uint32_t device_remote_cid[2];
static uint32_t device_local_cid[2];

// Inquiry data
static uint32_t device_page_scan_repetition_mode;
static uint32_t device_class;
static uint32_t device_clock_offset;

// Buffers
static struct wiimote_t wiimote_buffer;
static uint8_t psdata_buffer[512];

// MAIN THREAD ONLY
enum btpad_device_type_t btpad_get_connected_type()
{
   return device_type;
}

uint32_t btpad_get_buttons()
{
   switch (device_type)
   {
      case BTPAD_WIIMOTE:
         return wiimote_buffer.btns | (wiimote_buffer.exp.classic.btns << 16);
      case BTPAD_PS3:
         return psdata_buffer[3] | (psdata_buffer[4] << 8);
      default:
         return 0;
   }
}

// BT THREAD ONLY
static void set_ps3_data(unsigned leds)
{
   // TODO: LEDS

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
   
   bt_send_l2cap_ptr(device_local_cid[0], report_buffer, sizeof(report_buffer));
}

static bool btstack_read_incoming_connection_packet(uint8_t* packet, uint16_t size)
{
   const uint32_t psm = READ_BT_16(packet, 10);
   const unsigned interrupt = (psm == PSM_HID_INTERRUPT) ? 1 : 0;

   bt_flip_addr_ptr(device_address, &packet[2]);
   device_handle[interrupt] = READ_BT_16(packet, 8);
   device_local_cid[interrupt] = READ_BT_16(packet, 12);
   device_remote_cid[interrupt] = READ_BT_16(packet, 14);
   
   return interrupt;
}

static void btstack_not_initialized_handler(uint8_t* packet, uint16_t size)
{
   switch (packet[0])
   {
      case BTSTACK_EVENT_STATE:
      {
         if (packet[2] == HCI_STATE_WORKING)
         {
            ios_add_log_message("BTstack: Power state now HCI_STATE_WORKING");
            ios_add_log_message("BTstack: Registering HID INTERRUPT service");
            bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_INTERRUPT, 672);
         }
         else
            ios_add_log_message("BTstack: State is now %d", packet[2]);
         break;
      }

      case L2CAP_EVENT_SERVICE_REGISTERED:
      {
         if (READ_BT_16(packet, 3) == PSM_HID_INTERRUPT)
         {
            ios_add_log_message("BTstack: HID INTERRUPT service registered");
            ios_add_log_message("BTstack: Registering HID CONTROL service");
            bt_send_cmd_ptr(l2cap_register_service_ptr, PSM_HID_CONTROL, 672);
         }
         else if(READ_BT_16(packet, 3) == PSM_HID_CONTROL)
         {
            ios_add_log_message("BTstack: HID CONTROL service registered");

            ios_add_log_message("BTstack: Waiting for connection");
            bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 0);
            btpad_state = BTPAD_NO_CONNECTION;
         }
         break;
      }
   }
}

static void btstack_no_connection_handler(uint8_t* packet, uint16_t size)
{
   switch (packet[0])
   {
      // Device is connecting to iOS
      case L2CAP_EVENT_INCOMING_CONNECTION:
      {
         ios_add_log_message("BTstack: Incoming L2CAP connection (PS3 Pad?)");
      
         btpad_state = BTPAD_CONNECTION;
               
         bool ch = btstack_read_incoming_connection_packet(packet, size);
         bt_send_cmd_ptr(l2cap_accept_connection_ptr, device_local_cid[ch ? 1 : 0]);

         break;
      }

      // iOS is connecting to device
      // Here we just copy the details of the first device found
      case HCI_EVENT_INQUIRY_RESULT:
      {
         ios_add_log_message("BTstack: Inquiry found device (WiiMote?)");
      
         if (packet[2])
         {
            btpad_state = BTPAD_INQUIRY_CONNECTION;
               
            bt_flip_addr_ptr(device_address, &packet[3]);
            device_page_scan_repetition_mode =   packet [3 + packet[2] * (6)];
            device_class = READ_BT_24(packet, 3 + packet[2] * (6+1+1+1));
            device_clock_offset =   READ_BT_16(packet, 3 + packet[2] * (6+1+1+1+3)) & 0x7fff;
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

static void btstack_connection_handler(uint8_t* packet, uint16_t size)
{
   switch (packet[0])
   {
      case L2CAP_EVENT_INCOMING_CONNECTION:
      {
         ios_add_log_message("BTstack: Got other half L2CAP connection; requesting name\n");
      
         btpad_state = BTPAD_WANT_NAME;

         bool ch = btstack_read_incoming_connection_packet(packet, size);
         bt_send_cmd_ptr(l2cap_accept_connection_ptr, device_local_cid[ch ? 1 : 0]);
         bt_send_cmd_ptr(hci_remote_name_request_ptr, device_address, 0, 0, 0);

         break;
      }
   }
}

static void btstack_inquiry_connection_handler(uint8_t* packet, uint16_t size)
{
   switch (packet[0])
   {
      case HCI_EVENT_INQUIRY_COMPLETE:
      {
         ios_add_log_message("BTstack: Got inquiry complete; requesting name\n");
      
         btpad_state = BTPAD_WANT_NAME;
         bt_send_cmd_ptr(hci_remote_name_request_ptr, device_address, device_page_scan_repetition_mode,
                         0, device_clock_offset | 0x8000);
         break;
      }
   }
}

static void btstack_want_name_handler(uint8_t* packet, uint16_t size)
{
   bd_addr_t event_addr;

   switch (packet[0])
   {
      case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
      {
         bt_flip_addr_ptr(event_addr, &packet[3]);
               
         if (BD_ADDR_CMP(event_addr, device_address) == 0)
         {
            strncpy(device_name, (const char*)&packet[9], 248);
            device_name[248] = 0;

            ios_add_log_message("BTstack: Got device name: %s\n", device_name);

            if (strncmp(device_name, "Nintendo RVL-CNT-01", 19) == 0)
            {
               ios_add_log_message("BTstack: Got WiiMote\n");
               btpad_state = BTPAD_CONNECTED;
               device_type = BTPAD_WIIMOTE;
               bt_send_cmd_ptr(l2cap_create_channel_ptr, device_address, PSM_HID_CONTROL);
            }
            else if(strncmp(device_name, "PLAYSTATION(R)3 Controller", 26) == 0)
            {
               ios_add_log_message("BTstack: Got PS3 Pad; Sending startup packets\n");
            
               btpad_state = BTPAD_CONNECTED;
               device_type = BTPAD_PS3;

               // Special packet to tell PS3 controller to send reports
               uint8_t data[] = {0x53, 0xF4, 0x42, 0x03, 0x00, 0x00};
               bt_send_l2cap_ptr(device_local_cid[0], data, 6);
               set_ps3_data(0);
            }
            else
            {
               ios_add_log_message("BTstack: Unrecognized device; Will keep looking");
               btpad_state = BTPAD_NO_CONNECTION;
            }
         }
               
         break;
      }
   }
}

static void btstack_ps3_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   if (packet_type == L2CAP_DATA_PACKET && packet[0] == 0xA1)
   {
      static bool tag = false;
      if(!tag) ios_add_log_message("BTstack: Got PS3 Data Packet (Will only show once)\n");
      tag = true;
   
      memcpy(psdata_buffer, packet, size);
   }
}

static void btstack_wiimote_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   bd_addr_t event_addr;

   if (packet_type == HCI_EVENT_PACKET && packet[0] == L2CAP_EVENT_CHANNEL_OPENED)
   {
      bt_flip_addr_ptr(event_addr, &packet[3]);
      const uint16_t psm = READ_BT_16(packet, 11);
            
      if (packet[2] == 0 && BD_ADDR_CMP(event_addr, device_address) == 0)
      {
         ios_add_log_message("BTstack: WiiMote L2CAP channel openend: %02X\n", psm);
            
         const unsigned interrupt = (psm == PSM_HID_INTERRUPT) ? 1 : 0;
                  
         device_local_cid[interrupt] = READ_BT_16(packet, 13);
         device_remote_cid[interrupt] = READ_BT_16(packet, 15);
         device_handle[interrupt] = READ_BT_16(packet, 9);

         if (psm == PSM_HID_CONTROL)
         {
            bt_send_cmd_ptr(l2cap_create_channel_ptr, device_address, PSM_HID_INTERRUPT);

            memset(&wiimote_buffer, 0, sizeof(struct wiimote_t));
            wiimote_buffer.unid = 0;
            wiimote_buffer.c_source_cid = device_local_cid[0];
            wiimote_buffer.exp.type = EXP_NONE;
            wiimote_buffer.wiiMoteConHandle = device_handle[0];
            memcpy(&wiimote_buffer.addr, &device_address, BD_ADDR_LEN);
         }
         else if (psm == PSM_HID_INTERRUPT)
         {
            wiimote_buffer.i_source_cid = device_local_cid[1];
            wiimote_buffer.state = WIIMOTE_STATE_CONNECTED;
            wiimote_handshake(&wiimote_buffer, -1, NULL, -1);
         }
      }
      else
      {
         ios_add_log_message("BTstack: Failed to open WiiMote L2CAP channel: %d", psm);
         ios_add_log_message("BTstack: Won't recover, please exit RetroArch from the task switcher");
      }
   }
   else if(packet_type == L2CAP_DATA_PACKET)
   {
      static bool tag = false;
      if(!tag) ios_add_log_message("BTstack: Got Wii Data Packet (Will only show once)\n");
      tag = true;
   
      byte* msg = packet + 2;
         
      switch (packet[1])
      {
         case WM_RPT_BTN:
         {
            wiimote_pressed_buttons(&wiimote_buffer, msg);
            break;
         }

         case WM_RPT_READ:
         {
            wiimote_pressed_buttons(&wiimote_buffer, msg);

            byte len = ((msg[2] & 0xF0) >> 4) + 1;
            byte *data = (msg + 5);

            wiimote_handshake(&wiimote_buffer, WM_RPT_READ, data, len);
            return;
         }

         case WM_RPT_CTRL_STATUS:
         {
            wiimote_pressed_buttons(&wiimote_buffer, msg);
            wiimote_handshake(&wiimote_buffer,WM_RPT_CTRL_STATUS,msg,-1);

            return;
         }

         case WM_RPT_BTN_EXP:
         {
            wiimote_pressed_buttons(&wiimote_buffer, msg);
            wiimote_handle_expansion(&wiimote_buffer, msg+2);
            break;
         }
      }
   }
}


void btstack_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
   typedef void (*handler_t)(uint8_t*, uint16_t);
   static handler_t const handlers[] =
   {
      &btstack_not_initialized_handler,
      &btstack_no_connection_handler,
      &btstack_inquiry_connection_handler,
      &btstack_connection_handler,
      &btstack_want_name_handler
   };

   if (packet_type == HCI_EVENT_PACKET && btpad_state <= BTPAD_WANT_NAME)
   {
      handlers[(unsigned)btpad_state](packet, size);
   }
   else if (btpad_state == BTPAD_CONNECTED)
   {
      if (device_type == BTPAD_PS3)
         btstack_ps3_handler(packet_type, channel, packet, size);
      else if(device_type == BTPAD_WIIMOTE)
         btstack_wiimote_handler(packet_type, channel, packet, size);
   }

   if (packet_type == HCI_EVENT_PACKET)
   {
      bd_addr_t event_addr;
   
      switch (packet[0])
      {
         // This PIN will work for Wiimotes, other devices with PINs aren't supported
         case HCI_EVENT_PIN_CODE_REQUEST:
            ios_add_log_message("BTstack: Sending PIN (will fail if device isn't WiiMote");
         
            bt_flip_addr_ptr(event_addr, &packet[2]);
            bt_send_cmd_ptr(hci_pin_code_request_reply_ptr, device_address, 6, &packet[2]);
            break;
      
         case L2CAP_EVENT_CHANNEL_CLOSED:
            ios_add_log_message("BTstack: Lost L2CAP connection; will start searching for new device");
   
            btpad_state = BTPAD_NO_CONNECTION;
            device_type = BTPAD_NONE;
            
            bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 0);

            break;
      }
   }
}
