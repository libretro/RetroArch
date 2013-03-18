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

//
//  platform_iphone.c
//
//  Created by Matthias Ringwald on 8/15/09.
//

#include "platform_iphone.h"

#include "config.h"
#include "../SpringBoardAccess/SpringBoardAccess.h"

#include <stdio.h>
#include <unistd.h>

#ifdef USE_SPRINGBOARD
#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>

// update SpringBoard icons
void platform_iphone_status_handler(BLUETOOTH_STATE state){
    switch (state) {
        case BLUETOOTH_OFF:
            SBA_removeStatusBarImage("BTstack");
            SBA_removeStatusBarImage("BTstackActive");
            NSLog(@"Bluetooth status: OFF");
            break;
        case BLUETOOTH_ON:
            SBA_removeStatusBarImage("BTstackActive");
            SBA_addStatusBarImage("BTstack");
            NSLog(@"Bluetooth status: ON");
            break;
        case BLUETOOTH_ACTIVE:
            SBA_removeStatusBarImage("BTstack");
            SBA_addStatusBarImage("BTstackActive");
            NSLog(@"Bluetooth status: ACTIVE");
            break;
        default:
            break;
    }
}

static void (*window_manager_restart_callback)() = NULL;
static void springBoardDidLaunch(){
    NSLog(@"springBoardDidLaunch!\n");
    if (window_manager_restart_callback) {
        int timer;
        for (timer = 0 ; timer < 10 ; timer++){
            NSLog(@"ping SBA %u", timer);
            if (SBA_available()){
                NSLog(@"pong from SBA!");
                break;
            }
            sleep(1);
        }
        (*window_manager_restart_callback)();
    }
}

void platform_iphone_register_window_manager_restart(void (*callback)() ){
    static int registered = 0;
    if (!registered) {
        // register for launch notification
        CFNotificationCenterRef darwin = CFNotificationCenterGetDarwinNotifyCenter();
        CFNotificationCenterAddObserver(darwin, NULL, (CFNotificationCallback) springBoardDidLaunch,
                                        (CFStringRef) @"SBSpringBoardDidLaunchNotification", NULL, 0);
    }
    window_manager_restart_callback = callback;
}

static void (*preferences_changed_callback)() = NULL;
static void preferencesDidChange(){
    NSLog(@"ch.ringwald.btstack.preferences!\n");
    if (preferences_changed_callback) {
        (*preferences_changed_callback)();
    }
}

void platform_iphone_register_preferences_changed(void (*callback)() ){
    static int registered = 0;
    if (!registered) {
        // register for launch notification
        CFNotificationCenterRef darwin = CFNotificationCenterGetDarwinNotifyCenter();
        CFNotificationCenterAddObserver(darwin, NULL, (CFNotificationCallback) preferencesDidChange,
                                        (CFStringRef) @"ch.ringwald.btstack.preferences", NULL, 0);
    }
    preferences_changed_callback = callback;
}

int platform_iphone_logging_enabled(void){
    int result = 0;
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSDictionary * dict = [NSDictionary dictionaryWithContentsOfFile:@"/var/mobile/Library/Preferences/ch.ringwald.btstack.plist"];
    NSNumber *loggingEnabled = [dict objectForKey:@"Logging"];
    if (loggingEnabled){
        result = [loggingEnabled boolValue];
    }
    [pool release];
    return result;
}

#endif