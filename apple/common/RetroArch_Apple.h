/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../../core_info.h"
#include "../../settings.h"

@protocol RetroArch_Platform
- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore;
@end

#ifdef IOS
#include <UIKit/UIKit.h>

#include <CoreLocation/CoreLocation.h>
#import <AVFoundation/AVCaptureOutput.h>
#include "../iOS/views.h"

typedef struct
{
   char orientations[32];
   unsigned orientation_flags;
   char bluetooth_mode[64];
} apple_frontend_settings_t;
extern apple_frontend_settings_t apple_frontend_settings;

@interface RAGameView : UIViewController<CLLocationManagerDelegate, AVCaptureAudioDataOutputSampleBufferDelegate>
+ (RAGameView*)get;
@end

@interface RetroArch_iOS : UINavigationController<UIApplicationDelegate, UINavigationControllerDelegate, RetroArch_Platform>

@property (nonatomic) UIWindow* window;
@property (nonatomic) NSString* documentsDirectory; // e.g. /var/mobile/Documents

+ (RetroArch_iOS*)get;

- (void)showGameView;
- (void)toggleUI;

- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore;

- (void)refreshSystemConfig;
@end

void get_ios_version(int *major, int *minor);

#elif defined(OSX)
#include <AppKit/AppKit.h>
#ifdef HAVE_LOCATION
#include <CoreLocation/CoreLocation.h>
#endif

#include "../common/CFExtensions.h"

@interface RAGameView : NSView
#ifdef HAVE_LOCATION
<CLLocationManagerDelegate>
#endif

+ (RAGameView*)get;
#ifndef OSX
- (void)display;
#endif

@end

@interface RetroArch_OSX : NSObject<RetroArch_Platform>
{
   NSWindow* _window;
   NSWindowController* _settingsWindow;
   NSWindow* _coreSelectSheet;
   NSString* _file;
   NSString* _core;
}

@property (nonatomic, retain) NSWindow IBOutlet* window;

+ (RetroArch_OSX*)get;

- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore;

@end

#endif

extern id<RetroArch_Platform> apple_platform;

/* utility.m */
extern void apple_display_alert(const char *message, const char *title);

@interface RANumberFormatter : NSNumberFormatter
#ifdef IOS
<UITextFieldDelegate>
#endif

- (id)initWithSetting:(const rarch_setting_t*)setting;
@end

#define BOXSTRING(x) [NSString stringWithUTF8String:x]
#define BOXINT(x)    [NSNumber numberWithInt:x]
#define BOXUINT(x)   [NSNumber numberWithUnsignedInt:x]
#define BOXFLOAT(x)  [NSNumber numberWithDouble:x]

#endif
