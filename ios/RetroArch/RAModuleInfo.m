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

#include <glob.h>

#import "RetroArch_iOS.h"
#import "RAModuleInfo.h"
#import "views.h"

#include "file.h"

static NSMutableArray* moduleList;

@implementation RAModuleInfo
+ (NSArray*)getModules
{
   if (!moduleList)
   {
      char pattern[PATH_MAX];
      snprintf(pattern, PATH_MAX, "%s/modules/*.dylib", [[NSBundle mainBundle].bundlePath UTF8String]);

      glob_t files = {0};
      glob(pattern, 0, 0, &files);
      
      moduleList = [NSMutableArray arrayWithCapacity:files.gl_pathc];
   
      for (int i = 0; i != files.gl_pathc; i ++)
      {
         RAModuleInfo* newInfo = [RAModuleInfo new];
         newInfo.path = [NSString stringWithUTF8String:files.gl_pathv[i]];
         
         NSString* infoPath = [newInfo.path stringByReplacingOccurrencesOfString:@"_ios.dylib" withString:@".dylib"];
         infoPath = [infoPath stringByReplacingOccurrencesOfString:@".dylib" withString:@".info"];

         newInfo.data = config_file_new([infoPath UTF8String]);

         char* dispname = 0;
         char* extensions = 0;
   
         if (newInfo.data)
         {
            config_get_string(newInfo.data, "display_name", &dispname);
            config_get_string(newInfo.data, "supported_extensions", &extensions);
         }

         newInfo.configPath = [NSString stringWithFormat:@"%@/%@.cfg", [RetroArch_iOS get].systemDirectory, [[newInfo.path lastPathComponent] stringByDeletingPathExtension]];
         newInfo.displayName = dispname ? [NSString stringWithUTF8String:dispname] : [[newInfo.path lastPathComponent] stringByDeletingPathExtension];
         newInfo.supportedExtensions = extensions ? [[NSString stringWithUTF8String:extensions] componentsSeparatedByString:@"|"] : [NSArray array];

         free(dispname);
         free(extensions);

         [moduleList addObject:newInfo];
      }
      
      globfree(&files);
      
      [moduleList sortUsingComparator:^(RAModuleInfo* left, RAModuleInfo* right)
      {
         return [left.displayName caseInsensitiveCompare:right.displayName];
      }];
   }
   
   return moduleList;
}

- (void)dealloc
{
   config_file_free(self.data);
}

- (bool)supportsFileAtPath:(NSString*)path
{
   return [self.supportedExtensions containsObject:[[path pathExtension] lowercaseString]];
}

@end

// Build a string with a second associated string
static NSString* build_string_pair(NSString* stringA, NSString* stringB)
{
   NSString* string_pair = [NSString stringWithString:stringA];
   objc_setAssociatedObject(string_pair, "OTHER", stringB, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
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
      build_string_pair(@"Core Name", ios_get_value_from_config(_data.data, @"corename", @"Unspecified")),
      nil]];
   
   [self.sections addObject: [NSArray arrayWithObjects:@"Hardware/Software",
      build_string_pair(@"Developer", ios_get_value_from_config(_data.data, @"manufacturer", @"Unspecified")),
      build_string_pair(@"Name", ios_get_value_from_config(_data.data, @"systemname", @"Unspecified")),
      nil]];

   // Firmware
   _firmwareSectionIndex = 1000;
   uint32_t firmwareCount = 0;
   if (_data.data && config_get_uint(_data.data, "firmware_count", &firmwareCount) && firmwareCount)
   {
      NSMutableArray* firmwareSection = [NSMutableArray arrayWithObject:@"Firmware"];

      for (int i = 0; i < firmwareCount; i ++)
      {
         NSString* path = ios_get_value_from_config(_data.data, [NSString stringWithFormat:@"firmware%d_path", i + 1], @"Unspecified");
         path = [path stringByReplacingOccurrencesOfString:@"%sysdir%" withString:RetroArch_iOS.get.systemDirectory];      
         [firmwareSection addObject:build_string_pair(ios_get_value_from_config(_data.data, [NSString stringWithFormat:@"firmware%d_desc", i + 1], @"Unspecified"), path)];
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
      [RetroArch_iOS displayErrorMessage:objc_getAssociatedObject(item, "OTHER") withTitle:item];
   }
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"datacell"];
   
   if (!cell)
   {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"datacell"];
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      cell.detailTextLabel.adjustsFontSizeToFitWidth = YES;
   }
   
   NSString* item = (NSString*)[self itemForIndexPath:indexPath];
   NSString* value = (NSString*)objc_getAssociatedObject(item, "OTHER");
   
   cell.textLabel.text = item;
   cell.detailTextLabel.text = value;

   if (indexPath.section == _firmwareSectionIndex)
      cell.backgroundColor = path_file_exists(value.UTF8String) ? [UIColor blueColor] : [UIColor redColor];
   else
      cell.backgroundColor = [UIColor whiteColor];

   return cell;
}

@end
