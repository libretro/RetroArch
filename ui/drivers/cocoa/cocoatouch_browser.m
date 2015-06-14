/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <file/file_extract.h>
#include <file/dir_list.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <rhash.h>

#include "cocoa_common.h"
#include "../../../content.h"
#include "../../../general.h"

enum file_action
{
    FA_DELETE = 10000,
    FA_CREATE,
    FA_MOVE,
    FA_UNZIP
};

static const void* const associated_module_key = &associated_module_key;

static int zlib_extract_callback(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   char path[PATH_MAX_LENGTH];

   /* Make directory */
   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));
   path_basedir(path);

   if (!path_mkdir(path))
   {
      RARCH_ERR("Failed to create dir: %s.\n", path);
      return false;
   }

   /* Ignore directories */
   if (name[strlen(name) - 1] == '/')
      return 1;

   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));

   if (!zlib_perform_mode(path, valid_exts,
      cdata, cmode, csize, size, crc32, userdata))
   {
       if (cmode == 0)
       {
           RARCH_ERR("Failed to write file: %s.\n", path);
           return 0;
       }
       goto error;
   }

   return 1;
    
error:
   RARCH_ERR("Failed to deflate to: %s.\n", path);
   return 0;
}

static void unzip_file(const char* path, const char* output_directory)
{
   if (!path_file_exists(path))
      apple_display_alert("Could not locate zip file.", "Action Failed");
   else if (path_is_directory(output_directory))
      apple_display_alert("Output directory for zip must not already exist.", "Action Failed");
   else if (!path_mkdir(output_directory))
      apple_display_alert("Could not create output directory to extract zip.", "Action Failed");
   else if (!zlib_parse_file(path, NULL, zlib_extract_callback, (void*)output_directory))
      apple_display_alert("Could not process zip file.", "Action Failed");
}

static void file_action(enum file_action action, NSString* source, NSString* target)
{
   NSError* error         = nil;
   bool result            = false;
   NSFileManager* manager = [NSFileManager defaultManager];

   switch (action)
   {
      case FA_DELETE:
           result = [manager removeItemAtPath:target error:&error];
           break;
      case FA_CREATE:
           result = [manager createDirectoryAtPath:target withIntermediateDirectories:YES
                                        attributes:nil error:&error];
           break;
      case FA_MOVE:
           result = [manager moveItemAtPath:source toPath:target error:&error];
           break;
      case FA_UNZIP:
           unzip_file(source.UTF8String, target.UTF8String);
           break;
   }

   if (!result && error)
      apple_display_alert(error.localizedDescription.UTF8String, "Action failed");
}

@implementation RADirectoryItem
+ (RADirectoryItem*)directoryItemFromPath:(NSString*)path
{
   RADirectoryItem* item = [RADirectoryItem new];
   
   if (!item)
      return NULL;
   
   item.path             = path;
   item.isDirectory      = path_is_directory(path.UTF8String);
   
   return item;
}

+ (RADirectoryItem*)directoryItemFromElement:(struct string_list_elem*)element
{
   RADirectoryItem* item = [RADirectoryItem new];
   
   if (!item)
      return NULL;
   
   item.path             = BOXSTRING(element->data);
   item.isDirectory      = (element->attr.i == RARCH_DIRECTORY);
   
   return item;
}

- (UITableViewCell*)cellForTableView:(UITableView *)tableView
{
   static NSString* const cell_id       = @"path_item";
   static NSString* const icon_types[2] = { @"ic_file", @"ic_dir" };
   uint32_t type_id                     = self.isDirectory ? 1 : 0;
   UITableViewCell              *result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   
   if (!result)
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:cell_id];

   result.textLabel.text                = [self.path lastPathComponent];
   result.imageView.image               = [UIImage imageNamed:icon_types[type_id]];

   return result;
}

- (void)wasSelectedOnTableView:(UITableView *)tableView ofController:(UIViewController *)controller
{
   if (self.isDirectory)
      [(id)controller browseTo:self.path];
   else
      [(id)controller chooseAction]((id)controller, self);
}

@end

/*********************************************/
/* RAMenuItemBasic                           */
/* A simple menu item that displays a text   */
/* description and calls a block object when */
/* selected.                                 */
/*********************************************/
@interface RAMenuItemBasic : NSObject
@property (nonatomic) NSString* description;
@property (nonatomic) id userdata;
@property (copy) void (^action)(id userdata);
@property (copy) NSString* (^detail)(id userdata);

+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description action:(void (^)())action;
+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description action:(void (^)())action detail:(NSString* (^)())detail;
+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description association:(id)userdata action:(void (^)())action detail:(NSString* (^)())detail;

@end

/*********************************************/
/* RAMenuItemBasic                           */
/* A simple menu item that displays a text   */
/* description and calls a block object when */
/* selected.                                 */
/*********************************************/
@implementation RAMenuItemBasic
@synthesize description;
@synthesize userdata;
@synthesize action;
@synthesize detail;

+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description action:(void (^)())action
{
   return [self itemWithDescription:description action:action detail:Nil];
}

+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description action:(void (^)())action detail:(NSString* (^)())detail
{
   return [self itemWithDescription:description association:nil action:action detail:detail];
}

+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description association:(id)userdata action:(void (^)())action detail:(NSString* (^)())detail
{
   RAMenuItemBasic* item = [RAMenuItemBasic new];
   item.description = description;
   item.userdata = userdata;
   item.action = action;
   item.detail = detail;
   return item;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   static NSString* const cell_id = @"text";
   
   UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
   
   result.selectionStyle = UITableViewCellSelectionStyleNone;
   result.textLabel.text = self.description;
   result.detailTextLabel.text = self.detail ? self.detail(self.userdata) : nil;
   return result;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   if (self.action)
      self.action(self.userdata);
}

@end

@implementation RADirectoryList

- (id)initWithPath:(NSString*)path extensions:(const char*)extensions 
            action:(void (^)(RADirectoryList* list, RADirectoryItem* item))action
{
   if ((self = [super initWithStyle:UITableViewStylePlain]))
   {
      NSMutableArray *toolbarButtons;
      self.path                              = path ? path : NSHomeDirectory();
      self.chooseAction                      = action;
      self.extensions                        = extensions ? BOXSTRING(extensions) : 0;
      self.hidesHeaders                      = YES;

      self.navigationItem.leftBarButtonItem  = [[UIBarButtonItem alloc] initWithTitle:BOXSTRING("Up") style:UIBarButtonItemStyleBordered target:self
                                                                       action:@selector(gotoParent)];
      self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemCancel target:self
                                                                        action:@selector(cancelBrowser)];

      /* NOTE: The "App" and "Root" buttons aren't really needed for non-jailbreak devices. */
      toolbarButtons                         = [NSMutableArray arrayWithObjects:
         [[UIBarButtonItem alloc] initWithTitle:BOXSTRING("Home") style:UIBarButtonItemStyleBordered target:self
                                  action:@selector(gotoHomeDir)],
         [[UIBarButtonItem alloc] initWithTitle:BOXSTRING("App") style:UIBarButtonItemStyleBordered target:self
                                  action:@selector(gotoAppDir)],
         [[UIBarButtonItem alloc] initWithTitle:BOXSTRING("Root") style:UIBarButtonItemStyleBordered target:self
                                  action:@selector(gotoRootDir)],
         [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:self
                                  action:nil],
         [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemRefresh target:self
                                  action:@selector(refresh)],
         [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self
                                  action:@selector(createNewFolder)],
         nil
      ];

      self.toolbarItems                      = toolbarButtons;

      [self.tableView addGestureRecognizer:[[UILongPressGestureRecognizer alloc] initWithTarget:self
                      action:@selector(fileAction:)]];
   }

   return self;
}

- (void)cancelBrowser
{
   [self.navigationController popViewControllerAnimated:YES];
}

- (void)gotoParent
{
   [self browseTo:[self.path stringByDeletingLastPathComponent]];
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
   [self browseTo: self.path];
}

- (void)browseTo:(NSString*)path
{
   NSString *i;
   struct string_list *contents = NULL;
   settings_t         *settings = config_get_ptr();

   self.path                    = path;
   self.title                   = path.lastPathComponent;

   /* Need one array per section. */
   self.sections                = [NSMutableArray array];

   for (i in [self sectionIndexTitlesForTableView:self.tableView])
      [self.sections addObject:[NSMutableArray arrayWithObject:i]];

   /* List contents */
   contents                     = dir_list_new(path.UTF8String,
         settings->menu.navigation.browser.filter.supported_extensions_enable ? self.extensions.UTF8String : NULL, true);

   if (contents)
   {
      ssize_t i;
      RADirectoryList __weak* weakSelf = self;

      if (self.allowBlank)
         [self.sections[0] addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING("[ Use Empty Path ]")
                                                                   action:^{ weakSelf.chooseAction(weakSelf, nil); }]];
      if (self.forDirectory)
         [self.sections[0] addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING("[ Use This Folder ]")
                                                                   action:^{ weakSelf.chooseAction(weakSelf, [RADirectoryItem directoryItemFromPath:path]); }]];
      dir_list_sort(contents, true);

      for (i = 0; i < contents->size; i ++)
      {
         const char *basename = path_basename(contents->elems[i].data);
         char is_directory    = (contents->elems[i].attr.i == RARCH_DIRECTORY);
         uint32_t section     = is_directory ? 0 : isalpha(basename[0]) ? (toupper(basename[0]) - 'A') + 2 : 1;

         if (! ( self.forDirectory && ! is_directory ))
            [self.sections[section] addObject:[RADirectoryItem directoryItemFromElement:&contents->elems[i]]];
      }

      dir_list_free(contents);
   }
   else
   {
      [self gotoHomeDir];
      return;
   }

   [self.tableView scrollRectToVisible:CGRectMake(0, 0, 1, 1) animated:NO];
   [UIView transitionWithView:self.tableView duration:.25f options:UIViewAnimationOptionTransitionCrossDissolve
      animations:
      ^{
         [self.tableView reloadData];
      } completion:nil];
}

- (void)viewWillAppear:(BOOL)animated
{
   [super viewWillAppear:animated];
   [self browseTo: self.path];
}

- (NSArray*)sectionIndexTitlesForTableView:(UITableView*)tableView
{
   static NSArray* names = nil;

   if (!names)
      names = @[@"/", @"#", @"A", @"B", @"C", @"D", @"E", @"F", @"G", @"H", @"I", @"J", @"K", @"L",
                @"M", @"N", @"O", @"P", @"Q", @"R", @"S", @"T", @"U", @"V", @"W", @"X", @"Y", @"Z"];

   return names;
}

/* File management
 * Called as a selector from a toolbar button. */
- (void)createNewFolder
{
   UIAlertView* alertView   = [[UIAlertView alloc] initWithTitle:BOXSTRING("Enter new folder name") message:BOXSTRING("") delegate:self
                                                  cancelButtonTitle:BOXSTRING("Cancel") otherButtonTitles:BOXSTRING("OK"), nil];
   alertView.alertViewStyle = UIAlertViewStylePlainTextInput;
   alertView.tag            = FA_CREATE;
   [alertView show];
}

/* Called by the long press gesture recognizer. */
- (void)fileAction:(UILongPressGestureRecognizer*)gesture
{
   if (gesture.state == UIGestureRecognizerStateBegan)
   {
      CGPoint point         = [gesture locationInView:self.tableView];
      NSIndexPath* idx_path = [self.tableView indexPathForRowAtPoint:point];

      if (idx_path)
      {
         int major, minor;
         bool is_zip;
         UIActionSheet *menu;
         NSString *button4_name, *button5_name;
          
         get_ios_version(&major, &minor);
          
         button4_name = (major >= 7) ? BOXSTRING("AirDrop") : BOXSTRING("Delete");
         button5_name = (major >= 7) ? BOXSTRING("Delete")  : nil;
         
         self.selectedItem      = [self itemForIndexPath:idx_path];
         is_zip                 = !(strcmp(self.selectedItem.path.pathExtension.UTF8String, "zip"));

         menu = [[UIActionSheet alloc] initWithTitle:self.selectedItem.path.lastPathComponent delegate:self
                                   cancelButtonTitle:BOXSTRING("Cancel") destructiveButtonTitle:nil
                                   otherButtonTitles:is_zip ? BOXSTRING("Unzip") : BOXSTRING("Zip"), BOXSTRING("Move"), BOXSTRING("Rename"), button4_name, button5_name, nil];
         [menu showFromToolbar:self.navigationController.toolbar];
         
      }
   }
}

#define FILEBROWSER_ACTION_UNZIP    0x0e3f899bU
#define FILEBROWSER_ACTION_MOVE     0x7c89307cU
#define FILEBROWSER_ACTION_RENAME   0xce87affdU
#define FILEBROWSER_ACTION_AIRDROP  0x8c6cdb56U
#define FILEBROWSER_ACTION_DELETE   0xadde7058U
#define FILEBROWSER_ACTION_CANCEL   0xab41f40bU

/* Called by the action sheet created in (void)fileAction: */
- (void)actionSheet:(UIActionSheet*)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex
{
   uint32_t action_str_hash;
   int major, minor;
   NSString* target       = self.selectedItem.path;
   NSString* action       = [actionSheet buttonTitleAtIndex:buttonIndex];
   const char *action_str = action.UTF8String;
    
   get_ios_version(&major, &minor);

   action_str_hash = djb2_calculate(action_str);
    
   (void)major;
   (void)minor;

   switch (action_str_hash)
   {
      case FILEBROWSER_ACTION_UNZIP:
         {
            UIAlertView* alertView              = [[UIAlertView alloc] initWithTitle:BOXSTRING("Enter target directory") message:@"" delegate:self
                                                                   cancelButtonTitle:BOXSTRING("Cancel") otherButtonTitles:BOXSTRING("OK"), nil];
            alertView.alertViewStyle            = UIAlertViewStylePlainTextInput;
            alertView.tag                       = FA_UNZIP;
            [alertView textFieldAtIndex:0].text = [[target lastPathComponent] stringByDeletingPathExtension];
            [alertView show];
         }
         break;
      case FILEBROWSER_ACTION_MOVE:
         [self.navigationController pushViewController:[[RAFoldersList alloc] initWithFilePath:target] animated:YES];
         break;
      case FILEBROWSER_ACTION_RENAME:
         {
            UIAlertView* alertView              = [[UIAlertView alloc] 
               initWithTitle:BOXSTRING("Enter new name") message:@"" 
                    delegate:self cancelButtonTitle:BOXSTRING("Cancel") otherButtonTitles:BOXSTRING("OK"), nil];
            alertView.alertViewStyle            = UIAlertViewStylePlainTextInput;
            alertView.tag                       = FA_MOVE;
            [alertView textFieldAtIndex:0].text = target.lastPathComponent;
            [alertView show];
         }
         break;
      case FILEBROWSER_ACTION_AIRDROP:
#ifdef __IPHONE_7_0
         if ((major >= 7))
         {
            /* TODO: ZIP if not already zipped. */
            NSURL                    *url = [NSURL fileURLWithPath:self.selectedItem.path isDirectory:self.selectedItem.isDirectory];
            NSArray                *items = [NSArray arrayWithObject:url];
            UIActivityViewController* avc = [[UIActivityViewController alloc] initWithActivityItems:items applicationActivities:nil];

            [self presentViewController:avc animated:YES completion:nil];
         }
#endif
         break;
      case FILEBROWSER_ACTION_DELETE:
         {
            UIAlertView         *alertView = [[UIAlertView alloc] initWithTitle:BOXSTRING("Really delete?") message:@"" delegate:self cancelButtonTitle:BOXSTRING("Cancel") otherButtonTitles:BOXSTRING("OK"), nil];
            alertView.tag                  = FA_DELETE;
            [alertView show];
         }
         break;
      case FILEBROWSER_ACTION_CANCEL:
         apple_display_alert("Action not supported.", "Action Failed");
         break;
   }
}

/* Called by various alert views created in this class, 
 * the alertView.tag value is the action to take. */
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
         file_action((enum file_action)alertView.tag, self.selectedItem.path, [self.path stringByAppendingPathComponent:text]);
   }

   [self browseTo: self.path];
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
      NSString *sourceItem           = path.stringByDeletingLastPathComponent;  /* Parent item */
      RAMenuItemBasic *parentItem    = [RAMenuItemBasic itemWithDescription:BOXSTRING("<Parent>") association:sourceItem.stringByDeletingLastPathComponent
         action:^(id userdata){ [weakSelf moveInto:userdata]; } detail:NULL];
      struct string_list *contents   = dir_list_new([path stringByDeletingLastPathComponent].UTF8String, NULL, true);
      NSMutableArray *items          = [NSMutableArray arrayWithObject:BOXSTRING("")];

      [self.sections addObject:@[BOXSTRING(""), parentItem]];

      self.path = path;

      if (contents)
      {
         size_t i;
         dir_list_sort(contents, true);

         for (i = 0; i < contents->size; i ++)
         {
            if (contents->elems[i].attr.i == RARCH_DIRECTORY)
            {
               const char* basename  = path_basename(contents->elems[i].data);
               RAMenuItemBasic* item = [RAMenuItemBasic itemWithDescription:BOXSTRING(basename) association:BOXSTRING(contents->elems[i].data)
                                                                     action:^(id userdata){ [weakSelf moveInto:userdata]; } detail:NULL];
               [items addObject:item];
            }
         }
         dir_list_free(contents);
      }

      [self setTitle:[BOXSTRING("Move ") stringByAppendingString: self.path.lastPathComponent]];
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
