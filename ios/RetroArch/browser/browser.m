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
#include "conf/config_file.h"

@interface RADirectoryItem : NSObject
@property (strong) NSString* path;
@property bool isDirectory;
@end

@implementation RADirectoryItem
@end

@implementation RADirectoryList
{
   NSMutableArray* _list;
}

+ (id)directoryListAtBrowseRoot
{
   NSString* rootPath = RetroArch_iOS.get.documentsDirectory;
   NSString* ragPath = [rootPath stringByAppendingPathComponent:@"RetroArchGames"];
   
   return [RADirectoryList directoryListForPath:path_is_directory(ragPath.UTF8String) ? ragPath : rootPath];
}

+ (id)directoryListForPath:(NSString*)path
{
   // NOTE: Don't remove or ignore this abstraction, this function will be expanded when cover art comes back.
   return [[RADirectoryList alloc] initWithPath:path];
}

- (id)initWithPath:(NSString*)path
{
   self = [super initWithStyle:UITableViewStylePlain];
   [self setTitle: [path lastPathComponent]];

   // Need one array per section
   _list = [NSMutableArray arrayWithCapacity:28];
   for (int i = 0; i < 28; i ++)
      [_list addObject:[NSMutableArray array]];
   
   // List contents
   struct string_list* contents = dir_list_new(path.UTF8String, 0, true);
   
   if (contents)
   {
      dir_list_sort(contents, true);
   
      for (int i = 0; i < contents->size; i ++)
      {
         const char* basename = path_basename(contents->elems[i].data);
      
         if (basename[0] == '.')
            continue;
      
         uint32_t section = isalpha(basename[0]) ? (toupper(basename[0]) - 'A') + 2 : 1;
         section = contents->elems[i].attr.b ? 0 : section;
         
         RADirectoryItem* item = RADirectoryItem.new;
         item.path = [NSString stringWithUTF8String:contents->elems[i].data];
         item.isDirectory = contents->elems[i].attr.b;
         [_list[section] addObject:item];
      }
   
      dir_list_free(contents);
   }
   else
      [RetroArch_iOS displayErrorMessage:[NSString stringWithFormat:@"Browsed path is not a directory: %@", path]];
   
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
