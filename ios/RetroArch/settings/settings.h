//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#include "conf/config_file.h"

enum SettingTypes
{
   BooleanSetting, ButtonSetting, EnumerationSetting, FileListSetting, GroupSetting
};

@interface RASettingData : NSObject
@property enum SettingTypes type;

@property (strong) NSString* label;
@property (strong) NSString* name;
@property (strong) NSString* value;

@property (strong) NSString* path;
@property (strong) NSArray* subValues;
@end

@interface RAButtonGetter : NSObject<UIAlertViewDelegate>
- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table;
@end

@interface RASettingEnumerationList : UITableViewController
- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table;
@end
