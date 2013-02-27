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
//  BTDevice.h
//  BT-Keyboard
//
//  Created by Matthias Ringwald on 3/30/09.
//

#import <Foundation/Foundation.h>
#include "btstack/utils.h"

#define kCODHID      0x2540
#define kCODZeeMote   0x584
#define kCODInvalid  0xffff

typedef enum {
	kBluetoothDeviceTypeGeneric = 0,
	kBluetoothDeviceTypeHID,
	kBluetoothDeviceTypeMobilePhone,
	kBluetoothDeviceTypeSmartPhone,
	kBluetoothDeviceTypeZeeMote,
} BluetoothDeviceType;

typedef enum {
	kBluetoothConnectionNotConnected = 0,
	kBluetoothConnectionRemoteName,
	kBluetoothConnectionConnecting,
	kBluetoothConnectionConnected
} BluetoothConnectionState;

@interface BTDevice : NSObject {
	bd_addr_t address;
	NSString * name;
	uint8_t    pageScanRepetitionMode;
	uint16_t   clockOffset;
	uint32_t   classOfDevice;
	BluetoothConnectionState  connectionState;
}

- (void) setAddress:(bd_addr_t *)addr;
- (bd_addr_t *) address;
- (NSString *) toString;
+ (NSString *) stringForAddress:(bd_addr_t *) address;

@property (readonly)          BluetoothDeviceType deviceType;
@property (readonly)          NSString *          nameOrAddress;
@property (nonatomic, copy)   NSString *          name;
@property (nonatomic, assign) uint32_t            classOfDevice;
@property (nonatomic, assign) uint16_t            clockOffset;
@property (nonatomic, assign) uint8_t             pageScanRepetitionMode;
@property (nonatomic, assign) BluetoothConnectionState connectionState;

@end
