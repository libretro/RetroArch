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
#import "RAModuleInfo.h"

#define GSEVENT_TYPE_KEYDOWN 10
#define GSEVENT_TYPE_KEYUP 11

@protocol RetroArch_Platform
- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file;
- (void)unloadingCore:(RAModuleInfo*)core;
- (NSString*)retroarchConfigPath;
- (NSString*)corePath;
@end

#ifdef IOS
#import "../iOS/platform.h"
#elif defined(OSX)
#import "../OSX/platform.h"
#endif

extern bool apple_is_paused;
extern bool apple_is_running;
extern bool apple_use_tv_mode;
extern RAModuleInfo* apple_core;

extern id<RetroArch_Platform> apple_platform;

// main.m
enum basic_event_t { RESET = 1, LOAD_STATE = 2, SAVE_STATE = 3, QUIT = 4 };
extern void apple_event_basic_command(void* userdata);
extern void apple_event_set_state_slot(void* userdata);
extern void apple_event_show_rgui(void* userdata);

extern void apple_refresh_config();
extern void apple_enter_stasis();
extern void apple_exit_stasis();
extern void apple_run_core(RAModuleInfo* core, const char* file);

// utility.m
extern void apple_display_alert(NSString* message, NSString* title);
extern void objc_clear_config_hack();
extern bool path_make_and_check_directory(const char* path, mode_t mode, int amode);
extern NSString* objc_get_value_from_config(config_file_t* config, NSString* name, NSString* defaultValue);

// frontend/platform/platform_apple.c
extern void apple_frontend_post_event(void (*fn)(void*), void* userdata);

#endif
