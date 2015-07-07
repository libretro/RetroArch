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

#include <objc/runtime.h>

#include <boolean.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>

#include "cocoa_common.h"
#include "../../../input/input_common.h"
#include "../../../input/input_keymaps.h"
#include "../../../input/drivers/cocoa_input.h"

#include "../../../menu/menu_entry.h"

// Menu Support

static const void* const associated_delegate_key = &associated_delegate_key;

typedef void (^RAActionSheetCallback)(UIActionSheet*, NSInteger);

@interface RARunActionSheetDelegate : NSObject<UIActionSheetDelegate>
@property (nonatomic, copy) RAActionSheetCallback callbackBlock;
@end

@implementation RARunActionSheetDelegate

- (id)initWithCallbackBlock:(RAActionSheetCallback)callback
{
   if ((self = [super init]))
      _callbackBlock = callback;
   return self;
}

- (void)actionSheet:(UIActionSheet *)actionSheet willDismissWithButtonIndex:(NSInteger)buttonIndex
{
   if (self.callbackBlock)
      self.callbackBlock(actionSheet, buttonIndex);
}

@end

static void RunActionSheet(const char* title, const struct string_list* items,
                           UIView* parent, RAActionSheetCallback callback)
{
   size_t i;
   RARunActionSheetDelegate* delegate =
     [[RARunActionSheetDelegate alloc] initWithCallbackBlock:callback];
   UIActionSheet* actionSheet = [UIActionSheet new];

   actionSheet.title = BOXSTRING(title);
   actionSheet.delegate = delegate;
   
   for (i = 0; i < items->size; i ++)
      [actionSheet addButtonWithTitle:BOXSTRING(items->elems[i].data)];
   
   actionSheet.cancelButtonIndex = [actionSheet addButtonWithTitle:BOXSTRING("Cancel")];
   
   objc_setAssociatedObject(actionSheet, associated_delegate_key,
                            delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   
   [actionSheet showInView:parent];
}

// Menu Entries

@protocol RAMenuActioner
- (void)menuSelect:(uint32_t) i;
@end

@interface RAMenuItemBase : NSObject
@property (nonatomic) id main;
@property (nonatomic) uint32_t i;
@property (nonatomic, weak) UITableView* parentTable;
- (void)initialize:(NSObject <RAMenuActioner> *) main idx:(uint32_t) i;
- (UITableViewCell*)cellForTableView:(UITableView*)tableView;
- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller;
@end

@implementation RAMenuItemBase
- (void)initialize:(NSObject<RAMenuActioner>*)main idx:(uint32_t) i {
  _main = main;
  _i = i;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
  char buffer[PATH_MAX_LENGTH];
  char label[PATH_MAX_LENGTH];
  static NSString* const cell_id = @"text";

  self.parentTable = tableView;

  UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
  if (!result)
    result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
                                    reuseIdentifier:cell_id];

  menu_entry_get_path(self.i, label, sizeof(label));
  menu_entry_get_value(self.i, buffer, sizeof(buffer));
  
  result.selectionStyle = UITableViewCellSelectionStyleNone;
  result.textLabel.text = BOXSTRING(label);

  if (label[0] == '\0')
    strlcpy(buffer, "N/A", sizeof(buffer));
  result.detailTextLabel.text = BOXSTRING(buffer);
  return result;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller {
}

@end

@interface RAMenuItemBool : RAMenuItemBase
@end

@implementation RAMenuItemBool

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   char label[PATH_MAX_LENGTH];
   static NSString* const cell_id = @"boolean_setting";
   
   UITableViewCell* result =
     (UITableViewCell*)[tableView dequeueReusableCellWithIdentifier:cell_id];
   
   if (!result)
   {
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
                                      reuseIdentifier:cell_id];
      result.selectionStyle = UITableViewCellSelectionStyleNone;
      result.accessoryView = [UISwitch new];
   }

   menu_entry_get_path(self.i, label, sizeof(label));
   
   result.textLabel.text = BOXSTRING(label);
   [(id)result.accessoryView removeTarget:nil
                                   action:NULL
                         forControlEvents:UIControlEventValueChanged];
   [(id)result.accessoryView addTarget:self
                                action:@selector(handleBooleanSwitch:)
                      forControlEvents:UIControlEventValueChanged];
   [(id)result.accessoryView setOn:(menu_entry_get_bool_value(self.i))];
   return result;
}

- (void)handleBooleanSwitch:(UISwitch*)swt
{
  menu_entry_set_bool_value(self.i, swt.on ? true : false);
  [self.main menuSelect: self.i];
}

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
}
@end

@interface RAMenuItemAction : RAMenuItemBase
@end

@implementation RAMenuItemAction

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
  [self.main menuSelect: self.i];
}

@end

@interface RAMenuItemEnum : RAMenuItemBase
@end

@implementation RAMenuItemEnum
- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
   struct string_list* items;
   char label[PATH_MAX_LENGTH];
   RAMenuItemEnum __weak* weakSelf = self;

   menu_entry_get_path(self.i, label, sizeof(label));
   items = menu_entry_enum_values(self.i);

   RunActionSheet(label, items, self.parentTable,
      ^(UIActionSheet* actionSheet, NSInteger buttonIndex)
      {
         if (buttonIndex == actionSheet.cancelButtonIndex)
            return;

         menu_entry_enum_set_value_with_string
           (self.i, [[actionSheet buttonTitleAtIndex:buttonIndex] UTF8String]);
         [weakSelf.parentTable reloadData];
      });
   string_list_free(items);
}
@end

@interface RAMenuItemBind : RAMenuItemBase
@property (nonatomic) NSTimer* bindTimer;
@property (nonatomic) UIAlertView* alert;
@end

@implementation RAMenuItemBind

- (void)wasSelectedOnTableView:(UITableView *)tableView
                  ofController:(UIViewController *)controller
{
   char label[PATH_MAX_LENGTH];

   menu_entry_get_path(self.i, label, sizeof(label));

   self.alert = [[UIAlertView alloc]
   initWithTitle:BOXSTRING("RetroArch")
         message:BOXSTRING(label)
        delegate:self
                 cancelButtonTitle:BOXSTRING("Cancel")
                 otherButtonTitles:BOXSTRING("Clear Keyboard"),
      BOXSTRING("Clear Joystick"), BOXSTRING("Clear Axis"), nil];

   [self.alert show];

   [self.parentTable reloadData];

   self.bindTimer = [NSTimer
      scheduledTimerWithTimeInterval:.1f
                              target:self
                            selector:@selector(checkBind:)
                            userInfo:nil
                             repeats:YES];
}

- (void)finishWithClickedButton:(bool)clicked
{
   if (!clicked)
      [self.alert dismissWithClickedButtonIndex:self.alert.cancelButtonIndex
                                       animated:YES];
   self.alert = nil;

   [self.parentTable reloadData];

   [self.bindTimer invalidate];
   self.bindTimer = nil;
   
   cocoa_input_reset_icade_buttons();
}

- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
  if (buttonIndex == alertView.firstOtherButtonIndex) {
    menu_entry_bind_key_set(self.i, RETROK_UNKNOWN);
  } else if(buttonIndex == alertView.firstOtherButtonIndex + 1) {
    menu_entry_bind_joykey_set(self.i, NO_BTN);
  } else if(buttonIndex == alertView.firstOtherButtonIndex + 2) {
    menu_entry_bind_joyaxis_set(self.i, AXIS_NONE);
  }
   
  [self finishWithClickedButton:true];
}

- (void)checkBind:(NSTimer*)send
{
   int32_t value = 0;
   int32_t idx = menu_entry_bind_index(self.i);

   if ((value = cocoa_input_find_any_key())) {
     menu_entry_bind_key_set(self.i, input_keymaps_translate_keysym_to_rk(value));
   } else if ((value = cocoa_input_find_any_button(idx)) >= 0) {
     menu_entry_bind_joykey_set(self.i, value);
   } else if ((value = cocoa_input_find_any_axis(idx))) {
     menu_entry_bind_joyaxis_set(self.i, (value > 0) ? AXIS_POS(value - 1) : AXIS_NEG(abs(value) - 1));
   } else {
      return;
   }

   [self finishWithClickedButton:false];
}
@end

@interface RAMenuItemPathDir : RAMenuItemBase
@end

@interface RADirectoryItem : NSObject
@property (nonatomic) NSString* path;
@property (nonatomic) bool isDirectory;
@end

@interface RADirectoryList : RAMenuBase<UIActionSheetDelegate>
@property (nonatomic, weak) RADirectoryItem* selectedItem;

@property (nonatomic, copy) void (^chooseAction)(RADirectoryList* list, RADirectoryItem* item);
@property (nonatomic, copy) NSString* path;
@property (nonatomic, copy) NSString* extensions;

@property (nonatomic) bool allowBlank;
@property (nonatomic) bool forDirectory;

- (id)initWithPath:(NSString*)path extensions:(const char*)extensions action:(void (^)(RADirectoryList* list, RADirectoryItem* item))action;
- (void)browseTo:(NSString*)path;
@end

@implementation RAMenuItemPathDir

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
   char pathdir[PATH_MAX_LENGTH], pathdir_ext[PATH_MAX_LENGTH];
   NSString *path;
   RADirectoryList* list;
   RAMenuItemPathDir __weak* weakSelf = self;

   menu_entry_pathdir_selected(self.i);
   menu_entry_pathdir_get_value(self.i, pathdir, sizeof(pathdir));
   menu_entry_pathdir_extensions(self.i, pathdir_ext, sizeof(pathdir_ext));

   path = BOXSTRING(pathdir);
   
   if ( menu_entry_get_type(self.i) == MENU_ENTRY_PATH )
     path = [path stringByDeletingLastPathComponent];
      
   list =
     [[RADirectoryList alloc]
            initWithPath:path
              extensions:pathdir_ext
                  action:^(RADirectoryList* list, RADirectoryItem* item) {
         const char *newval = "";
         if (item) {
           if (list.forDirectory && !item.isDirectory)
             return;

           newval = [item.path UTF8String];
         } else {
           if (!list.allowBlank)
             return;
         }

         menu_entry_pathdir_set_value(self.i, newval);
         [[list navigationController] popViewControllerAnimated:YES];
         menu_entry_select(self.i);
         [weakSelf.parentTable reloadData];
       }];

   list.allowBlank = menu_entry_pathdir_allow_empty(self.i);
   // JM: Is this just Dir vs Path?
   list.forDirectory = menu_entry_pathdir_for_directory(self.i);
   
   [controller.navigationController pushViewController:list animated:YES];
}

@end

@interface RAMenuItemPath : RAMenuItemPathDir
@end

@implementation RAMenuItemPath
@end

@interface RAMenuItemDir : RAMenuItemPathDir
@end

@implementation RAMenuItemDir
@end

@interface RANumberFormatter : NSNumberFormatter<UITextFieldDelegate>
- (void)setRangeFrom: (NSNumber*) min To: (NSNumber*) max;
@end

@implementation RANumberFormatter
- (void)setRangeFrom: (NSNumber*) min To: (NSNumber*) max {
  [self setMinimum: min];
  [self setMaximum: max];
}

- (BOOL)isPartialStringValid:(NSString*)partialString
            newEditingString:(NSString**)newString
            errorDescription:(NSString**)error
{
    unsigned i;
    bool hasDot = false;
    
    if (partialString.length)
        for (i = 0; i < partialString.length; i ++)
        {
            unichar ch = [partialString characterAtIndex:i];
            
            if (i == 0 && (!self.minimum || self.minimum.intValue < 0) && ch == '-')
                continue;
            else if (self.allowsFloats && !hasDot && ch == '.')
                hasDot = true;
            else if (!isdigit(ch))
                return NO;
        }
    
    return YES;
}

- (BOOL)textField:(UITextField *)textField
shouldChangeCharactersInRange:(NSRange)range
replacementString:(NSString *)string
{
    NSString* text = (NSString*)[[textField text]
                                  stringByReplacingCharactersInRange:range
                                                          withString:string];
    return [self isPartialStringValid:text
                     newEditingString:nil
                     errorDescription:nil];
}

@end

@interface RAMenuItemGeneric : RAMenuItemBase<UIAlertViewDelegate>
@property (nonatomic) RANumberFormatter* formatter;
@end

@implementation RAMenuItemGeneric
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   UITableViewCell* result;

   result = [super cellForTableView: tableView];
   
   [self attachDefaultingGestureTo:result];

   return result;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
   char buffer[PATH_MAX_LENGTH];
   char label[PATH_MAX_LENGTH];
   NSString *desc = BOXSTRING("N/A");
   UIAlertView *alertView;
   UITextField *field;

   menu_entry_get_path(self.i, label, sizeof(label));

   desc = BOXSTRING(label);
    
   alertView =
     [[UIAlertView alloc] initWithTitle:BOXSTRING("Enter new value")
                                message:desc
                               delegate:self
                      cancelButtonTitle:BOXSTRING("Cancel")
                      otherButtonTitles:BOXSTRING("OK"), nil];
   alertView.alertViewStyle = UIAlertViewStylePlainTextInput;

   field = [alertView textFieldAtIndex:0];
   
   field.delegate = self.formatter;

   menu_entry_get_value(self.i, buffer, sizeof(buffer));
   if (buffer[0] == '\0')
      strlcpy(buffer, "N/A", sizeof(buffer));

   field.placeholder = BOXSTRING(buffer);

   [alertView show];
}

- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   NSString* text = (NSString*)[alertView textFieldAtIndex:0].text;

   if (buttonIndex != alertView.firstOtherButtonIndex)
       return;
    if (!text.length)
        return;

    menu_entry_set_value(self.i, [text UTF8String]);
    [self.parentTable reloadData];
}

- (void)attachDefaultingGestureTo:(UIView*)view
{
   for (UIGestureRecognizer* i in view.gestureRecognizers)
      [view removeGestureRecognizer:i];
   [view addGestureRecognizer:
           [[UILongPressGestureRecognizer alloc]
             initWithTarget:self
                     action:@selector(resetValue:)]];
}

- (void)resetValue:(UIGestureRecognizer*)gesture
{
   struct string_list* items;
   RAMenuItemGeneric __weak* weakSelf;
   
   if (gesture.state != UIGestureRecognizerStateBegan)
      return;
   
   weakSelf = self;
   items = (struct string_list*)string_split("OK", "|");
   RunActionSheet("Really Reset Value?", items, self.parentTable,
         ^(UIActionSheet* actionSheet, NSInteger buttonIndex)
         {
           if (buttonIndex != actionSheet.cancelButtonIndex) {
             menu_entry_reset(self.i);
           }
           [weakSelf.parentTable reloadData];
         });
   
   string_list_free(items);
}

@end

@interface RAMenuItemNum : RAMenuItemGeneric
@end

@implementation RAMenuItemNum

- (void)initialize:(NSObject <RAMenuActioner> *) main idx:(uint32_t) i {
  [super initialize:main idx:i];
  self.formatter = [RANumberFormatter new];

  if (menu_entry_num_has_range(self.i)) {
    [self.formatter setRangeFrom:BOXFLOAT(menu_entry_num_min(self.i))
                              To:BOXFLOAT(menu_entry_num_max(self.i))];
  }
}
@end

@interface RAMenuItemInt : RAMenuItemNum
@end

@implementation RAMenuItemInt
@end

@interface RAMenuItemUInt : RAMenuItemNum
@end

@implementation RAMenuItemUInt
@end

@interface RAMenuItemFloat : RAMenuItemNum
@end

@implementation RAMenuItemFloat
- (void)initialize:(NSObject <RAMenuActioner> *) main idx:(uint32_t) i {
  [super initialize:main idx:i];
  [self.formatter setAllowsFloats: true];
}
@end

@interface RAMenuItemString : RAMenuItemGeneric
@end

@implementation RAMenuItemString
@end

// XXX This should be a new kind that opens a color picker
@interface RAMenuItemHex : RAMenuItemGeneric
@end

@implementation RAMenuItemHex
@end

@implementation RAMenuBase

- (id)initWithStyle:(UITableViewStyle)style
{
   if ((self = [super initWithStyle:style]))
      _sections = [NSMutableArray array];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return self.sections.count;
}

- (NSString*)tableView:(UITableView*)tableView
titleForHeaderInSection:(NSInteger)section
{
   if (self.hidesHeaders)
       return nil;
   return self.sections[section][0];
}

- (NSInteger)tableView:(UITableView *)tableView
 numberOfRowsInSection:(NSInteger)section
{
   return [self.sections[section] count] - 1;
}

- (id)itemForIndexPath:(NSIndexPath*)indexPath
{
   return self.sections[indexPath.section][indexPath.row + 1];
}

- (UITableViewCell*)tableView:(UITableView *)tableView
        cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   return [[self itemForIndexPath:indexPath] cellForTableView:tableView];
}

- (void)tableView:(UITableView *)tableView
didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
  [[self itemForIndexPath:indexPath]
     wasSelectedOnTableView:tableView
               ofController:self];
}

- (void)willReloadData
{
}

- (void)reloadData
{
   [self willReloadData];

   // Here are two options:

   // Option 1. This is like how setting app works, but not exactly.
   // There is a typedef for the 'withRowAnimation' that has lots of
   // options, just Google UITableViewRowAnimation

   [self.tableView reloadSections:[NSIndexSet indexSetWithIndex:0]
                 withRowAnimation:UITableViewRowAnimationAutomatic];

   // Optione 2. This is a "bigger" transistion, but doesn't look as
   // much like Settings. It has more options. Just Google
   // UIViewAnimationOptionTransition

   /*
   [UIView transitionWithView:self.tableView
                     duration:0.35f
                      options:UIViewAnimationOptionTransitionCurlUp
                   animations:^(void)
           {
             [self.tableView reloadData];
           }
   completion: nil];
   */
}

@end

@interface RAMainMenu : RAMenuBase<RAMenuActioner>
@end

@implementation RAMainMenu

- (id)init
{
   if ((self = [super initWithStyle:UITableViewStylePlain]))
      self.title = BOXSTRING("RetroArch");
   return self;
}

- (void)viewWillAppear:(BOOL)animated
{
   [self reloadData];
}

- (void)willReloadData
{
   size_t i, end;
   char title[256], title_msg[256];
   NSMutableArray *everything;
   RAMainMenu* __weak weakSelf;
   
   everything = [NSMutableArray array];

   menu_entries_get_core_title(title_msg, sizeof(title_msg));
   self.title = BOXSTRING(title_msg);

   menu_entries_get_title(title, sizeof(title));
   [everything addObject:BOXSTRING(title)];
  
   end = menu_entries_get_end();    
   for (i = menu_entries_get_start(); i < end; i++)
     [everything addObject:[self make_menu_item_for_entry: i]];
   
   self.sections = [NSMutableArray array];
   [self.sections addObject:everything];

   weakSelf = self;
   if (menu_entries_show_back())
     [self set_leftbutton:BOXSTRING("Back")
                   target:weakSelf
                   action:@selector(menuBack)];
    
   [self set_rightbutton:BOXSTRING("Switch")
                   target:[RetroArch_iOS get]
                   action:@selector(showGameView)];
}

- (void) set_leftbutton:(NSString *)title target:(id)target action:(SEL)action
{
  self.navigationItem.leftBarButtonItem =
    [[UIBarButtonItem alloc]
           initWithTitle:title
                   style:UIBarButtonItemStyleBordered
                  target:target
                  action:action];
}

- (void) set_rightbutton:(NSString *)title target:(id)target action:(SEL)action
{
    self.navigationItem.rightBarButtonItem =
    [[UIBarButtonItem alloc]
     initWithTitle:title
     style:UIBarButtonItemStyleBordered
     target:target
     action:action];
}

- (RAMenuItemBase*)make_menu_item_for_entry: (uint32_t) i
{
  RAMenuItemBase *me = nil;
  switch (menu_entry_get_type(i)) {
  case MENU_ENTRY_ACTION:
    me = [RAMenuItemAction new]; break;
  case MENU_ENTRY_BOOL:
    me = [RAMenuItemBool new]; break;
  case MENU_ENTRY_INT:
    me = [RAMenuItemInt new]; break;
  case MENU_ENTRY_UINT:
    me = [RAMenuItemUInt new]; break;
  case MENU_ENTRY_FLOAT:
    me = [RAMenuItemFloat new]; break;
  case MENU_ENTRY_PATH:
    me = [RAMenuItemPath new]; break;
  case MENU_ENTRY_DIR:
    me = [RAMenuItemDir new]; break;
  case MENU_ENTRY_STRING:
    me = [RAMenuItemString new]; break;
  case MENU_ENTRY_HEX:
    me = [RAMenuItemHex new]; break;
  case MENU_ENTRY_BIND:
    me = [RAMenuItemBind new]; break;
  case MENU_ENTRY_ENUM:
    me = [RAMenuItemEnum new]; break;
  };

  [me initialize:self idx:i];

  return me;
}

- (void)menuSelect: (uint32_t) i
{
  menu_entry_select(i);
}

- (void)menuBack
{
  menu_entry_go_back();
}

@end
