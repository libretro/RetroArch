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

#include "btdynamic.h"

#import "BTstackManager.h"

#import "btstack/btstack.h"
#import "BTDevice.h"

#define INQUIRY_INTERVAL 3

static BTstackManager * btstackManager = nil;

@interface BTstackManager (privat)
- (void)handlePacketWithType:(uint8_t) packet_type forChannel:(uint16_t) channel andData:(uint8_t *)packet withLen:(uint16_t) size;
@end

// needed for libBTstack
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
	[btstackManager handlePacketWithType:packet_type forChannel:channel andData:packet withLen:size];
}

@implementation BTstackManager

@synthesize delegate = _delegate;
@synthesize listeners;
@synthesize discoveredDevices;

-(BTstackManager *) init {
	self = [super init];
	if (!self) return self;
	
	state = kDeactivated;
	discoveryState = kInactive;
	connectedToDaemon = NO;
	
	// device discovery
	[self setDiscoveredDevices: [[NSMutableArray alloc] init]];
	
	// delegate and listener
	_delegate = nil;
	[self setListeners:[[NSMutableSet alloc] init]];
	
	// Use Cocoa run loop
	run_loop_init_ptr(RUN_LOOP_COCOA);
	
	// our packet handler
	bt_register_packet_handler_ptr(packet_handler);
	
	return self;
}

+(BTstackManager *) sharedInstance {
	if (!btstackManager) {
		btstackManager = [[BTstackManager alloc] init];
	}
	return btstackManager;
}

// listeners
-(void) addListener:(id<BTstackManagerListener>)listener{
	[listeners addObject:listener];
}

-(void) removeListener:(id<BTstackManagerListener>)listener{
	[listeners removeObject:listener];
}

// send events
-(void) sendActivated {
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(activatedBTstackManager:)]){
			[listener activatedBTstackManager:self];
		}
	}
}
-(void) sendActivationFailed:(BTstackError)error {
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(btstackManager:activationFailed:)]){
			[listener btstackManager:self activationFailed:error];
		}
	}
}
-(void) sendDeactivated {
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(deactivatedBTstackManager:)]){
			[listener deactivatedBTstackManager:self];
		}
	}
}
-(void) sendSleepEnter {
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(sleepModeEnterBTstackManager:)]){
			[listener sleepModeEnterBTstackManager:self];
		}
	}
}
-(void) sendSleepExit {
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(sleepModeExtitBTstackManager:)]){
			[listener sleepModeExtitBTstackManager:self];
		}
	}
}
-(void) sendDiscoveryStoppedEvent {
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(discoveryStoppedBTstackManager:)]){
			[listener discoveryStoppedBTstackManager:self];
		}
	}
}
-(void) sendDiscoveryInquiry{
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(discoveryInquiryBTstackManager:)]){
			[listener discoveryInquiryBTstackManager:self];
		}
	}
}
-(void) sendDiscoveryQueryRemoteName:(int)index {
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(btstackManager:discoveryQueryRemoteName:)]){
			[listener btstackManager:self discoveryQueryRemoteName:index];
		}
	}
}
-(void) sendDeviceInfo:(BTDevice*) device{
	for (NSObject<BTstackManagerListener>* listener in listeners) {
		if ([listener respondsToSelector:@selector(btstackManager:deviceInfo:)]){
			[listener btstackManager:self deviceInfo:device];
		}
	}
}


// Activation
-(BTstackError) activate {
	
	BTstackError err = 0;
	if (!connectedToDaemon) {
		err = bt_open_ptr();
		if (err) return BTSTACK_CONNECTION_TO_BTDAEMON_FAILED;
	}
	connectedToDaemon = YES;
	
	// check system BT
	state = kW4SysBTState;
	bt_send_cmd_ptr(btstack_get_system_bluetooth_enabled_ptr);
	
	return err;
}

-(BTstackError) deactivate {
	if (!connectedToDaemon) return BTSTACK_CONNECTION_TO_BTDAEMON_FAILED;
	state = kW4Deactivated;
	bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_OFF);
	return 0;
}

-(BOOL) isActive {
	return state == kActivated;
}

-(BOOL) isActivating {
	switch (state){
		case kW4SysBTState:
		case kW4SysBTDisabled:
		case kW4Activated:
			return YES;
		default:
			return NO;
	}
}
-(BOOL) isDiscoveryActive {
	return state == kActivated && (discoveryState != kInactive);
}

// Discovery
-(BTstackError) startDiscovery {
	if (state < kActivated) return BTSTACK_NOT_ACTIVATED;
	
	discoveryState = kW4InquiryMode;
	bt_send_cmd_ptr(hci_write_inquiry_mode_ptr, 0x01); // with RSSI
	return 0;
};

-(BTstackError) stopDiscovery{
	if (state < kActivated) return BTSTACK_NOT_ACTIVATED;
	switch (discoveryState){
		case kInactive:
			[self sendDiscoveryStoppedEvent];
			break;
		case kW4InquiryMode:
			discoveryState = kW4InquiryModeBeforeStop;
			break;
		case kInquiry:
			discoveryState = kW4InquiryStop;
			bt_send_cmd_ptr(hci_inquiry_cancel_ptr);
			break;
		case kRemoteName: {
			discoveryState = kW4RemoteNameBeforeStop;
			BTDevice *device = [discoveredDevices objectAtIndex:discoveryDeviceIndex];
			bt_send_cmd_ptr(hci_remote_name_request_cancel_ptr, [device address]);
			break;
		}
		default:
			NSLog(@"[BTstackManager stopDiscovery] invalid discoveryState %u", discoveryState);
			[self sendDiscoveryStoppedEvent];
			break;
	}
	return 0;
};

-(int) numberOfDevicesFound{
	return [discoveredDevices count];
};

-(BTDevice*) deviceAtIndex:(int)index{
	return (BTDevice*) [discoveredDevices objectAtIndex:index];
};

-(BTDevice*) deviceForAddress:(bd_addr_t*) address{
	for (BTDevice *device in discoveredDevices){
		// NSLog(@"compare %@ to %@", [BTDevice stringForAddress:address], [device addressString]); 
		if ( BD_ADDR_CMP(address, [device address]) == 0){
			return device;
		}
	}
	return nil;
}

- (void) activationHandleEvent:(uint8_t *)packet withLen:(uint16_t) size {
	switch (state) {
						
		case kW4SysBTState:
		case kW4SysBTDisabled:
			
			// BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED
			if ( packet[0] == BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED){
				if (packet[2]){
					// system bt on - first time try to disable it
					if ( state == kW4SysBTState) {
						if (_delegate == nil
							|| ![_delegate respondsToSelector:@selector(disableSystemBluetoothBTstackManager:)]
							|| [_delegate disableSystemBluetoothBTstackManager:self]){
							state = kW4SysBTDisabled;
							bt_send_cmd_ptr(btstack_set_system_bluetooth_enabled_ptr, 0);
						} else {
							state = kDeactivated;
							[self sendActivationFailed:BTSTACK_ACTIVATION_FAILED_SYSTEM_BLUETOOTH];
						}
					} else {
						state = kDeactivated;
						[self sendActivationFailed:BTSTACK_ACTIVATION_FAILED_UNKNOWN];
					}
				} else {
					state = kW4Activated;
					bt_send_cmd_ptr(btstack_set_power_mode_ptr, HCI_POWER_ON);
				}
			}
			break;
			
		case kW4Activated:
			switch (packet[0]){
				case BTSTACK_EVENT_STATE:
					if (packet[2] == HCI_STATE_WORKING) {
						state = kActivated;
						[self sendActivated];
					}
					break;
				case BTSTACK_EVENT_POWERON_FAILED:
					state = kDeactivated;
					[self sendActivationFailed:BTSTACK_ACTIVATION_POWERON_FAILED];
					break;
					
				default:
					break;
			}
			break;
			
		case kW4Deactivated:
			if (packet[0] != BTSTACK_EVENT_STATE && packet[2] == HCI_STATE_OFF){
				state = kDeactivated;
				[self sendDeactivated];
			}
			break;
		
		case kActivated:
			if (packet[0] != BTSTACK_EVENT_STATE && packet[2] == HCI_STATE_FALLING_ASLEEP){
				state = kSleeping;
				[self sendSleepEnter];
			}
			break;
			
		case kSleeping:
			if (packet[0] != BTSTACK_EVENT_STATE && packet[2] == HCI_STATE_WORKING){
				state = kActivated;
				[self sendSleepExit];
			}
			break;
			
		default:
			break;
	}
}

-(void) discoveryRemoteName{
	BOOL found = NO;
	while ( discoveryDeviceIndex < [discoveredDevices count]){
		BTDevice *device = [discoveredDevices objectAtIndex:discoveryDeviceIndex];
		if (device.name) {
			discoveryDeviceIndex ++;
			continue;
		}
		bt_send_cmd_ptr(hci_remote_name_request_ptr, [device address], device.pageScanRepetitionMode,
					0, device.clockOffset | 0x8000);
		[self sendDiscoveryQueryRemoteName:discoveryDeviceIndex];
		found = YES;
		break;
	}
	if (!found) {
		// printf("Queried all devices, restart.\n");
		discoveryState = kInquiry;
		bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
		[self sendDiscoveryInquiry];
	}
}

- (NSString *) createRemoteNameFromRemoteNameEvent:(uint8_t *) packet {
    // get lenght: first null byte or max 248 chars
    int nameLen = 0;
    while (nameLen < 248 && packet[9+nameLen]) nameLen++;
    // Bluetooth specification mandates UTF-8 encoding...
    NSString *name = [[NSString alloc] initWithBytes:&packet[9] length:nameLen encoding:NSUTF8StringEncoding];
    // but fallback to latin-1 for non-standard products like old Microsoft Wireless Presenter 
    if (!name){
        name = [[NSString alloc] initWithBytes:&packet[9] length:nameLen encoding:NSISOLatin1StringEncoding];
    }
    return name;
}

- (void) handleRemoteNameCached: (uint8_t *) packet {
	bd_addr_t addr;
	bt_flip_addr_ptr(addr, &packet[3]);
	// NSLog(@"Get remote name done for %@", [BTDevice stringForAddress:&addr]);
	BTDevice* device = [self deviceForAddress:&addr];
    if (!device) return;

    [device setName:[self createRemoteNameFromRemoteNameEvent:packet]];
    [self sendDeviceInfo:device];
}

- (void) handleRemoteName: (uint8_t *) packet {
	bd_addr_t addr;
	bt_flip_addr_ptr(addr, &packet[3]);
	// NSLog(@"Get remote name done for %@", [BTDevice stringForAddress:&addr]);
	BTDevice* device = [self deviceForAddress:&addr];
    if (!device) return;

    [device setName:[self createRemoteNameFromRemoteNameEvent:packet]];
    [self sendDeviceInfo:device];
    
    discoveryDeviceIndex++;
    [self discoveryRemoteName];
}

-(void) discoveryHandleEvent:(uint8_t *)packet withLen:(uint16_t) size {
	bd_addr_t addr;
	int i;
	int numResponses;
	
	switch (discoveryState) {
			
		case kInactive:
			break;
			
		case kW4InquiryMode:
			if (packet[0] == HCI_EVENT_COMMAND_COMPLETE && COMMAND_COMPLETE_EVENT(packet, (*hci_write_inquiry_mode_ptr)) ) {
				discoveryState = kInquiry;
				bt_send_cmd_ptr(hci_inquiry_ptr, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
				[self sendDiscoveryInquiry];
			}
			break;
		
		case kInquiry:
			
			switch (packet[0]){
				case HCI_EVENT_INQUIRY_RESULT:
					numResponses = packet[2];
					for (i=0; i<numResponses ; i++){
						bt_flip_addr_ptr(addr, &packet[3+i*6]);
						// NSLog(@"found %@", [BTDevice stringForAddress:&addr]);
						BTDevice* device = [self deviceForAddress:&addr];
						if (!device) {
							device = [[BTDevice alloc] init];
							[discoveredDevices addObject:device];
							[device setAddress:&addr];
						}
						// update
						device.pageScanRepetitionMode =   packet [3 + numResponses*(6)         + i*1];
						device.classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1+1)   + i*3);
						device.clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+1+3) + i*2) & 0x7fff;
						device.rssi  = 0;
						[self sendDeviceInfo:device];
					}
					break;
					
				case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
					numResponses = packet[2];
					for (i=0; i<numResponses ;i++){
						bt_flip_addr_ptr(addr, &packet[3+i*6]);
						// NSLog(@"found %@", [BTDevice stringForAddress:&addr]);
						BTDevice* device = [self deviceForAddress:&addr];
						if (!device) {
							device = [[BTDevice alloc] init];
							[discoveredDevices addObject:device];
							[device setAddress:&addr];
						}
						device.pageScanRepetitionMode =   packet [3 + numResponses*(6)         + i*1];
						device.classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1)     + i*3);
						device.clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+3)   + i*2) & 0x7fff;
						device.rssi  =                    packet [3 + numResponses*(6+1+1+3+2) + i*1];
						[self sendDeviceInfo:device];
					}
					break;

				case BTSTACK_EVENT_REMOTE_NAME_CACHED:
                    [self handleRemoteNameCached:packet];
					break;
					
				case HCI_EVENT_INQUIRY_COMPLETE:
					// printf("Inquiry scan done.\n");
					discoveryState = kRemoteName;
					discoveryDeviceIndex = 0;
					[self discoveryRemoteName];
					break;
			}
			break;
			
		case kRemoteName:
			if (packet[0] == HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE){
				[self handleRemoteName:packet];
			}
			break;

		case kW4InquiryModeBeforeStop:
			if (packet[0] == HCI_EVENT_COMMAND_COMPLETE && COMMAND_COMPLETE_EVENT(packet, (*hci_write_inquiry_mode_ptr)) ) {
				discoveryState = kInactive;
				[self sendDiscoveryStoppedEvent];
			}
			break;
			
		case kW4InquiryStop:
			if (packet[0] == HCI_EVENT_INQUIRY_COMPLETE
			||	COMMAND_COMPLETE_EVENT(packet, (*hci_inquiry_cancel_ptr))) {
				discoveryState = kInactive;
				[self sendDiscoveryStoppedEvent];
			}
			break;
			
		case kW4RemoteNameBeforeStop:
			if (packet[0] == HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE
			||  COMMAND_COMPLETE_EVENT(packet, (*hci_remote_name_request_cancel_ptr))){
				discoveryState = kInactive;
				[self sendDiscoveryStoppedEvent];
			}
			break;
			
		default:
			break;
	}
}

-(void) dropLinkKeyForAddress:(bd_addr_t*) address {
    bt_send_cmd_ptr(hci_delete_stored_link_key_ptr, address, 0);
	// NSLog(@"Removing link key for %@", devAddress);
}

-(void) handlePacketWithType:(uint8_t)packet_type forChannel:(uint16_t)channel andData:(uint8_t *)packet withLen:(uint16_t) size {
	switch (state) {
			
		case kDeactivated:
			break;
		
		// Activation
		case kW4SysBTState:
		case kW4SysBTDisabled:
		case kW4Activated:
		case kW4Deactivated:
		case kSleeping:
			if (packet_type != HCI_EVENT_PACKET) break;
			[self activationHandleEvent:packet withLen:size];
			break;
		
		// Pairing + Discovery
		case kActivated:
			if (packet_type != HCI_EVENT_PACKET) break;
			switch (packet[0]){
				case BTSTACK_EVENT_STATE:
					[self activationHandleEvent:packet withLen:size];
					break;
				default:
					break;
			}
			[self discoveryHandleEvent:packet withLen:size];
			break;
		
		default:
			break;
	}
	if ([_delegate respondsToSelector:@selector(btstackManager:handlePacketWithType:forChannel:andData:withLen:)]){
		[_delegate btstackManager:self handlePacketWithType:packet_type forChannel:channel andData:packet withLen:size];
	}
}

// Connections
-(BTstackError) createL2CAPChannelAtAddress:(bd_addr_t*) address withPSM:(uint16_t)psm authenticated:(BOOL)authentication {
	if (state < kActivated) return BTSTACK_NOT_ACTIVATED;
	if (state != kActivated) return BTSTACK_BUSY;
#if 0
	// ...f (state 
	// store params
	connType = 0;
	BD_ADDR_COPY(&connAddr, address);
	connPSM = psm;
	connAuth = authentication;
	
	// send write authentication enabled
	bt_send_cmd_ptr(&hci_write_authentication_enable, authentication);	
	state = kW4AuthenticationEnableCommand;
#endif
	return 0;
};
-(BTstackError) sendL2CAPPacketForChannelID:(uint16_t)channelID {
	if (state < kActivated) return BTSTACK_NOT_ACTIVATED;
	return 0;
};
-(BTstackError) closeL2CAPChannelWithID:(uint16_t) channelID {
	if (state < kActivated) return BTSTACK_NOT_ACTIVATED;
	return 0;
};

-(BTstackError) createRFCOMMConnectionAtAddress:(bd_addr_t*) address withChannel:(uint16_t)channel authenticated:(BOOL)authentication {
	if (state < kActivated) return BTSTACK_NOT_ACTIVATED;
	if (state != kActivated) return BTSTACK_BUSY;
#if 0
	// store params
	connType = 1;
	BD_ADDR_COPY(&connAddr, address);
	connChan = channel;
	connAuth = authentication;
#endif
	return 0;
};
-(BTstackError) sendRFCOMMPacketForChannelID:(uint16_t)connectionID {
	if (state < kActivated) return BTSTACK_NOT_ACTIVATED;
	return 0;
};
-(BTstackError) closeRFCOMMConnectionWithID:(uint16_t) connectionID {
	if (state <kActivated) return BTSTACK_NOT_ACTIVATED;
	return 0;
};

@end
