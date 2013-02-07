//
//  dirlist.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#include <dirent.h>
#import "dirlist.h"
#import "gameview.h"

struct dirent_list
{
   struct dirent_list* next;
   struct dirent entry;
};

const struct dirent* get_dirent_at_index(struct dirent_list* list, unsigned index)
{
   while (list && index)
   {
      list = list->next;
      index --;
   }
   
   return list ? &list->entry : 0;
}

unsigned get_dirent_list_count(struct dirent_list* list)
{
   unsigned result = 0;
   
   while (list)
   {
      result ++;
      list = list->next;
   }
   
   return result;
}

void free_dirent_list(struct dirent_list* list)
{
   struct dirent_list* next = list ? list : 0;
   
   while (next)
   {
      struct dirent_list* me = next;
      next = next->next;
      free(me);
   }
}

struct dirent_list* build_dirent_list(const char* path)
{
   struct dirent_list* result = 0;
   struct dirent_list* iterate = 0;

   DIR* dir = opendir(path);
   if (dir)
   {   
      struct dirent* ent = 0;
      while ((ent = readdir(dir)))
      {
         if (!iterate)
         {
            iterate = malloc(sizeof(struct dirent_list));
            iterate->next = 0;
            result = iterate;
         }
         else
         {
            iterate->next = malloc(sizeof(struct dirent_list));
            iterate = iterate->next;
            iterate->next = 0;
         }
      
         memcpy(&iterate->entry, ent, sizeof(struct dirent));
      }
      
      closedir(dir);
   }
   
   return result;
}

@implementation dirlist_view
{
   char path[4096];
   UITableView* table;
   struct dirent_list* files;
};

-(void)dealloc
{
   free_dirent_list(files);
   files = 0;
}

- (void)viewDidLoad
{
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

   strcat(path, item->d_name);

   if (item->d_type & DT_DIR)
   {
      strcat(path, "/");
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
    
      game_view* game = (game_view*)window.rootViewController;
      [game load_game:path];
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
      cell.accessoryType = (item->d_type & DT_DIR) ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
   }

   return cell;
}

@end
