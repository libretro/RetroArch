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

#import <objc/runtime.h>
#import "apple/common/RetroArch_Apple.h"
#include "apple/common/setting_data.h"
#include "apple/common/apple_input.h"

#include "driver.h"
#include "input/input_common.h"

static const void* associated_name_tag = (void*)&associated_name_tag;

@interface RAInputBinder : NSWindow
@end

@implementation RAInputBinder

- (IBAction)goAway:(id)sender
{
   [NSApp endSheet:self];
   [self orderOut:nil];
}

// Stop the annoying sound when pressing a key
- (void)keyDown:(NSEvent*)theEvent
{
}

@end

@interface RASettingCell : NSTableCellView
@property (nonatomic) const rarch_setting_t* setting;

@property (nonatomic) NSString* stringValue;
@property (nonatomic) IBOutlet NSNumber* numericValue;
@property (nonatomic) bool booleanValue;

@property (nonatomic) NSTimer* bindTimer;
@end

@implementation RASettingCell
- (void)setSetting:(const rarch_setting_t *)aSetting
{
   _setting = aSetting;
   
   if (!_setting)
      return;
   
   if (aSetting->type == ST_INT || aSetting->type == ST_UINT || aSetting->type == ST_FLOAT)
   {
      self.textField.formatter = [[RANumberFormatter alloc] initWithSetting:aSetting];
   }
   else
      self.textField.formatter = nil;

   // Set value
   switch (aSetting->type)
   {
      case ST_INT:    self.numericValue = @(*aSetting->value.integer); break;
      case ST_UINT:   self.numericValue = @(*aSetting->value.unsigned_integer); break;
      case ST_FLOAT:  self.numericValue = @(*aSetting->value.fraction); break;
      case ST_STRING: self.stringValue =  @( aSetting->value.string); break;
      case ST_PATH:   self.stringValue =  @( aSetting->value.string); break;
      case ST_BOOL:   self.booleanValue =   *aSetting->value.boolean; break;
      case ST_BIND:   [self updateInputString]; break;
      default:        break;
   }
}

- (IBAction)doBrowse:(id)sender
{
   NSOpenPanel* panel = [NSOpenPanel new];
   [panel runModal];
   
   if (panel.URLs.count == 1)
      self.stringValue = panel.URL.path;
}

- (void)setNumericValue:(NSNumber *)numericValue
{
   _numericValue = numericValue;
   
   if (_setting && _setting->type == ST_INT)
      *_setting->value.integer = _numericValue.intValue;
   else if (_setting && _setting->type == ST_UINT)
      *_setting->value.unsigned_integer = _numericValue.intValue;
   else if (_setting && _setting->type == ST_FLOAT)
      *_setting->value.fraction = _numericValue.floatValue;
}

- (void)setBooleanValue:(bool)booleanValue
{
   _booleanValue = booleanValue;
   
   if (_setting && _setting->type == ST_BOOL)
      *_setting->value.boolean= _booleanValue;
}

- (void)setStringValue:(NSString *)stringValue
{
   _stringValue = stringValue ? stringValue : @"";
   
   if (_setting && (_setting->type == ST_STRING || _setting->type == ST_PATH))
      strlcpy(_setting->value.string, _stringValue.UTF8String, _setting->size);
}

// Input Binding
- (void)updateInputString
{
   char buffer[256];
   self.stringValue = @(setting_data_get_string_representation(_setting, buffer, sizeof(buffer)));
}

- (void)dismissBinder
{
   [self.bindTimer invalidate];
   self.bindTimer = nil;

   [self updateInputString];

   [(id)self.window.attachedSheet goAway:nil];
}

- (void)checkBind:(NSTimer*)send
{
   int32_t value = 0;

   if ((value = apple_input_find_any_key()))
      BINDFOR(*_setting).key = input_translate_keysym_to_rk(value);
   else if ((value = apple_input_find_any_button(0)) >= 0)
      BINDFOR(*_setting).joykey = value;
   else if ((value = apple_input_find_any_axis(0)))
      BINDFOR(*_setting).joyaxis = (value > 0) ? AXIS_POS(value - 1) : AXIS_NEG(value - 1);
   else
      return;
   
   [self dismissBinder];
}

- (IBAction)doGetBind:(id)sender
{
   static NSWindowController* controller;
   if (!controller)
      controller = [[NSWindowController alloc] initWithWindowNibName:@"InputBinder"];
   
   self.bindTimer = [NSTimer timerWithTimeInterval:.1f target:self selector:@selector(checkBind:) userInfo:nil repeats:YES];
   [[NSRunLoop currentRunLoop] addTimer:self.bindTimer forMode:NSModalPanelRunLoopMode];

   [NSApp beginSheet:controller.window modalForWindow:self.window modalDelegate:nil didEndSelector:nil contextInfo:nil];
}

@end

@interface RASettingsDelegate : NSObject<NSTableViewDataSource,   NSTableViewDelegate,
                                         NSOutlineViewDataSource, NSOutlineViewDelegate,
                                         NSWindowDelegate>
@end

@implementation RASettingsDelegate
{
   NSWindow IBOutlet* _inputWindow;
   NSTableView IBOutlet* _table;
   NSOutlineView IBOutlet* _outline;
   
   NSMutableArray* _settings;
   NSMutableArray* _currentGroup;
}

- (void)awakeFromNib
{
   apple_enter_stasis();

   NSMutableArray* thisGroup = nil;
   NSMutableArray* thisSubGroup = nil;
   _settings = [NSMutableArray array];

   setting_data_load_current();

   const rarch_setting_t* setting_data = setting_data_get_list();

   for (int i = 0; setting_data[i].type; i ++)
   {
      switch (setting_data[i].type)
      {
         case ST_GROUP:
         {
            thisGroup = [NSMutableArray array];
            objc_setAssociatedObject(thisGroup, associated_name_tag, [NSString stringWithFormat:@"%s", setting_data[i].name], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            break;
         }
         
         case ST_END_GROUP:
         {
            [_settings addObject:thisGroup];
            thisGroup = nil;
            break;
         }
         
         case ST_SUB_GROUP:
         {
            thisSubGroup = [NSMutableArray array];
            objc_setAssociatedObject(thisSubGroup, associated_name_tag, [NSString stringWithFormat:@"%s", setting_data[i].name], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            break;
         }
         
         case ST_END_SUB_GROUP:
         {
            [thisGroup addObject:thisSubGroup];
            thisSubGroup = nil;
            break;
         }

         default:
         {
            [thisSubGroup addObject:[NSNumber numberWithInt:i]];
            break;
         }
      }
   }
   
   setting_data_load_config_path(setting_data_get_list(), apple_platform.globalConfigFile.UTF8String);
}

- (void)windowWillClose:(NSNotification *)notification
{
   setting_data_save_config_path(setting_data_get_list(), apple_platform.globalConfigFile.UTF8String);

   apple_exit_stasis(true);

   [NSApp stopModal];
}

#pragma mark View Builders
- (NSView*)labelAccessoryFor:(NSString*)text onTable:(NSTableView*)table
{
   RASettingCell* result = [table makeViewWithIdentifier:@"RALabelSetting" owner:nil];
   result.stringValue = text;
   return result;
}

#pragma mark Section Table
- (NSInteger)numberOfRowsInTableView:(NSTableView*)view
{
   return _settings.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
   return [self labelAccessoryFor:objc_getAssociatedObject(_settings[row], associated_name_tag) onTable:tableView];
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
   _currentGroup = _settings[_table.selectedRow];
   [_outline reloadData];
}

#pragma mark Setting Outline
- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
   return (item == nil) ? _currentGroup.count : [item count];
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
   return (item == nil) ? _currentGroup[index] : [item objectAtIndex:index];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
   return [item isKindOfClass:[NSArray class]];
}

- (BOOL)validateProposedFirstResponder:(NSResponder *)responder forEvent:(NSEvent *)event {
    return YES;
}

- (NSView *)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
   if ([item isKindOfClass:[NSArray class]])
   {
      if ([tableColumn.identifier isEqualToString:@"title"])
         return [self labelAccessoryFor:objc_getAssociatedObject(item, associated_name_tag) onTable:outlineView];
      else
         return [self labelAccessoryFor:[NSString stringWithFormat:@"%d items", (int)[item count]] onTable:outlineView];
   }
   else
   {
      const rarch_setting_t* setting_data = setting_data_get_list();
      const rarch_setting_t* setting = &setting_data[[item intValue]];

      if ([tableColumn.identifier isEqualToString:@"title"])
         return [self labelAccessoryFor:@(setting->short_description) onTable:outlineView];
      else if([tableColumn.identifier isEqualToString:@"accessory"])
      {
         RASettingCell* s = nil;
         switch (setting->type)
         {
            case ST_BOOL:
               s = [outlineView makeViewWithIdentifier:@"RABooleanSetting" owner:nil];
               break;
            case ST_INT:
               s = [outlineView makeViewWithIdentifier:@"RANumericSetting" owner:nil];
               break;
            case ST_UINT:
               s = [outlineView makeViewWithIdentifier:@"RANumericSetting" owner:nil];
               break;
            case ST_FLOAT:
               s = [outlineView makeViewWithIdentifier:@"RANumericSetting" owner:nil];
               break;
            case ST_PATH:
               s = [outlineView makeViewWithIdentifier:@"RAPathSetting"    owner:nil];
               break;
            case ST_STRING:
               s = [outlineView makeViewWithIdentifier:@"RAStringSetting"  owner:nil];
               break;
            case ST_BIND:
               s = [outlineView makeViewWithIdentifier:@"RABindSetting"    owner:nil];
               break;
            default:
               break;
         }
         s.setting = setting;
         return s;
      }
   }
   
   return nil;
}

@end
