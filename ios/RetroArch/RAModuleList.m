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

@implementation RAModuleList
{
   NSArray* _modules;
}

- (id)init
{
   self = [super initWithStyle:UITableViewStylePlain];

   // Get the contents of the modules directory of the bundle.
   NSString* module_dir = [NSString stringWithFormat:@"%@/modules", [[NSBundle mainBundle] bundlePath]];
   
   _modules = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:module_dir error:nil];
   
   if (_modules != nil)
   {
      _modules = [module_dir stringsByAppendingPaths:_modules];
      _modules = [_modules pathsMatchingExtensions:[NSArray arrayWithObject:@"dylib"]];
   }
   
   if (_modules == nil || [_modules count] == 0)
   {
      display_error_alert(@"No libretro cores were found.");
   }
   
   [self setTitle:@"Choose Emulator"];
   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   [RetroArch_iOS get].module_path = [_modules objectAtIndex:indexPath.row];
   [[RetroArch_iOS get] pushViewController:[[RADirectoryList alloc] initWithPath:nil]];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return _modules ? [_modules count] : 0;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];
   
   cell.textLabel.text = [[[_modules objectAtIndex:indexPath.row] lastPathComponent] stringByDeletingPathExtension];

   return cell;
}

@end
