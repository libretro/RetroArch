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

#include "btdynamic.h"
#include "wiimote.h"

#import "WiiMoteHelper.h"

#import "BTDevice.h"

static WiiMoteHelper* instance;
static bool btstackOpen;
static bool btOK;

static BTDevice* discoveredDevice;
static bd_addr_t address;
static uint32_t handle[2];
static uint32_t remote_cid[2];
static uint32_t local_cid[2];
uint8_t psdata_buffer[512];

void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
#if 1 // WiiMote
   bd_addr_t event_addr;

   switch (packet_type)
   {
      // Connection
      case HCI_EVENT_PACKET:
      {
         switch (packet[0])
         {
            // Bluetooth is active, search for remote         
            case BTSTACK_EVENT_STATE:
            {
               if (packet[2] == HCI_STATE_WORKING)
                  bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 0);
					break;
            }
         
            // Identifies devices found during inquiry, does not signal the end of the inquiry.
				case HCI_EVENT_INQUIRY_RESULT:
            {
					for (int i = 0; i != packet[2]; i ++)
               {
                  if (!discoveredDevice)
                  {
                     bt_flip_addr_ptr(event_addr, &packet[3 + i * 6]);
                     discoveredDevice = [[BTDevice alloc] init];
                     [discoveredDevice setAddress:&event_addr];
                  }

						// update
						discoveredDevice.pageScanRepetitionMode =   packet [3 + packet[2] * (6)         + i*1];
						discoveredDevice.classOfDevice = READ_BT_24(packet, 3 + packet[2] * (6+1+1+1)   + i*3);
						discoveredDevice.clockOffset =   READ_BT_16(packet, 3 + packet[2] * (6+1+1+1+3) + i*2) & 0x7fff;
						discoveredDevice.rssi  = 0;
					}
               
					break;
            }
            
            // The inquiry has ended
            case HCI_EVENT_INQUIRY_COMPLETE:
            {
               // If we a device, ask for its name
               if (discoveredDevice)
                  bt_send_cmd_ptr(hci_remote_name_request_ptr, [discoveredDevice address], discoveredDevice.pageScanRepetitionMode,
                                     0, discoveredDevice.clockOffset | 0x8000);
               // Keep looking
               else
                  bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, 3, 0);

               break;
            }
            
            // Received the name of a device
            case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            {
               bt_flip_addr_ptr(event_addr, &packet[3]);
               
               if (discoveredDevice && BD_ADDR_CMP(event_addr, discoveredDevice.address) == 0)
               {
                  char cname[249];
                  strncpy(cname, (const char*)&packet[9], 248);
                  cname[248] = 0;

                  NSString* name = [NSString stringWithUTF8String:cname];
                  [discoveredDevice setName:name];
                  
                  // We found a WiiMote, pair with it
                  if ([name hasPrefix:@"Nintendo RVL-CNT-01"])
                     bt_send_cmd_ptr(l2cap_create_channel_ptr, [discoveredDevice address], PSM_HID_INTERRUPT);
               }
               
               break;
            }

            // Send PIN for pairing
            case HCI_EVENT_PIN_CODE_REQUEST:
            {
               bt_flip_addr_ptr(event_addr, &packet[2]);
               
               if (discoveredDevice && BD_ADDR_CMP(event_addr, discoveredDevice.address) == 0)
               {
                  // WiiMote: Use inverse bd_addr as PIN
                  if (discoveredDevice.name && [discoveredDevice.name hasPrefix:@"Nintendo RVL-CNT-01"])
                     bt_send_cmd_ptr(hci_pin_code_request_reply_ptr, event_addr, 6, &packet[2]);
               }
               break;
            }

            // WiiMote connections
            case L2CAP_EVENT_CHANNEL_OPENED:
            {
               // data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16)
               if (packet[2] == 0)
               {
                  // inform about new l2cap connection
                  bt_flip_addr_ptr(event_addr, &packet[3]);
                  uint16_t psm = READ_BT_16(packet, 11);
                  uint16_t source_cid = READ_BT_16(packet, 13);
                  uint16_t wiiMoteConHandle = READ_BT_16(packet, 9);

                  if (psm == 0x13)
                  {
                     // interupt channel openedn succesfully, now open control channel, too.
                     bt_send_cmd_ptr(l2cap_create_channel_ptr, event_addr, 0x11);
                     struct wiimote_t *wm = &joys[myosd_num_of_joys];
                     memset(wm, 0, sizeof(struct wiimote_t));
                     wm->unid = myosd_num_of_joys;
                     wm->i_source_cid = source_cid;
                     memcpy(&wm->addr,&event_addr,BD_ADDR_LEN);
                     wm->exp.type = EXP_NONE;
                  }
                  else
                  {
                     //inicializamos el wiimote!
                     struct wiimote_t *wm = &joys[myosd_num_of_joys];
                     wm->wiiMoteConHandle = wiiMoteConHandle;
                     wm->c_source_cid = source_cid;
                     wm->state = WIIMOTE_STATE_CONNECTED;
                     myosd_num_of_joys++;
                     wiimote_handshake(wm,-1,NULL,-1);
                  }
               }
 
               break;
            }

            case L2CAP_EVENT_CHANNEL_CLOSED:
            {
               // data: event (8), len(8), channel (16)
               uint16_t  source_cid = READ_BT_16(packet, 2);

               bd_addr_t addr;
               wiimote_remove(source_cid, &addr);
               break;
            }
         }
      }
      
      // WiiMote handling
      case L2CAP_DATA_PACKET:
      {
         struct wiimote_t *wm = wiimote_get_by_source_cid(channel);
         if (wm)
         {
            byte* msg = packet + 2;
         
            switch (packet[1])
            {
               case WM_RPT_BTN:
               {
                  wiimote_pressed_buttons(wm, msg);
                  break;
               }

               case WM_RPT_READ:
               {
                  wiimote_pressed_buttons(wm, msg);

                  byte len = ((msg[2] & 0xF0) >> 4) + 1;
                  byte *data = (msg + 5);
										
                  wiimote_handshake(wm, WM_RPT_READ, data, len);
                  return;
               }
					
               case WM_RPT_CTRL_STATUS:
               {
                  wiimote_pressed_buttons(wm, msg);
                  wiimote_handshake(wm,WM_RPT_CTRL_STATUS,msg,-1);

                  return;
               }

               case WM_RPT_BTN_EXP:
               {
                  wiimote_pressed_buttons(wm, msg);
                  wiimote_handle_expansion(wm, msg+2);
                  break;
               }
            }
         }
         break;
      }
   }
#else // SixAxis
   switch (packet_type)
   {
      // Connection
      case HCI_EVENT_PACKET:
      {
         switch (packet[0])
         {
            // Bluetooth is active, search for remote         
            case BTSTACK_EVENT_STATE:
            {
               if (packet[2] == HCI_STATE_WORKING)
                  bt_send_cmd_ptr(l2cap_register_service_ptr, 0x11, 672);
					break;
            }
            
            case L2CAP_EVENT_SERVICE_REGISTERED:
            {
               if (READ_BT_16(packet, 3) == 0x11)
                  bt_send_cmd_ptr(l2cap_register_service_ptr, 0x13, 672);
               break;
            }
            
            case L2CAP_EVENT_INCOMING_CONNECTION:
            {
               const uint32_t psm = READ_BT_16(packet, 10);
               const bool second = (psm == 0x11) ? 0 : 1;

               handle[second] = READ_BT_16(packet, 8);
               local_cid[second] = READ_BT_16(packet, 12);
               remote_cid[second] = READ_BT_16(packet, 14);
           
               bt_flip_addr_ptr(address, &packet[2]);
               bt_send_cmd_ptr(l2cap_accept_connection_ptr, local_cid[second]);
               
               break;
            }

            case L2CAP_EVENT_CHANNEL_OPENED:
            {
               if (READ_BT_16(packet, 11) == PSM_HID_INTERRUPT)
               {
                  uint8_t data[] = {0x53, 0xF4, 0x42, 0x03, 0x00, 0x00};
                  bt_send_l2cap_ptr(local_cid[0], data, 6);
                  set_ps3_data(0);
               }
            
               break;
            }
            
            break;
         }

         break;
      }
         
      case L2CAP_DATA_PACKET:
      {
         if (packet[0] == 0xA1)
            memcpy(psdata_buffer, packet, size);
         break;
      }
   }
#endif
}

@implementation WiiMoteHelper
+ (BOOL)haveBluetooth
{
   if (!btstackOpen)
   {
      btstackOpen = load_btstack();
      
      if (btstackOpen)
      {         
         run_loop_init_ptr(RUN_LOOP_COCOA);
         bt_register_packet_handler_ptr(packet_handler);
      }
   }

   return btstackOpen;
}

+ (void)startBluetooth
{
   if (btstackOpen)
   {
      instance = instance ? instance : [WiiMoteHelper new];

      if (!btOK)
      {
         if (bt_open_ptr())
         {
            btOK = false;
            return;
         }

         bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_ON);

         btOK = true;
      }
   }
}

+ (BOOL)isBluetoothRunning
{
   return btstackOpen && btOK;
}

+ (void)stopBluetooth
{
   if (btstackOpen)
   {
      myosd_num_of_joys = 0;

      if (btOK)
         bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_OFF);

      btOK = false;
      instance = nil;
   }
}

@end
