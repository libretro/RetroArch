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
#import "RAModuleInfo.h"
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

- (void)infoButtonTapped:(id)sender
{
   RAModuleInfo* info = objc_getAssociatedObject(sender, "MODULE");
   if (info && info.data)
      [RetroArch_iOS.get pushViewController:[[RAModuleInfoList alloc] initWithModuleInfo:info] animated:YES];
   else
      [RetroArch_iOS displayErrorMessage:@"No information available."];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   
   if (!cell)
   {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];
      
      UIButton* infoButton = [UIButton buttonWithType:UIButtonTypeInfoDark];
      [infoButton addTarget:self action:@selector(infoButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
      cell.accessoryView = infoButton;
   }
   
   RAModuleInfo* info = [self moduleInfoForIndexPath:indexPath];
   cell.textLabel.text = info.displayName;
   objc_setAssociatedObject(cell.accessoryView, "MODULE", info, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

   return cell;
}

@end
