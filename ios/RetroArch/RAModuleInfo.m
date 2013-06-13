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
#import "RAModuleInfo.h"
#import "browser/browser.h"

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


static NSString* get_data_string(config_file_t* config, const char* name, NSString* defaultValue)
{
   char* result = 0;
   if (config_get_string(config, name, &result))
   {
      NSString* output = [NSString stringWithUTF8String:result];
      free(result);
      return output;
   }
   
   return defaultValue;
}

@implementation RAModuleInfoList
{
   RAModuleInfo* _data;
   NSMutableArray* _sections;
   uint32_t _firmwareSectionIndex;
}

- (id)initWithModuleInfo:(RAModuleInfo*)info
{
   self = [super initWithStyle:UITableViewStyleGrouped];

   _data = info;

   _sections = [NSMutableArray array];

   [_sections addObject: [NSArray arrayWithObjects:@"Core",
      @"Core Name", get_data_string(_data.data, "corename", @"Unspecified"),
      nil]];
   
   [_sections addObject: [NSArray arrayWithObjects:@"Hardware/Software",
      @"Developer", get_data_string(_data.data, "manufacturer", @"Unspecified"),
      @"Name", get_data_string(_data.data, "systemname", @"Unspecified"),
      nil]];

   // Firmware
   _firmwareSectionIndex = 1000;
   uint32_t firmwareCount = 0;
   if (config_get_uint(_data.data, "firmware_count", &firmwareCount) && firmwareCount)
   {
      NSMutableArray* firmwareSection = [NSMutableArray arrayWithObject:@"Firmware"];

      for (int i = 0; i < firmwareCount; i ++)
      {
         char namebuf[512];
         
         snprintf(namebuf, 512, "firmware%d_desc", i + 1);
         [firmwareSection addObject:get_data_string(_data.data, namebuf, @"Unspecified")];

         snprintf(namebuf, 512, "firmware%d_path", i + 1);
         NSString* path = get_data_string(_data.data, namebuf, @"Unspecified");
         path = [path stringByReplacingOccurrencesOfString:@"%sysdir%" withString:RetroArch_iOS.get.systemDirectory];

         [firmwareSection addObject:path];
      }

      _firmwareSectionIndex = _sections.count;
      [_sections addObject:firmwareSection];
   }

   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return _sections.count;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   return _sections[section][0];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return ([_sections[section] count] - 1) / 2;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   if (indexPath.section == _firmwareSectionIndex)
      [RetroArch_iOS displayErrorMessage:_sections[indexPath.section][indexPath.row * 2 + 2] withTitle:_sections[indexPath.section][indexPath.row * 2 + 1]];
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
   
   cell.textLabel.text = _sections[indexPath.section][indexPath.row * 2 + 1];
   cell.detailTextLabel.text = _sections[indexPath.section][indexPath.row * 2 + 2];

   if (indexPath.section == _firmwareSectionIndex)
      cell.backgroundColor = path_file_exists(((NSString*)_sections[indexPath.section][indexPath.row * 2 + 2]).UTF8String) ? [UIColor blueColor] : [UIColor redColor];
   else
      cell.backgroundColor = [UIColor whiteColor];

   return cell;
}

@end
