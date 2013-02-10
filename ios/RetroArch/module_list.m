//
//  module_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

@implementation module_list
{
   NSArray* modules;
};

- (id)init
{
   self = [super initWithStyle:UITableViewStylePlain];

   // Get the contents of the modules directory of the bundle.
   NSString* module_dir = [NSString stringWithFormat:@"%@/%@",
                          [[NSBundle mainBundle] bundlePath],
                          @"modules"];
   
   modules = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:module_dir error:nil];
   modules = [module_dir stringsByAppendingPaths:modules];
   modules = [modules pathsMatchingExtensions:[NSArray arrayWithObject:@"dylib"]];
   
   [self setTitle:@"Choose Emulator"];
   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   [RetroArch_iOS get].module_path = [modules objectAtIndex:indexPath.row];
   [[RetroArch_iOS get].navigator pushViewController:[[directory_list alloc] initWithPath:nil] animated:YES];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return modules ? [modules count] : 0;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];
   
   cell.textLabel.text = [[[modules objectAtIndex:indexPath.row] lastPathComponent] stringByDeletingPathExtension];

   return cell;
}

@end
