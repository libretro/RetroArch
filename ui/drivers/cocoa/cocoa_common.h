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
#include <QuartzCore/QuartzCore.h>

#if TARGET_OS_IPHONE && defined(HAVE_COCOATOUCH)
#include <UIKit/UIKit.h>
#if TARGET_OS_TV
#import <GameController/GameController.h>
#endif
#else
#include <AppKit/AppKit.h>
#endif

/* The CGDisplayModeRef family (CGDisplayCopyDisplayMode,
 * CGDisplayCopyAllDisplayModes, CGDisplayModeGetRefreshRate, ...)
 * arrived in 10.6 Snow Leopard.  The 10.5 Leopard SDK only offers
 * the older CGDisplayCurrentMode + CFDictionaryRef path.  Sites that
 * use either API (cocoa_common.m's cocoa_get_refresh_rate and
 * dispserv_apple.m's resolution-switching code) branch on this
 * macro.  Defined here so every translation unit sees the same
 * answer. */
#if defined(OSX) && defined(MAC_OS_X_VERSION_10_6) && \
    (!defined(MAC_OS_X_VERSION_MIN_REQUIRED) || \
     MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6)
#define RARCH_HAS_CGDISPLAYMODE_API 1
#endif

/* RetroArchPlaylistManager.m/.h uses Obj-C nullability macros
 * (NS_ASSUME_NONNULL_BEGIN/END, nullable, _Nonnull) and
 * lightweight generics (NSArray<...>) - all Xcode 7+ (2015)
 * features requiring SDK 10.11 / iOS 9.0 or newer.  Synthesize
 * HAVE_RETROARCH_PLAYLIST_MANAGER here for build systems that
 * don't pass it in (e.g. the RetroArch_PPC.xcodeproj, non-qb
 * builds).  qb/config.libs.sh sets the flag directly for make
 * builds and takes priority if already defined. */
#ifndef HAVE_RETROARCH_PLAYLIST_MANAGER
#if defined(HAVE_COCOATOUCH) || \
    (defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 101100)
#define HAVE_RETROARCH_PLAYLIST_MANAGER 1
#endif
#endif

#include "../../../retroarch.h"

#if TARGET_OS_IPHONE && defined(HAVE_COCOATOUCH)
#define RAScreen UIScreen

#ifndef UIUserInterfaceIdiomTV
#define UIUserInterfaceIdiomTV 2
#endif

#ifndef UIUserInterfaceIdiomCarPlay
#define UIUserInterfaceIdiomCarPlay 3
#endif

#ifdef HAVE_IOS_SWIFT
@class EmulatorKeyboardController;
@class EmulatorTouchMouseHandler;
#endif

#if TARGET_OS_IOS
@interface CocoaView : UIViewController

#elif TARGET_OS_TV
@interface CocoaView : GCEventViewController
#endif

#ifdef HAVE_IOS_SWIFT
@property(nonatomic,strong) EmulatorKeyboardController *keyboardController;
@property(nonatomic,assign) unsigned int keyboardModifierState;
-(void)toggleCustomKeyboard;

@property(nonatomic,strong) EmulatorTouchMouseHandler *mouseHandler;

@property(nonatomic,strong) UIView *helperBarView;
#endif

#if TARGET_OS_IOS
@property(readwrite) BOOL shouldLockCurrentInterfaceOrientation;
@property(readwrite) UIInterfaceOrientation lockInterfaceOrientation;
#endif

@property(nonatomic,readwrite) CADisplayLink *displayLink;

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

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140000
@property(nonatomic,readwrite,retain) CADisplayLink *displayLink API_AVAILABLE(macos(14.0));
#endif

@end
#endif

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

#ifdef HAVE_RETROARCH_PLAYLIST_MANAGER
bool cocoa_launch_game_by_filename(NSString *filename);
#endif

#ifdef HAVE_COCOATOUCH
float cocoa_screen_get_native_scale(void);
#else
float cocoa_screen_get_backing_scale_factor(void);
#endif

bool cocoa_get_metrics(
      void *data, enum display_metric_types type,
      float *value);

/* Shared display-info helpers.
 *
 * Three vtables call these via thin wrappers, because different
 * call sites reach them through different paths:
 *   - video_driver_get_refresh_rate / _get_video_output_size go
 *     through dispserv first, poke second.
 *   - video_context_driver_get_refresh_rate (used by vulkan.c's
 *     vulkan_get_refresh_rate) goes straight to the gfx_ctx_driver_t
 *     vtable, bypassing dispserv.
 *   - video_thread_wrapper.c's thread_get_video_output_size calls
 *     poke->get_video_output_size directly, bypassing dispserv.
 *
 * Each vtable therefore keeps a registered function; the bodies all
 * funnel here so there is only one implementation per platform. */
float cocoa_get_refresh_rate(void);

void  cocoa_get_video_output_size(unsigned *width, unsigned *height,
      char *desc, size_t desc_len);

#endif

void cocoa_file_load_with_detect_core(const char *filename);
