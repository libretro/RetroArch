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

#import "browser.h"

@implementation RAModuleList
{
   NSMutableArray* _supported;
   NSMutableArray* _other;

   NSString* _game;
}

- (id)initWithGame:(NSString*)path
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   _game = path;

   // Get the contents of the modules directory of the bundle.
   NSString* module_dir = [NSString stringWithFormat:@"%@/modules", [[NSBundle mainBundle] bundlePath]];
   
   NSArray* moduleList = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:module_dir error:nil];
   
   if (moduleList != nil)
   {
      moduleList = [module_dir stringsByAppendingPaths:moduleList];
      moduleList = [moduleList pathsMatchingExtensions:[NSArray arrayWithObject:@"dylib"]];
   }
   
   if (moduleList == nil || [moduleList count] == 0)
   {
      [RetroArch_iOS displayErrorMessage:@"No libretro cores were found."];
   }
   
   // Load the modules with their data
   _supported = [NSMutableArray array];
   _other = [NSMutableArray array];
   
   for (int i = 0; i != [moduleList count]; i ++)
   {
      NSString* modulePath = [moduleList objectAtIndex:i];
      NSString* baseName = [[modulePath stringByDeletingPathExtension] stringByAppendingPathExtension:@"info"];

      RAModuleInfo* module = [RAModuleInfo moduleWithPath:modulePath data:[[RAConfig alloc] initWithPath:baseName]];
      
      if ([module supportsFileAtPath:_game])
         [_supported addObject:module];
      else
         [_other addObject:module];
   }

   // Sort
   [_supported sortUsingComparator:^(RAModuleInfo* left, RAModuleInfo* right)
   {
      return [left.displayName caseInsensitiveCompare:right.displayName];
   }];

   [_other sortUsingComparator:^(RAModuleInfo* left, RAModuleInfo* right)
   {
      return [left.displayName caseInsensitiveCompare:right.displayName];
   }];

   [self setTitle:[_game lastPathComponent]];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return _supported.count ? 2 : 1;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   if (_supported.count)
      return (section == 0) ? @"Suggested Emulators" : @"Other Emulators";

   return @"All Emulators";
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   NSMutableArray* sectionData = (_supported.count && section == 0) ? _supported : _other;
   return sectionData.count;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSMutableArray* sectionData = (_supported.count && indexPath.section == 0) ? _supported : _other;
   [RetroArch_iOS get].moduleInfo = (RAModuleInfo*)sectionData[indexPath.row];

   [[RetroArch_iOS get] runGame:_game];
}

- (void)tableView:(UITableView *)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath
{
   NSMutableArray* sectionData = (_supported.count && indexPath.section == 0) ? _supported : _other;
   [RetroArch_iOS get].moduleInfo = (RAModuleInfo*)sectionData[indexPath.row];

   [[RetroArch_iOS get] showSettings];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];

   NSMutableArray* sectionData = (_supported.count && indexPath.section == 0) ? _supported : _other;
   RAModuleInfo* info = (RAModuleInfo*)sectionData[indexPath.row];

   cell.textLabel.text = info.displayName;
   cell.accessoryType = (info.data) ? UITableViewCellAccessoryDetailDisclosureButton : UITableViewCellAccessoryNone;

   return cell;
}

@end
