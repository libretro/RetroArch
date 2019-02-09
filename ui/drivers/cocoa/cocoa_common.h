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

#ifndef __COCOA_COMMON_H
#define __COCOA_COMMON_H

#include "cocoa_common_shared.h"

#if defined(HAVE_COCOATOUCH)
#if TARGET_OS_IOS
@interface CocoaView : UIViewController<CLLocationManagerDelegate,
AVCaptureAudioDataOutputSampleBufferDelegate>
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

#endif
