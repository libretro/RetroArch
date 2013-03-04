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

#import <objc/runtime.h>
#import "settings.h"

static const char* const SETTINGID = "SETTING";

@implementation RASettingsSubList
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

- (void)handleCustomAction:(NSString*)action
{

}

- (void)writeSettings:(NSArray*)settingList toConfig:(RAConfig*)config
{
   NSArray* list = settingList ? settingList : settings;

   for (int i = 0; i != [list count]; i ++)
   {
      NSArray* group = [list objectAtIndex:i];
   
      for (int j = 1; j < [group count]; j ++)
      {
         RASettingData* setting = [group objectAtIndex:j];
         
         switch (setting.type)
         {         
            case GroupSetting:
               [self writeSettings:setting.subValues toConfig:config];
               break;
               
            case FileListSetting:
               if ([setting.value length] > 0)
                  [config putStringNamed:setting.name value:[setting.path stringByAppendingPathComponent:setting.value]];
               else
                  [config putStringNamed:setting.name value:@""];
               break;

            case ButtonSetting:
               if (setting.msubValues[0] && [setting.msubValues[0] length])
                  [config putStringNamed:setting.name value:setting.msubValues[0]];
               if (setting.msubValues[1] && [setting.msubValues[1] length])
                  [config putStringNamed:[setting.name stringByAppendingString:@"_btn"] value:setting.msubValues[1]];
               break;

            case CustomAction:
               break;

            default:
               [config putStringNamed:setting.name value:setting.value];
               break;
         }
      }
   }
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   RASettingData* setting = [[settings objectAtIndex:indexPath.section] objectAtIndex:indexPath.row + 1];
   
   switch (setting.type)
   {
      case EnumerationSetting:
      case FileListSetting:
         [[RetroArch_iOS get] pushViewController:[[RASettingEnumerationList alloc] initWithSetting:setting fromTable:(UITableView*)self.view] isGame:NO];
         break;
         
      case ButtonSetting:
         (void)[[RAButtonGetter alloc] initWithSetting:setting fromTable:(UITableView*)self.view];
         break;
         
      case GroupSetting:
         [[RetroArch_iOS get] pushViewController:[[RASettingsSubList alloc] initWithSettings:setting.subValues title:setting.label] isGame:NO];
         break;
         
      case CustomAction:
         [self handleCustomAction:setting.label];
         break;
         
      default:
         break;
   }
}

- (void)handleBooleanSwitch:(UISwitch*)swt
{
   RASettingData* setting = objc_getAssociatedObject(swt, SETTINGID);
   setting.value = (swt.on ? @"true" : @"false");
}

- (UITableViewCell*)tableView:(UITableView*)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   RASettingData* setting = [[settings objectAtIndex:indexPath.section] objectAtIndex:indexPath.row + 1];
  
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
            [accessory addTarget:self action:@selector(handleBooleanSwitch:) forControlEvents:UIControlEventValueChanged];
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
      case CustomAction:
      {
         cell = [self.tableView dequeueReusableCellWithIdentifier:@"enumeration"];
         cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"enumeration"];
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
   
   if (setting.type != ButtonSetting)
      cell.detailTextLabel.text = setting.value;
   else
      cell.detailTextLabel.text = [NSString stringWithFormat:@"[KB:%@] [JS:%@]",
            [setting.msubValues[0] length] ? setting.msubValues[0] : @"N/A",
            [setting.msubValues[1] length] ? setting.msubValues[1] : @"N/A"];

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
