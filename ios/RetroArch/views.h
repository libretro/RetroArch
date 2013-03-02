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

#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>

#import "RAConfig.h"

@interface RAGameView : UIViewController
@end

@interface RAModuleInfo : NSObject
@property (strong) NSString* path;
@property (strong) NSString* configPath;
@property (strong) RAConfig* data;
@property (strong) NSArray* recommendedExtensions;
@property (strong) NSArray* suggestedExtensions;

+ (RAModuleInfo*)moduleWithPath:(NSString*)thePath data:(RAConfig*)theData;
- (unsigned)supportLevelOfPath:(NSString*)thePath;
@end

@interface RAModuleInfoList : UITableViewController
- (id)initWithModuleInfo:(RAModuleInfo*)info;
@end

@interface RAModuleList : UITableViewController
- (id)initWithGame:(NSString*)path;
@end

@interface RASettingsSubList : UITableViewController
- (id)initWithSettings:(NSArray*)values title:(NSString*)title;
- (void)writeSettings:(NSArray*)settingList toConfig:(RAConfig*)config;
@end

@interface RASettingsList : RASettingsSubList
+ (void)refreshConfigFile;
@end
