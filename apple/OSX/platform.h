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

#ifndef __RARCH_OSX_PLATFORM_H
#define __RARCH_OSX_PLATFORM_H

#import <AppKit/AppKit.h>

@interface RAGameView : NSView

+ (RAGameView*)get;
- (void)display;

@end

@interface RetroArch_OSX : NSObject<RetroArch_Platform, NSApplicationDelegate>
{
@public
   NSWindow IBOutlet *window;
}

+ (RetroArch_OSX*)get;

- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore:(NSString*)core;

@property (nonatomic) NSString* configDirectory;    // e.g. /var/mobile/Documents/.RetroArch
@property (nonatomic) NSString* globalConfigFile;   // e.g. /var/mobile/Documents/.RetroArch/retroarch.cfg
@property (nonatomic) NSString* coreDirectory;      // e.g. /Applications/RetroArch.app/modules

@end

#endif
