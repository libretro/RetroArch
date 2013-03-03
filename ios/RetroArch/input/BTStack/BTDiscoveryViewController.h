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
#import <UIKit/UIKit.h>
#import "BTstackManager.h"

@class BTstackManager;
@protocol BTDiscoveryDelegate;

typedef enum {
	kInquiryInactive,
	kInquiryActive,
	kInquiryRemoteName
} InquiryState;

@interface BTDiscoveryViewController : UITableViewController<BTstackManagerListener>
{
	BTstackManager *bt;
	NSObject<BTDiscoveryDelegate> * _delegate;
	UIActivityIndicatorView *deviceActivity;
	UIActivityIndicatorView *bluetoothActivity;
	UIFont * deviceNameFont;
	UIFont * macAddressFont;
	InquiryState inquiryState;
	int remoteNameIndex;
	BOOL showIcons;
	int connectingIndex;
	NSString *customActivityText;
}
-(void) markConnecting:(int)index; // use -1 for no connection active
@property (nonatomic, assign) NSObject<BTDiscoveryDelegate> * delegate;
@property (nonatomic, assign) BOOL showIcons;
@property (nonatomic, retain) NSString *customActivityText;
@end

@protocol BTDiscoveryDelegate
@optional
-(BOOL) discoveryView:(BTDiscoveryViewController*)discoveryView willSelectDeviceAtIndex:(int)deviceIndex; // returns NO to ignore selection
-(void) statusCellSelectedDiscoveryView:(BTDiscoveryViewController*)discoveryView;
@end
