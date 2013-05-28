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

#import "RAMOduleInfo.h"
#import "browser.h"
#import "settings.h"

@implementation RAModuleList
{
   NSMutableArray* _supported;
   NSMutableArray* _other;

   NSString* _game;
}

- (id)initWithGame:(NSString*)path
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   [self setTitle:[path lastPathComponent]];
   
   _game = path;

   //
   NSArray* moduleList = [RAModuleInfo getModules];
   
   if (moduleList.count == 0)
      [RetroArch_iOS displayErrorMessage:@"No libretro cores were found."];

   // Load the modules with their data
   _supported = [NSMutableArray array];
   _other = [NSMutableArray array];
   
   for (RAModuleInfo* i in moduleList)
   {
      NSMutableArray* target = [i supportsFileAtPath:_game] ? _supported : _other;
      [target addObject:i];
   }

   // No sort, [RAModuleInfo getModules] is already sorted by display name

   return self;
}

- (RAModuleInfo*)moduleInfoForIndexPath:(NSIndexPath*)path
{
   NSMutableArray* sectionData = (_supported.count && path.section == 0) ? _supported : _other;
   return (RAModuleInfo*)sectionData[path.row];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return _supported.count ? 2 : 1;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   if (_supported.count)
      return (section == 0) ? @"Suggested Cores" : @"Other Cores";

   return @"All Cores";
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   NSMutableArray* sectionData = (_supported.count && section == 0) ? _supported : _other;
   return sectionData.count;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   [RetroArch_iOS.get runGame:_game withModule:[self moduleInfoForIndexPath:indexPath]];
}

- (void)tableView:(UITableView *)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath
{
   [RetroArch_iOS.get pushViewController:[[RAModuleInfoList alloc] initWithModuleInfo:[self moduleInfoForIndexPath:indexPath]] animated:YES];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   cell = (cell) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];

   RAModuleInfo* info = [self moduleInfoForIndexPath:indexPath];

   cell.textLabel.text = info.displayName;
   cell.accessoryType = UITableViewCellAccessoryDetailDisclosureButton;

   return cell;
}

@end
