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

#import "settings.h"

@implementation RASettingEnumerationList
{
   RASettingData* value;
   UITableView* view;
};

- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   
   value = setting;
   view = table;
   [self setTitle: value.label];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return 2;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
   return (section == 1) ? [value.subValues count] : 1;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"option"];
   cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"option"];
   
   if (indexPath.section == 1)
      cell.textLabel.text = [value.subValues objectAtIndex:indexPath.row];
   else
      cell.textLabel.text = @"None";

   return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   if (indexPath.section == 1)
      value.value = [value.subValues objectAtIndex:indexPath.row];
   else
      value.value = @"";

   [view reloadData];
   [[RetroArch_iOS get] popViewController];
}

@end

