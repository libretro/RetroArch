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

#ifndef __RARCH_IOS_PLATFORM_H
#define __RARCH_IOS_PLATFORM_H

#include "views.h"

@interface RAGameView : UIViewController
+ (RAGameView*)get;
- (void)openPauseMenu;
- (void)closePauseMenu;
@end

@interface RetroArch_iOS : UINavigationController<UIApplicationDelegate, UINavigationControllerDelegate, RetroArch_Platform,
                                                   RADirectoryListDelegate, RAModuleListDelegate>

+ (RetroArch_iOS*)get;

- (void)loadingCore:(RAModuleInfo*)core withFile:(const char*)file;
- (void)unloadingCore:(RAModuleInfo*)core;
- (NSString*)retroarchConfigPath;

- (void)refreshSystemConfig;

@property (strong, nonatomic) NSString* documentsDirectory; // e.g. /var/mobile/Documents
@property (strong, nonatomic) NSString* systemDirectory;    // e.g. /var/mobile/Documents/.RetroArch
@property (strong, nonatomic) NSString* systemConfigPath;   // e.g. /var/mobile/Documents/.RetroArch/frontend.cfg

@end

// modes are: keyboard, icade and btstack
void ios_set_bluetooth_mode(NSString* mode);

#endif