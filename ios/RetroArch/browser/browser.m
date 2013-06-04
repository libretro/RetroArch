/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dirent.h>
#include <sys/stat.h>
#import "browser.h"
#import "conf/config_file.h"

@implementation RADirectoryItem
+ (RADirectoryItem*)directoryItemFromPath:(const char*)thePath
{
   RADirectoryItem* result = [RADirectoryItem new];
   result.path = [NSString stringWithUTF8String:thePath];

   struct stat statbuf;
   if (stat(thePath, &statbuf) == 0)
      result.isDirectory = S_ISDIR(statbuf.st_mode);
   
   return result;
}
@end


BOOL ra_ios_is_file(NSString* path)
{
   return [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:nil];
}

BOOL ra_ios_is_directory(NSString* path)
{
   BOOL result = NO;
   [[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&result];
   return result;
}

static NSArray* ra_ios_list_directory(NSString* path)
{
   NSMutableArray* result = [NSMutableArray arrayWithCapacity:27];
   for (int i = 0; i < 28; i ++)
   {
      [result addObject:[NSMutableArray array]];
   }

   // Build list
   char* cpath = malloc([path length] + sizeof(struct dirent));
   sprintf(cpath, "%s/", [path UTF8String]);
   size_t cpath_end = strlen(cpath);

   DIR* dir = opendir(cpath);
   if (!dir)
      return result;
   
   for(struct dirent* item = readdir(dir); item; item = readdir(dir))
   {
      if (strncmp(item->d_name, ".", 1) == 0)
         continue;
      
      cpath[cpath_end] = 0;
      strcat(cpath, item->d_name);

      RADirectoryItem* value = [RADirectoryItem directoryItemFromPath:cpath];

      uint32_t section = isalpha(item->d_name[0]) ? (toupper(item->d_name[0]) - 'A') + 2 : 1;
      section = value.isDirectory ? 0 : section;
      [result[section] addObject:[RADirectoryItem directoryItemFromPath:cpath]];
   }
   
   closedir(dir);
   free(cpath);
   
   // Sort
   for (int i = 0; i < result.count; i ++)
      [result[i] sortUsingComparator:^(RADirectoryItem* left, RADirectoryItem* right)
      {
         return (left.isDirectory != right.isDirectory) ?
                (left.isDirectory ? -1 : 1) :
                ([left.path caseInsensitiveCompare:right.path]);
      }];
     
   return result;
}

NSString* ra_ios_check_path(NSString* path)
{
   if (path && ra_ios_is_directory(path))
      return path;

   // The error message can be ugly if it is a long message, but it should never be displayed during normal operation.
   if (path)
      [RetroArch_iOS displayErrorMessage:[NSString stringWithFormat:@"Browsed path is not a directory: %@", path]];

   return [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"];
}

@implementation RADirectoryList
{
   NSString* _path;
   NSArray* _list;
}

+ (id)directoryListForPath:(NSString*)path
{
   path = ra_ios_check_path(path);
   return [[RADirectoryList alloc] initWithPath:path];
}

- (id)initWithPath:(NSString*)path
{
   self = [super initWithStyle:UITableViewStylePlain];

   _path = path;
   _list = ra_ios_list_directory(_path);

   [self setTitle: [_path lastPathComponent]];
   
   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = _list[indexPath.section][indexPath.row];

   if(path.isDirectory)
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListForPath:path.path] animated:YES];
   else
      [[RetroArch_iOS get] pushViewController:[[RAModuleList alloc] initWithGame:path.path] animated:YES];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
   return _list.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return [_list[section] count];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = _list[indexPath.section][indexPath.row];

   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"path"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"path"];
   cell.textLabel.text = [path.path lastPathComponent];
   cell.accessoryType = (path.isDirectory) ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
   cell.imageView.image = [UIImage imageNamed:(path.isDirectory) ? @"ic_dir" : @"ic_file"];
   return cell;
}

- (NSArray*)sectionIndexTitlesForTableView:(UITableView*)tableView
{
   return [NSArray arrayWithObjects:@"/", @"#", @"A", @"B", @"C", @"D", @"E", @"F", @"G", @"H", @"I", @"J", @"K", @"L", @"M",
                                          @"N", @"O", @"P", @"Q", @"R", @"S", @"T", @"U", @"V", @"W", @"X", @"Y", @"Z", nil];
}

@end
