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

// RAGameView.m
@interface RAGameView : UIViewController
+ (RAGameView*)get;
- (void)openPauseMenu;
- (void)closePauseMenu;

- (void)suspend;
- (void)resume;
@end

// RALogView.m
@interface RALogView : UITableViewController
@end

// utility.m
@interface RATableViewController : UITableViewController
@property NSMutableArray* sections;
@property BOOL hidesHeaders;

- (id)initWithStyle:(UITableViewStyle)style;
- (id)itemForIndexPath:(NSIndexPath*)indexPath;
@end

// browser.m
@interface RADirectoryList : RATableViewController
+ (id)directoryListAtBrowseRoot;
+ (id)directoryListForPath:(NSString*)path;
- (id)initWithPath:(NSString*)path;
@end

// browser.m
@interface RAModuleList : RATableViewController
- (id)initWithGame:(NSString*)path;
@end

// RAModuleInfo.m
@interface RAModuleInfoList : RATableViewController
- (id)initWithModuleInfo:(RAModuleInfo*)info;
@end

// settings.m
@interface RASettingsSubList : RATableViewController
- (id)initWithSettings:(NSArray*)values title:(NSString*)title;
- (void)writeSettings:(NSArray*)settingList toConfig:(config_file_t*)config;

- (bool)isSettingsView;
@end

// settings.m
@interface RASettingsList : RASettingsSubList
+ (void)refreshModuleConfig:(RAModuleInfo*)module;
- (id)initWithModule:(RAModuleInfo*)module;
@end

// settings.m
@interface RASystemSettingsList : RASettingsSubList<UIAlertViewDelegate>
@end
