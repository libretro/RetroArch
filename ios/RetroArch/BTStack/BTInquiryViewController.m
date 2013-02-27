/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

//
//  BTInquiryViewController.m
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import "BTInquiryViewController.h"
#import "BTDevice.h"
#import <UIKit/UIToolbar.h>

#include "btstack/btstack.h"
//#include "wiimote.h"

#define INQUIRY_INTERVAL 3

unsigned myosd_num_of_joys = 0;

static BTInquiryViewController *inqView; 
static btstack_packet_handler_t clientHandler;
static uint8_t remoteNameIndex;

@interface BTInquiryViewController (Private) 
- (void) handlePacket:(uint8_t) packet_type channel:(uint16_t) channel packet:(uint8_t*) packet size:(uint16_t) size;
- (BTDevice *) getDeviceForAddress:(bd_addr_t *)addr;
- (void) getNextRemoteName;
- (void) startInquiry;
@end

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	if (inqView) {
		[inqView handlePacket:packet_type channel:channel packet:packet size:size];
	}
}

@implementation BTInquiryViewController


@synthesize devices;
@synthesize delegate;
@synthesize allowSelection;
@synthesize showIcons;

- (id) init {
	self = [super initWithStyle:UITableViewStyleGrouped];
	bluetoothState = HCI_STATE_OFF;
	inquiryState = kInquiryInactive;
	allowSelection = false;
	showIcons = false;
	remoteDevice = nil;
	restartInquiry = true;
	notifyDelegateOnInquiryStopped = false;
	
	macAddressFont = [UIFont fontWithName:@"Courier New" size:[UIFont labelFontSize]];
	deviceNameFont = [UIFont boldSystemFontOfSize:[UIFont labelFontSize]];
	
	deviceActivity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
	[deviceActivity startAnimating];
	bluetoothActivity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
	[bluetoothActivity startAnimating];
	
	devices = [[NSMutableArray alloc] init];
	inqView = self;
	return self;
}

- (void) myStartInquiry{
	if (inquiryState != kInquiryInactive) {
		NSLog(@"Inquiry already active");
		return;
	}
	NSLog(@"Inquiry started");

	stopRemoteNameGathering = false;
	restartInquiry = true;

	inquiryState = kInquiryActive;
	[[self tableView] reloadData];
	
	bt_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
}

- (void) handlePacket:(uint8_t) packet_type channel:(uint16_t) channel packet:(uint8_t*) packet size:(uint16_t) size {
	bd_addr_t event_addr;
	switch (packet_type) {
			
		case HCI_EVENT_PACKET:
			
			switch (packet[0]){
					
				case BTSTACK_EVENT_STATE: 
				{
					// bt stack activated
					bluetoothState = (HCI_STATE)packet[2];
					[[self tableView] reloadData];
					
					// set BT state
/*					
					if (bluetoothState == HCI_STATE_WORKING) {
						[self myStartInquiry];
					}
*/					
					break;
				}
				case BTSTACK_EVENT_POWERON_FAILED:
				{
					bluetoothState = HCI_STATE_OFF;
					[[self tableView] reloadData];
					
					UIAlertView* alertView = [[UIAlertView alloc] init];
					alertView.title = @"Bluetooth not accessible!";
					alertView.message = @"Hardware initialization failed!\n"
					"Make sure you have turned off Bluetooth in the System Settings.";
					NSLog(@"Alert: %@ - %@", alertView.title, alertView.message);
					[alertView addButtonWithTitle:@"Dismiss"];
					[alertView show];
					break;
				}	
				case HCI_EVENT_INQUIRY_RESULT:
				case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
				{
					int numResponses = packet[2];
					int i;
					for (i=0; i<numResponses;i++){
						bd_addr_t addr;
						bt_flip_addr(addr, &packet[3+i*6]);
						if ([inqView getDeviceForAddress:&addr]) {
							NSLog(@"Device %@ already in list", [BTDevice stringForAddress:&addr]);
							continue;
						}
						BTDevice *dev = [[BTDevice alloc] init];
						[dev setAddress:&addr];
						[dev setPageScanRepetitionMode:packet[3 + numResponses*6 + i]];
						[dev setClassOfDevice:READ_BT_24(packet, 3 + numResponses*(6+1+1+1) + i*3)];
						[dev setClockOffset:(READ_BT_16(packet, 3 + numResponses*(6+1+1+1+3) + i*2) & 0x7fff)];
						// hexdump(packet, size);
						NSLog(@"--> adding %@", [dev toString] );
						[devices addObject:dev];
						if (delegate) {						   
							[delegate deviceDetected:self device:dev];
						}
					}
				  
					[[inqView tableView] reloadData];
					NSLog(@"bye" );
					break;
				}	
				case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
				{
					bt_flip_addr(event_addr, &packet[3]);
					BTDevice *dev = [inqView getDeviceForAddress:&event_addr];
					if (!dev) break;
					[dev setConnectionState:kBluetoothConnectionNotConnected];
					if (packet[2] == 0) {
						[dev setName:[NSString stringWithUTF8String:(const char *) &packet[9]]];
						if (delegate) {
							[delegate deviceDetected:self device:dev];
						}
					}
					[[self tableView] reloadData];
					remoteNameIndex++;
					[self getNextRemoteName];
					break;
				}							
				case HCI_EVENT_COMMAND_COMPLETE:
				{
					if (COMMAND_COMPLETE_EVENT(packet, hci_inquiry_cancel)){
						// inquiry canceled
						NSLog(@"Inquiry cancelled successfully");
						inquiryState = kInquiryInactive;
						[[self tableView] reloadData];
						if (notifyDelegateOnInquiryStopped){
							notifyDelegateOnInquiryStopped = false;
							if (delegate) {
								[delegate inquiryStopped];
							}
						}
					}
					if (COMMAND_COMPLETE_EVENT(packet, hci_remote_name_request_cancel)){
						// inquiry canceled
						NSLog(@"Remote name request cancelled successfully");
						inquiryState = kInquiryInactive;
						[[self tableView] reloadData];
						if (notifyDelegateOnInquiryStopped){
							notifyDelegateOnInquiryStopped = false;
							if (delegate) {
								[delegate inquiryStopped];
							}
						}
					}
					
					break;
				}	
				case HCI_EVENT_INQUIRY_COMPLETE:
				{
					NSLog(@"Inquiry complete");
					// reset name check
					remoteNameIndex = 0;
					[self getNextRemoteName];
					break;
				}	
				default:
					break;
					
					// hexdump(packet, size);
					//break;
			}
			
		default:
			break;
	}
	// forward to client app
	(*clientHandler)(packet_type, channel, packet, size);
}

- (BTDevice *) getDeviceForAddress:(bd_addr_t *)addr {
	uint8_t j;
	for (j=0; j<[devices count]; j++){
		BTDevice *dev = [devices objectAtIndex:j];
		if (BD_ADDR_CMP(addr, [dev address]) == 0){
			return dev;
		}
	}
	return nil;
}


- (void) removeDeviceForAddress:(bd_addr_t *)addr {
	uint8_t j;
	for (j=0; j<[devices count]; j++){
		BTDevice *dev = [devices objectAtIndex:j];
		if (BD_ADDR_CMP(addr, [dev address]) == 0){
		    NSLog(@"--> removed %@", [dev toString] );
			[devices removeObject:dev];
			[[self tableView] reloadData];
			return;
		}
	}
	
}

- (void) getNextRemoteName{
	
	// stopped?
	if (stopRemoteNameGathering) {
		inquiryState = kInquiryInactive;
		[[self tableView] reloadData];
		
		if (notifyDelegateOnInquiryStopped){
			notifyDelegateOnInquiryStopped = false;
			if (delegate) {
				[delegate inquiryStopped];
			}
		}
		return;
	}
	
	remoteNameDevice = nil;
		
	for (remoteNameIndex = 0; remoteNameIndex < [devices count]; remoteNameIndex++){
		BTDevice *dev = [devices objectAtIndex:remoteNameIndex];
		if (![dev name]){
			remoteNameDevice = dev;
			break;
		}
	}
	if (remoteNameDevice) {
		inquiryState = kInquiryRemoteName;
		[remoteNameDevice setConnectionState:kBluetoothConnectionRemoteName];
		bt_send_cmd(&hci_remote_name_request, [remoteNameDevice address], [remoteNameDevice pageScanRepetitionMode], 0, [remoteNameDevice clockOffset] | 0x8000);
	} else  {
		inquiryState = kInquiryInactive;
		// inquiry done.
		if (restartInquiry) {
			[self myStartInquiry];
		}
	}
	[[self tableView] reloadData];
}

- (void) startInquiry {
   //static int b = 0;
   //if(!b)
   //{
	 //b=1;
	// put into loop

	// @TODO: cannot be called a second time!
	clientHandler = bt_register_packet_handler(packet_handler);

	bluetoothState = HCI_STATE_INITIALIZING;
	[[self tableView] reloadData];

	stopRemoteNameGathering = false;
	restartInquiry = true;
	
	bt_send_cmd(&btstack_set_power_mode, HCI_POWER_ON );
	//}
}

- (void) stopInquiry {
	
	NSLog(@"stop inquiry called, state %u", inquiryState);
	restartInquiry = false;
	stopRemoteNameGathering = true;
	bool immediateNotify = true;
	
	switch (inquiryState) {
		case kInquiryActive:
			// just stop inquiry 
			immediateNotify = false;
			bt_send_cmd(&hci_inquiry_cancel);
			break;
		case kInquiryInactive:
			NSLog(@"stop inquiry called although inquiry inactive?");
			break;
		case kInquiryRemoteName:
			if (remoteNameDevice) {
				// just stop remote name request 
				immediateNotify = false;
				bt_send_cmd(&hci_remote_name_request_cancel, [remoteNameDevice address]);
			}
			break;
		default:
			break;
	}
	if (immediateNotify && delegate){
		[delegate inquiryStopped];
	} else {
		notifyDelegateOnInquiryStopped = true;
	}
}

- (void) showConnecting:(BTDevice *) device {
	remoteDevice = device;
	[[self tableView] reloadData];
}

- (void) showConnected:(BTDevice *) device {
	connectedDevice = device;
	[[self tableView] reloadData];
}

/*
- (void)loadView {
    
    [super loadView];
    
}
*/
/*
- (void)viewDidLoad {
    [super viewDidLoad];
    
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    //self.navigationItem.rightBarButtonItem = self.editButtonItem;
    
}
*/

/*
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
}
*/
/*
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
}
*/
/*
- (void)viewWillDisappear:(BOOL)animated {
	[super viewWillDisappear:animated];
}
*/
/*
- (void)viewDidDisappear:(BOOL)animated {
	[super viewDidDisappear:animated];
}
*/

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
	return YES;
	//return NO;
	//return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (void)didReceiveMemoryWarning {
	// Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
	
	// Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload {
	// Release any retained subviews of the main view.
	// e.g. self.myOutlet = nil;
}

- (void)dealloc {
	// unregister self
	bt_register_packet_handler(clientHandler);
	// done
    [super dealloc];
}


#pragma mark Table view methods

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section{
	return @"Devices";
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}


// Customize the number of rows in the table view.
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
	int rows = 1;  // 1 for status line 
	if (bluetoothState == HCI_STATE_WORKING) {
		rows += [devices count];
	}
	return rows;
}


// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
        cell = [[[UITableViewCell alloc] initWithStyle:/* UITableViewCellStyleDefault = */(UITableViewCellStyle)0 reuseIdentifier:CellIdentifier] autorelease];
		// cell.selectionStyle = UITableViewCellSelectionStyleNone;
    }
    
    // Set up the cell...
	NSString *label = nil;
	int idx = [indexPath indexAtPosition:1];
	if (bluetoothState != HCI_STATE_WORKING || idx >= [devices count]) {
		if (bluetoothState == HCI_STATE_INITIALIZING){
			label = @"Activating BTstack...";
			cell.accessoryView = bluetoothActivity;
		} else if (bluetoothState == HCI_STATE_OFF){
			label = @"Bluetooth not accessible!";
			cell.accessoryView = nil;
		} else {
			if (connectedDevice) {
				label = @"Disconnect";
				cell.accessoryView = nil;
			} else if (remoteDevice) {
				label = @"Connecting...";
				cell.accessoryView = bluetoothActivity;
			} else {
				switch (inquiryState){
					case kInquiryInactive:
					    if (myosd_num_of_joys==4)
					    {
					       label = @"Maximun devices connected!";
					    }
						else if ([devices count] > 0){
							label = @"Press here to find more devices...";
						} else {
							label = @"Press here to find first device...";
						}
						cell.accessoryView = nil;
						break;
					case kInquiryActive:
						//label = @"Searching...";
						label = @"Press 1 and 2 on the WiiMote to sync";
						cell.accessoryView = bluetoothActivity;
						break;
					case kInquiryRemoteName:
						label = @"Query device names...";
						cell.accessoryView = bluetoothActivity;
						break;
				}
			}
		}
	} else {
		BTDevice *dev = [devices objectAtIndex:idx];
		label = [dev nameOrAddress];
		if ([dev name]){
			cell.font = deviceNameFont;
		} else {
			cell.font = macAddressFont;
		}
		// pick an icon for the devices
		if (showIcons) {
			int major = ([dev classOfDevice] & 0x1f00) >> 8;
			if (major == 0x01) {
				cell.image = [UIImage imageNamed:@"computer.png"];
			} else if (major == 0x02) {
				cell.image = [UIImage imageNamed:@"smartphone.png"];
			} else if ( major == 0x05 && ([dev classOfDevice] & 0xff) == 0x40){ 
				cell.image = [UIImage imageNamed:@"keyboard.png"];
			} else {
				cell.image = [UIImage imageNamed:@"bluetooth.png"];
			}
		}
		switch ([dev connectionState]) {
			case kBluetoothConnectionNotConnected:
			case kBluetoothConnectionConnected:
				cell.accessoryView = nil;
				break;
			case kBluetoothConnectionConnecting:
			case kBluetoothConnectionRemoteName:
				cell.accessoryView = deviceActivity;
				break;
		}
	}
	cell.text = label;
    return cell;
}


- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	NSLog(@"didSelectRowAtIndexPath %@", indexPath);
    // Navigation logic may go here. Create and push another view controller.
	// AnotherViewController *anotherViewController = [[AnotherViewController alloc] initWithNibName:@"AnotherView" bundle:nil];
	// [self.navigationController pushViewController:anotherViewController];
	// [anotherViewController release];
	
	// valid selection?
	
	
	int idx = [indexPath indexAtPosition:1];
	//printf("sleccion %d\n",idx);
	if (bluetoothState == HCI_STATE_WORKING) {
		if (delegate) {
			if (idx < [devices count]){
				[delegate deviceChoosen:self device:[devices objectAtIndex:idx]];
			} else if (idx == [devices count]) {
			    //printf("seleccionado %d %d\n",idx,connectedDevice);
				if (connectedDevice) {
					// DISCONNECT button 
					[delegate disconnectDevice:self device:connectedDevice];
				} else if (myosd_num_of_joys<4){
					// Find more devices
					[self myStartInquiry];
				}
			}
		}
	} else {
		[tableView deselectRowAtIndexPath:indexPath animated:TRUE];
	}
	
}

- (NSIndexPath *)tableView:(UITableView *)tableView willSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	if (allowSelection) {
		return indexPath;
	}
	return nil;
}

@end

