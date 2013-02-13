//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "settings.h"

@implementation SettingEnumerationList
{
   SettingData* value;
   UITableView* view;
};

- (id)initWithSetting:(SettingData*)setting fromTable:(UITableView*)table
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
   [[RetroArch_iOS get].navigator popViewControllerAnimated:YES];
}

@end

