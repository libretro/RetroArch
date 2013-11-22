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

#import "RetroArch_Apple.h"
#import "RAModuleInfo.h"

#include "file.h"
#include "core_info.h"

static NSMutableArray* moduleList;
static core_info_list_t* coreList;

NSArray* apple_get_modules()
{
   if (!moduleList)
   {   
      coreList = core_info_list_new(apple_platform.coreDirectory.UTF8String);
      
      if (!coreList)
         return nil;

      moduleList = [NSMutableArray arrayWithCapacity:coreList->count];
      
      for (int i = 0; coreList && i < coreList->count; i ++)
      {
         core_info_t* core = &coreList->list[i];
      
         RAModuleInfo* newInfo = [RAModuleInfo new];
         newInfo.path = [NSString stringWithCString:core->path encoding:NSUTF8StringEncoding];
         newInfo.baseName = newInfo.path.lastPathComponent.stringByDeletingPathExtension;
         newInfo.info = core;
         newInfo.data = core->data;
         newInfo.description = [NSString stringWithCString:core->display_name encoding:NSUTF8StringEncoding];
         newInfo.customConfigFile = [NSString stringWithFormat:@"%@/%@.cfg", apple_platform.configDirectory, newInfo.baseName];
         newInfo.configFile = newInfo.hasCustomConfig ? newInfo.customConfigFile : apple_platform.globalConfigFile;

         [moduleList addObject:newInfo];
      }
      
      [moduleList sortUsingComparator:^(RAModuleInfo* left, RAModuleInfo* right)
      {
         return [left.description caseInsensitiveCompare:right.description];
      }];
   }
   
   return moduleList;
}

@implementation RAModuleInfo

- (id)copyWithZone:(NSZone *)zone
{
   return self;
}

- (bool)supportsFileAtPath:(NSString*)path
{
   return core_info_does_support_file(self.info, path.UTF8String);
}

- (void)createCustomConfig
{
   if (!self.hasCustomConfig)
      [NSFileManager.defaultManager copyItemAtPath:apple_platform.globalConfigFile toPath:self.customConfigFile error:nil];
}

- (void)deleteCustomConfig
{
   if (self.hasCustomConfig)
      [NSFileManager.defaultManager removeItemAtPath:self.customConfigFile error:nil];
}

- (bool)hasCustomConfig
{
   return path_file_exists(self.customConfigFile.UTF8String);
}

@end

#ifdef IOS
#import "../iOS/views.h"

static const void* const associated_string_key = &associated_string_key;

// Build a string with a second associated string
static NSString* build_string_pair(NSString* stringA, NSString* stringB)
{
   NSString* string_pair = [NSString stringWithString:stringA];
   objc_setAssociatedObject(string_pair, associated_string_key, stringB, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   return string_pair;
}

@implementation RAModuleInfoList
{
   RAModuleInfo* _data;
   uint32_t _firmwareSectionIndex;
}

- (id)initWithModuleInfo:(RAModuleInfo*)info
{
   self = [super initWithStyle:UITableViewStyleGrouped];

   _data = info;

   [self.sections addObject: [NSArray arrayWithObjects:@"Core",
      build_string_pair(@"Core Name", objc_get_value_from_config(_data.data, @"corename", @"Unspecified")),
      nil]];
   
   [self.sections addObject: [NSArray arrayWithObjects:@"Hardware/Software",
      build_string_pair(@"Developer", objc_get_value_from_config(_data.data, @"manufacturer", @"Unspecified")),
      build_string_pair(@"Name", objc_get_value_from_config(_data.data, @"systemname", @"Unspecified")),
      nil]];

   // Firmware
   _firmwareSectionIndex = 1000;
   uint32_t firmwareCount = 0;
   if (_data.data && config_get_uint(_data.data, "firmware_count", &firmwareCount) && firmwareCount)
   {
      NSMutableArray* firmwareSection = [NSMutableArray arrayWithObject:@"Firmware"];

      for (int i = 0; i < firmwareCount; i ++)
      {
         NSString* path = objc_get_value_from_config(_data.data, [NSString stringWithFormat:@"firmware%d_path", i], @"Unspecified");
         path = [path stringByReplacingOccurrencesOfString:@"%sysdir%" withString:[RetroArch_iOS get].systemDirectory];
         [firmwareSection addObject:build_string_pair(objc_get_value_from_config(_data.data, [NSString stringWithFormat:@"firmware%d_desc", i], @"Unspecified"), path)];
      }

      _firmwareSectionIndex = self.sections.count;
      [self.sections addObject:firmwareSection];
   }

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   if (indexPath.section == _firmwareSectionIndex)
   {
      NSString* item = (NSString*)[self itemForIndexPath:indexPath];
      apple_display_alert(objc_getAssociatedObject(item, associated_string_key), item);
   }
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   static NSString* const cell_id = @"datacell";

   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:cell_id];
   
   if (!cell)
   {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      cell.detailTextLabel.adjustsFontSizeToFitWidth = YES;
   }
   
   NSString* item = (NSString*)[self itemForIndexPath:indexPath];
   NSString* value = (NSString*)objc_getAssociatedObject(item, associated_string_key);
   
   cell.textLabel.text = item;
   cell.detailTextLabel.text = value;

   if (indexPath.section == _firmwareSectionIndex)
      cell.backgroundColor = path_file_exists(value.UTF8String) ? [UIColor blueColor] : [UIColor redColor];
   else
      cell.backgroundColor = [UIColor whiteColor];

   return cell;
}

@end

#endif
