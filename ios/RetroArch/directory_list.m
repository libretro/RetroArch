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
   char* path;

   UITableView* table;
   struct dirent_list* files;
};

- (id)load_path:(const char*)directory
{
   free_dirent_list(files);
   files = build_dirent_list(directory);
   [table reloadData];
   
   free(path);
   path = strdup(directory);
   
   [self setTitle: [[NSString alloc] initWithUTF8String:directory]];
   
   return self;
}

- (void)dealloc
{
   free_dirent_list(files);
   free(path);
}

- (void)viewDidLoad
{
   [super viewDidLoad];

   table = [[UITableView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) style:UITableViewStylePlain];
   table.dataSource = self;
   table.delegate = self;

   self.navigationItem.backBarButtonItem = [[UIBarButtonItem alloc]
                                            initWithTitle:@"Parent"
                                            style:UIBarButtonItemStyleBordered
                                            target:nil action:nil];


   self.view = table;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   const struct dirent* item = get_dirent_at_index(files, indexPath.row);

   if (!item) return;

   char new_path[4096];
   strcpy(new_path, path);
   strcat(new_path, item->d_name);

   if (item->d_type)
   {
      strcat(new_path, "/");
      
      [[RetroArch_iOS get].navigator pushViewController:[[[directory_list alloc] init] load_path: new_path] animated:YES];
   }
   else
   {
      [RetroArch_iOS get].window.rootViewController = [[game_view alloc]
         initWithNibName: [RetroArch_iOS get].nib_name
         bundle:nil];
      
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
   UITableViewCell* cell = [table dequeueReusableCellWithIdentifier:@"path"];
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
