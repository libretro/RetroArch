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

/*
 *  run_loop_cocoa.c
 *
 *  Created by Matthias Ringwald on 8/2/09.
 */

#include <btstack/btstack.h>

#include <btstack/run_loop.h>
#include "run_loop_private.h"
#include "debug.h"

#import <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include <stdio.h>
#include <stdlib.h>

static void theCFRunLoopTimerCallBack (CFRunLoopTimerRef timer,void *info){
    timer_source_t * ts = (timer_source_t*)info;
    ts->process(ts);
}

static void socketDataCallback (
						 CFSocketRef s,
						 CFSocketCallBackType callbackType,
						 CFDataRef address,
						 const void *data,
						 void *info) {
	
    if (callbackType == kCFSocketReadCallBack && info){
        data_source_t *dataSource = (data_source_t *) info;
        // printf("cocoa_data_source %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) dataSource, dataSource->fd, (int) s, (int) dataSource->item.next);
        dataSource->process(dataSource);
    }
}

void cocoa_add_data_source(data_source_t *dataSource){

	// add fd as CFSocket
	
	// store our dataSource in socket context
	CFSocketContext socketContext;
	memset(&socketContext, 0, sizeof(CFSocketContext));
	socketContext.info = dataSource;

	// create CFSocket from file descriptor
	CFSocketRef socket = CFSocketCreateWithNative (
										  kCFAllocatorDefault,
										  dataSource->fd,
										  kCFSocketReadCallBack,
										  socketDataCallback,
										  &socketContext
    );
    
    // don't close native fd on CFSocketInvalidate
    CFSocketSetSocketFlags(socket, CFSocketGetSocketFlags(socket) & ~kCFSocketCloseOnInvalidate);
    
	// create run loop source
	CFRunLoopSourceRef socketRunLoop = CFSocketCreateRunLoopSource ( kCFAllocatorDefault, socket, 0);
    
    // hack: store CFSocketRef in "next" and CFRunLoopSourceRef in "user_data" of linked_item_t
    dataSource->item.next      = (void *) socket;
    dataSource->item.user_data = (void *) socketRunLoop;

    // add to run loop
	CFRunLoopAddSource( CFRunLoopGetCurrent(), socketRunLoop, kCFRunLoopCommonModes);
    // printf("cocoa_add_data_source    %x - fd %u - CFSocket %x, CFRunLoopSource %x\n", (int) dataSource, dataSource->fd, (int) socket, (int) socketRunLoop);
    
}

int  cocoa_remove_data_source(data_source_t *dataSource){
    // printf("cocoa_remove_data_source %x - fd %u, CFSocket %x, CFRunLoopSource %x\n", (int) dataSource, dataSource->fd, (int) dataSource->item.next, (int) dataSource->item.user_data);
    CFRunLoopRemoveSource( CFRunLoopGetCurrent(), (CFRunLoopSourceRef) dataSource->item.user_data, kCFRunLoopCommonModes);
    CFRelease(dataSource->item.user_data);
    CFSocketInvalidate((CFSocketRef) dataSource->item.next);
    CFRelease(dataSource->item.next);
	return 0;
}

void  cocoa_add_timer(timer_source_t * ts)
{
    // note: ts uses unix time: seconds since Jan 1st 1970, CF uses Jan 1st 2001 as reference date
    // printf("kCFAbsoluteTimeIntervalSince1970 = %f\n", kCFAbsoluteTimeIntervalSince1970);
    CFAbsoluteTime fireDate = ((double)ts->timeout.tv_sec) + (((double)ts->timeout.tv_usec)/1000000.0) - kCFAbsoluteTimeIntervalSince1970; // unix time - since Jan 1st 1970
    CFRunLoopTimerContext timerContext = {0, ts, NULL, NULL, NULL};
    CFRunLoopTimerRef timerRef = CFRunLoopTimerCreate (kCFAllocatorDefault,fireDate,0,0,0,theCFRunLoopTimerCallBack,&timerContext);
    CFRetain(timerRef);

    // hack: store CFRunLoopTimerRef in next pointer of linked_item
    ts->item.next = (void *)timerRef;
    // printf("cocoa_add_timer %x -> %x now %f, then %f\n", (int) ts, (int) ts->item.next, CFAbsoluteTimeGetCurrent(),fireDate);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timerRef, kCFRunLoopCommonModes);
}

int  cocoa_remove_timer(timer_source_t * ts){
    // printf("cocoa_remove_timer %x -> %x\n", (int) ts, (int) ts->item.next);
	if (ts->item.next != NULL) {
    	CFRunLoopTimerInvalidate((CFRunLoopTimerRef) ts->item.next);    // also removes timer from run loops + releases it
    	CFRelease((CFRunLoopTimerRef) ts->item.next);
	}
	return 0;
}

void cocoa_init(void){
}

void cocoa_execute(void)
{
    CFRunLoopRun();
}

void cocoa_dump_timer(void){
    log_error("WARNING: run_loop_dump_timer not implemented!");
	return;
}

run_loop_t run_loop_cocoa = {
    &cocoa_init,
    &cocoa_add_data_source,
    &cocoa_remove_data_source,
    &cocoa_add_timer,
    &cocoa_remove_timer,
    &cocoa_execute,
    &cocoa_dump_timer
};

