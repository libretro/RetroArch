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

#pragma once

#import <Foundation/Foundation.h>
#import <stdint.h>
#import "btstack/btstack.h"

#define PREFS_REMOTE_NAME  @"RemoteName"
#define PREFS_LINK_KEY     @"LinkKey"
#define BTstackManagerID   @"ch.ringwald.btstack"

@class BTDevice;

/*
 * Information on devices is stored in a system-wide plist
 * it is maintained by BTstackManager
 * this includes the link keys
 */

// TODO enumerate BTstackError type
typedef int BTstackError;

typedef enum {
	kDeactivated = 1,
	kW4SysBTState,
	kW4SysBTDisabled,
	kW4Activated,
	kActivated,
	kW4Deactivated,
	kSleeping,
#if 0
	kW4DisoveryStopped,
	kW4AuthenticationEnableCommand
#endif
} ManagerState;

typedef enum {
	kInactive = 1,
	kW4InquiryMode,
	kInquiry,
	kRemoteName,
	// stopping
	kW4InquiryModeBeforeStop,
	kW4InquiryStop,
	kW4RemoteNameBeforeStop,
} DiscoveryState;

@protocol BTstackManagerDelegate;
@protocol BTstackManagerListener;

@interface BTstackManager : NSObject {
@private
	NSObject<BTstackManagerDelegate>* _delegate;
	NSMutableArray *discoveredDevices;
	NSMutableSet *listeners;
	BOOL connectedToDaemon;
	ManagerState state;
	DiscoveryState discoveryState;
	int discoveryDeviceIndex;
#if 0
	// current connection - kind a ugly
	uint8_t   connType; // 0 = L2CAP, 1 = RFCOMM
	bd_addr_t connAddr;
	uint16_t  connPSM;
	uint16_t  connChan;
	uint8_t   connAuth;
#endif
}

// shared instance
+(BTstackManager *) sharedInstance;

// listeners
-(void) addListener:(id<BTstackManagerListener>)listener;
-(void) removeListener:(id<BTstackManagerListener>)listener;

// Activation
-(BTstackError) activate;
-(BTstackError) deactivate;
-(BOOL) isActivating;
-(BOOL) isActive;

// Discovery
-(BTstackError) startDiscovery;
-(BTstackError) stopDiscovery;
-(int) numberOfDevicesFound;
-(BTDevice*) deviceAtIndex:(int)index;
-(BOOL) isDiscoveryActive;

// Link Key Management
-(void) dropLinkKeyForAddress:(bd_addr_t*) address;

// Connections
-(BTstackError) createL2CAPChannelAtAddress:(bd_addr_t*) address withPSM:(uint16_t)psm authenticated:(BOOL)authentication;
-(BTstackError) closeL2CAPChannelWithID:(uint16_t) channelID;
-(BTstackError) sendL2CAPPacketForChannelID:(uint16_t)channelID;

-(BTstackError) createRFCOMMConnectionAtAddress:(bd_addr_t*) address withChannel:(uint16_t)channel authenticated:(BOOL)authentication;
-(BTstackError) closeRFCOMMConnectionWithID:(uint16_t) connectionID;
-(BTstackError) sendRFCOMMPacketForChannelID:(uint16_t)connectionID;


// TODO add l2cap and rfcomm incoming commands
@property (nonatomic, assign) NSObject<BTstackManagerDelegate>* delegate;
@property (nonatomic, retain) NSMutableArray *discoveredDevices;
@property (nonatomic, retain) NSMutableSet *listeners;
@end


@protocol BTstackManagerDelegate
@optional

// Activation callbacks
-(BOOL) disableSystemBluetoothBTstackManager:(BTstackManager*) manager; // default: YES

// Connection events
-(NSString*) btstackManager:(BTstackManager*) manager pinForAddress:(bd_addr_t)addr; // default: "0000"

// direct access
-(void) btstackManager:(BTstackManager*) manager
  handlePacketWithType:(uint8_t) packet_type
			forChannel:(uint16_t) channel
			   andData:(uint8_t *)packet
			   withLen:(uint16_t) size;
@end


@protocol BTstackManagerListener
@optional

// Activation events
-(void) activatedBTstackManager:(BTstackManager*) manager;
-(void) btstackManager:(BTstackManager*)manager activationFailed:(BTstackError)error;
-(void) deactivatedBTstackManager:(BTstackManager*) manager;

// Power management events
-(void) sleepModeEnterBTstackManager:(BTstackManager*) manager;
-(void) sleepModeExtitBTstackManager:(BTstackManager*) manager;

// Discovery events: general
-(void) btstackManager:(BTstackManager*)manager deviceInfo:(BTDevice*)device;
-(void) btstackManager:(BTstackManager*)manager discoveryQueryRemoteName:(int)deviceIndex;
-(void) discoveryStoppedBTstackManager:(BTstackManager*) manager;
-(void) discoveryInquiryBTstackManager:(BTstackManager*) manager;

// Connection
-(void) l2capChannelCreatedAtAddress:(bd_addr_t)addr withPSM:(uint16_t)psm asID:(uint16_t)channelID;
-(void) l2capChannelCreateFailedAtAddress:(bd_addr_t)addr withPSM:(uint16_t)psm error:(BTstackError)error;
-(void) l2capChannelClosedForChannelID:(uint16_t)channelID;
-(void) l2capDataReceivedForChannelID:(uint16_t)channelID withData:(uint8_t *)packet ofLen:(uint16_t)size;

-(void) rfcommConnectionCreatedAtAddress:(bd_addr_t)addr forChannel:(uint16_t)channel asID:(uint16_t)connectionID;
-(void) rfcommConnectionCreateFailedAtAddress:(bd_addr_t)addr forChannel:(uint16_t)channel error:(BTstackError)error;
-(void) rfcommConnectionClosedForConnectionID:(uint16_t)connectionID;
-(void) rfcommDataReceivedForConnectionID:(uint16_t)connectionID withData:(uint8_t *)packet ofLen:(uint16_t)size;

// TODO add l2cap and rfcomm incoming events
@end
