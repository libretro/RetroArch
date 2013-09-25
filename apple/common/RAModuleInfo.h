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

#ifndef _RA_MODULE_INFO_H
#define _RA_MODULE_INFO_H

#import <Foundation/Foundation.h>
#include "compat/apple_compat.h"

#include "conf/config_file.h"
#include "core_info.h"

extern NSArray* apple_get_modules();

@interface RAModuleInfo : NSObject
@property (nonatomic) NSString* path;                    // e.g. /path/to/corename_libretro.dylib
@property (nonatomic) NSString* baseName;                // e.g. corename_libretro
@property (nonatomic) core_info_t* info;
@property (nonatomic) config_file_t* data;
@property (nonatomic) NSString* description;             // Friendly name from config file, else just the filename
@property (nonatomic) NSString* customConfigFile;        // Path where custom config file would reside
@property (nonatomic) NSString* configFile;              // Path to effective config file

- (bool)supportsFileAtPath:(NSString*)path;

- (void)createCustomConfig;
- (void)deleteCustomConfig;
- (bool)hasCustomConfig;

@end

#endif

