//
//  dirlist.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#include "dirent_list.h"

@implementation directory_list
{
   char* directory;
   struct dirent_list* files;
};

- (id)initWithPath:(const char*)path
{
   self = [super initWithStyle:UITableViewStylePlain];

   directory = strdup(path);
   files = build_dirent_list(directory);

   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;
   [self setTitle: [[[NSString alloc] initWithUTF8String:directory] lastPathComponent]];
   
   return self;
}

- (void)dealloc
{
   free_dirent_list(files);
   free(directory);
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   const struct dirent* item = get_dirent_at_index(files, indexPath.row);

   if (!item) return;

   char new_path[4096];
   snprintf(new_path, 4096, "%s/%s", directory, item->d_name);
   new_path[4095] = 0;

   if (item->d_type)
   {      
      [[RetroArch_iOS get].navigator
         pushViewController:[[directory_list alloc] initWithPath:new_path]
         animated:YES];
   }
   else
   {
      [RetroArch_iOS get].window.rootViewController = [[game_view alloc] init];
      
      extern void ios_load_game(const char*);
      ios_load_game(new_path);
   }
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return get_dirent_list_count(files);
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"path"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"path"];
   
   const struct dirent* item = get_dirent_at_index(files, indexPath.row);
   
   if (item)
   {
      cell.textLabel.text = [[NSString string] initWithUTF8String:item->d_name];
      cell.accessoryType = (item->d_type) ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
      cell.imageView.image = (item->d_type) ? [RetroArch_iOS get].folder_icon : [RetroArch_iOS get].file_icon;
      [cell.imageView sizeToFit];
   }

   return cell;
}

@end
