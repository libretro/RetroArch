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

#ifndef __APPLE_RARCH_IOS_MENU_H__
#define __APPLE_RARCH_IOS_MENU_H__

#include "frontend/menu/history.h"
#include "views.h"
#include "apple/common/setting_data.h"

@protocol RAMenuItemBase
- (UITableViewCell*)cellForTableView:(UITableView*)tableView;
- (void)wasSelectedOnTableView:(UITableView*)tableView ofController:(UIViewController*)controller;
@end

/*********************************************/
/* RAMenuBase                                */
/* A menu class that displays RAMenuItemBase */
/* objects.                                  */
/*********************************************/
@interface RAMenuBase : UITableViewController
@property (nonatomic) NSMutableArray* sections;
@property (nonatomic) BOOL hidesHeaders;

- (id)initWithStyle:(UITableViewStyle)style;
- (id)itemForIndexPath:(NSIndexPath*)indexPath;
@end

/*********************************************/
/* RAMenuItemBasic                           */
/* A simple menu item that displays a text   */
/* description and calls a block object when */
/* selected.                                 */
/*********************************************/
@interface RAMenuItemBasic : NSObject<RAMenuItemBase>
@property (nonatomic) NSString* description;
@property (nonatomic) id userdata;
@property (copy) void (^action)(id userdata);
@property (copy) NSString* (^detail)(id userdata);

+ (RAMenuItemBasic*)itemWithDescription:(NSString*)description association:(id)userdata action:(void (^)())action detail:(NSString* (^)())detail;
@end

/*********************************************/
/* RAMenuItemGeneralSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a string or */
/* numeric setting.                          */
/*********************************************/
@interface RAMenuItemGeneralSetting : NSObject<RAMenuItemBase>
@property (nonatomic) const rarch_setting_t* setting;
@property (nonatomic, weak) UITableView* parentTable;
+ (id)itemForSetting:(const rarch_setting_t*)setting;
- (id)initWithSetting:(const rarch_setting_t*)setting;
@end

/*********************************************/
/* RAMenuItemBooleanSetting                  */
/* A simple menu item that displays the      */
/* state, and allows editing, of a boolean   */
/* setting.                                  */
/*********************************************/
@interface RAMenuItemBooleanSetting : NSObject<RAMenuItemBase>
@property (nonatomic) const rarch_setting_t* setting;
- (id)initWithSetting:(const rarch_setting_t*)setting;
@end

/*********************************************/
/* RAMenuItemPathSetting                     */
/* A menu item that displays and allows      */
/* browsing for a path setting.              */
/*********************************************/
@interface RAMenuItemPathSetting : RAMenuItemGeneralSetting<RAMenuItemBase> @end

/*********************************************/
/* RAMenuItemEnumSetting                     */
/* A menu item that displays and allows      */
/* a setting to be set from a list of        */
/* allowed choices.                          */
/*********************************************/
@interface RAMenuItemEnumSetting : RAMenuItemGeneralSetting<RAMenuItemBase> @end

/*********************************************/
/* RAMenuItemBindSetting                     */
/* A menu item that displays and allows      */
/* mapping of a keybinding.                  */
/*********************************************/
@interface RAMenuItemBindSetting : RAMenuItemGeneralSetting<RAMenuItemBase> @end

/*********************************************/
/* RAMainMenu                                */
/* Menu object that is displayed immediately */
/* after startup.                            */
/*********************************************/
@interface RAMainMenu : RAMenuBase<RADirectoryListDelegate>
@property (nonatomic) NSString* core;
@end

/*********************************************/
/* RAHistoryMenu                             */
/* Menu object that displays and allows      */
/* launching a file from the ROM history.    */
/*********************************************/
@interface RAHistoryMenu : RAMenuBase
@property (nonatomic) rom_history_t* history;
- (id)initWithHistoryPath:(NSString*)historyPath;
@end

/*********************************************/
/* RASettingsGroupMenu                       */
/* Menu object that displays and allows      */
/* editing of the a group of                 */
/* rarch_setting_t structures.               */
/*********************************************/
@interface RASettingsGroupMenu : RAMenuBase
- (id)initWithGroup:(const rarch_setting_t*)settings;
@end

/*********************************************/
/* RACoreSettingsMenu                        */
/* Menu object that displays and allows      */
/* editing of the setting_data list.         */
/*********************************************/
@interface RACoreSettingsMenu : RAMenuBase
@property (nonatomic) NSString* core;
- (id)initWithCore:(NSString*)core;
@end

/*********************************************/
/* RAFrontendSettingsMenu                    */
/* Menu object that displays and allows      */
/* editing of cocoa frontend related         */
/* settings.                                 */
/*********************************************/
@interface RAFrontendSettingsMenu : RASettingsGroupMenu @end

/*********************************************/
/* RACoreOptionsMenu                         */
/* Menu object that allows editing of        */
/* options specific to the running core.     */
/*********************************************/
@interface RACoreOptionsMenu : RAMenuBase @end

/*********************************************/
/* RAMenuItemCoreList                        */
/* Menu item that handles display and        */
/* selection of an item in RAMenuCoreList.   */
/* This item will not function on anything   */
/* but an RAMenuCoreList type menu.          */
/*********************************************/
@class RAMenuCoreList;
@interface RAMenuItemCoreList : NSObject<RAMenuItemBase>
@property (nonatomic, weak) RAMenuCoreList* parent;
@property (nonatomic) NSString* core;
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
@interface RAMenuCoreList : RAMenuBase
@property (nonatomic) NSString* path;
@property (copy) void (^action)(NSString* coreID);
@property (nonatomic) bool actionRan;
- (id)initWithPath:(NSString*)path action:(void (^)(NSString*))action;
- (void)runAction:(NSString*)coreID;
@end

/*********************************************/
/* RALogMenu                                 */
/* Displays a text file line-by-line.        */
/*********************************************/
@interface RALogMenu : RAMenuBase
- (id)initWithFile:(const char*)path;
@end

/*********************************************/
/* RAMenuItemStateSelect                     */
/* Menu item that allows save state slots    */
/* 0-9 to be selected.                       */
/*********************************************/
@interface RAMenuItemStateSelect : NSObject<RAMenuItemBase> @end

#endif