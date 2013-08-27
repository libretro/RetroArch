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
#import "../RetroArch/RetroArch_Apple.h"
#include "../RetroArch/setting_data.h"
#include "../RetroArch/apple_input.h"

#include "driver.h"
#include "input/input_common.h"

struct settings fake_settings;
struct global fake_extern;

static const void* associated_name_tag = (void*)&associated_name_tag;

#define BINDFOR(s) (*(struct retro_keybind*)(&s)->value)

static const char* key_name_for_id(uint32_t hidkey)
{
   for (int i = 0; apple_key_name_map[i].hid_id; i ++)
      if (apple_key_name_map[i].hid_id == hidkey)
         return apple_key_name_map[i].keyname;

   return "nul";
}

static uint32_t key_id_for_name(const char* name)
{
   for (int i = 0; apple_key_name_map[i].hid_id; i ++)
      if (strcmp(name, apple_key_name_map[i].keyname) == 0)
         return apple_key_name_map[i].hid_id;
   
   return 0;
}

#define key_name_for_rk(X) key_name_for_id(input_translate_rk_to_keysym(X))
#define key_rk_for_name(X) input_translate_keysym_to_rk(key_id_for_name(X))

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
   
   switch (aSetting->type)
   {
      case ST_INT:    self.numericValue = @(*(int*)aSetting->value); break;
      case ST_FLOAT:  self.numericValue = @(*(float*)aSetting->value); break;
      case ST_STRING: self.stringValue = @((const char*)aSetting->value); break;
      case ST_PATH:   self.stringValue = @((const char*)aSetting->value); break;
      case ST_BOOL:   self.booleanValue = *(bool*)aSetting->value; break;
      case ST_BIND:   [self updateInputString]; break;
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
      *(int*)_setting->value = _numericValue.intValue;
   else if (_setting && _setting->type == ST_FLOAT)
      *(float*)_setting->value = _numericValue.floatValue;
}

- (void)setBooleanValue:(bool)booleanValue
{
   _booleanValue = booleanValue;
   
   if (_setting && _setting->type == ST_BOOL)
      *(bool*)_setting->value = _booleanValue;
}

- (void)setStringValue:(NSString *)stringValue
{
   _stringValue = stringValue;
   
   if (_setting && (_setting->type == ST_STRING || _setting->type == ST_PATH))
      strlcpy(_setting->value, _stringValue.UTF8String, _setting->size);
}

// Input Binding
- (void)updateInputString
{
   self.stringValue = [NSString stringWithFormat:@"[KB:%s] [JS:%lld] [AX:nul]", key_name_for_rk(BINDFOR(*_setting).key),
                                                                                BINDFOR(*_setting).joykey];
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
   // Keyboard
   for (int i = 0; apple_key_name_map[i].hid_id; i++)
   {
      if (g_current_input_data.keys[apple_key_name_map[i].hid_id])
      {
         BINDFOR(*_setting).key = input_translate_keysym_to_rk(apple_key_name_map[i].hid_id);
         [self dismissBinder];
         return;
      }
   }

   // Joystick
   if (g_current_input_data.pad_buttons[0])
   {
      for (int i = 0; i != 32; i ++)
      {
         if (g_current_input_data.pad_buttons[0] & (1 << i))
         {
            BINDFOR(*_setting).joykey = i;
            [self dismissBinder];
            return;
         }
      }
   }
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

@protocol RASettingView
@property const rarch_setting_t* setting;
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
   NSMutableArray* thisGroup = nil;
   NSMutableArray* thisSubGroup = nil;
   _settings = [NSMutableArray array];
   
   memcpy(&fake_settings, &g_settings, sizeof(struct settings));
   memcpy(&fake_extern, &g_extern, sizeof(struct global));
   
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
   
   [self load];
}

- (void)load
{
   config_file_t* conf = config_file_new([RetroArch_OSX get].configPath.UTF8String);
   for (int i = 0; setting_data[i].type; i ++)
   {
      switch (setting_data[i].type)
      {
         case ST_BOOL:   config_set_bool  (conf, setting_data[i].name, * (bool*)setting_data[i].value); break;
         case ST_INT:    config_set_int   (conf, setting_data[i].name, *  (int*)setting_data[i].value); break;
         case ST_FLOAT:  config_set_float (conf, setting_data[i].name, *(float*)setting_data[i].value); break;
         case ST_PATH:   config_set_string(conf, setting_data[i].name,   (char*)setting_data[i].value); break;
         case ST_STRING: config_set_string(conf, setting_data[i].name,   (char*)setting_data[i].value); break;
         
         case ST_BIND:   input_config_parse_key(conf, "input_player1", input_config_bind_map[0].base, setting_data[i].value);
                         input_config_parse_joy_button(conf, "input_player1", input_config_bind_map[0].base, setting_data[i].value);
                         input_config_parse_joy_axis(conf, "input_player1", input_config_bind_map[0].base, setting_data[i].value);
                         break;
         
         case ST_HEX:    break;
         default:        break;
      }
   }
   config_file_free(conf);
}

- (void)windowWillClose:(NSNotification *)notification
{
   config_file_t* conf = config_file_new(0);
   for (int i = 0; setting_data[i].type; i ++)
   {
      switch (setting_data[i].type)
      {
         case ST_BOOL:   config_set_bool  (conf, setting_data[i].name, * (bool*)setting_data[i].value); break;
         case ST_INT:    config_set_int   (conf, setting_data[i].name, *  (int*)setting_data[i].value); break;
         case ST_FLOAT:  config_set_float (conf, setting_data[i].name, *(float*)setting_data[i].value); break;
         case ST_PATH:   config_set_string(conf, setting_data[i].name,   (char*)setting_data[i].value); break;
         case ST_STRING: config_set_string(conf, setting_data[i].name,   (char*)setting_data[i].value); break;
         
         case ST_BIND:
         {
            config_set_string(conf, setting_data[i].name, key_name_for_rk(BINDFOR(setting_data[i]).key));
            break;
         }
         
         case ST_HEX:    break;
         default:        break;
      }
   }
   config_file_write(conf, [RetroArch_OSX get].configPath.UTF8String);
   config_file_free(conf);

   apple_refresh_config();

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
      const rarch_setting_t* setting = &setting_data[[item intValue]];

      if ([tableColumn.identifier isEqualToString:@"title"])
         return [self labelAccessoryFor:@(setting->short_description) onTable:outlineView];
      else if([tableColumn.identifier isEqualToString:@"accessory"])
      {
         RASettingCell* s = nil;
         switch (setting->type)
         {
            case ST_BOOL:   s = [outlineView makeViewWithIdentifier:@"RABooleanSetting" owner:nil]; break;
            case ST_INT:    s = [outlineView makeViewWithIdentifier:@"RANumericSetting" owner:nil]; break;
            case ST_FLOAT:  s = [outlineView makeViewWithIdentifier:@"RANumericSetting" owner:nil]; break;
            case ST_PATH:   s = [outlineView makeViewWithIdentifier:@"RAPathSetting"    owner:nil]; break;
            case ST_STRING: s = [outlineView makeViewWithIdentifier:@"RAStringSetting"  owner:nil]; break;
            case ST_BIND:   s = [outlineView makeViewWithIdentifier:@"RABindSetting"    owner:nil]; break;
         }
         s.setting = setting;
         return s;
      }
   }
   
   return nil;
}

@end
