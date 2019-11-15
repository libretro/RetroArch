/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <queues/task_queue.h>

#include "cocoa_common.h"
#include "../../../input/input_keymaps.h"
#include "../../../input/drivers/cocoa_input.h"

#include "../../../configuration.h"
#include "../../../retroarch.h"

#ifdef HAVE_MENU
#include "../../../menu/menu_entries.h"
#include "../../../menu/menu_driver.h"
#include "../../../menu/drivers/menu_generic.h"
#endif

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
  menu_entry_t entry;
  char buffer[PATH_MAX_LENGTH];
  const char *label              = NULL;
  static NSString* const cell_id = @"text";

  self.parentTable = tableView;

  UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
  if (!result)
    result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
                                    reuseIdentifier:cell_id];

  menu_entry_init(&entry);
  menu_entry_get(&entry, 0, (unsigned)self.i, NULL, true);
  menu_entry_get_path(&entry, &label);
  menu_entry_get_value(&entry, &buffer);

  if (string_is_empty(label))
    strlcpy(buffer,
          msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
          sizeof(buffer));

  if (!string_is_empty(label))
     result.textLabel.text    = BOXSTRING(label);
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
   menu_entry_t entry;
   const char *label              = NULL;
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

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, (unsigned)self.i, NULL, true);

   menu_entry_get_path(&entry, &label);

   if (!string_is_empty(label))
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
   menu_entry_t entry;
   struct string_list* items       = NULL;
   const char *label               = NULL;
   RAMenuItemEnum __weak* weakSelf = self;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, (unsigned)self.i, NULL, true);
   menu_entry_get_path(&entry, &label);
   items = menu_entry_enum_values(self.i);

   if (!string_is_empty(label))
   {
      RunActionSheet(label, items, self.parentTable,
      ^(UIActionSheet* actionSheet, NSInteger buttonIndex)
      {
         if (buttonIndex == actionSheet.cancelButtonIndex)
            return;

         menu_entry_enum_set_value_with_string
           (self.i, [[actionSheet buttonTitleAtIndex:buttonIndex] UTF8String]);
         [weakSelf.parentTable reloadData];
      });
   }
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
   menu_entry_t entry;
   const char *label = NULL;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, (unsigned)self.i, NULL, true);
   menu_entry_get_path(&entry, &label);

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
}

- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   if (buttonIndex == alertView.firstOtherButtonIndex)
      menu_entry_bind_key_set(self.i, RETROK_UNKNOWN);
   else if(buttonIndex == alertView.firstOtherButtonIndex + 1)
      menu_entry_bind_joykey_set(self.i, NO_BTN);
   else if(buttonIndex == alertView.firstOtherButtonIndex + 2)
      menu_entry_bind_joyaxis_set(self.i, AXIS_NONE);

   [self finishWithClickedButton:true];
}

- (void)checkBind:(NSTimer*)send
{
   int32_t value = 0;
   int32_t idx = menu_entry_bind_index(self.i);

   if ((value = cocoa_input_find_any_key()))
      menu_entry_bind_key_set(self.i, input_keymaps_translate_keysym_to_rk(value));
   else if ((value = cocoa_input_find_any_button(idx)) >= 0)
      menu_entry_bind_joykey_set(self.i, value);
   else if ((value = cocoa_input_find_any_axis(idx)))
      menu_entry_bind_joyaxis_set(self.i, (value > 0) ? AXIS_POS(value - 1) : AXIS_NEG(abs(value) - 1));
   else
      return;

   [self finishWithClickedButton:false];
}
@end

@interface RAMenuItemPathDir : RAMenuItemBase
@end

@implementation RAMenuItemPathDir

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
}

@end

@interface RAMenuItemPath : RAMenuItemPathDir
@end

@implementation RAMenuItemPath

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
  [self.main menuSelect: self.i];
}

@end

@interface RAMenuItemDir : RAMenuItemPathDir
@end

@implementation RAMenuItemDir

- (void)wasSelectedOnTableView:(UITableView*)tableView
                  ofController:(UIViewController*)controller
{
  [self.main menuSelect: self.i];
}

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
   menu_entry_t entry;
   char buffer[PATH_MAX_LENGTH];
   const char *label      = NULL;
   UIAlertView *alertView = NULL;
   UITextField     *field = NULL;
   NSString         *desc = NULL;

   menu_entry_init(&entry);
   menu_entry_get(&entry, 0, (unsigned)self.i, NULL, true);
   menu_entry_get_path(&entry, &label);

   desc      = BOXSTRING(label);

   alertView =
     [[UIAlertView alloc] initWithTitle:BOXSTRING("Enter new value")
                                message:desc
                               delegate:self
                      cancelButtonTitle:BOXSTRING("Cancel")
                      otherButtonTitles:BOXSTRING("OK"), nil];
   alertView.alertViewStyle = UIAlertViewStylePlainTextInput;

   field          = [alertView textFieldAtIndex:0];
   field.delegate = self.formatter;

   menu_entry_get_value(&entry, &buffer);
   if (string_is_empty(buffer))
      strlcpy(buffer,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
            sizeof(buffer));

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

  if (menu_entry_num_has_range(self.i))
     [self.formatter setRangeFrom:BOXFLOAT(menu_entry_num_min(self.i))
                               To:BOXFLOAT(menu_entry_num_max(self.i))];
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
   return nil;
}

- (NSInteger)tableView:(UITableView *)tableView
 numberOfRowsInSection:(NSInteger)section
{
   return [self.sections[section] count];
}

- (id)itemForIndexPath:(NSIndexPath*)indexPath
{
   return self.sections[indexPath.section][indexPath.row];
}

- (UITableViewCell*)tableView:(UITableView *)tableView
        cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   return [[self itemForIndexPath:indexPath] cellForTableView:tableView];
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
    return UITableViewAutomaticDimension;
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
   [self.tableView reloadData];
}

-(void)renderMessageBox:(NSString *)msg
{
    UIAlertView *message = [[UIAlertView alloc] initWithTitle:@"Help"
                                                      message:msg
                                                     delegate:self
                                            cancelButtonTitle:@"OK"
                                            otherButtonTitles:nil];

    [message show];
}

-(void)msgQueuePush:(NSString *)msg
{
   self.osdmessage.text = msg;
}

- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   menu_ctx_iterate_t iter;

   switch (buttonIndex)
   {
      case 0:
         iter.action = MENU_ACTION_OK;
         menu_driver_iterate(&iter);
         break;
   }
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
   UIBarButtonItem *item = NULL;
   settings_t *settings  = config_get_ptr();

   [self reloadData];

   self.osdmessage = [[UILabel alloc] initWithFrame:CGRectMake(0, 0, 300, 44)];
   self.osdmessage.backgroundColor = [UIColor clearColor];

   item = [[UIBarButtonItem alloc] initWithCustomView:self.osdmessage];
   [self setToolbarItems: [NSArray arrayWithObject:item]];

   if (settings->bools.menu_core_enable)
   {
      char title_msg[256];
      menu_entries_get_core_title(title_msg, sizeof(title_msg));
      self.osdmessage.text = BOXSTRING(title_msg);
   }
}

- (void)willReloadData
{
   size_t i, end;
   char title[256];
   RAMainMenu* __weak weakSelf = NULL;
   NSMutableArray *everything  = [NSMutableArray array];
   settings_t *settings        = config_get_ptr();

   if (settings->bools.menu_core_enable)
   {
      char title_msg[256];
      menu_entries_get_core_title(title_msg, sizeof(title_msg));
      self.osdmessage.text = BOXSTRING(title_msg);
   }

   menu_entries_get_title(title, sizeof(title));
   self.title = BOXSTRING(title);

   end = menu_entries_get_size();
   menu_entries_ctl(MENU_ENTRIES_CTL_START_GET, &i);

   for (; i < end; i++)
     [everything addObject:[self make_menu_item_for_entry: (uint32_t)i]];

   self.sections = [NSMutableArray array];
   [self.sections addObject:everything];

   weakSelf = self;
   if (menu_entries_ctl(MENU_ENTRIES_CTL_SHOW_BACK, NULL))
     [self set_leftbutton:BOXSTRING("Back")
                   target:weakSelf
                   action:@selector(menuBack)];

    dispatch_async(dispatch_get_main_queue(), ^{
        [self set_rightbutton:BOXSTRING("Switch")
                       target:[RetroArch_iOS get]
                       action:@selector(showGameView)];
    });
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
   switch (menu_entry_get_type(i))
   {
      case MENU_ENTRY_ACTION:
         me = [RAMenuItemAction new];
         break;
      case MENU_ENTRY_BOOL:
         me = [RAMenuItemBool new];
         break;
      case MENU_ENTRY_INT:
         me = [RAMenuItemInt new];
         break;
      case MENU_ENTRY_UINT:
         me = [RAMenuItemUInt new];
         break;
      case MENU_ENTRY_FLOAT:
         me = [RAMenuItemFloat new];
         break;
      case MENU_ENTRY_PATH:
         me = [RAMenuItemPath new];
         break;
      case MENU_ENTRY_DIR:
         me = [RAMenuItemDir new];
         break;
      case MENU_ENTRY_STRING:
         me = [RAMenuItemString new];
         break;
      case MENU_ENTRY_HEX:
         me = [RAMenuItemHex new];
         break;
      case MENU_ENTRY_BIND:
         me = [RAMenuItemBind new];
         break;
      case MENU_ENTRY_ENUM:
         me = [RAMenuItemEnum new];
         break;
       case MENU_ENTRY_SIZE:
         /* TODO/FIXME - implement this */
         break;
   };

   [me initialize:self idx:i];

   return me;
}

- (void)menuSelect: (uint32_t) i
{
   menu_entry_select(i);
   task_queue_check();
}

- (void)menuBack
{
#ifdef HAVE_MENU
   menu_entry_t entry = {0};
   size_t selection   = menu_navigation_get_selection();

   menu_entry_get(&entry, 0, selection, NULL, false);
   menu_entry_action(&entry, selection, MENU_ACTION_CANCEL);
#endif
}

@end
