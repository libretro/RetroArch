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

#include <objc/runtime.h>
#include "apple/common/RetroArch_Apple.h"
#include "apple/common/apple_input.h"
#include "menu.h"

/*********************************************/
/* RAMenuBase                                */
/* A menu class that displays RAMenuItemBase */
/* objects.                                  */
/*********************************************/
@implementation RAMenuBase

- (id)initWithStyle:(UITableViewStyle)style
{
   if ((self = [super initWithStyle:style]))
      self.sections = [NSMutableArray array];
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

@end

/*********************************************/
/* RAMenuItemBasic                           */
/* A simple menu item that displays a text   */
/* description and calls a block object when */
/* selected.                                 */
/*********************************************/
@implementation RAMenuItemBasic

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
/* RAMenuItemBooleanSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a boolean   */
/* setting.                                  */
/*********************************************/
@implementation RAMenuItemBooleanSetting

+ (RAMenuItemBooleanSetting*)itemForSetting:(const rarch_setting_t*)setting
{
   RAMenuItemBooleanSetting* item = [RAMenuItemBooleanSetting new];
   item.setting = setting;
   return item;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   static NSString* const cell_id = @"boolean_setting";
   
   UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
   {
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
      result.selectionStyle = UITableViewCellSelectionStyleNone;
      result.accessoryView = [UISwitch new];
   }

   result.textLabel.text = @(self.setting->short_description);
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
/* RAMenuItemGeneralSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a string or */
/* numeric setting.                          */
/*********************************************/
@interface RAMenuItemGeneralSetting() <UIAlertViewDelegate>
@property (nonatomic) RANumberFormatter* formatter;
@end

@implementation RAMenuItemGeneralSetting

+ (RAMenuItemGeneralSetting*)itemForSetting:(const rarch_setting_t*)setting
{
   RAMenuItemGeneralSetting* item = [RAMenuItemGeneralSetting new];
   item.setting = setting;
   
   if (item.setting->type == ST_INT || item.setting->type == ST_UINT || item.setting->type == ST_FLOAT)
      item.formatter = [[RANumberFormatter alloc] initWithSetting:item.setting];
   
   return item;
}

- (UITableViewCell*)cellForTableView:(UITableView*)tableView
{
   static NSString* const cell_id = @"string_setting";

   self.parentTable = tableView;

   UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
   {
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
      result.selectionStyle = UITableViewCellSelectionStyleNone;
   }

   char buffer[256];
   result.textLabel.text = @(self.setting->short_description);

   if (self.setting)
      result.detailTextLabel.text = @(setting_data_get_string_representation(self.setting, buffer, sizeof(buffer)));
   return result;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   UIAlertView* alertView = [[UIAlertView alloc] initWithTitle:@"Enter new value" message:@(self.setting->short_description) delegate:self
                                                  cancelButtonTitle:@"Cancel" otherButtonTitles:@"OK", nil];
   alertView.alertViewStyle = UIAlertViewStylePlainTextInput;

   UITextField* field = [alertView textFieldAtIndex:0];
   char buffer[256];
   
   field.delegate = self.formatter;
   field.placeholder = @(setting_data_get_string_representation(self.setting, buffer, sizeof(buffer)));

   [alertView show];
}

- (void)alertView:(UIAlertView*)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   NSString* text = [alertView textFieldAtIndex:0].text;

   if (buttonIndex == alertView.firstOtherButtonIndex && text.length)
   {
      setting_data_set_with_string_representation(self.setting, text.UTF8String);
      [self.parentTable reloadData];
   }
}

@end

/*********************************************/
/* RAMenuItemPathSetting                     */
/* A menu item that displays and allows      */
/* browsing for a path setting.              */
/*********************************************/
@interface RAMenuItemPathSetting() <RADirectoryListDelegate> @end
@implementation RAMenuItemPathSetting

+ (RAMenuItemPathSetting*)itemForSetting:(const rarch_setting_t*)setting
{
   RAMenuItemPathSetting* item = [RAMenuItemPathSetting new];
   item.setting = setting;
   return item;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   RADirectoryList* list = [[RADirectoryList alloc] initWithPath:nil delegate:self];
   [controller.navigationController pushViewController:list animated:YES];
}

- (bool)directoryList:(id)list itemWasSelected:(RADirectoryItem *)path
{
   setting_data_set_with_string_representation(self.setting, path.path.UTF8String);
   [[list navigationController] popViewControllerAnimated:YES];
      
   [self.parentTable reloadData];

   return true;
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

+ (RAMenuItemBindSetting*)itemForSetting:(const rarch_setting_t*)setting
{
   RAMenuItemBindSetting* item = [RAMenuItemBindSetting new];
   item.setting = setting;
   return item;
}

- (void)wasSelectedOnTableView:(UITableView *)tableView ofController:(UIViewController *)controller
{
   self.alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                     message:@(self.setting->short_description)
                                     delegate:self
                                     cancelButtonTitle:@"Cancel"
                                     otherButtonTitles:@"Clear Keyboard", @"Clear Joystick", @"Clear Axis", nil];
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

   if ((value = apple_input_find_any_key()))
      BINDFOR(*self.setting).key = input_translate_keysym_to_rk(value);
   else if ((value = apple_input_find_any_button(0)) >= 0)
      BINDFOR(*self.setting).joykey = value;
   else if ((value = apple_input_find_any_axis(0)))
      BINDFOR(*self.setting).joyaxis = (value > 0) ? AXIS_POS(value - 1) : AXIS_NEG(value - 1);
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
   {
      RAMainMenu* __weak weakSelf = self;
   
      self.title = @"RetroArch";
   
      self.sections =
      (id)@[
         @[ @"",
            [RAMenuItemBasic itemWithDescription:@"Choose Core"
               action:^{ [weakSelf chooseCoreWithPath:nil]; }
               detail:^{ return weakSelf.core ? apple_get_core_display_name(weakSelf.core) : @"Auto Detect"; }],
            [RAMenuItemBasic itemWithDescription:@"Load Content"                 action:^{ [weakSelf loadGame]; }],
            [RAMenuItemBasic itemWithDescription:@"Load Content (History)"       action:^{ [weakSelf loadHistory]; }],
            [RAMenuItemBasic itemWithDescription:@"Settings"         action:^{ [[RetroArch_iOS get] showSystemSettings]; }]
         ]
      ];
   }
   
   return self;
}

- (void)chooseCoreWithPath:(NSString*)path
{
   RAMainMenu* __weak weakSelf = self;

   RAMenuCoreList* list = [[RAMenuCoreList alloc] initWithPath:path
      action: ^(NSString* core)
      {
         if (path)
            apple_run_core(weakSelf.core, path.UTF8String);
         else
         {
            weakSelf.core = core;
            [weakSelf.tableView reloadData];
            [weakSelf.navigationController popViewControllerAnimated:YES];
         }
      }];
   [self.navigationController pushViewController:list animated:YES];
}

- (void)loadGame
{
   NSString* rootPath = RetroArch_iOS.get.documentsDirectory;
   NSString* ragPath = [rootPath stringByAppendingPathComponent:@"RetroArchGames"];
   NSString* target = path_is_directory(ragPath.UTF8String) ? ragPath : rootPath;

   [self.navigationController pushViewController:[[RADirectoryList alloc] initWithPath:target delegate:self] animated:YES];
}

- (void)loadHistory
{
   NSString* history_path = [NSString stringWithFormat:@"%@/%s", RetroArch_iOS.get.systemDirectory, ".retroarch-game-history.txt"];
   [self.navigationController pushViewController:[[RAHistoryMenu alloc] initWithHistoryPath:history_path] animated:YES];
}

- (bool)directoryList:(id)list itemWasSelected:(RADirectoryItem*)path
{
   if (!path.isDirectory)
   {
      if (self.core)
         apple_run_core(self.core, path.path.UTF8String);
      else
         [self chooseCoreWithPath:path.path];
   }

   return true;
}

@end

/*********************************************/
/* RAHistoryMenu                             */
/* Menu object that displays and allows      */
/* launching a file from the ROM history.    */
/*********************************************/
@implementation RAHistoryMenu

- (id)initWithHistoryPath:(NSString *)historyPath
{
   if ((self = [super initWithStyle:UITableViewStylePlain]))
   {
      RAHistoryMenu* __weak weakSelf = self;
   
      _history = rom_history_init(historyPath.UTF8String, 100);

      NSMutableArray* section = [NSMutableArray arrayWithObject:@""];
      [self.sections addObject:section];
      
      for (int i = 0; _history && i != rom_history_size(_history); i ++)
      {
         RAMenuItemBasic* item = [RAMenuItemBasic itemWithDescription:@(path_basename(apple_rom_history_get_path(weakSelf.history, i)))
                                                  action:^{ apple_run_core(@(apple_rom_history_get_core_path(weakSelf.history, i)),
                                                                             apple_rom_history_get_path(weakSelf.history, i)); }
                                                  detail:^{ return @(apple_rom_history_get_core_name(weakSelf.history, i)); }];
         [section addObject:item];
      }
   }
   
   return self;
}

- (void)dealloc
{
   rom_history_free(self.history);
}

@end

/*********************************************/
/* RASettingsGroupMenu                       */
/* Menu object that displays and allows      */
/* editing of the a group of                 */
/* rarch_setting_t structures.               */
/*********************************************/
@implementation RASettingsGroupMenu

- (id)initWithGroup:(const rarch_setting_t*)group
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      self.title = @(group->name);
   
      NSMutableArray* settings = nil;

      for (const rarch_setting_t* i = group + 1; i->type != ST_END_GROUP; i ++)
      {
         if (i->type == ST_SUB_GROUP)
            settings = [NSMutableArray arrayWithObjects:@(i->name), nil];
         else if (i->type == ST_END_SUB_GROUP)
         {
            if (settings.count)
               [self.sections addObject:settings];
         }
         else if (i->type == ST_BOOL)
            [settings addObject:[RAMenuItemBooleanSetting itemForSetting:i]];
         else if (i->type == ST_INT || i->type == ST_UINT || i->type == ST_FLOAT || i->type == ST_STRING)
            [settings addObject:[RAMenuItemGeneralSetting itemForSetting:i]];
         else if (i->type == ST_PATH)
            [settings addObject:[RAMenuItemPathSetting itemForSetting:i]];
         else if (i->type == ST_BIND)
            [settings addObject:[RAMenuItemBindSetting itemForSetting:i]];
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
@property (nonatomic) NSString* pathToSave; // < Leave nil to not save
@end

@implementation RACoreSettingsMenu

- (id)initWithCore:(NSString*)core
{
   char buffer[PATH_MAX];

   RACoreSettingsMenu* __weak weakSelf = self;

   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      if (apple_core_info_has_custom_config(core.UTF8String))
         _pathToSave = @(apple_core_info_get_custom_config(core.UTF8String, buffer, sizeof(buffer)));
      else
         _pathToSave = apple_platform.globalConfigFile;
      
      const rarch_setting_t* setting_data = setting_data_get_list();
      
      setting_data_reset(setting_data);
      setting_data_load_config_path(setting_data, _pathToSave.UTF8String);
      
      // HACK: Load the key mapping table
      apple_input_find_any_key();
      
      self.core = core;
      self.title = self.core ? apple_get_core_display_name(core) : @"Global Core Config";
   
      NSMutableArray* settings = [NSMutableArray arrayWithObjects:@"", nil];
      [self.sections addObject:settings];

      for (const rarch_setting_t* i = setting_data; i->type != ST_NONE; i ++)
         if (i->type == ST_GROUP)
            [settings addObject:[RAMenuItemBasic itemWithDescription:@(i->name) action:
            ^{
               [weakSelf.navigationController pushViewController:[[RASettingsGroupMenu alloc] initWithGroup:i] animated:YES];
            }]];
      
      [settings addObject:[RAMenuItemBasic itemWithDescription:@"Core Options"
         action:^{ [weakSelf.navigationController pushViewController:[RACoreOptionsMenu new] animated:YES]; }]];
   }
   
   return self;
}

- (void)dealloc
{
   if (self.pathToSave)
   {
      config_file_t* config = config_file_new(self.pathToSave.UTF8String);
      if (!config)
         config = config_file_new(0);
      
      setting_data_save_config(setting_data_get_list(), config);
      
      config_set_string(config, "system_directory", [[RetroArch_iOS get].systemDirectory UTF8String]);
      config_set_string(config, "savefile_directory", [[RetroArch_iOS get].systemDirectory UTF8String]);
      config_set_string(config, "savestate_directory", [[RetroArch_iOS get].systemDirectory UTF8String]);
      config_file_write(config, self.pathToSave.UTF8String);
      config_file_free(config);
   }
}

@end

/*********************************************/
/* RAFrontendSettingsMenu                    */
/* Menu object that displays and allows      */
/* editing of cocoa frontend related         */
/* settings.                                 */
/*********************************************/
static const void* const associated_core_key = &associated_core_key;

@interface RAFrontendSettingsMenu() <UIAlertViewDelegate> @end
@implementation RAFrontendSettingsMenu

- (id)init
{
   const rarch_setting_t* apple_get_frontend_settings();
   const rarch_setting_t* frontend_setting_data = apple_get_frontend_settings();

   if ((self = [super initWithGroup:frontend_setting_data]))
   {
      RAFrontendSettingsMenu* __weak weakSelf = self;

      self.title = @"Frontend Settings";
      
      RAMenuItemBasic* diagnostic_item = [RAMenuItemBasic itemWithDescription:@"Diagnostic Log"
         action:^{ [weakSelf.navigationController pushViewController:[[RALogMenu alloc] initWithFile:RetroArch_iOS.get.logPath.UTF8String] animated:YES]; }];
      [self.sections insertObject:@[@"", diagnostic_item] atIndex:0];
  
      // Core items to load core settings
      NSMutableArray* cores = [NSMutableArray arrayWithObject:@"Cores"];
      
      [cores addObject:[RAMenuItemBasic itemWithDescription:@"Global Core Config"
         action: ^{ [weakSelf showCoreConfigFor:nil]; }]];

      const core_info_list_t* core_list = apple_core_info_list_get();
      for (int i = 0; i < core_list->count; i ++)
         [cores addObject:[RAMenuItemBasic itemWithDescription:@(core_list->list[i].display_name)
            association:apple_get_core_id(&core_list->list[i])
            action: ^(id userdata) { [weakSelf showCoreConfigFor:userdata]; }
            detail: ^(id userdata) { return apple_core_info_has_custom_config([userdata UTF8String]) ? @"[Custom]" : @"[Global]"; }]];
      [self.sections addObject:cores];
   }
   
   return self;
}

- (void)dealloc
{
   const rarch_setting_t* apple_get_frontend_settings();
   setting_data_save_config_path(apple_get_frontend_settings(), [RetroArch_iOS get].systemConfigPath.UTF8String);
   [[RetroArch_iOS get] refreshSystemConfig];
}

- (void)showCoreConfigFor:(NSString*)core
{
   if (core && !apple_core_info_has_custom_config(core.UTF8String))
   {
      UIAlertView* alert = [[UIAlertView alloc] initWithTitle:@"RetroArch"
                                                      message:@"No custom configuration for this core exists, "
                                                               "would you like to create one?"
                                                     delegate:self
                                            cancelButtonTitle:@"No"
                                            otherButtonTitles:@"Yes", nil];
      objc_setAssociatedObject(alert, associated_core_key, core, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      [alert show];
   }
   else
      [self.navigationController pushViewController:[[RACoreSettingsMenu alloc] initWithCore:core] animated:YES];
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex
{
   NSString* core_id = objc_getAssociatedObject(alertView, associated_core_key);
   
   if (buttonIndex == alertView.firstOtherButtonIndex && core_id)
   {
      char path[PATH_MAX];
      apple_core_info_get_custom_config(core_id.UTF8String, path, sizeof(path));
   
      if (![[NSFileManager defaultManager] copyItemAtPath:apple_platform.globalConfigFile toPath:@(path) error:nil])
         RARCH_WARN("Could not create custom config at %s", path);
      [self.tableView reloadData];
   }

   [self.navigationController pushViewController:[[RACoreSettingsMenu alloc] initWithCore:core_id] animated:YES];
}

@end

/*********************************************/
/* RACoreOptionsMenu                         */
/* Menu object that allows editing of        */
/* options specific to the running core.     */
/*********************************************/
@interface RACoreOptionsMenu() <UIActionSheetDelegate>
@property (nonatomic) uint32_t currentIndex;
@end

@implementation RACoreOptionsMenu

- (id)init
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      RACoreOptionsMenu* __weak weakSelf = self;
      core_option_manager_t* options = g_extern.system.core_options;
   
      NSMutableArray* section = [NSMutableArray arrayWithObject:@""];
      [self.sections addObject:section];
   
      if (options)
      {
         for (int i = 0; i != core_option_size(options); i ++)
            [section addObject:[RAMenuItemBasic itemWithDescription:@(core_option_get_desc(options, i)) association:nil
               action:^{ [weakSelf editValue:i]; }
               detail:^{ return @(core_option_get_val(options, i)); }]];
      }
      else
         [section addObject:[RAMenuItemBasic itemWithDescription:@"The running core has no options." action:NULL]];
   }
   
   return self;
}

- (void)editValue:(uint32_t)index
{
   self.currentIndex = index;

   UIActionSheet* sheet = [UIActionSheet new];
   sheet.title = @(core_option_get_desc(g_extern.system.core_options, index));
   sheet.delegate = self;
  
   struct string_list* values = core_option_get_vals(g_extern.system.core_options, index);
   
   for (int i = 0; i != values->size; i ++)
      [sheet addButtonWithTitle:@(values->elems[i].data)];
   
   [sheet addButtonWithTitle:@"Cancel"];
   sheet.cancelButtonIndex = sheet.numberOfButtons - 1;
   
   [sheet showInView:self.tableView];
}

- (void)actionSheet:(UIActionSheet*)actionSheet clickedButtonAtIndex:(NSInteger)buttonIndex
{
   if (buttonIndex != actionSheet.cancelButtonIndex)
      core_option_set_val(g_extern.system.core_options, self.currentIndex, buttonIndex);
   
   [self.tableView reloadData];
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
   static NSString* const cell_id = @"RAMenuItemCoreList";

   UITableViewCell* result = [tableView dequeueReusableCellWithIdentifier:cell_id];
   if (!result)
   {
      result = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:cell_id];
//      UIButton* infoButton = [UIButton buttonWithType:UIButtonTypeInfoDark];
//      [infoButton addTarget:self action:@selector(infoButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
//      result.accessoryView = infoButton;
   }

   result.textLabel.text = apple_get_core_display_name(self.core);
   return result;
}

- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller
{
   if (self.parent.action)
      self.parent.action(self.core);
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

- (id)initWithPath:(NSString*)path action:(void (^)(NSString *))action
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      self.title = @"Choose Core";
      _action = action;
      _path = path;

      if (!_path)
      {
         RAMenuCoreList* __weak weakSelf = self;
         [self.sections addObject: @[@"", [RAMenuItemBasic itemWithDescription:@"Auto Detect"
               action: ^{ if(weakSelf.action) weakSelf.action(nil); }]]];
      }

      NSMutableArray* core_section = [NSMutableArray arrayWithObject:@"Cores"];
      [self.sections addObject:core_section];

      core_info_list_t* core_list = apple_core_info_list_get();
      if (core_list)
      {
         if (!_path)
            [self load:core_list->count coresFromList:core_list->list toSection:core_section];
         else
         {
            const core_info_t* core_support = 0;
            size_t core_count = 0;
            core_info_list_get_supported_cores(core_list, _path.UTF8String, &core_support, &core_count);
            
            if (core_count == 1 && _action)
               _action(apple_get_core_id(&core_support[0]));
            else if (core_count > 1)
               [self load:core_count coresFromList:core_support toSection:core_section];
         }
      }
   }

   return self;
}

- (void)load:(uint32_t)count coresFromList:(const core_info_t*)list toSection:(NSMutableArray*)array
{
   for (int i = 0; i < count; i ++)
      [array addObject:[[RAMenuItemCoreList alloc] initWithCore:apple_get_core_id(&list[i]) parent:self]];
}

@end

/*********************************************/
/* RALogMenu                                 */
/* Displays a text file line-by-line.        */
/*********************************************/
@implementation RALogMenu

- (id)initWithFile:(const char*)path
{
   if ((self = [super initWithStyle:UITableViewStylePlain]))
   {
      NSMutableArray* data = [NSMutableArray arrayWithObject:@""];

      fflush(stdout);
      fflush(stderr);
      FILE* file = fopen(path, "r");
      if (file)
      {
         char buffer[1024];
         while (fgets(buffer, 1024, file))
            [data addObject:[RAMenuItemBasic itemWithDescription:@(buffer) action:NULL]];
         fclose(file);
      }
      else
         [data addObject:[RAMenuItemBasic itemWithDescription:@"Logging not enabled" action:NULL]];
      
      [self.sections addObject:data];
      
   }
   
   return self;
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

      result.textLabel.text = @"Slot";
      
      UISegmentedControl* accessory = [[UISegmentedControl alloc] initWithItems:@[@"0", @"1", @"2", @"3", @"4", @"5", @"6", @"7", @"8", @"9"]];
      [accessory addTarget:self action:@selector(changed:) forControlEvents:UIControlEventValueChanged];
      accessory.segmentedControlStyle = UISegmentedControlStyleBar;
      result.accessoryView = accessory;
   }
   
   [(id)result.accessoryView setSelectedSegmentIndex:(g_extern.state_slot < 10) ? g_extern.state_slot : -1];

   return result;
}

- (void)changed:(UISegmentedControl*)sender
{
   g_extern.state_slot = sender.selectedSegmentIndex;
}

- (void)wasSelectedOnTableView:(UITableView *)tableView ofController:(UIViewController *)controller
{

}

@end

/*********************************************/
/* RAPauseMenu                               */
/* Menu which provides options for the       */
/* currently running game.                   */
/*********************************************/
@implementation RAPauseMenu

- (id)init
{
   if ((self = [super initWithStyle:UITableViewStyleGrouped]))
   {
      RAPauseMenu* __weak weakSelf = self;

      [self.sections addObject:@[@"Actions",
         [RAMenuItemBasic itemWithDescription:@"Reset Content" action:^{ [weakSelf performBasicAction:RESET]; }],
         [RAMenuItemBasic itemWithDescription:@"Close Content" action:^{ [weakSelf performBasicAction:QUIT]; }]
      ]];

      [self.sections addObject:@[@"States",
         [RAMenuItemStateSelect new],
         [RAMenuItemBasic itemWithDescription:@"Load State" action:^{ [weakSelf performBasicAction:LOAD_STATE]; }],
         [RAMenuItemBasic itemWithDescription:@"Save State" action:^{ [weakSelf performBasicAction:SAVE_STATE]; }]
      ]];
   
      [self.sections addObject:@[@"Settings",
         [RAMenuItemBasic itemWithDescription:@"System Config" action:^{ [[RetroArch_iOS get] showSystemSettings]; }],
         [RAMenuItemBasic itemWithDescription:@"Core Config"   action:^{ [[RetroArch_iOS get] showSettings]; }]
      ]];
   }
   
   return self;
}

- (void)performBasicAction:(enum basic_event_t)action
{
   [self.navigationController popViewControllerAnimated:(action != QUIT)];
   apple_frontend_post_event(apple_event_basic_command, action);
}

@end
