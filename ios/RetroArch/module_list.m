//
//  module_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

static void display_error_alert(NSString* message)
{
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                             message:message
                                             delegate:nil
                                             cancelButtonTitle:@"OK"
                                             otherButtonTitles:nil];
   [alert show];
}

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
   
   if (modules != nil)
   {
      modules = [module_dir stringsByAppendingPaths:modules];
      modules = [modules pathsMatchingExtensions:[NSArray arrayWithObject:@"dylib"]];
   }
   
   if (modules == nil || [modules count] == 0)
   {
      display_error_alert(@"No libretro cores were found.");
   }
   
   [self setTitle:@"Choose Emulator"];
   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   [RetroArch_iOS get].module_path = [modules objectAtIndex:indexPath.row];
   [[RetroArch_iOS get] pushViewController:[[directory_list alloc] initWithPath:nil]];
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
