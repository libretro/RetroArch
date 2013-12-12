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

#import <AVFoundation/AVCaptureOutput.h>
#include "views.h"

typedef struct
{
   char orientations[32];    
   bool logging_enabled;
    
   char bluetooth_mode[64];
    
   struct
   {
      int stdout;
      int stderr;
        
      FILE* file;
   }  logging;
} apple_frontend_settings_t;
extern apple_frontend_settings_t apple_frontend_settings;

const void* apple_get_frontend_settings(void);


@interface RAGameView : UIViewController<AVCaptureAudioDataOutputSampleBufferDelegate>
+ (RAGameView*)get;
@end

@interface RetroArch_iOS : UINavigationController<UIApplicationDelegate, UINavigationControllerDelegate, RetroArch_Platform>

+ (RetroArch_iOS*)get;

- (void)loadingCore:(NSString*)core withFile:(const char*)file;
- (void)unloadingCore:(NSString*)core;

- (void)refreshSystemConfig;

@property (nonatomic) NSString* configDirectory;    // e.g. /var/mobile/Documents/.RetroArch
@property (nonatomic) NSString* globalConfigFile;   // e.g. /var/mobile/Documents/.RetroArch/retroarch.cfg
@property (nonatomic) NSString* coreDirectory;      // e.g. /Applications/RetroArch.app/modules

@property (nonatomic) NSString* documentsDirectory; // e.g. /var/mobile/Documents
@property (nonatomic) NSString* systemDirectory;    // e.g. /var/mobile/Documents/.RetroArch
@property (nonatomic) NSString* systemConfigPath;   // e.g. /var/mobile/Documents/.RetroArch/frontend.cfg
@property (nonatomic) NSString* logPath;

@end

// modes are: keyboard, icade and btstack
void ios_set_bluetooth_mode(NSString* mode);

#endif
