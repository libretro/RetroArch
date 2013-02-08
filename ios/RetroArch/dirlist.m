//
//  dirlist.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#include "dirent_list.h"
#import "dirlist.h"
#import "gameview.h"

@implementation dirlist_view
{
   char path[4096];
   UITableView* table;
   struct dirent_list* files;
   
   UIImage* file_icon;
   UIImage* folder_icon;
};

-(void)dealloc
{
   free_dirent_list(files);
   files = 0;
}

- (void)viewDidLoad
{
   file_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_file" ofType:@"png"]];
   folder_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"ic_dir" ofType:@"png"]];

   [super viewDidLoad];
   
   strcpy(path, "/");
   files = build_dirent_list(path);
   
   table = [[UITableView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) style:UITableViewStylePlain];
   table.dataSource = self;
   table.delegate = self;

   self.view = table;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   const struct dirent* item = get_dirent_at_index(files, indexPath.row);

   if (!item) return;

   if (item->d_type)
   {
      if (strcmp(item->d_name, "..") == 0)
      {
         char* last_slash = strrchr(path, '/');
         if (last_slash) *last_slash = 0;
         path[0] = (path[0] == 0) ? '/' : path[0];
      }
      else
      {
         strcat(path, "/");
         strcat(path, item->d_name);
      }
      
      free_dirent_list(files);
      files = build_dirent_list(path);
      [table reloadData];
   }
   else
   {
      UIWindow *window = [UIApplication sharedApplication].keyWindow;

      if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
         window.rootViewController = [[game_view alloc] initWithNibName:@"ViewController_iPhone" bundle:nil];
      else
         window.rootViewController = [[game_view alloc] initWithNibName:@"ViewController_iPad" bundle:nil];
       
      strcat(path, "/");
      strcat(path, item->d_name);
      
      extern void ios_run_game(const char*);
      ios_load_game(path);
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
      
      if (item->d_type)
      {
         cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
         cell.imageView.image = folder_icon;
      }
      else
      {
         cell.accessoryType = UITableViewCellAccessoryNone;
         cell.imageView.image = file_icon;
      }
      
      [cell.imageView sizeToFit];
   }

   return cell;
}

@end
