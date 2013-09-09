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

#import "apple/common/RetroArch_Apple.h"
#import "views.h"

#include "conf/config_file.h"
#include "file.h"

@implementation RADirectoryItem
@end

@implementation RADirectoryList
{
   NSString* _path;
   NSMutableArray* _sectionNames;
   id<RADirectoryListDelegate> _delegate;
}


- (id)initWithPath:(NSString*)path delegate:(id<RADirectoryListDelegate>)delegate
{
   _path = path;
   _delegate = delegate;

   self = [super initWithStyle:UITableViewStylePlain];
   self.title = path.lastPathComponent;
   self.hidesHeaders = YES;
    
   NSMutableArray *toolbarButtons = [[NSMutableArray alloc] initWithCapacity:3];
    
   UIBarButtonItem *refreshButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemRefresh target:self action:@selector(refresh)];
   refreshButton.style = UIBarButtonItemStyleBordered;
   [toolbarButtons addObject:refreshButton];
    
   UIBarButtonItem *flexibleSpace = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:self action:nil];
   [toolbarButtons addObject:flexibleSpace];
    
   UIBarButtonItem *newFolderButton = [[UIBarButtonItem alloc] initWithTitle:@"New Folder" style:UIBarButtonItemStyleBordered target:self action:@selector(createNewFolder)];
   [toolbarButtons addObject:newFolderButton];
    
   [[[RetroArch_iOS get] toolbar] setItems:toolbarButtons];
   [self setToolbarItems:toolbarButtons];

   return self;
}

- (void)viewWillAppear:(BOOL)animated
{
   [self refresh];
}
 
- (void)viewDidDisappear:(BOOL)animated
{
   [self reset];
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
         item.path = @(contents->elems[i].data);
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
   [_delegate directoryList:self itemWasSelected:[self itemForIndexPath:indexPath]];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   static NSString* const cell_types[2] = { @"file", @"folder" };
   static NSString* const icon_types[2] = { @"ic_file", @"ic_dir" };
   static const UITableViewCellAccessoryType accessory_types[2] = { UITableViewCellAccessoryDetailDisclosureButton,
                                                                    UITableViewCellAccessoryDisclosureIndicator };
   RADirectoryItem* path = [self itemForIndexPath:indexPath];
   uint32_t type_id = path.isDirectory ? 1 : 0;

   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:cell_types[type_id]];
   if (!cell)
   {
      cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cell_types[type_id]];
      cell.imageView.image = [UIImage imageNamed:icon_types[type_id]];
      cell.accessoryType = accessory_types[type_id];
   }
   
   cell.textLabel.text = [path.path lastPathComponent];
    
   return cell;
}

- (void)tableView:(UITableView*)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath*)indexPath {
    self.selectedItem = [self itemForIndexPath:indexPath];
    UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
    UIActionSheet *menu = [[UIActionSheet alloc] initWithTitle:cell.textLabel.text delegate:self cancelButtonTitle:@"Cancel" destructiveButtonTitle:nil otherButtonTitles:@"Move", nil];

    [menu showInView:[self view]];
}

- (void)actionSheet:(UIActionSheet *)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex {
    if (buttonIndex == 0) {
        RAFoldersList *foldersListViewController = [[RAFoldersList alloc] initWithFilePath:self.selectedItem.path];
        UINavigationController *navigationController = [[UINavigationController alloc] initWithRootViewController:foldersListViewController];

        [self presentViewController:navigationController animated:YES completion:nil];
    }
}

- (NSArray*)sectionIndexTitlesForTableView:(UITableView*)tableView
{
   return _sectionNames;
}

- (void)tableView:(UITableView*)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath*)indexPath {
      if (editingStyle == UITableViewCellEditingStyleDelete) {
         NSFileManager *fileManager = [NSFileManager defaultManager];
         RADirectoryItem *path = (RADirectoryItem*)[self itemForIndexPath:indexPath];
         
         BOOL didRemoveItem = [fileManager removeItemAtPath:path.path error:nil];
          
         if (didRemoveItem) {
            [self refresh];
         } else {
            apple_display_alert(@"It was not possible to delete file.", 0);
         }
    }
}

- (void)createNewFolder {
    UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:@"Enter new folder name" message:@"" delegate:self cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
    alertView.alertViewStyle = UIAlertViewStylePlainTextInput;
    [alertView show];
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex {
    NSString *text = [[alertView textFieldAtIndex:0] text];
    
    if (buttonIndex == 1 && ![text isEqualToString:@""]) {
        NSString *directoryPath = [_path stringByAppendingPathComponent:text];
        BOOL didCreateFolder = [[NSFileManager defaultManager] createDirectoryAtPath:directoryPath withIntermediateDirectories:YES attributes:nil error:nil];
        
        if (didCreateFolder) {
            [self refresh];
        } else {
            apple_display_alert(@"It was not possible to create folder.", 0);
        }
    }
}

@end

@implementation RAModuleList
{
   id<RAModuleListDelegate> _delegate;
}

- (id)initWithGame:(NSString*)path delegate:(id<RAModuleListDelegate>)delegate
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   [self setTitle:path ? [path lastPathComponent] : @"Cores"];
   _delegate = delegate;

   // Load the modules with their data
   NSArray* moduleList = apple_get_modules();

   NSMutableArray* supported = [NSMutableArray arrayWithObject:@"Suggested Cores"];
   NSMutableArray* other = [NSMutableArray arrayWithObject:@"Other Cores"];
   
   for (RAModuleInfo* i in moduleList)
   {
      if (path && [i supportsFileAtPath:path]) [supported addObject:i];
      else                                     [other     addObject:i];
   }

   if (supported.count > 1)
      [self.sections addObject:supported];

   if (other.count > 1)
      [self.sections addObject:other];

   return self;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   [_delegate moduleList:self itemWasSelected:[self itemForIndexPath:indexPath]];
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
   cell.textLabel.text = info.description;
   objc_setAssociatedObject(cell.accessoryView, "MODULE", info, OBJC_ASSOCIATION_RETAIN_NONATOMIC);

   return cell;
}

@end

@implementation RAFoldersList {
    NSMutableArray *directories;
    NSString *currentDirectoryPath, *selectedFilePath, *fileName;
}

- (id)initWithFilePath:(NSString *)path
{
    self = [super initWithStyle:UITableViewStyleGrouped];

    selectedFilePath = path;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    fileName = [fileManager displayNameAtPath:path];
    currentDirectoryPath = [path stringByDeletingLastPathComponent];
    NSArray *files = [fileManager contentsOfDirectoryAtPath:currentDirectoryPath error:nil];
    directories = [[NSMutableArray alloc] init];

    for (int i = 0; i < files.count; i++) {
        NSString *filePath = [currentDirectoryPath stringByAppendingPathComponent:files[i]];

        BOOL isDir;
        if ([fileManager fileExistsAtPath:filePath isDirectory:&isDir] && isDir) {
            [directories addObject:files[i]];
        }
    }

    [self setTitle:[@"Move " stringByAppendingString:fileName]];

    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"Cancel" style:UIBarButtonItemStyleBordered target:self action:@selector(dismissViewController)];
}

- (void) dismissViewController
{
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return directories.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"Directory";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier];

    if (cell == nil) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:CellIdentifier];
    }

    cell.textLabel.text = directories[indexPath.row];

    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
    NSString *directoryPath = [currentDirectoryPath stringByAppendingPathComponent:cell.textLabel.text];
    NSString *newPath = [directoryPath stringByAppendingPathComponent:fileName];

    if (![[NSFileManager defaultManager] moveItemAtPath:selectedFilePath toPath:newPath error:nil])
        apple_display_alert(@"It was not possible to move the file", 0);

    [self dismissViewController];
}


@end
