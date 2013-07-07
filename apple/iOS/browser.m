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

#import "../RetroArch/RetroArch_Apple.h"
#import "views.h"

#include "conf/config_file.h"
#include "file.h"

@interface RADirectoryItem : NSObject
@property (strong) NSString* path;
@property bool isDirectory;
@end

@implementation RADirectoryItem
@end

@implementation RADirectoryList
{
   NSString* _path;
   NSMutableArray* _sectionNames;
}

+ (id)directoryListAtBrowseRoot
{
   NSString* rootPath = RetroArch_iOS.get.documentsDirectory;
   NSString* ragPath = [rootPath stringByAppendingPathComponent:@"RetroArchGames"];
   
   RADirectoryList* list = [RADirectoryList directoryListForPath:path_is_directory(ragPath.UTF8String) ? ragPath : rootPath];
   list.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Refresh"
                                                                            style:UIBarButtonItemStyleBordered
                                                                           target:list
                                                                           action:@selector(refresh)];
                                            
   return list;
}

+ (id)directoryListForPath:(NSString*)path
{
   // NOTE: Don't remove or ignore this abstraction, this function will be expanded when cover art comes back.
   return [[RADirectoryList alloc] initWithPath:path];
}

- (id)initWithPath:(NSString*)path
{
   _path = path;

   self = [super initWithStyle:UITableViewStylePlain];
   self.title = path.lastPathComponent;
   self.hidesHeaders = YES;
   [self refresh];

   return self;
}

- (void)refresh
{
   static const char sectionNames[28] = { '/', '#', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
                                          'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z' };
   static const uint32_t sectionCount = sizeof(sectionNames) / sizeof(sectionNames[0]);

   self.sections = [NSMutableArray array];

   // Need one array per section
   NSMutableArray* sectionLists[sectionCount];
   for (int i = 0; i != sectionCount; i ++)
      sectionLists[i] = [NSMutableArray arrayWithObject:[NSString stringWithFormat:@"%c", sectionNames[i]]];
   
   // List contents
   struct string_list* contents = dir_list_new(_path.UTF8String, 0, true);
   
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
         [sectionLists[section] addObject:item];
      }
   
      dir_list_free(contents);
      
      // Add the sections
      _sectionNames = [NSMutableArray array];
      for (int i = 0; i != sectionCount; i ++)
      {
         [self.sections addObject:sectionLists[i]];
         [_sectionNames addObject:sectionLists[i][0]];
      }
   }
   else
      apple_display_alert([NSString stringWithFormat:@"Browsed path is not a directory: %@", _path], 0);
   
   [self.tableView reloadData];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = (RADirectoryItem*)[self itemForIndexPath:indexPath];

   if(path.isDirectory)
      [[RetroArch_iOS get] pushViewController:[RADirectoryList directoryListForPath:path.path] animated:YES];
   else
   {
      if (access(_path.UTF8String, R_OK | W_OK | X_OK))
         apple_display_alert(@"The directory containing the selected file has limited permissions. This may "
                              "prevent zipped games from loading, and will cause some cores to not function.", 0);

      [[RetroArch_iOS get] pushViewController:[[RAModuleList alloc] initWithGame:path.path] animated:YES];
   }
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   RADirectoryItem* path = (RADirectoryItem*)[self itemForIndexPath:indexPath];

   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"path"];
   cell = (cell != nil) ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"path"];
   cell.textLabel.text = [path.path lastPathComponent];
   cell.accessoryType = (path.isDirectory) ? UITableViewCellAccessoryDisclosureIndicator : UITableViewCellAccessoryNone;
   cell.imageView.image = [UIImage imageNamed:(path.isDirectory) ? @"ic_dir" : @"ic_file"];
   return cell;
}

- (NSArray*)sectionIndexTitlesForTableView:(UITableView*)tableView
{
   return _sectionNames;
}

@end

@implementation RAModuleList
{
   NSString* _game;
}

- (id)initWithGame:(NSString*)path
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   [self setTitle:[path lastPathComponent]];
   
   _game = path;

   // Load the modules with their data
   NSArray* moduleList = [RAModuleInfo getModules];

   NSMutableArray* supported = [NSMutableArray arrayWithObject:@"Suggested Cores"];
   NSMutableArray* other = [NSMutableArray arrayWithObject:@"Other Cores"];
   
   for (RAModuleInfo* i in moduleList)
   {
      if ([i supportsFileAtPath:_game]) [supported addObject:i];
      else                              [other     addObject:i];
   }

   if (supported.count > 1)
      [self.sections addObject:supported];

   if (other.count > 1)
      [self.sections addObject:other];

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   apple_run_core((RAModuleInfo*)[self itemForIndexPath:indexPath], _game.UTF8String);
}

- (void)infoButtonTapped:(id)sender
{
   RAModuleInfo* info = objc_getAssociatedObject(sender, "MODULE");
   if (info && info.data)
      [RetroArch_iOS.get pushViewController:[[RAModuleInfoList alloc] initWithModuleInfo:info] animated:YES];
   else
      apple_display_alert(@"No information available.", 0);
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"module"];
   
   if (!cell)
   {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"module"];
      
      UIButton* infoButton = [UIButton buttonWithType:UIButtonTypeInfoDark];
      [infoButton addTarget:self action:@selector(infoButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
      cell.accessoryView = infoButton;
   }
   
   RAModuleInfo* info = (RAModuleInfo*)[self itemForIndexPath:indexPath];
   cell.textLabel.text = info.displayName;
   objc_setAssociatedObject(cell.accessoryView, "MODULE", info, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

   return cell;
}

@end
