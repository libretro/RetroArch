//
//  module_list.m
//  RetroArch
//
//  Created by Jason Fetters on 2/8/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

@implementation RAModuleList
{
   NSMutableArray* _modules;
}

- (id)init
{
   self = [super initWithStyle:UITableViewStylePlain];

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
   _modules = [NSMutableArray arrayWithCapacity:[moduleList count]];
   
   for (int i = 0; i != [moduleList count]; i ++)
   {
      NSString* modulePath = [moduleList objectAtIndex:i];

      NSString* baseName = [[modulePath stringByDeletingPathExtension] stringByAppendingPathExtension:@"info"];
      [_modules addObject:[RAModuleInfo moduleWithPath:modulePath data:config_file_new([baseName UTF8String])]];
   }
   
   [self setTitle:@"Choose Emulator"];

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   RAModuleInfo* info = (RAModuleInfo*)[_modules objectAtIndex:indexPath.row];
   [RetroArch_iOS get].module_path = info.path;
   
   [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListWithPath:nil]];
}

- (void)tableView:(UITableView *)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath
{
   RAModuleInfo* info = (RAModuleInfo*)[_modules objectAtIndex:indexPath.row];
   [[RetroArch_iOS get] pushViewController:[[RAModuleInfoList alloc] initWithModuleInfo:info]];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return _modules ? [_modules count] : 0;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];
   
   RAModuleInfo* info = (RAModuleInfo*)[_modules objectAtIndex:indexPath.row];
   cell.textLabel.text = [[info.path lastPathComponent] stringByDeletingPathExtension];
   cell.accessoryType = (info.data) ? UITableViewCellAccessoryDetailDisclosureButton : UITableViewCellAccessoryNone;


   return cell;
}

@end
