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

#import "browser.h"

static NSString* check_path(NSString* path)
{
   if (path && !ra_ios_is_directory(path))
   {
      [RetroArch_iOS displayErrorMessage:@"Browsed path is not a directory."];
      return nil;
   }
   else
      return path;
}

@implementation RADirectoryList
{
   NSString* _path;
   NSArray* _list;
}

+ (id)directoryListWithPath:(NSString*)path
{
   path = check_path(path);
   
   NSRegularExpression* expr = nil;
   RADirectoryFilterList* filterList = [RADirectoryFilterList directoryFilterListAtPath:path useExpression:&expr];

   return filterList ? filterList : [RADirectoryList directoryListWithPath:path filter:expr];
}

+ (id)directoryListWithPath:(NSString*)path filter:(NSRegularExpression*)regex
{
   path = check_path(path);

   if ([UICollectionViewController instancesRespondToSelector:@selector(initWithCollectionViewLayout:)])
   {
      NSString* coverDir = path ? [path stringByAppendingPathComponent:@".coverart"] : nil;
      if (coverDir && ra_ios_is_directory(coverDir) && ra_ios_is_file([coverDir stringByAppendingPathComponent:@"template.png"]))
         return [[RADirectoryGrid alloc] initWithPath:path filter:regex];
   }

   return [[RADirectoryList alloc] initWithPath:path filter:regex];
}

- (id)initWithPath:(NSString*)path filter:(NSRegularExpression*)regex
{
   self = [super initWithStyle:UITableViewStylePlain];

   _path = path ? path : ra_ios_get_browser_root();
   _list = ra_ios_list_directory(_path, regex);

   self.navigationItem.rightBarButtonItem = [RetroArch_iOS get].settings_button;
   [self setTitle: [_path lastPathComponent]];
   
   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* path = [_list objectAtIndex: indexPath.row];

   if(ra_ios_is_directory(path))
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListWithPath:path]];
   else
      [[RetroArch_iOS get] runGame:path];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return [_list count];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   NSString* path = [_list objectAtIndex: indexPath.row];
   BOOL isdir = ra_ios_is_directory(path);

   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"path"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"path"];
   cell.textLabel.text = [path lastPathComponent];
   cell.accessoryType = (isdir) ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
   cell.imageView.image = (isdir) ? [RetroArch_iOS get].folder_icon : [RetroArch_iOS get].file_icon;
   return cell;
}

@end
