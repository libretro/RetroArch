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
//  BTDevice.m
//
//  Created by Matthias Ringwald on 3/30/09.
//

#import "BTDevice.h"

@implementation BTDevice

@synthesize name;
@synthesize classOfDevice;
@synthesize connectionState;
@synthesize pageScanRepetitionMode;
@synthesize clockOffset;

- (BTDevice *)init {
	name = NULL;
	bzero(&address, 6);
	classOfDevice = kCODInvalid;
	connectionState = kBluetoothConnectionNotConnected;
	return self;
}

- (void) setAddress:(bd_addr_t *)newAddr{
	BD_ADDR_COPY( &address, newAddr);
}

- (bd_addr_t *) address{
	return &address;
}

+ (NSString *) stringForAddress:(bd_addr_t *) address {
	uint8_t * addr = (uint8_t*) address;
	return [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]];
}

- (NSString *) nameOrAddress{
	if (name) return name;
	return [BTDevice stringForAddress:&address];
}

- (BluetoothDeviceType) deviceType{
	switch (classOfDevice) {
		case kCODHID:
			return kBluetoothDeviceTypeHID;
		case kCODZeeMote:
			return kBluetoothDeviceTypeZeeMote;
		default:
			return kBluetoothDeviceTypeGeneric;
	}
}
- (NSString *) toString{
	return [NSString stringWithFormat:@"Device addr %@ name %@ COD %x", [BTDevice stringForAddress:&address], name, classOfDevice];
}

- (void)dealloc {
	[name release];
	[super dealloc];
}

@end
