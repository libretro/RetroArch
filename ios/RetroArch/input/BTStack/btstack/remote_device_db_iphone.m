/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */
#include "remote_device_db.h"
#include "debug.h"

#import <Foundation/Foundation.h>

#define BTdaemonID         "ch.ringwald.btdaemon"
#define BTDaemonPrefsPath  "Library/Preferences/ch.ringwald.btdaemon.plist"

#define DEVICES_KEY        "devices"
#define PREFS_REMOTE_NAME  @"RemoteName"
#define PREFS_LINK_KEY     @"LinkKey"

#define MAX_RFCOMM_CHANNEL_NR 30

#define RFCOMM_SERVICES_KEY "rfcommServices"
#define PREFS_CHANNEL      @"channel"
#define PREFS_LAST_USED    @"lastUsed"

static void put_name(bd_addr_t *bd_addr, device_name_t *device_name);

static NSMutableDictionary *remote_devices  = nil;
static NSMutableDictionary *rfcomm_services = nil;

// Device info
static void db_open(void){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    // NSUserDefaults didn't work
    // 
	// NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	// NSDictionary * dict = [defaults persistentDomainForName:BTdaemonID];
	
    // NSDictionary * dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(DEVICES_KEY), CFSTR(BTdaemonID));
    NSDictionary * dict;
    dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(DEVICES_KEY), CFSTR(BTdaemonID));
    remote_devices = [[NSMutableDictionary alloc] initWithCapacity:([dict count]+5)];
    
	// copy entries
	for (id key in dict) {
		NSDictionary *value = [dict objectForKey:key];
		NSMutableDictionary *deviceEntry = [NSMutableDictionary dictionaryWithCapacity:[value count]];
		[deviceEntry addEntriesFromDictionary:value];
		[remote_devices setObject:deviceEntry forKey:key];
	}
    
    dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(RFCOMM_SERVICES_KEY), CFSTR(BTdaemonID));
    rfcomm_services = [[NSMutableDictionary alloc] initWithCapacity:([dict count]+5)];
    
	// copy entries
	for (id key in dict) {
		NSDictionary *value = [dict objectForKey:key];
		NSMutableDictionary *serviceEntry = [NSMutableDictionary dictionaryWithCapacity:[value count]];
		[serviceEntry addEntriesFromDictionary:value];
		[rfcomm_services setObject:serviceEntry forKey:key];
	}
    
    log_info("read prefs for %u devices\n", (unsigned int) [dict count]);
    
    [pool release];
}

static void db_synchronize(void){
    log_info("stored prefs for %u devices\n", (unsigned int) [remote_devices count]);
    
    // 3 different ways
    
    // Core Foundation
    CFPreferencesSetValue(CFSTR(DEVICES_KEY), (CFPropertyListRef) remote_devices, CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    CFPreferencesSetValue(CFSTR(RFCOMM_SERVICES_KEY), (CFPropertyListRef) rfcomm_services, CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    
    // NSUserDefaults didn't work
    // 
	// NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    // [defaults setPersistentDomain:remote_devices forName:BTdaemonID];
    // [defaults synchronize];
}

static void db_close(void){ 
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    // don't call db_synchronize();
    // a) we're calling db_synchronize() after each change already
    // b) db_close is called during the SIGINT handler which causes a corrupt prefs file
    
    [remote_devices release];
    remote_devices = nil;
    [pool release];
}

static NSString * stringForAddress(bd_addr_t* address) {
	uint8_t *addr = (uint8_t*) *address;
	return [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]];
}
                             
static void set_value(bd_addr_t *bd_addr, NSString *key, id value){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	if (!deviceDict){
		deviceDict = [NSMutableDictionary dictionaryWithCapacity:3];
		[remote_devices setObject:deviceDict forKey:devAddress];
	}
    [deviceDict setObject:value forKey:key];
    db_synchronize();
}

static void delete_value(bd_addr_t *bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	[deviceDict removeObjectForKey:key];
    db_synchronize();

}

static id get_value(bd_addr_t *bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
    if (!deviceDict) return nil;
    return [deviceDict objectForKey:key];
}

static int get_link_key(bd_addr_t *bd_addr, link_key_t *link_key) {
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSData *linkKey = get_value(bd_addr, PREFS_LINK_KEY);
    if ([linkKey length] == LINK_KEY_LEN){
        memcpy(link_key, [linkKey bytes], LINK_KEY_LEN);
    }
    [pool release];
    return (linkKey != nil);
}

static void put_link_key(bd_addr_t *bd_addr, link_key_t *link_key){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSData *linkKey = [NSData dataWithBytes:link_key length:16];
    set_value(bd_addr, PREFS_LINK_KEY, linkKey);
    [pool release];
}

static void delete_link_key(bd_addr_t *bd_addr){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    delete_value(bd_addr, PREFS_LINK_KEY);
    [pool release];
}

static void put_name(bd_addr_t *bd_addr, device_name_t *device_name){
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSString *remoteName = [NSString stringWithUTF8String:(char*)device_name];
	if (!remoteName){
        remoteName = [NSString stringWithCString:(char*)device_name encoding:NSISOLatin1StringEncoding];
    }
    if (remoteName) {
        set_value(bd_addr, PREFS_REMOTE_NAME, remoteName);
    }
    [pool release];
}

static void delete_name(bd_addr_t *bd_addr){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    delete_value(bd_addr, PREFS_REMOTE_NAME);
    [pool release];
}

static int  get_name(bd_addr_t *bd_addr, device_name_t *device_name) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSString *remoteName = get_value(bd_addr, PREFS_REMOTE_NAME);
    if (remoteName){
        memset(device_name, 0, sizeof(device_name_t));
        strncpy((char*) device_name, [remoteName UTF8String], sizeof(device_name_t)-1);
    }
    [pool release];
    return (remoteName != nil);
}

// MARK: PERSISTENT RFCOMM CHANNEL ALLOCATION

static int firstFreeChannelNr(void){
    BOOL channelUsed[MAX_RFCOMM_CHANNEL_NR+1];
    int i;
    for (i=0; i<=MAX_RFCOMM_CHANNEL_NR ; i++) channelUsed[i] = NO;
    channelUsed[0] = YES;
    channelUsed[1] = YES; // preserve channel #1 for testing
    for (NSDictionary * serviceEntry in [rfcomm_services allValues]){
        int channel = [(NSNumber *) [serviceEntry objectForKey:PREFS_CHANNEL] intValue];
        channelUsed[channel] = YES;
    }
    for (i=0;i<=MAX_RFCOMM_CHANNEL_NR;i++) {
        if (channelUsed[i] == NO) return i;
    }
    return -1;
}

static void deleteLeastUsed(void){
    NSString * leastUsedName = nil;
    NSDate *   leastUsedDate = nil;
    for (NSString * serviceName in [rfcomm_services allKeys]){
        NSDictionary *service = [rfcomm_services objectForKey:serviceName];
        NSDate *serviceDate = [service objectForKey:PREFS_LAST_USED];
        if (leastUsedName == nil || [leastUsedDate compare:serviceDate] == NSOrderedDescending) {
            leastUsedName = serviceName;
            leastUsedDate = serviceDate;
            continue;
        }
    }
    if (leastUsedName){
        // NSLog(@"removing %@", leastUsedName);
        [rfcomm_services removeObjectForKey:leastUsedName];
    }
}

static void addService(NSString * serviceName, int channel){
    NSMutableDictionary * serviceEntry = [NSMutableDictionary dictionaryWithCapacity:2];
    [serviceEntry setObject:[NSNumber numberWithInt:channel] forKey:PREFS_CHANNEL];
    [serviceEntry setObject:[NSDate date] forKey:PREFS_LAST_USED];
    [rfcomm_services setObject:serviceEntry forKey:serviceName];
}

static uint8_t persistent_rfcomm_channel(char *serviceName){

    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    NSLog(@"persistent_rfcomm_channel for %s", serviceName);

    // find existing entry
    NSString *serviceString = [NSString stringWithUTF8String:serviceName];
    NSMutableDictionary *serviceEntry = [rfcomm_services objectForKey:serviceString];
    if (serviceEntry){
        // update timestamp
        [serviceEntry setObject:[NSDate date] forKey:PREFS_LAST_USED];
        
        db_synchronize();
        
        return [(NSNumber *) [serviceEntry objectForKey:PREFS_CHANNEL] intValue];
    }
    // free channel exist?
    int channel = firstFreeChannelNr();
    if (channel < 0){
        // free channel
        deleteLeastUsed();
        channel = firstFreeChannelNr();
    }
    addService(serviceString, channel);

    db_synchronize();

    [pool release];
    
    return channel;
}


remote_device_db_t remote_device_db_iphone = {
    db_open,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
    get_name,
    put_name,
    delete_name,
    persistent_rfcomm_channel
};

