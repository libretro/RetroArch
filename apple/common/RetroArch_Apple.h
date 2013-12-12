/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "core_info.h"
#include "core_info_ext.h"
#include "setting_data.h"
#include "apple_export.h"

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

@protocol RetroArch_Platform
- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore:(NSString*)core;

- (NSString*)configDirectory;   // < This returns the directory that contains retroarch.cfg and other custom configs
- (NSString*)globalConfigFile;  // < This is the full path to retroarch.cfg
- (NSString*)coreDirectory;     // < This is the default path to where libretro cores are installed
@end

#ifdef IOS
#include <UIKit/UIKit.h>
#import "../iOS/platform.h"
#elif defined(OSX)
#import "../OSX/platform.h"
#endif

extern char** apple_argv;
extern bool apple_is_paused;
extern bool apple_is_running;
extern bool apple_use_tv_mode;
extern NSString* apple_core;

extern id<RetroArch_Platform> apple_platform;

// main.m
extern void apple_run_core(NSString* core, const char* file);

// utility.m
extern void apple_display_alert(NSString* message, NSString* title);
extern NSString *objc_get_value_from_config(config_file_t* config, NSString* name, NSString* defaultValue);
extern NSString *apple_get_core_id(const core_info_t *core);
extern NSString *apple_get_core_display_name(NSString *core_id);

@interface RANumberFormatter : NSNumberFormatter
#ifdef IOS
<UITextFieldDelegate>
#endif

- (id)initWithSetting:(const rarch_setting_t*)setting;
@end

// frontend/platform/platform_apple.c
extern void apple_frontend_post_event(void (*fn)(void*), void* userdata);

//
#define BOXSTRING(x) [NSString stringWithUTF8String:x]
#define BOXINT(x)    [NSNumber numberWithInt:x]
#define BOXUINT(x)   [NSNumber numberWithUnsignedInt:x]
#define BOXFLOAT(x)  [NSNumber numberWithDouble:x]

#endif
