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

#import "BTDiscoveryViewController.h"
#import "BTDevice.h"

// fix compare for 3.0
#ifndef __IPHONE_3_0
#define __IPHONE_3_0 30000
#endif

#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_3_0
// SDK 30 defines missing in SDK 20
@interface UITableViewCell (NewIn30) 
- (id)initWithStyle:(int)style reuseIdentifier:(NSString *)reuseIdentifier;
@end
#endif
@interface UIDevice (privateAPI)
-(BOOL) isWildcat;
@end

@implementation BTDiscoveryViewController
@synthesize showIcons;
@synthesize delegate = _delegate;
@synthesize customActivityText;

- (id) init {
	self = [super initWithStyle:UITableViewStyleGrouped];
	macAddressFont = [UIFont fontWithName:@"Courier New" size:[UIFont labelFontSize]];
	deviceNameFont = [UIFont boldSystemFontOfSize:[UIFont labelFontSize]];
	inquiryState = kInquiryInactive;
	connectingIndex = -1;
	
	deviceActivity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
	[deviceActivity startAnimating];
	bluetoothActivity = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray];
	[bluetoothActivity startAnimating];
	
	bt = [BTstackManager sharedInstance];
	_delegate = nil;
	return self;
}

-(void) reload{
	[[self tableView] reloadData];
}

/*
- (id)initWithStyle:(UITableViewStyle)style {
    // Override initWithStyle: if you create the controller programmatically and want to perform customization that is not appropriate for viewDidLoad.
    if (self = [super initWithStyle:style]) {
    }
    return self;
}
*/

/*
- (void)viewDidLoad {
    [super viewDidLoad];

    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
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

/*
// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (void)didReceiveMemoryWarning {
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}
 */


// BTstackManagerListenerDelegate
-(void) activatedBTstackManager:(BTstackManager*) manager{
	[self reload];
}
-(void) btstackManager:(BTstackManager*)manager activationFailed:(BTstackError)error {
	[self reload];
}
-(void) discoveryInquiryBTstackManager:(BTstackManager*) manager {
	inquiryState = kInquiryActive;
	[self reload];
}
-(void) btstackManager:(BTstackManager*)manager discoveryQueryRemoteName:(int)deviceIndex {
	inquiryState = kInquiryRemoteName;
	remoteNameIndex = deviceIndex;
	[self reload];
}
-(void) discoveryStoppedBTstackManager:(BTstackManager*) manager {
	inquiryState = kInquiryInactive;
	[self reload];
}
-(void) btstackManager:(BTstackManager*)manager deviceInfo:(BTDevice*)device {
	[self reload];
}

-(void) markConnecting:(int)index; {
	connectingIndex = index;
	[self reload];
}

-(void) setCustomActivityText:(NSString*) text{
	[text retain];
	[customActivityText release];
	customActivityText = text;
	[self reload];
}

// MARK: Table view methods

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section{
	return @"Devices";
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

// Customize the number of rows in the table view.
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return 1 + [bt numberOfDevicesFound];
}


// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    static NSString *CellIdentifier = @"Cell";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];
    if (cell == nil) {
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_3_0
		cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier] autorelease];
#else
		cell = [[[UITableViewCell alloc] initWithFrame:CGRectNull reuseIdentifier:(NSString *)CellIdentifier] autorelease];
#endif
    }
    
    // Set up the cell...
	NSString *theLabel = nil;
	UIImage *theImage = nil;
	UIFont *theFont = nil;
	
	int idx = [indexPath indexAtPosition:1];
	if (idx >= [bt numberOfDevicesFound]) {
		if (customActivityText) {
			theLabel = customActivityText;
			cell.accessoryView = bluetoothActivity;
		} else if ([bt isActivating]){
			theLabel = @"Activating BTstack...";
			cell.accessoryView = bluetoothActivity;
		} else if (![bt isActive]){
			theLabel = @"Bluetooth not accessible!";
			cell.accessoryView = nil;
		} else {
			
#if 0
			if (connectedDevice) {
				theLabel = @"Disconnect";
				cell.accessoryView = nil;
			}
#endif
			
			if (connectingIndex >= 0) {
				theLabel = @"Connecting...";
				cell.accessoryView = bluetoothActivity;
			} else {
				switch (inquiryState){
					case kInquiryInactive:
						if ([bt numberOfDevicesFound] > 0){
							theLabel = @"Find more devices...";
						} else {
							theLabel = @"Find devices...";
						}
						cell.accessoryView = nil;
						break;
					case kInquiryActive:
						theLabel = @"Searching...";
						cell.accessoryView = bluetoothActivity;
						break;
					case kInquiryRemoteName:
						theLabel = @"Query device names...";
						cell.accessoryView = bluetoothActivity;
						break;
				}
			}
		}
	} else {
		
		BTDevice *dev = [bt deviceAtIndex:idx];
		
		// pick font
		theLabel = [dev nameOrAddress];
		if ([dev name]){
			theFont = deviceNameFont;
		} else {
			theFont = macAddressFont;
		}
		
		// pick an icon for the devices
		if (showIcons) {
			NSString *imageName = @"bluetooth.png";
			// check major device class
			switch (([dev classOfDevice] & 0x1f00) >> 8) {
				case 0x01:
					imageName = @"computer.png";
					break;
				case 0x02:
					imageName = @"smartphone.png";
					break;
				case 0x05:
					switch ([dev classOfDevice] & 0xff){
						case 0x40:
							imageName = @"keyboard.png";
							break;
						case 0x80:
							imageName = @"mouse.png";
							break;
						case 0xc0:
							imageName = @"keyboard.png";
							break;
						default:
							imageName = @"HID.png";
							break;
					}
			}
			
#ifdef LASER_KB
			if ([dev name] && [[dev name] isEqualToString:@"CL800BT"]){
				imageName = @"keyboard.png";
			}
			
			if ([dev name] && [[dev name] isEqualToString:@"CL850"]){
				imageName = @"keyboard.png";
			}
			
			// Celluon CL800BT, CL850 have 00-0b-24-aa-bb-cc, COD 0x400210
			uint8_t *addr = (uint8_t *) [dev address];
			if (addr[0] == 0x00 && addr[1] == 0x0b && addr[2] == 0x24){
				imageName = @"keyboard.png";
			}
#endif
			theImage = [UIImage imageNamed:imageName];
		}
		
		// set accessory view
		if (idx == connectingIndex){
			cell.accessoryView = deviceActivity;
		} else {
			cell.accessoryView = nil;
		}
	}
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_3_0
	if (theLabel) cell.textLabel.text =  theLabel;
	if (theFont)  cell.textLabel.font =  theFont;
	if (theImage) cell.imageView.image = theImage; 
#else
	if (theLabel) cell.text  = theLabel;
	if (theFont)  cell.font  = theFont;
	if (theImage) cell.image = theImage; 
#endif
    return cell;
}

	
- (NSIndexPath *)tableView:(UITableView *)tableView willSelectRowAtIndexPath:(NSIndexPath *)indexPath {
	if (!_delegate) return nil;
	int index = [indexPath indexAtPosition:1];
	if (index >= [bt numberOfDevicesFound]){
		if ([_delegate respondsToSelector:@selector(statusCellSelectedDiscoveryView:)]){
			[_delegate statusCellSelectedDiscoveryView:self];
			return nil;
		}
	}
	if ([_delegate respondsToSelector:@selector(discoveryView:willSelectDeviceAtIndex:)] && [_delegate discoveryView:self willSelectDeviceAtIndex:index]){
		return indexPath;
	}
	return nil;
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
	UIDevice * uiDevice = [UIDevice currentDevice];
	return [uiDevice respondsToSelector:@selector(isWildcat)] && [uiDevice isWildcat];
}

- (void)dealloc {
    [super dealloc];
}


@end

