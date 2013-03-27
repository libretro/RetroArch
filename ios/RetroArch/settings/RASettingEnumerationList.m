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
   RASettingData* _value;
   UITableView* _view;
   unsigned _mainSection;
};

- (id)initWithSetting:(RASettingData*)setting fromTable:(UITableView*)table
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   
   _value = setting;
   _view = table;
   _mainSection = _value.haveNoneOption ? 1 : 0;
   
   [self setTitle: _value.label];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return _value.haveNoneOption ? 2 : 1;
}

- (NSInteger)tableView:(UITableView*)tableView numberOfRowsInSection:(NSInteger)section
{
   return (section == _mainSection) ? _value.subValues.count : 1;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"option"];
   cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"option"];

   cell.textLabel.text = (indexPath.section == _mainSection) ? _value.subValues[indexPath.row] : @"None";

   return cell;
}

- (void)tableView:(UITableView*)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   _value.value = (indexPath.section == _mainSection) ? _value.subValues[indexPath.row] : @"";
   
   [_view reloadData];
   [[RetroArch_iOS get] popViewControllerAnimated:YES];
}

@end

