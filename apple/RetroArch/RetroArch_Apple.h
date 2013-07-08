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
#import "RAModuleInfo.h"

void apple_run_core(RAModuleInfo* core, const char* file);

@protocol RetroArch_Platform
- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file;
- (void)unloadingCore:(RAModuleInfo*)core;
- (NSString*)retroarchConfigPath;
- (NSString*)corePath;
@end

extern id<RetroArch_Platform> apple_platform;

#ifdef IOS

// RAGameView.m
@interface RAGameView : UIViewController
+ (RAGameView*)get;
- (void)openPauseMenu;
- (void)closePauseMenu;

- (void)suspend;
- (void)resume;
@end

@interface RetroArch_iOS : UINavigationController<UIApplicationDelegate, UINavigationControllerDelegate, RetroArch_Platform>

+ (RetroArch_iOS*)get;

- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file;
- (void)unloadingCore:(RAModuleInfo*)core;
- (NSString*)retroarchConfigPath;

- (void)refreshConfig;
- (void)refreshSystemConfig;

@property (strong, nonatomic) NSString* documentsDirectory; // e.g. /var/mobile/Documents
@property (strong, nonatomic) NSString* systemDirectory;    // e.g. /var/mobile/Documents/.RetroArch
@property (strong, nonatomic) NSString* systemConfigPath;   // e.g. /var/mobile/Documents/.RetroArch/frontend.cfg

@end

#elif defined(OSX)

#import <AppKit/AppKit.h>

@interface RAGameView : NSOpenGLView

+ (RAGameView*)get;
- (void)display;

@end

@interface RetroArch_OSX : NSObject<RetroArch_Platform, NSApplicationDelegate>
{
@public
   NSWindow IBOutlet *window;
}

+ (RetroArch_OSX*)get;

- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file;
- (void)unloadingCore:(RAModuleInfo*)core;

@end

#endif

// utility.m
extern void apple_display_alert(NSString* message, NSString* title);
extern void objc_clear_config_hack();
extern bool path_make_and_check_directory(const char* path, mode_t mode, int amode);
extern NSString* objc_get_value_from_config(config_file_t* config, NSString* name, NSString* defaultValue);

#endif
