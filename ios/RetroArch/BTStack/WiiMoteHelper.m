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
#import "BTInquiryViewController.h"

#import "btstack/btstack.h"
#import "btstack/run_loop.h"
#import "btstack/hci_cmds.h"

bool btOK = false;
bool initLoop = false;
BTDevice *device;
uint16_t wiiMoteConHandle = 0;
bool conected = false;
bool activated = false;

BTInquiryViewController *inqViewControl;
  
void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
        bd_addr_t event_addr;
        

        switch (packet_type) {

                case L2CAP_DATA_PACKET://0x06
                {
                        struct wiimote_t *wm = NULL; 
                        
                        wm = wiimote_get_by_source_cid(channel);
                        
                        if(wm!=NULL)
                        {
                            
                            byte* msg = packet + 2;
                            byte event = packet[1];
                        
                            	switch (event) {
									case WM_RPT_BTN:
									{
										/* button */
										wiimote_pressed_buttons(wm, msg);
										break;
									}
									case WM_RPT_READ:
									{
										/* data read */
																
										if(WIIMOTE_DBG)printf("WM_RPT_READ data arrive!\n");
										
										wiimote_pressed_buttons(wm, msg);
																				
									    byte err = msg[2] & 0x0F;
									
										if (err == 0x08)
											printf("Unable to read data - address does not exist.\n");
										else if (err == 0x07)
											printf("Unable to read data - address is for write-only registers.\n");
										else if (err)
											printf("Unable to read data - unknown error code %x.\n", err);
											
										unsigned short offset = BIG_ENDIAN_SHORT(*(unsigned short*)(msg + 3));
											
										byte len = ((msg[2] & 0xF0) >> 4) + 1;
										
										byte *data = (msg + 5);
										
										if(WIIMOTE_DBG)
										{
											int i = 0;
											printf("Read: 0x%04x ; ",offset);
											for (; i < len; ++i)
												printf("%x ", data[i]);
											printf("\n");
										}
										
										if(wiimote_handshake(wm,WM_RPT_READ,data,len))
										{
										   //btUsed = 1;                                                    
                                           [inqViewControl showConnected:nil];
                                           [inqViewControl showConnecting:nil];
                                           //Create UIAlertView alert
                                           [inqViewControl showConnecting:nil];
                                           
                                           UIAlertView* alert = 
                                           [[UIAlertView alloc] initWithTitle:@"Connection detected!"
                                                                message: [NSString stringWithFormat:@"%@ '%@' connection sucessfully completed!",
                                                                   (wm->exp.type != EXP_NONE ? @"Classic Controller" : @"WiiMote"),
                                                                  [NSNumber numberWithInt:(wm->unid)+1]]        
                                                                delegate:nil cancelButtonTitle:@"Dismiss" otherButtonTitles: nil];
                                           [alert show];                                           
                                           //[alert dismissWithClickedButtonIndex:0 animated:TRUE];                                           
                                           [alert release];
                                           
                                           if(device!=nil)
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
                                        
                                        /* find the battery level and normalize between 0 and 1 */
                                        if(WIIMOTE_DBG)
                                        {
	                                       wm->battery_level = (msg[5] / (float)WM_MAX_BATTERY_CODE);
	                                    
	                                       printf("BATTERY LEVEL %f\n", wm->battery_level);
	                                    }
	                                    
	                                    //handshake stuff!
	                                    if(wiimote_handshake(wm,WM_RPT_CTRL_STATUS,msg,-1))
	                                    {
	                                       //btUsed = 1;                                                    
                                           [inqViewControl showConnected:nil];
                                           [inqViewControl showConnecting:nil];
                                                                                      UIAlertView* alert = 
                                           [[UIAlertView alloc] initWithTitle:@"Connection detected!"
                                                                message: [NSString stringWithFormat:@"WiiMote '%@' connection sucessfully completed!",[NSNumber numberWithInt:(wm->unid)+1]]        
                                                                delegate:nil cancelButtonTitle:@"Dismiss" otherButtonTitles: nil];
                                           [alert show];                                           
                                           //[alert dismissWithClickedButtonIndex:0 animated:TRUE];                                           
                                           [alert release];
                                           [device setConnectionState:kBluetoothConnectionConnected];
                                           
                                           if(device!=nil)
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
                        switch (packet[0]){

                                case L2CAP_EVENT_CHANNEL_OPENED:
                                        
                                        // data: event (8), len(8), status (8), address(48), handle (16), psm (16), local_cid(16), remote_cid (16)                                         
                                        if (packet[2] == 0) {
                                               
                                                // inform about new l2cap connection
                                                bt_flip_addr(event_addr, &packet[3]);
                                                uint16_t psm = READ_BT_16(packet, 11);
                                                uint16_t source_cid = READ_BT_16(packet, 13);
                                                wiiMoteConHandle = READ_BT_16(packet, 9);
                                                NSLog(@"Channel successfully opened: handle 0x%02x, psm 0x%02x, source cid 0x%02x, dest cid 0x%02x",
                                                           wiiMoteConHandle, psm, source_cid,  READ_BT_16(packet, 15));
                                                                                                                                                                                                                  
                                                if (psm == 0x13) {
                                                
                                                        // interupt channel openedn succesfully, now open control channel, too.
                                                        if(WIIMOTE_DBG)printf("open control channel\n");
                                                        bt_send_cmd(&l2cap_create_channel, event_addr, 0x11);
                                                        struct wiimote_t *wm = NULL;  
                                                        wm = &joys[myosd_num_of_joys];
                                                        memset(wm, 0, sizeof(struct wiimote_t));
                                                        wm->unid = myosd_num_of_joys;                                                        
                                                        wm->i_source_cid = source_cid;
                                                        memcpy(&wm->addr,&event_addr,BD_ADDR_LEN);
                                                        if(WIIMOTE_DBG)printf("addr %02x:%02x:%02x:%02x:%02x:%02x\n", wm->addr[0], wm->addr[1], wm->addr[2],wm->addr[3], wm->addr[4], wm->addr[5]);                                                    
                                                        if(WIIMOTE_DBG)printf("saved 0x%02x  0x%02x\n",source_cid,wm->i_source_cid);
                                                        wm->exp.type = EXP_NONE;
                                                        
                                                } else {
                                                                                                        
                                                        //inicializamos el wiimote!   
                                                        struct wiimote_t *wm = NULL;  
                                                        wm = &joys[myosd_num_of_joys];                                                                                                                                                                  
                                                        wm->wiiMoteConHandle = wiiMoteConHandle; 
                                                        wm->c_source_cid = source_cid;                                                           
                                                        wm->state = WIIMOTE_STATE_CONNECTED;
                                                        myosd_num_of_joys++;
                                                        if(WIIMOTE_DBG)printf("Devices Number: %d\n",myosd_num_of_joys);
                                                        wiimote_handshake(wm,-1,NULL,-1);                                                                                                                                                                                                                                                                      
                                                }
                                        }
                                        break;
                                case L2CAP_EVENT_CHANNEL_CLOSED:
                                       {                                
	                                        // data: event (8), len(8), channel (16)                                                                                       
	                                        uint16_t  source_cid = READ_BT_16(packet, 2);                                              
	                                        NSLog(@"Channel successfully closed: cid 0x%02x",source_cid);
	                                        
	                                        bd_addr_t addr;
	                                        int unid = wiimote_remove(source_cid,&addr);
	                                        if(unid!=-1)
	                                        {
	                                           [inqViewControl removeDeviceForAddress:&addr];
	                                           UIAlertView* alert = 
                                               [[UIAlertView alloc] initWithTitle:@"Disconnection!"
                                                                     message:[NSString stringWithFormat:@"WiiMote '%@' disconnection detected.\nIs battery drainned?",[NSNumber numberWithInt:(unid+1)]] 
                                                                     delegate:nil cancelButtonTitle:@"Dismiss" otherButtonTitles: nil];
                                               [alert show];                                           
                                                                                      
                                               [alert release];
	                                        }
                                                                                                                                                                                                                                                 
                                        }
                                        break;                                        

                                default:
                                        break;
                        }
                        break;
                }
                default:
                        break;
        }
}

@implementation WiiMoteHelper
 
+(void) startwiimote:(UIViewController *)controller{

  if(!initLoop)
  {
     run_loop_init(RUN_LOOP_COCOA);
     initLoop = true;
  }   
  if(!btOK )
  {
	  if (bt_open() ){
	      // Alert user?
	  } else {
	      bt_register_packet_handler(packet_handler);
	      btOK = true;
	  }
  }
  
  if (btOK) 
  {
    // create inq controller
    if(inqViewControl==nil)
    {
      inqViewControl = [[BTInquiryViewController alloc] init];
    
      struct CGRect rect = controller.view.frame;

	  CGFloat navBarWidht =  rect.size.width;
	  CGFloat navBarHeight = 45;
        
	  UINavigationBar *navBar = [ [ UINavigationBar alloc ] initWithFrame: CGRectMake(0, 0, navBarWidht , navBarHeight)];
	  [navBar autorelease];
	  [navBar setDelegate: inqViewControl ];
	  
	  UIButton *button = [UIButton buttonWithType:UIButtonTypeRoundedRect];
      [button setFrame:CGRectMake(rect.size.width-70,5,60,35)];
      [button setTitle:@"Done" forState:UIControlStateNormal];
      button.autoresizingMask = UIViewAutoresizingFlexibleLeftMargin;
      [button addTarget:self action:@selector(cancelWiiMoteSearch) forControlEvents:UIControlEventTouchUpInside];
      
      [navBar addSubview:button];
      	   	   
      UILabel *navLabel = [[UILabel alloc] initWithFrame:CGRectMake(40,0,300, navBarHeight)];
	  navLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth;
	  navLabel.text = @"WiiMote Sync";
	  navLabel.backgroundColor = [UIColor clearColor];
	  navLabel.textColor = [UIColor blackColor];
	  navLabel.font = [UIFont systemFontOfSize: 18];
	  navLabel.textAlignment = UITextAlignmentLeft;
	  [navBar addSubview:navLabel];
	  [navLabel release];
	   	   
	  [[inqViewControl tableView] setTableHeaderView:navBar];
	  [navBar release];
	    	    
    }
    
    if(!activated)
    {

	    
	        UIAlertView* alertView=[[UIAlertView alloc] initWithTitle:nil
	                     message:@"are you sure you to activate BTstack?"
					     delegate:self cancelButtonTitle:nil
	                     otherButtonTitles:@"Yes",@"No",nil];
	                      
	        [alertView show];
            [alertView release];
    }
       
    [controller presentModalViewController:inqViewControl animated:YES];
    
  }

}

+ (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{

  if(buttonIndex == 0 )
  {
     [inqViewControl setDelegate:self];
     [inqViewControl setAllowSelection:true];
     activated = true;
     [inqViewControl startInquiry];
  }
  else
  {
     [inqViewControl dismissModalViewControllerAnimated:YES];
  }
}

+(void) cancelWiiMoteSearch {        
    [inqViewControl stopInquiry];
    [inqViewControl dismissModalViewControllerAnimated:YES];
}


+(void) deviceChoosen:(BTInquiryViewController *) inqView device:(BTDevice*) deviceChoosen;
{
        NSLog(@"deviceChoosen %@", [device toString]);
}

+ (void) deviceDetected:(BTInquiryViewController *) inqView device:(BTDevice*) selectedDevice {

        NSLog(@"deviceDetected %@", [device toString]);
        if ([selectedDevice name] && [[selectedDevice name] caseInsensitiveCompare:@"Nintendo RVL-CNT-01"] == NSOrderedSame){
                NSLog(@"WiiMote found with address %@", [BTDevice stringForAddress:[selectedDevice address]]);
                device = selectedDevice;
                
         
                [inqViewControl stopInquiry];
               
                [inqViewControl showConnecting:device];

                // connect to device
                [device setConnectionState:kBluetoothConnectionConnecting];
                [[inqViewControl tableView] reloadData];
                bt_send_cmd(&l2cap_create_channel, [device address], 0x13);
                
           
        }
}

+ (void) inquiryStopped{
}

+ (void) disconnectDevice:(BTInquiryViewController *) inqView device:(BTDevice*) selectedDevice {
}

+ (void)endwiimote {

    if(btOK)
    {
		int i=0;
		while(i!=myosd_num_of_joys){
			[inqViewControl removeDeviceForAddress:&joys[i].addr];
			i++;
		}
			
		myosd_num_of_joys=0;
	    bt_send_cmd(&btstack_set_power_mode, HCI_POWER_OFF );
	    bt_close();
	    activated= false;
		btOK = false;	
    }
}

@end

