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

#include "wiimote.h"

#import "WiiMoteHelper.h"

#import "BTDevice.h"
#import "BTstackManager.h"

static WiiMoteHelper* instance;
static BTDevice *device;
static bool btOK;

@implementation WiiMoteHelper
+ (void)startBluetooth
{
   instance = instance ? instance : [WiiMoteHelper new];
   
   if (!btOK)
   {
      BTstackManager* bt = [BTstackManager sharedInstance];
      [bt setDelegate:instance];
      [bt addListener:instance];

      btOK = [bt activate] == 0;
   }
}

+ (BOOL)isBluetoothRunning
{
   return btOK;
}

+ (void)stopBluetooth
{
   myosd_num_of_joys = 0;

   if (btOK)
   {
      BTstackManager* bt = [BTstackManager sharedInstance];
   
      [bt deactivate];
      [bt setDelegate:nil];
      [bt removeListener:instance];
      btOK = false;
   }
   
   instance = nil;
}

// BTStackManagerListener
-(void) activatedBTstackManager:(BTstackManager*) manager
{
	[[BTstackManager sharedInstance] startDiscovery];
}

-(void) btstackManager:(BTstackManager*)manager deviceInfo:(BTDevice*)newDevice
{
	if ([newDevice name] && [[newDevice name] hasPrefix:@"Nintendo RVL-CNT-01"])
   {
		device = newDevice;
		[[BTstackManager sharedInstance] stopDiscovery];
	}
}

-(void) discoveryStoppedBTstackManager:(BTstackManager*) manager
{
	bt_send_cmd(&hci_write_authentication_enable, 0);
}

// BTStackManagerDelegate
-(void) btstackManager:(BTstackManager*) manager
  handlePacketWithType:(uint8_t)packet_type
			   forChannel:(uint16_t)channel
			      andData:(uint8_t*)packet
			      withLen:(uint16_t)size
{
   bd_addr_t event_addr;

   switch (packet_type)
   {
      case L2CAP_DATA_PACKET://0x06
      {
         struct wiimote_t *wm = wiimote_get_by_source_cid(channel);
         
         if (wm != NULL)
         {
            byte* msg = packet + 2;
            byte event = packet[1];
                        
            switch (event)
            {
               case WM_RPT_BTN:
               {
                  wiimote_pressed_buttons(wm, msg);
                  break;
               }

               case WM_RPT_READ:
               {
                  /* data read */
                  wiimote_pressed_buttons(wm, msg);
											
                  byte len = ((msg[2] & 0xF0) >> 4) + 1;
                  byte *data = (msg + 5);
										
                  if(wiimote_handshake(wm, WM_RPT_READ, data, len))
                  {
                     if (device != nil)
                     {
                        [device setConnectionState:kBluetoothConnectionConnected];
                        device = nil;
                     }
                  }

                  return;
               }
					
               case WM_RPT_CTRL_STATUS:
               {
                  wiimote_pressed_buttons(wm, msg);
          
                  //handshake stuff!
                  if(wiimote_handshake(wm,WM_RPT_CTRL_STATUS,msg,-1))
                  {
                     [device setConnectionState:kBluetoothConnectionConnected];

                     if (device != nil)
                     {
                        [device setConnectionState:kBluetoothConnectionConnected];
                        device = nil;
                     }
                  }

                  return;
               }

               case WM_RPT_BTN_EXP:
               {
                  /* button - expansion */
                  wiimote_pressed_buttons(wm, msg);
                  wiimote_handle_expansion(wm, msg+2);

                  break;
               }
               
               case WM_RPT_WRITE:
               {
                  /* write feedback - safe to skip */
                  break;
               }

               default:
               {
                  printf("Unknown event, can not handle it [Code 0x%x].", event);
                  return;
               }
            }
         }
         break;
      }
      
      case HCI_EVENT_PACKET://0x04
      {
         switch (packet[0])
         {
            case HCI_EVENT_COMMAND_COMPLETE:
            {
               if (COMMAND_COMPLETE_EVENT(packet, hci_write_authentication_enable))
                  bt_send_cmd(&l2cap_create_channel, [device address], PSM_HID_INTERRUPT);
               break;
            }
         
            case HCI_EVENT_PIN_CODE_REQUEST:
            {
               bt_flip_addr(event_addr, &packet[2]);
               if (BD_ADDR_CMP([device address], event_addr)) break;
                    
               // inform about pin code request
               NSLog(@"HCI_EVENT_PIN_CODE_REQUEST\n");
               bt_send_cmd(&hci_pin_code_request_reply, event_addr, 6,  &packet[2]); // use inverse bd_addr as PIN
               break;
            }
            
            case L2CAP_EVENT_CHANNEL_OPENED:
            {
               // data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16)
               if (packet[2] == 0)
               {
                  // inform about new l2cap connection
                  bt_flip_addr(event_addr, &packet[3]);
                  uint16_t psm = READ_BT_16(packet, 11);
                  uint16_t source_cid = READ_BT_16(packet, 13);
                  uint16_t wiiMoteConHandle = READ_BT_16(packet, 9);

                  if (psm == 0x13)
                  {
                     // interupt channel openedn succesfully, now open control channel, too.
                     bt_send_cmd(&l2cap_create_channel, event_addr, 0x11);
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
               wiimote_remove(source_cid,&addr);
               break;
            }
         }
      }
   }
}
@end
