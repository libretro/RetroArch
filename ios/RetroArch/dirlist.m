//
//  dirlist.m
//  RetroArch
//
//  Created by Jason Fetters on 2/7/13.
//  Copyright (c) 2013 RetroArch. All rights reserved.
//

#include <dirent.h>
#import "dirlist.h"

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
   struct dirent_list* next = 0;
   if (list)
   {
      next = list->next;
      free(list);
   }
   
   free_dirent_list(next);
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

@implementation DirectoryView
{
   UITableView* table;
   struct dirent_list* files;
};

-(void)dealloc
{
   free_dirent_list(files);
   files = 0;
   
   // Do I need to kill table here?
}

- (void)viewDidLoad
{
   [super viewDidLoad];
   
   files = build_dirent_list([NSTemporaryDirectory() UTF8String]);
   
   table = [[UITableView alloc] initWithFrame:CGRectMake(0, 0, 640, 480) style:UITableViewStylePlain];
   table.dataSource = self;
   table.delegate = self;

   self.view = table;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   const struct dirent* item = get_dirent_at_index(files, indexPath.row);
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
