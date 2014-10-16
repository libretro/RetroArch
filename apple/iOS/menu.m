/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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
#include "../common/RetroArch_Apple.h"
#include "../../input/input_common.h"
#include "../../input/apple_input.h"
#include "menu.h"

/*********************************************/
/* RunActionSheet                            */
/* Creates and displays a UIActionSheet with */
/* buttons pulled from a RetroArch           */
/* string_list structure.                    */
/*********************************************/
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

static void RunActionSheet(const char* title, const struct string_list* items, UIView* parent, RAActionSheetCallback callback)
{
   size_t i;
   RARunActionSheetDelegate* delegate = [[RARunActionSheetDelegate alloc] initWithCallbackBlock:callback];
   UIActionSheet* actionSheet = [UIActionSheet new];

   actionSheet.title = BOXSTRING(title);
   actionSheet.delegate = delegate;
   
   for (i = 0; i < items->size; i ++)
      [actionSheet addButtonWithTitle:BOXSTRING(items->elems[i].data)];
   
   actionSheet.cancelButtonIndex = [actionSheet addButtonWithTitle:BOXSTRING("Cancel")];
   
   objc_setAssociatedObject(actionSheet, associated_delegate_key, delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
   
   [actionSheet showInView:parent];
}


/*********************************************/
/* RAMenuBase                                */
/* A menu class that displays RAMenuItemBase */
/* objects.                                  */
/*********************************************/
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

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   return self.hidesHeaders ? nil : self.sections[section][0];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   return [self.sections[section] count] - 1;
}

- (id)itemForIndexPath:(NSIndexPath*)indexPath
{
   return self.sections[indexPath.section][indexPath.row + 1];
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   return [[self itemForIndexPath:indexPath] cellForTableView:tableView];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
   [[self itemForIndexPath:indexPath] wasSelectedOnTableView:tableView ofController:self];
}

- (void)willReloadData
{
   
}

- (void)reloadData
{
   [self willReloadData];
   [[self tableView] reloadData];
}

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

/*********************************************/
/* RAMenuItemGeneralSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a string or */
/* numeric setting.                          */
/*********************************************/
@interface RAMenuItemGeneralSetting() <UIAlertViewDelegate>
@property (nonatomic) RANumberFormatter* formatter;
@end

@implementation RAMenuItemGeneralSetting

+ (id)itemForSetting:(rarch_setting_t*)setting
{
   switch (setting->type)
   {
      case ST_BOOL:
           return [[RAMenuItemBooleanSetting alloc] initWithSetting:setting];
      case ST_PATH:
           return [[RAMenuItemPathSetting alloc] initWithSetting:setting];
      case ST_BIND:
           return [[RAMenuItemBindSetting alloc] initWithSetting:setting];
      default:
           break;
   }

   if (setting->type == ST_STRING && setting->values)
      return [[RAMenuItemEnumSetting alloc] initWithSetting:setting];
   
   RAMenuItemGeneralSetting* item = [[RAMenuItemGeneralSetting alloc] initWithSetting:setting];
   
   if (
       item.setting->type == ST_INT  ||
       item.setting->type == ST_UINT ||
       item.setting->type == ST_FLOAT)
      item.formatter = [[RANumberFormatter alloc] initWithSetting:item.setting];
   
   return item;
}

- (id)initWithSetting:(rarch_setting_t*)setting
{
   if ((self = [super init]))
      _setting = setting;
   return self;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   char buffer[PATH_MAX];
   static NSString* const cell_id = @"string_setting";

   self.parentTable = tableView;

   UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
   {
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
      result.selectionStyle = UITableViewCellSelectionStyleNone;
   }
   
   [self attachDefaultingGestureTo:result];

   result.textLabel.text = BOXSTRING("N/A");

   if (self.setting)
   {
      if (self.setting->short_description)
         result.textLabel.text = BOXSTRING(self.setting->short_description);

      setting_data_get_string_representation(self.setting, buffer, sizeof(buffer));
      if (buffer[0] == '\0')
         strlcpy(buffer, "N/A", sizeof(buffer));

      result.detailTextLabel.text = BOXSTRING(buffer);

      if (self.setting->type == ST_PATH)
         result.detailTextLabel.text = [result.detailTextLabel.text lastPathComponent];
   }
   return result;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   char buffer[PATH_MAX];
   NSString *desc = BOXSTRING("N/A");
   UIAlertView *alertView;
   UITextField *field;
    
   if (self.setting && self.setting->short_description)
      desc = BOXSTRING(self.setting->short_description);
    
   alertView = [[UIAlertView alloc] initWithTitle:BOXSTRING("Enter new value") message:desc delegate:self cancelButtonTitle:BOXSTRING("Cancel") otherButtonTitles:BOXSTRING("OK"), nil];
   alertView.alertViewStyle = UIAlertViewStylePlainTextInput;

   field = [alertView textFieldAtIndex:0];
   
   field.delegate = self.formatter;

   setting_data_get_string_representation(self.setting, buffer, sizeof(buffer));
   if (buffer[0] == '\0')
      strlcpy(buffer, "N/A", sizeof(buffer));

   field.placeholder = BOXSTRING(buffer);

   [alertView show];
}

- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   NSString* text = (NSString*)[alertView textFieldAtIndex:0].text;

   if (buttonIndex == alertView.firstOtherButtonIndex && text.length)
   {
      setting_data_set_with_string_representation(self.setting, [text UTF8String]);
      [self.parentTable reloadData];
   }
}

- (void)attachDefaultingGestureTo:(UIView*)view
{
   for (UIGestureRecognizer* i in view.gestureRecognizers)
      [view removeGestureRecognizer:i];
   [view addGestureRecognizer:[[UILongPressGestureRecognizer alloc] initWithTarget:self
                                                                    action:@selector(resetValue:)]];
}

- (void)resetValue:(UIGestureRecognizer*)gesture
{
   if (gesture.state == UIGestureRecognizerStateBegan)
   {
      struct string_list* items;
      RAMenuItemGeneralSetting __weak* weakSelf = self;

      items = (struct string_list*)string_split("OK", "|");
      RunActionSheet("Really Reset Value?", items, self.parentTable,
         ^(UIActionSheet* actionSheet, NSInteger buttonIndex)
         {
            if (buttonIndex != actionSheet.cancelButtonIndex)
               setting_data_reset_setting(self.setting);
            [weakSelf.parentTable reloadData];
         });
      string_list_free(items);
   }
}

@end

/*********************************************/
/* RAMenuItemBooleanSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a boolean   */
/* setting.                                  */
/*********************************************/
@implementation RAMenuItemBooleanSetting

- (id)initWithSetting:(rarch_setting_t*)setting
{
   if ((self = [super init]))
      _setting = setting;
   return self;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   static NSString* const cell_id = @"boolean_setting";
   
   UITableViewCell* result = (UITableViewCell*)[tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
   {
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
      result.selectionStyle = UITableViewCellSelectionStyleNone;
      result.accessoryView = [UISwitch new];
   }
   
   result.textLabel.text = BOXSTRING(self.setting->short_description);
   [(id)result.accessoryView removeTarget:nil action:NULL forControlEvents:UIControlEventValueChanged];
   [(id)result.accessoryView addTarget:self action:@selector(handleBooleanSwitch:) forControlEvents:UIControlEventValueChanged];
   
   if (self.setting)
      [(id)result.accessoryView setOn:*self.setting->value.boolean];
   return result;
}

- (void)handleBooleanSwitch:(UISwitch*)swt
{
   if (self.setting)
      *self.setting->value.boolean = swt.on ? true : false;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
}

@end

/*********************************************/
/* RAMenuItemPathSetting                     */
/* A menu item that displays and allows      */
/* browsing for a path setting.              */
/*********************************************/
@interface RAMenuItemPathSetting() @end
@implementation RAMenuItemPathSetting

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   NSString *path;
   RADirectoryList* list;
   RAMenuItemPathSetting __weak* weakSelf = self;
   
   path = [BOXSTRING(self.setting->value.string) stringByDeletingLastPathComponent];
   list = [[RADirectoryList alloc] initWithPath:path extensions:self.setting->values action:
      ^(RADirectoryList* list, RADirectoryItem* item)
      {
         if (!list.allowBlank && !item)
            return;
         
         if (list.forDirectory && !item.isDirectory)
            return;
         
         setting_data_set_with_string_representation(weakSelf.setting, item ? [item.path UTF8String] : "");
         [[list navigationController] popViewControllerAnimated:YES];
         
         [weakSelf.parentTable reloadData];
      }];
   
   list.allowBlank = (self.setting->flags & SD_FLAG_ALLOW_EMPTY);
   list.forDirectory = (self.setting->flags & SD_FLAG_PATH_DIR);
   
   [controller.navigationController pushViewController:list animated:YES];
}

@end

/*********************************************/
/* RAMenuItemEnumSetting                     */
/* A menu item that displays and allows      */
/* a setting to be set from a list of        */
/* allowed choices.                          */
/*********************************************/
@implementation RAMenuItemEnumSetting

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   struct string_list* items;
   RAMenuItemEnumSetting __weak* weakSelf = self;
   
   items = (struct string_list*)string_split(self.setting->values, "|");
   RunActionSheet(self.setting->short_description, items, self.parentTable,
      ^(UIActionSheet* actionSheet, NSInteger buttonIndex)
      {
         if (buttonIndex != actionSheet.cancelButtonIndex)
         {
            setting_data_set_with_string_representation(self.setting, [[actionSheet buttonTitleAtIndex:buttonIndex] UTF8String]);
            [weakSelf.parentTable reloadData];
         }
      });
   string_list_free(items);
}

@end


/*********************************************/
/* RAMenuItemBindSetting                     */
/* A menu item that displays and allows      */
/* mapping of a keybinding.                  */
/*********************************************/
@interface RAMenuItemBindSetting() <UIAlertViewDelegate>
@property (nonatomic) NSTimer* bindTimer;
@property (nonatomic) UIAlertView* alert;
@end

@implementation RAMenuItemBindSetting

- (void)wasSelectedOnTableView:(UITableView *)tableView ofController:(UIViewController *)controller
{
   self.alert = [[UIAlertView alloc] initWithTitle:BOXSTRING("RetroArch")
                                     message:BOXSTRING(self.setting->short_description)
                                     delegate:self
                                     cancelButtonTitle:BOXSTRING("Cancel")
                                     otherButtonTitles:BOXSTRING("Clear Keyboard"), BOXSTRING("Clear Joystick"), BOXSTRING("Clear Axis"), nil];

   [self.alert show];
   
   [self.parentTable reloadData];
   
   self.bindTimer = [NSTimer scheduledTimerWithTimeInterval:.1f target:self selector:@selector(checkBind:)
                             userInfo:nil repeats:YES];
}

- (void)finishWithClickedButton:(bool)clicked
{
   if (!clicked)
      [self.alert dismissWithClickedButtonIndex:self.alert.cancelButtonIndex animated:YES];
   self.alert = nil;

   [self.parentTable reloadData];

   [self.bindTimer invalidate];
   self.bindTimer = nil;
   
   apple_input_reset_icade_buttons();
}

- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   if (buttonIndex == alertView.firstOtherButtonIndex)
      BINDFOR(*self.setting).key = RETROK_UNKNOWN;
   else if(buttonIndex == alertView.firstOtherButtonIndex + 1)
      BINDFOR(*self.setting).joykey = NO_BTN;
   else if(buttonIndex == alertView.firstOtherButtonIndex + 2)
      BINDFOR(*self.setting).joyaxis = AXIS_NONE;
   
   [self finishWithClickedButton:true];
}

- (void)checkBind:(NSTimer*)send
{
   int32_t value = 0;
   int32_t index = 0;

   if (self.setting->index)
      index = self.setting->index - 1;

   if ((value = apple_input_find_any_key()))
      BINDFOR(*self.setting).key = input_translate_keysym_to_rk(value);
   else if ((value = apple_input_find_any_button(index)) >= 0)
      BINDFOR(*self.setting).joykey = value;
   else if ((value = apple_input_find_any_axis(index)))
      BINDFOR(*self.setting).joyaxis = (value > 0) ? AXIS_POS(value - 1) : AXIS_NEG(abs(value) - 1);
   else
      return;

   [self finishWithClickedButton:false];
}

@end


/*********************************************/
/* RAMainMenu                                */
/* Menu object that is displayed immediately */
/* after startup.                            */
/*********************************************/
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
   RAMainMenu* __weak weakSelf = self;
   self.sections = [NSMutableArray array];
   
    [self.sections addObject:[NSArray arrayWithObjects:BOXSTRING("Content"),
                              [RAMenuItemBasic itemWithDescription:BOXSTRING("Core")
                                                            action:^{ [weakSelf chooseCoreWithPath:nil]; }
                                                            detail:^{
                                                                const core_info_t *core = (const core_info_t*)core_info_list_get_by_id(weakSelf.core.UTF8String);
                                                                
                                                                if (weakSelf.core)
                                                                {
                                                                    return core ? BOXSTRING(core->display_name) : BOXSTRING(weakSelf.core.UTF8String);
                                                                }
                                                                else
                                                                    return BOXSTRING("Auto Detect");
                                                            }],
                              [RAMenuItemBasic itemWithDescription:BOXSTRING("Load Content (History)")       action:^{ [weakSelf loadHistory]; }],
                              [RAMenuItemBasic itemWithDescription:BOXSTRING("Load Content")                 action:^{ [weakSelf loadGame]; }],
                              [RAMenuItemBasic itemWithDescription:BOXSTRING("Core Options")
                                                                                action:^{ [weakSelf.navigationController pushViewController:[RACoreOptionsMenu new] animated:YES]; }],
                              nil]];
    [self.sections addObject:[NSMutableArray arrayWithObjects:BOXSTRING("Settings"),
                              [RAMenuItemBasic itemWithDescription:BOXSTRING("Settings")
                                                            action:^{
                                                                char config_name[PATH_MAX];
                                                                fill_pathname_base(config_name, g_settings.libretro, sizeof(config_name));
                                                                
                                                                [weakSelf.navigationController pushViewController:[[RACoreSettingsMenu alloc] initWithCore:BOXSTRING(config_name)] animated:YES];
                                                            }],
                              [RAMenuItemBasic itemWithDescription:BOXSTRING("Configurations")
                                                            action:^{ [weakSelf.navigationController pushViewController:[RAFrontendSettingsMenu new] animated:YES]; }],
                              nil]];
   if (g_extern.main_is_init)
   {
       [self.sections addObject:[NSArray arrayWithObjects:BOXSTRING("States"),
                                 [RAMenuItemStateSelect new],
                                 [RAMenuItemBasic itemWithDescription:BOXSTRING("Save State") action:^{ [weakSelf performBasicAction:RARCH_CMD_SAVE_STATE]; }],
                                 [RAMenuItemBasic itemWithDescription:BOXSTRING("Load State") action:^{ [weakSelf performBasicAction:RARCH_CMD_LOAD_STATE]; }],
                                 nil]];
       [self.sections addObject:[NSArray arrayWithObjects:BOXSTRING("Actions"),
                                 [RAMenuItemBasic itemWithDescription:BOXSTRING("Restart Content") action:^{ [weakSelf performBasicAction:RARCH_CMD_RESET]; }],
                                 nil]];
   }
   
   if (g_extern.main_is_init)
      self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:BOXSTRING("Resume") style:UIBarButtonItemStyleBordered target:[RetroArch_iOS get] action:@selector(showGameView)];
   else
      self.navigationItem.leftBarButtonItem = nil;
}

- (void)performBasicAction:(unsigned)action
{
   [[RetroArch_iOS get] showGameView];
   rarch_main_command(action);
}

- (void)chooseCoreWithPath:(NSString*)path
{
   RAMenuCoreList* list;
   RAMainMenu* __weak weakSelf = self;

   list = [[RAMenuCoreList alloc] initWithPath:path allowAutoDetect:!path
      action: ^(NSString* core)
      {
         if (path)
            apple_run_core(0, NULL, core.UTF8String, path.UTF8String);
         else
         {
            weakSelf.core = core;
            [weakSelf.tableView reloadData];
            [weakSelf.navigationController popViewControllerAnimated:YES];
         }
      }];
   
   // Don't push view controller if it already launched a game
   if (!list.actionRan)
      [self.navigationController pushViewController:list animated:YES];
}

- (void)loadGame
{
   RADirectoryList* list;
   RAMainMenu __weak* weakSelf = self;
   
   list = [[RADirectoryList alloc] initWithPath:[RetroArch_iOS get].documentsDirectory extensions:NULL action:
      ^(RADirectoryList *list, RADirectoryItem *item)
      {
         if (item && !item.isDirectory)
         {
            if (weakSelf.core)
               apple_run_core(0, NULL, weakSelf.core.UTF8String, item.path.UTF8String);
            else
               [weakSelf chooseCoreWithPath:item.path];
         }
      }];
   
   [self.navigationController pushViewController:list animated:YES];
}

- (void)loadHistory
{
   char history_path[PATH_MAX];
   fill_pathname_join(history_path, g_defaults.system_dir, "retroarch-content-history.txt", sizeof(history_path));
   [self.navigationController pushViewController:[[RAHistoryMenu alloc] initWithHistoryPath:history_path] animated:YES];
}

@end

/*************************************************/
/* RAHistoryMenu                                 */
/* Menu object that displays and allows          */
/* launching a file from the content history.    */
/*************************************************/
@implementation RAHistoryMenu

- (void)dealloc
{
   if (self.history)
      content_playlist_free(self.history);
}

- (id)initWithHistoryPath:(const char*)historyPath
{
   if ((self = [super initWithStyle:UITableViewStylePlain]))
   {
      self.history = content_playlist_init(historyPath, 100);
      [self reloadData];
      self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:BOXSTRING("Clear History")
                                                style:UIBarButtonItemStyleBordered target:self action:@selector(clearHistory)];
   }
   
   return self;
}

- (void)clearHistory
{
   if (self.history)
      content_playlist_clear(self.history);
   [self reloadData];
}

- (void)willReloadData
{
   size_t i;
   RAHistoryMenu* __weak weakSelf = self;
   NSMutableArray *section = [NSMutableArray arrayWithObject:BOXSTRING("")];
   
   for (i = 0; self.history && i < content_playlist_size(self.history); i ++)
   {
       RAMenuItemBasic *item;
       const char *path      = NULL;
       const char *core_path = NULL;
       const char *core_name = NULL;
       
       content_playlist_get_index(weakSelf.history, i, &path, &core_path, &core_name);
       
       item = [
               RAMenuItemBasic itemWithDescription:BOXSTRING(path_basename(path ? path : ""))
               action:
               ^{
                   const char *path      = NULL;
                   const char *core_path = NULL;
                   const char *core_name = NULL;
                   
                   content_playlist_get_index(weakSelf.history, i, &path, &core_path, &core_name);
                   
                   apple_run_core(0, NULL, core_path ? core_path : "",
                                  path ? path : "");
               }
               detail:
               ^{
                   const char *path       = NULL;
                   const char *core_path  = NULL;
                   const char *core_name  = NULL;
                   
                   content_playlist_get_index(weakSelf.history, i, &path, &core_path, &core_name);
                   
                   if (core_name)
                       return BOXSTRING(core_name);
                   return BOXSTRING("");
               }
                ];
       [section addObject:item];
   }
   
   self.sections = [NSMutableArray arrayWithObject:section];
}

@end

/*********************************************/
/* RASettingsGroupMenu                       */
/* Menu object that displays and allows      */
/* editing of the a group of                 */
/* rarch_setting_t structures.               */
/*********************************************/
@implementation RASettingsGroupMenu

- (id)initWithGroup:(rarch_setting_t*)group
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      rarch_setting_t *i;
      NSMutableArray* settings = nil;

      self.title = BOXSTRING(group->name);
      
      for (i = group + 1; i->type < ST_END_GROUP; i ++)
      {
         if (i->type == ST_SUB_GROUP)
            settings = [NSMutableArray arrayWithObjects:BOXSTRING(i->name), nil];
         else if (i->type == ST_END_SUB_GROUP)
         {
            if (settings.count)
               [self.sections addObject:settings];
         }
         else
            [settings addObject:[RAMenuItemGeneralSetting itemForSetting:i]];
      }
   }

   return self;
}

@end


/*********************************************/
/* RACoreSettingsMenu                        */
/* Menu object that displays and allows      */
/* editing of the setting_data list.         */
/*********************************************/
@interface RACoreSettingsMenu()
@property (nonatomic, copy) NSString* pathToSave; // < Leave nil to not save
@property (nonatomic, assign) bool isCustom;
@end

@implementation RACoreSettingsMenu

- (id)initWithCore:(NSString*)core
{
   char buffer[PATH_MAX];

   RACoreSettingsMenu* __weak weakSelf = self;

   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      int i, j;
      rarch_setting_t *setting_data, *setting;
      NSMutableArray* settings;

      _isCustom = core_info_get_custom_config(core.UTF8String, buffer, sizeof(buffer));
      if (_isCustom)
      {
          const core_info_t *tmp = (const core_info_t*)core_info_list_get_by_id(core.UTF8String);
          self.title = tmp ? BOXSTRING(tmp->display_name) : BOXSTRING(core.UTF8String);
         _pathToSave = BOXSTRING(buffer);
         self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemTrash target:self action:@selector(deleteCustom)];
      }
      else
      {
         self.title = BOXSTRING("Global Core Config");
         _pathToSave = BOXSTRING(g_defaults.config_path);
      }
      
      // If we initialized with a nil core(like when starting up) driver.menu will be NULL
      if (!driver.menu)
         return NULL;
      
      setting_data = (rarch_setting_t*)driver.menu->list_settings;
      
      if (!setting_data)
         return NULL;
      
      setting_data_load_config_path(setting_data, _pathToSave.UTF8String);
      
      // HACK: Load the key mapping table
      apple_input_find_any_key();
      
      self.core = core;
   
      // Add common options
      const char* emula[] = { "General Options", "rewind_enable", "fps_show", 0 };
      const char* video[] = { "Video Options", "video_scale_integer", "video_smooth", 0 };
      const char* audio[] = { "Audio Options", "audio_enable", "audio_mute", "audio_rate_control_delta", 0 };
      const char* input[] = { "Input Options", "input_overlay", "input_overlay_opacity", 0 };
      const char** groups[] = { emula, video, audio, input, 0 };
      
      for (i = 0; groups[i]; i ++)
      {
         NSMutableArray* section = [NSMutableArray arrayWithObject:BOXSTRING(groups[i][0])];
         [self.sections addObject:section];
         
         for (j = 1; groups[i][j]; j ++)
         {
            rarch_setting_t *current = (rarch_setting_t*)setting_data_find_setting(setting_data, groups[i][j]);
            if (current)
               [section addObject:[RAMenuItemGeneralSetting itemForSetting:current]];
         }
      }

      settings = [NSMutableArray arrayWithObjects:BOXSTRING(""), nil];
      [self.sections addObject:settings];

      for (setting = &setting_data[0]; setting->type < ST_NONE; setting++)
         if (setting->type == ST_GROUP)
            [settings addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING(setting->name) action:
            ^{
               [weakSelf.navigationController pushViewController:[[RASettingsGroupMenu alloc] initWithGroup:setting] animated:YES];
            }]];
   }
   
   return self;
}

- (void)dealloc
{
   if (self.pathToSave)
   {
      config_file_t* config = (config_file_t*)config_file_new(self.pathToSave.UTF8String);

      if (!config)
         return;
      
      setting_data_save_config(driver.menu->list_settings, config);
      config_file_write(config, self.pathToSave.UTF8String);
      config_file_free(config);
   }
}

- (void)deleteCustom
{
   if (self.isCustom && self.pathToSave)
   {
       remove(self.pathToSave.UTF8String);
       self.pathToSave = false;
       [self.navigationController popViewControllerAnimated:YES];
   }
}

@end

/*********************************************/
/* RAFrontendSettingsMenu                    */
/* Menu object that displays and allows      */
/* editing of cocoa frontend related         */
/* settings.                                 */
/*********************************************/
@interface RAFrontendSettingsMenu()
@property (nonatomic, retain) NSMutableArray* coreConfigOptions;
@end

@implementation RAFrontendSettingsMenu

- (id)init
{
   rarch_setting_t* frontend_setting_data = (rarch_setting_t*)apple_get_frontend_settings();

   if ((self = [super initWithGroup:frontend_setting_data]))
   {
      self.title = BOXSTRING("Settings");
  
      _coreConfigOptions = [NSMutableArray array];
      [self.sections addObject:_coreConfigOptions];
   }
   
   return self;
}

- (void)dealloc
{
}

- (void)willReloadData
{
   size_t i;
   const core_info_list_t* core_list;
   RAFrontendSettingsMenu* __weak weakSelf = self;
   NSMutableArray* cores = (NSMutableArray*)self.coreConfigOptions;
   
   [cores removeAllObjects];
   
   [cores addObject:BOXSTRING("Configurations")];
   [cores addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING("Global Core Config")
                                                  action: ^{ [weakSelf showCoreConfigFor:nil]; }]];
   
   [cores addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING("New Config for Core")
                                                  action: ^{ [weakSelf createNewConfig]; }]];

   if (!(core_list = (const core_info_list_t*)core_info_list_get()))
      return;

   for (i = 0; i < core_list->count; i ++)
   {
       char path[PATH_MAX];
       NSString* core_id = BOXSTRING(core_list->list[i].path);

      if (core_info_get_custom_config(core_id.UTF8String, path, sizeof(path)))
      {
         [cores addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING(core_list->list[i].display_name)
                                                   association:core_id
                                                        action: ^(id userdata) { [weakSelf showCoreConfigFor:userdata]; }
                                                        detail: ^(id userdata) { return BOXSTRING(""); }]];
      }
   }
}

- (void)viewWillAppear:(BOOL)animated
{
   [super viewWillAppear:animated];
   [self reloadData];
}

- (void)showCoreConfigFor:(NSString*)core
{
   [self.navigationController pushViewController:[[RACoreSettingsMenu alloc] initWithCore:core] animated:YES];
}

static bool copy_config(const char *src_path, const char *dst_path)
{
   char ch;
   FILE *dst;
   FILE *src = fopen(src_path, "r");

   if (!src)
      return false;

   dst = fopen(dst_path, "w");

   if (!dst)
   {
      fclose(src);
      return false;
   }

   while((ch = fgetc(src)) != EOF)
      fputc(ch, dst);

   fclose(src);
   fclose(dst);

   return true;
}

- (void)createNewConfig
{
   RAFrontendSettingsMenu* __weak weakSelf = self;
   RAMenuCoreList* list = [[RAMenuCoreList alloc] initWithPath:nil allowAutoDetect:false
      action:^(NSString* core)
      {
         char path[PATH_MAX];
         if (!core_info_get_custom_config(core.UTF8String, path, sizeof(path)))
         {
            if (g_defaults.config_path[0] != '\0' && path[0] != '\0')
            {
               if (!copy_config(g_defaults.config_path, path))
                  RARCH_WARN("Could not create custom config at %s.", path);
            }
         }
         
         [weakSelf.navigationController popViewControllerAnimated:YES];
      }];
   [self.navigationController pushViewController:list animated:YES];
}

@end

/*********************************************/
/* RACoreOptionsMenu                         */
/* Menu object that allows editing of        */
/* options specific to the running core.     */
/*********************************************/
@interface RACoreOptionsMenu()
@property (nonatomic) uint32_t currentIndex;
@end

@implementation RACoreOptionsMenu

- (id)init
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      RACoreOptionsMenu* __weak weakSelf = self;
      core_option_manager_t* options = (core_option_manager_t*)g_extern.system.core_options;
   
      NSMutableArray* section = (NSMutableArray*)[NSMutableArray arrayWithObject:BOXSTRING("")];
      [self.sections addObject:section];
   
      if (options)
      {
         unsigned i;
         for (i = 0; i < core_option_size(options); i ++)
            [section addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING(core_option_get_desc(options, i)) association:nil
               action:^{ [weakSelf editValue:i]; }
               detail:^{ return BOXSTRING(core_option_get_val(options, i)); }]];
      }
      else
         [section addObject:[RAMenuItemBasic itemWithDescription:BOXSTRING("The running core has no options.") action:NULL]];
   }
   
   return self;
}

- (void)editValue:(uint32_t)index
{
   RACoreOptionsMenu __weak* weakSelf = self;
   self.currentIndex = index;

   RunActionSheet(core_option_get_desc(g_extern.system.core_options, index), core_option_get_vals(g_extern.system.core_options, index), self.tableView,
   ^(UIActionSheet* actionSheet, NSInteger buttonIndex)
   {
      if (buttonIndex != actionSheet.cancelButtonIndex)
         core_option_set_val(g_extern.system.core_options, self.currentIndex, buttonIndex);
      
      [weakSelf.tableView reloadData];
   });
}

@end

/*********************************************/
/* RAMenuItemCoreList                        */
/* Menu item that handles display and        */
/* selection of an item in RAMenuCoreList.   */
/* This item will not function on anything   */
/* but an RAMenuCoreList type menu.          */
/*********************************************/
@implementation RAMenuItemCoreList

- (id)initWithCore:(NSString*)core parent:(RAMenuCoreList* __weak)parent
{
   if ((self = [super init]))
   {
      _core = core;
      _parent = parent;
   }
   
   return self;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   UITableViewCell *result;
   const core_info_t *core = (const core_info_t*)core_info_list_get_by_id(self.core.UTF8String);
   static NSString* const cell_id = @"RAMenuItemCoreList";

   if (!(result = [tableView dequeueReusableCellWithIdentifier:cell_id]))
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];

    result.textLabel.text = core ? BOXSTRING(core->display_name) : BOXSTRING(self.core.UTF8String);
   return result;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   [self.parent runAction:self.core];
}

@end

/*********************************************/
/* RAMenuCoreList                            */
/* Menu object that displays and allows      */
/* selection from a list of cores.           */
/* If the path is not nil, only cores that   */
/* may support the file is listed.           */
/* If the path is nil, an 'Auto Detect'      */
/* entry is added to the menu, when tapped   */
/* the action function will be called with   */
/* nil as the argument.                      */
/*********************************************/
@implementation RAMenuCoreList

- (id)initWithPath:(NSString*)path allowAutoDetect:(bool)autoDetect action:(void (^)(NSString *))action
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      NSMutableArray* core_section;
      core_info_list_t* core_list = NULL;

      self.title = BOXSTRING("Choose Core");
      _action = action;
      _path = path;

      if (autoDetect)
      {
         RAMenuCoreList* __weak weakSelf = self;
         [self.sections addObject: @[BOXSTRING(""), [RAMenuItemBasic itemWithDescription:BOXSTRING("Auto Detect")
               action: ^{ if(weakSelf.action) weakSelf.action(nil); }]]];
      }

      core_section = (NSMutableArray*)[NSMutableArray arrayWithObject:BOXSTRING("Cores")];

      [self.sections addObject:core_section];

      if ((core_list = (core_info_list_t*)core_info_list_get()))
      {
         if (_path)
         {
            const core_info_t* core_support = NULL;
            size_t core_count = 0;

            core_info_list_get_supported_cores(core_list, _path.UTF8String, &core_support, &core_count);
            
            if (core_count == 1 && _action)
               [self runAction:BOXSTRING(core_support[0].path)];
            else if (core_count > 1)
               [self load:(uint32_t)core_count coresFromList:core_support toSection:core_section];
         }
         
         if (!_path || [core_section count] == 1)
            [self load:(uint32_t)core_list->count coresFromList:core_list->list toSection:core_section];
      }
   }

   return self;
}

- (void)runAction:(NSString*)coreID
{
   self.actionRan = true;
   
   if (self.action)
      self.action(coreID);
}

- (void)load:(uint32_t)count coresFromList:(const core_info_t*)list toSection:(NSMutableArray*)array
{
   int i;
    
   for (i = 0; i < count; i++)
      [array addObject:[[RAMenuItemCoreList alloc] initWithCore:BOXSTRING(list[i].path) parent:self]];
}

@end

/*********************************************/
/* RAMenuItemStateSelect                     */
/* Menu item that allows save state slots    */
/* 0-9 to be selected.                       */
/*********************************************/
@implementation RAMenuItemStateSelect

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   static NSString* const cell_id = @"state_slot_setting";

   UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
   {
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
      result.selectionStyle = UITableViewCellSelectionStyleNone;

      result.textLabel.text = BOXSTRING("Slot");
      
      UISegmentedControl* accessory = [[UISegmentedControl alloc] initWithItems:@[@"0", @"1", @"2", @"3", @"4", @"5", @"6", @"7", @"8", @"9"]];
      [accessory addTarget:[self class] action:@selector(changed:) forControlEvents:UIControlEventValueChanged];
      accessory.segmentedControlStyle = UISegmentedControlStyleBar;
      result.accessoryView = accessory;
   }
   
   [(id)result.accessoryView setSelectedSegmentIndex:(g_settings.state_slot < 10) ? g_settings.state_slot : -1];

   return result;
}

+ (void)changed:(UISegmentedControl*)sender
{
   g_settings.state_slot = (int)sender.selectedSegmentIndex;
}

- (void)wasSelectedOnTableView:(UITableView *)tableView ofController:(UIViewController *)controller
{

}

@end
