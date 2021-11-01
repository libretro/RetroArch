/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __COCOA_COMMON_SHARED_H
#define __COCOA_COMMON_SHARED_H

#include <Foundation/Foundation.h>

#if defined(HAVE_COCOATOUCH)
#include <UIKit/UIKit.h>
#if TARGET_OS_TV
#import <GameController/GameController.h>
#endif
#else
#include <AppKit/AppKit.h>
#endif

#include "../../../retroarch.h"

#if defined(HAVE_COCOATOUCH)
#define RAScreen UIScreen

#ifndef UIUserInterfaceIdiomTV
#define UIUserInterfaceIdiomTV 2
#endif

#ifndef UIUserInterfaceIdiomCarPlay
#define UIUserInterfaceIdiomCarPlay 3
#endif

#if TARGET_OS_IOS
@interface CocoaView : UIViewController
#elif TARGET_OS_TV
@interface CocoaView : GCEventViewController
#endif
+ (CocoaView*)get;
@end

void get_ios_version(int *major, int *minor);
#else
#define RAScreen NSScreen

@interface CocoaView : NSView

+ (CocoaView*)get;
#if !defined(HAVE_COCOA) && !defined(HAVE_COCOA_METAL)
- (void)display;
#endif

@end
#endif

typedef struct
{
   char orientations[32];
   unsigned orientation_flags;
   char bluetooth_mode[64];
} apple_frontend_settings_t;
extern apple_frontend_settings_t apple_frontend_settings;

#define BOXSTRING(x) [NSString stringWithUTF8String:x]
#define BOXINT(x)    [NSNumber numberWithInt:x]
#define BOXUINT(x)   [NSNumber numberWithUnsignedInt:x]
#define BOXFLOAT(x)  [NSNumber numberWithDouble:x]

#if defined(__clang__)
/* ARC is only available for Clang */
#if __has_feature(objc_arc)
#define RELEASE(x)   x = nil
#define BRIDGE       __bridge
#define UNSAFE_UNRETAINED __unsafe_unretained
#else
#define RELEASE(x)   [x release]; \
   x = nil
#define BRIDGE
#define UNSAFE_UNRETAINED
#endif
#else
/* On compilers other than Clang (e.g. GCC), assume ARC 
   is going to be unavailable */
#define RELEASE(x)   [x release]; \
   x = nil
#define BRIDGE
#define UNSAFE_UNRETAINED
#endif

void *nsview_get_ptr(void);

void nsview_set_ptr(CocoaView *ptr);

bool cocoa_has_focus(void *data);

void cocoa_show_mouse(void *data, bool state);

void *cocoa_screen_get_chosen(void);

#ifdef HAVE_COCOATOUCH
float cocoa_screen_get_native_scale(void);
#else
float cocoa_screen_get_backing_scale_factor(void);
void cocoa_update_title(void *data);
#endif

bool cocoa_get_metrics(
      void *data, enum display_metric_types type,
      float *value);

#endif
