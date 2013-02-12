//
//  settings_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#import "settings.h"

@implementation enumeration_list
{
   NSMutableDictionary* value;
   UITableView* view;
};

- (id)initWithSetting:(NSMutableDictionary*)setting fromTable:(UITableView*)table
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   
   value = setting;
   view = table;
   [self setTitle: [value objectForKey:@"LABEL"]];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
   return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return (section == 1) ? [[value objectForKey:@"VALUES"] count] : 1;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"option"];
   cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"option"];
   
   if (indexPath.section == 1)
      cell.textLabel.text = [[value objectForKey:@"VALUES"] objectAtIndex:indexPath.row];
   else
      cell.textLabel.text = @"None";

   return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   if (indexPath.section == 1)
      [value setObject:[[value objectForKey:@"VALUES"] objectAtIndex:indexPath.row] forKey:@"VALUE"];
   else
      [value setObject:@"" forKey:@"VALUE"];

   [view reloadData];
   [[RetroArch_iOS get].navigator popViewControllerAnimated:YES];
}

@end

