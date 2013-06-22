/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#import "RAModuleInfo.h"

@interface RetroArch_iOS : UINavigationController<UIApplicationDelegate, UINavigationControllerDelegate>

+ (void)displayErrorMessage:(NSString*)message;
+ (void)displayErrorMessage:(NSString*)message withTitle:(NSString*)title;

+ (RetroArch_iOS*)get;

- (void)runGame:(NSString*)path withModule:(RAModuleInfo*)module;
- (void)refreshConfig;
- (void)refreshSystemConfig;

@property (strong, nonatomic) NSString* documentsDirectory; // e.g. /var/mobile/Documents
@property (strong, nonatomic) NSString* systemDirectory;    // e.g. /var/mobile/Documents/.RetroArch
@property (strong, nonatomic) NSString* systemConfigPath;   // e.g. /var/mobile/Documents/.RetroArch/frontend.cfg

@end

// utility.m
extern void ios_clear_config_hack();
extern bool path_make_and_check_directory(const char* path, mode_t mode, int amode);
extern NSString* ios_get_value_from_config(config_file_t* config, NSString* name, NSString* defaultValue);
