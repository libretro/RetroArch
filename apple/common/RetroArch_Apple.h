/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RARCH_APPLE_H
#define __RARCH_APPLE_H

#include <Foundation/Foundation.h>
#import <CoreFoundation/CoreFoundation.h>

#include "../../frontend/info/core_info.h"
#include "../../settings_data.h"
#include "apple_export.h"

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

@protocol RetroArch_Platform
- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore;
@end

#ifdef IOS
#include <UIKit/UIKit.h>
#import "../iOS/platform.h"
#elif defined(OSX)
#import "../OSX/platform.h"
#endif

extern char** apple_argv;

extern id<RetroArch_Platform> apple_platform;

// main.m
extern void apple_run_core(NSString* core, const char* file);
extern void apple_start_iteration();
extern void apple_stop_iteration();

// utility.m
extern void apple_display_alert(const char *message, const char *title);

@interface RANumberFormatter : NSNumberFormatter
#ifdef IOS
<UITextFieldDelegate>
#endif

- (id)initWithSetting:(const rarch_setting_t*)setting;
@end

//
#define BOXSTRING(x) [NSString stringWithUTF8String:x]
#define BOXINT(x)    [NSNumber numberWithInt:x]
#define BOXUINT(x)   [NSNumber numberWithUnsignedInt:x]
#define BOXFLOAT(x)  [NSNumber numberWithDouble:x]

#endif
