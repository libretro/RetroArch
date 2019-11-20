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

#ifdef HAVE_MENU
#include "../../menu/menu_setting.h"
#include "../../menu/menu_driver.h"
#endif

#if defined(HAVE_COCOATOUCH)
#define GLContextClass EAGLContext
#define GLFrameworkID CFSTR("com.apple.opengles")
#define RAScreen UIScreen

#ifndef UIUserInterfaceIdiomTV
#define UIUserInterfaceIdiomTV 2
#endif

#ifndef UIUserInterfaceIdiomCarPlay
#define UIUserInterfaceIdiomCarPlay 3
#endif
#else
#define GLContextClass NSOpenGLContext
#define GLFrameworkID CFSTR("com.apple.opengl")
#define RAScreen NSScreen
#endif

typedef enum apple_view_type {
   APPLE_VIEW_TYPE_NONE,
   APPLE_VIEW_TYPE_OPENGL_ES,
   APPLE_VIEW_TYPE_OPENGL,
   APPLE_VIEW_TYPE_VULKAN,
   APPLE_VIEW_TYPE_METAL,
} apple_view_type_t;

#if defined(HAVE_COCOATOUCH)
#include <UIKit/UIKit.h>

#if TARGET_OS_TV
#import <GameController/GameController.h>
#endif

/*********************************************/
/* RAMenuBase                                */
/* A menu class that displays RAMenuItemBase */
/* objects.                                  */
/*********************************************/
@interface RAMenuBase : UITableViewController
@property (nonatomic) NSMutableArray* sections;
@property (nonatomic) BOOL hidesHeaders;
@property (nonatomic) RAMenuBase* last_menu;
@property (nonatomic) UILabel *osdmessage;

- (id)initWithStyle:(UITableViewStyle)style;
- (id)itemForIndexPath:(NSIndexPath*)indexPath;

@end

#if TARGET_OS_IOS
@interface CocoaView : UIViewController
#elif TARGET_OS_TV
@interface CocoaView : GCEventViewController
#endif
+ (CocoaView*)get;
@end

@interface RetroArch_iOS : UINavigationController<UIApplicationDelegate,
UINavigationControllerDelegate>

@property (nonatomic) UIWindow* window;
@property (nonatomic) NSString* documentsDirectory;
@property (nonatomic) RAMenuBase* mainmenu;
@property (nonatomic) int menu_count;

+ (RetroArch_iOS*)get;

- (void)showGameView;
- (void)toggleUI;
- (void)supportOtherAudioSessions;

- (void)refreshSystemConfig;
- (void)mainMenuPushPop: (bool)pushp;
- (void)mainMenuRefresh;
@end

void get_ios_version(int *major, int *minor);

#endif

typedef struct
{
   char orientations[32];
   unsigned orientation_flags;
   char bluetooth_mode[64];
} apple_frontend_settings_t;
extern apple_frontend_settings_t apple_frontend_settings;

#if defined(HAVE_COCOA) || defined(HAVE_COCOA_METAL)
#include <AppKit/AppKit.h>

@interface CocoaView : NSView

+ (CocoaView*)get;
#if !defined(HAVE_COCOA) && !defined(HAVE_COCOA_METAL)
- (void)display;
#endif

@end

#endif

#define BOXSTRING(x) [NSString stringWithUTF8String:x]
#define BOXINT(x)    [NSNumber numberWithInt:x]
#define BOXUINT(x)   [NSNumber numberWithUnsignedInt:x]
#define BOXFLOAT(x)  [NSNumber numberWithDouble:x]

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

void *nsview_get_ptr(void);

void nsview_set_ptr(CocoaView *ptr);

void *get_chosen_screen(void);

#endif
