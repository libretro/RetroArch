//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import <objc/runtime.h>
#import "settings.h"

static const char* const SETTINGID = "SETTING";

@implementation SettingsSubList
{
   NSArray* settings;
};

- (id)initWithSettings:(NSArray*)values title:(NSString*)title
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   settings = values;
  
   [self setTitle:title];
   return self;
}

- (void)writeSettings:(NSArray*)settingList toConfig:(config_file_t *)config
{
   NSArray* list = settingList ? settingList : settings;

   for (int i = 0; i != [list count]; i ++)
   {
      NSArray* group = [list objectAtIndex:i];
   
      for (int j = 1; j < [group count]; j ++)
      {
         SettingData* setting = [group objectAtIndex:j];
         
         switch (setting.type)
         {
            case GroupSetting:
               [self writeSettings:setting.subValues toConfig:config];
               break;
               
            case FileListSetting:
               if ([setting.value length] > 0)
                  config_set_string(config, [setting.name UTF8String], [[setting.path stringByAppendingPathComponent:setting.value] UTF8String]);
               else
                  config_set_string(config, [setting.name UTF8String], "");
               break;
               
            default:
               config_set_string(config, [setting.name UTF8String], [setting.value UTF8String]);
               break;
         }
      }
   }
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   SettingData* setting = [[settings objectAtIndex:indexPath.section] objectAtIndex:indexPath.row + 1];
   
   switch (setting.type)
   {
      case EnumerationSetting:
      case FileListSetting:
         [[RetroArch_iOS get] pushViewController:[[SettingEnumerationList alloc] initWithSetting:setting fromTable:(UITableView*)self.view]];
         break;
         
      case ButtonSetting:
         (void)[[ButtonGetter alloc] initWithSetting:setting fromTable:(UITableView*)self.view];
         break;
         
      case GroupSetting:
         [[RetroArch_iOS get] pushViewController:[[SettingsSubList alloc] initWithSettings:setting.subValues title:setting.label]];
         break;
         
      default:
         break;
   }
}

- (void)handle_boolean_switch:(UISwitch*)swt
{
   SettingData* setting = objc_getAssociatedObject(swt, SETTINGID);
   setting.value = (swt.on ? @"true" : @"false");
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   SettingData* setting = [[settings objectAtIndex:indexPath.section] objectAtIndex:indexPath.row + 1];
  
   UITableViewCell* cell = nil;

   switch (setting.type)
   {
      case BooleanSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"boolean"];

         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"boolean"];
         
            UISwitch* accessory = [[UISwitch alloc] init];
            [accessory addTarget:self action:@selector(handle_boolean_switch:) forControlEvents:UIControlEventValueChanged];
            cell.accessoryView = accessory;
            
            [cell setSelectionStyle:UITableViewCellSelectionStyleNone];
         }
      
         UISwitch* swt = (UISwitch*)cell.accessoryView;
         swt.on = [setting.value isEqualToString:@"true"];
         objc_setAssociatedObject(swt, SETTINGID, setting, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      }
      break;
         
      case EnumerationSetting:
      case FileListSetting:
      case ButtonSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"enumeration"];
   
         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"enumeration"];
         }
      }
      break;
      
      case GroupSetting:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"group"];
   
         if (cell == nil)
         {
            cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"group"];
            cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
         }
      }
      break;
   }

   cell.textLabel.text = setting.label;
   cell.detailTextLabel.text = setting.value;

   return cell;
}

// UITableView item counts
- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return [settings count];
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
   return [[settings objectAtIndex:section] count] -1;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   return [[settings objectAtIndex:section] objectAtIndex:0];
}


@end
