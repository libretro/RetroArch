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
   NSMutableArray* _modules[3];

   unsigned _sectionCount;
   unsigned _sectionMap[3];

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
   _modules[0] = [NSMutableArray array];
   _modules[1] = [NSMutableArray array];
   _modules[2] = [NSMutableArray array];
   
   for (int i = 0; i != [moduleList count]; i ++)
   {
      NSString* modulePath = [moduleList objectAtIndex:i];
      NSString* baseName = [[modulePath stringByDeletingPathExtension] stringByAppendingPathExtension:@"info"];

      RAModuleInfo* module = [RAModuleInfo moduleWithPath:modulePath data:[[RAConfig alloc] initWithPath:baseName]];
      [_modules[[module supportLevelOfPath:_game]] addObject:module];
   }
   
   for (int i = 0; i != 3; i ++)
      if ([_modules[i] count])
      {
         _sectionMap[_sectionCount] = i;
         _sectionCount ++;
      }
   
   [self setTitle:[_game lastPathComponent]];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return _sectionCount;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   switch (_sectionMap[section])
   {
      case 0: return @"Recommended Emulators";
      case 1: return @"Suggested Emulators";
      default: return @"Other Emulators";
   }
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return _modules[section] ? [_modules[_sectionMap[section]] count] : 0;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   unsigned section = _sectionMap[indexPath.section];
   [RetroArch_iOS get].moduleInfo = (RAModuleInfo*)[_modules[section] objectAtIndex:indexPath.row];;
   [[RetroArch_iOS get] runGame:_game];
}

- (void)tableView:(UITableView *)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath
{
   unsigned section = _sectionMap[indexPath.section];
   [RetroArch_iOS get].moduleInfo = (RAModuleInfo*)[_modules[section] objectAtIndex:indexPath.row];
   [[RetroArch_iOS get] showSettings];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];

   unsigned section = _sectionMap[indexPath.section];   
   RAModuleInfo* info = (RAModuleInfo*)[_modules[section] objectAtIndex:indexPath.row];
   cell.textLabel.text = info.displayName;
   cell.accessoryType = (info.data) ? UITableViewCellAccessoryDetailDisclosureButton : UITableViewCellAccessoryNone;

   return cell;
}

@end
