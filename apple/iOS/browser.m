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

#include "file_extract.h"

#import "apple/common/RetroArch_Apple.h"
#import "views.h"

#include "conf/config_file.h"
#include "file.h"

static const void* const associated_module_key = &associated_module_key;

static bool zlib_extract_callback(const char *name,
                                const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
                                uint32_t crc32, void *userdata)
{
   char path[PATH_MAX];   
   
   if (cmode != 0 && cmode != 8)
   {
      apple_display_alert([NSString stringWithFormat:@"Could not unzip %s (unknown mode %d)", name, cmode], @"Action Failed");
      return false;
   }

   // Make directory
   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));
   path_basedir(path);
   path_mkdir(path);

   // Ignore directories
   if (name[strlen(name) - 1] == '/')
      return true;

   snprintf(path, PATH_MAX, "%s/%s", (const char*)userdata, name);
   
   switch (cmode)
   {
      case 0: // Uncompressed
         write_file(path, cdata, size);
         return true;
      case 8: // Deflate
         zlib_inflate_data_to_file(path, cdata, csize, size, crc32);
         return true;
   }

   return true;
}

static void unzip_file(const char* path, const char* output_directory)
{
   if (!path_file_exists(path))
      apple_display_alert(@"Could not locate zip file.", @"Action Failed");
   else if (path_is_directory(output_directory))
      apple_display_alert(@"Output directory for zip must not already exist.", @"Action Failed");
   else if (!path_mkdir(output_directory))
      apple_display_alert(@"Could not create output directory to extract zip.", @"Action Failed");
   else if (!zlib_parse_file(path, zlib_extract_callback, (void*)output_directory))
      apple_display_alert(@"Could not process zip file.", @"Action Failed");
}

enum file_action { FA_DELETE = 10000, FA_CREATE, FA_MOVE, FA_UNZIP };
static void file_action(enum file_action action, NSString* source, NSString* target)
{
   NSError* error = nil;
   bool result = false;
   
   NSFileManager* manager = [NSFileManager defaultManager];
   
   switch (action)
   {
      case FA_DELETE: result = [manager removeItemAtPath:target error:&error]; break;
      case FA_CREATE: result = [manager createDirectoryAtPath:target withIntermediateDirectories:YES
                                        attributes:nil error:&error]; break;
      case FA_MOVE:   result = [manager moveItemAtPath:source toPath:target error:&error]; break;
      case FA_UNZIP:  unzip_file([source UTF8String], [target UTF8String]); break;
   }

   if (!result && error)
      apple_display_alert(error.localizedDescription, @"Action failed");
}

@implementation RADirectoryItem
+ (RADirectoryItem*)directoryItemFromPath:(NSString*)path
{
   RADirectoryItem* item = [RADirectoryItem new];
   item.path = path;
   item.isDirectory = path_is_absolute(path.UTF8String);
   return item;
}

+ (RADirectoryItem*)directoryItemFromElement:(struct string_list_elem*)element
{
   RADirectoryItem* item = [RADirectoryItem new];
   item.path = BOXSTRING(element->data);
   item.isDirectory = element->attr.b;
   return item;
}

- (UITableViewCell*)cellForTableView:(UITableView *)tableView
{
   static NSString* const cell_id = @"path_item";
   static NSString* const icon_types[2] = { @"ic_file", @"ic_dir" };
   
   uint32_t type_id = self.isDirectory ? 1 : 0;
   
   UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cell_id];

   result.textLabel.text = [self.path lastPathComponent];
   result.imageView.image = [UIImage imageNamed:icon_types[type_id]];
   
   return result;
}

- (void)wasSelectedOnTableView:(UITableView *)tableView ofController:(UIViewController *)controller
{
   if (self.isDirectory)
      [(id)controller browseTo:self.path];
   else
      [[(id)controller directoryDelegate] directoryList:controller itemWasSelected:self];
}

@end

@implementation RADirectoryList
{
   NSString* _path;
   NSString* _extensions;
}

- (id)initWithPath:(NSString*)path extensions:(const char*)extensions forDirectory:(bool)forDirectory delegate:(id<RADirectoryListDelegate>)delegate
{
   if ((self = [super initWithStyle:UITableViewStylePlain]))
   {
      _path = path ? path : NSHomeDirectory();
      _directoryDelegate = delegate;
      _extensions = extensions ? BOXSTRING(extensions) : 0;

      self = [super initWithStyle:UITableViewStylePlain];
      self.hidesHeaders = YES;

      // NOTE: The "App" and "Root" buttons aren't really needed for non-jailbreak devices.
      self.toolbarItems =
      @[
         [[UIBarButtonItem alloc] initWithTitle:@"Home" style:UIBarButtonItemStyleBordered target:self
                                  action:@selector(gotoHomeDir)],
         [[UIBarButtonItem alloc] initWithTitle:@"App" style:UIBarButtonItemStyleBordered target:self
                                  action:@selector(gotoAppDir)],
         [[UIBarButtonItem alloc] initWithTitle:@"Root" style:UIBarButtonItemStyleBordered target:self
                                  action:@selector(gotoRootDir)],
         [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:self
                                  action:nil],
         [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemRefresh target:self
                                  action:@selector(refresh)],
         [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self
                                  action:@selector(createNewFolder)]
      ];
      
      [self.tableView addGestureRecognizer:[[UILongPressGestureRecognizer alloc] initWithTarget:self
                      action:@selector(fileAction:)]];
      
      self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Up"
                                                style:UIBarButtonItemStyleBordered target:self action:@selector(gotoParent)];
   }

   return self;
}

- (void)gotoHomeDir
{
    [self browseTo:NSHomeDirectory()];
}

- (void)gotoAppDir
{
    [self browseTo:NSBundle.mainBundle.bundlePath];
}

- (void)gotoRootDir
{
    [self browseTo:@"/"];
}

- (void)refresh
{
   [self browseTo:_path];
}

- (void)browseTo:(NSString*)path
{
   _path = path;
   self.title = _path.lastPathComponent;

   // Need one array per section
   self.sections = [NSMutableArray array];
   
   for (NSString* i in [self sectionIndexTitlesForTableView:self.tableView])
      [self.sections addObject:[NSMutableArray arrayWithObject:i]];
   
   // List contents
   struct string_list* contents = dir_list_new(_path.UTF8String, _extensions.UTF8String, true);
   
   if (contents)
   {
      dir_list_sort(contents, true);
   
      for (int i = 0; i < contents->size; i ++)
      {
         const char* basename = path_basename(contents->elems[i].data);
      
         uint32_t section = isalpha(basename[0]) ? (toupper(basename[0]) - 'A') + 2 : 1;
         section = contents->elems[i].attr.b ? 0 : section;

         [self.sections[section] addObject:[RADirectoryItem directoryItemFromElement:&contents->elems[i]]];
      }
   
      dir_list_free(contents);
   }
   else
      apple_display_alert([NSString stringWithFormat:@"Browsed path is not a directory: %@", _path], 0);

   [self.tableView scrollRectToVisible:CGRectMake(0, 0, 1, 1) animated:NO];
   [UIView transitionWithView:self.tableView duration:.25f options:UIViewAnimationOptionTransitionCrossDissolve
      animations:
      ^{
         [self.tableView reloadData];
      } completion:nil];
}

- (void)gotoParent
{
   [self browseTo:[_path stringByDeletingLastPathComponent]];
}

- (void)viewWillAppear:(BOOL)animated
{
   [super viewWillAppear:animated];
   [self browseTo:_path];
}

- (NSArray*)sectionIndexTitlesForTableView:(UITableView*)tableView
{
   static NSArray* names = nil;

   if (!names)
      names = @[@"/", @"#", @"A", @"B", @"C", @"D", @"E", @"F", @"G", @"H", @"I", @"J", @"K", @"L",
                @"M", @"N", @"O", @"P", @"Q", @"R", @"S", @"T", @"U", @"V", @"W", @"X", @"Y", @"Z"];

   return names;
}

// File management
// Called as a selector from a toolbar button
- (void)createNewFolder
{
   UIAlertView* alertView = [[UIAlertView alloc] initWithTitle:@"Enter new folder name" message:@"" delegate:self
                                                  cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
   alertView.alertViewStyle = UIAlertViewStylePlainTextInput;
   alertView.tag = FA_CREATE;
   [alertView show];
}

// Called by the long press gesture recognizer
- (void)fileAction:(UILongPressGestureRecognizer*)gesture
{
   if (gesture.state == UIGestureRecognizerStateBegan)
   {
      CGPoint point = [gesture locationInView:self.tableView];
      NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:point];
      
      if (indexPath)
      {
         self.selectedItem = [self itemForIndexPath:indexPath];
         bool is_zip = [[self.selectedItem.path pathExtension] isEqualToString:@"zip"];

         UIActionSheet* menu = [[UIActionSheet alloc] initWithTitle:self.selectedItem.path.lastPathComponent delegate:self
                                                      cancelButtonTitle:@"Cancel" destructiveButtonTitle:nil
                                                      otherButtonTitles:is_zip ? @"Unzip" : @"Zip", @"Move", @"Rename", @"Delete", nil];
         menu.destructiveButtonIndex = 3;
         [menu showFromToolbar:self.navigationController.toolbar];
         
      }
   }
}

// Called by the action sheet created in (void)fileAction:
- (void)actionSheet:(UIActionSheet*)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex
{
   NSString* target = self.selectedItem.path;

   // Zip/Unzip
   if (buttonIndex == actionSheet.firstOtherButtonIndex + 0)
   {
      if ([[self.selectedItem.path pathExtension] isEqualToString:@"zip"])
      {
         UIAlertView* alertView = [[UIAlertView alloc] initWithTitle:@"Enter target directory" message:@"" delegate:self
                                                   cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
         alertView.alertViewStyle = UIAlertViewStylePlainTextInput;
         alertView.tag = FA_UNZIP;
         [alertView textFieldAtIndex:0].text = [[target lastPathComponent] stringByDeletingPathExtension];
         [alertView show];
      }
      else
         apple_display_alert(@"Action not supported.", @"Action Failed");
   }
   // Move
   if (buttonIndex == actionSheet.firstOtherButtonIndex + 1)
      [self.navigationController pushViewController:[[RAFoldersList alloc] initWithFilePath:target] animated:YES];
   // Rename
   else if (buttonIndex == actionSheet.firstOtherButtonIndex + 2)
   {
      UIAlertView* alertView = [[UIAlertView alloc] initWithTitle:@"Enter new name" message:@"" delegate:self
                                                    cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
      alertView.alertViewStyle = UIAlertViewStylePlainTextInput;
      alertView.tag = FA_MOVE;
      [alertView textFieldAtIndex:0].text = target.lastPathComponent;
      [alertView show];
   }
   // Delete
   else if (buttonIndex == actionSheet.destructiveButtonIndex)
   {
      UIAlertView* alertView = [[UIAlertView alloc] initWithTitle:@"Really delete?" message:@"" delegate:self
                                                    cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
      alertView.tag = FA_DELETE;
      [alertView show];
   }
}

// Called by various alert views created in this class, the alertView.tag value is the action to take.
- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   if (buttonIndex != alertView.firstOtherButtonIndex)
      return;

   if (alertView.tag == FA_DELETE)
      file_action(FA_DELETE, nil, self.selectedItem.path);
   else
   {
      NSString* text = [alertView textFieldAtIndex:0].text;

      if (text.length)
         file_action(alertView.tag, self.selectedItem.path, [_path stringByAppendingPathComponent:text]);
   }

   [self browseTo:_path];
}

@end

@interface RAFoldersList()
@property (nonatomic) NSString* path;
@end

@implementation RAFoldersList

- (id)initWithFilePath:(NSString*)path
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      RAFoldersList* __weak weakSelf = self;
      _path = path;

      // Parent item
      NSString* sourceItem = _path.stringByDeletingLastPathComponent;
      
      RAMenuItemBasic* parentItem = [RAMenuItemBasic itemWithDescription:@"<Parent>" association:sourceItem.stringByDeletingLastPathComponent
         action:^(id userdata){ [weakSelf moveInto:userdata]; } detail:NULL];
      [self.sections addObject:@[@"", parentItem]];


      // List contents
      struct string_list* contents = dir_list_new([_path stringByDeletingLastPathComponent].UTF8String, 0, true);
      NSMutableArray* items = [NSMutableArray arrayWithObject:@""];
   
      if (contents)
      {
         dir_list_sort(contents, true);

         for (int i = 0; i < contents->size; i ++)
         {
            if (contents->elems[i].attr.b)
            {
               const char* basename = path_basename(contents->elems[i].data);
               
               RAMenuItemBasic* item = [RAMenuItemBasic itemWithDescription:BOXSTRING(basename) association:BOXSTRING(contents->elems[i].data)
                  action:^(id userdata){ [weakSelf moveInto:userdata]; } detail:NULL];
               [items addObject:item];
            }
         }
   
         dir_list_free(contents);
      }

      [self setTitle:[@"Move " stringByAppendingString:_path.lastPathComponent]];
      
      [self.sections addObject:items];
   }

   return self;
}

- (void)moveInto:(NSString*)path
{
   NSString* targetPath = [path stringByAppendingPathComponent:self.path.lastPathComponent];
   file_action(FA_MOVE, self.path, targetPath);
   [self.navigationController popViewControllerAnimated:YES];
}

@end
