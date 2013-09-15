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

static const char* get_input_config_key(const rarch_setting_t* setting, const char* type)
{
   static char buffer[32];
   if (setting->input_player)
      snprintf(buffer, 32, "input_player%d_%s%c%s", setting->input_player, setting->name, type ? '_' : '\0', type);
   else
      snprintf(buffer, 32, "input_%s%c%s", setting->name, type ? '_' : '\0', type);
   return buffer;
}

static const char* get_button_name(const rarch_setting_t* setting)
{
   static char buffer[32];

   if (BINDFOR(*setting).joykey == NO_BTN)
      return "nul";

   snprintf(buffer, 32, "%lld", BINDFOR(*setting).joykey);
   return buffer;
}

static const char* get_axis_name(const rarch_setting_t* setting)
{
   static char buffer[32];
   
   uint32_t joyaxis = BINDFOR(*setting).joyaxis;
   
   if (AXIS_NEG_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buffer, 8, "-%d", AXIS_NEG_GET(joyaxis));
   else if (AXIS_POS_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buffer, 8, "+%d", AXIS_POS_GET(joyaxis));
   else
      return "nul";
   
   return buffer;
}

@interface RANumberFormatter : NSNumberFormatter
@end

@implementation RANumberFormatter
- (id)initWithFloatSupport:(bool)allowFloat minimum:(double)min maximum:(double)max
{
   self = [super init];
   self.allowsFloats = allowFloat;
   self.maximumFractionDigits = 10;
   
   if (min || max)
   {
      self.minimum = @(min);
      self.maximum = @(max);
   }
   
   return self;
}

- (BOOL)isPartialStringValid:(NSString*)partialString newEditingString:(NSString**)newString errorDescription:(NSString**)error
{
   bool hasDot = false;

   if (partialString.length)
      for (int i = 0; i != partialString.length; i ++)
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
@end


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
   
   if (aSetting->type == ST_INT || aSetting->type == ST_FLOAT)
   {
      self.textField.formatter = [[RANumberFormatter alloc] initWithFloatSupport:aSetting->type == ST_FLOAT
                                                                         minimum:aSetting->min
                                                                         maximum:aSetting->max];
   }
   else
      self.textField.formatter = nil;

   // Set value
   switch (aSetting->type)
   {
      case ST_INT:    self.numericValue = @(*(int*)aSetting->value); break;
      case ST_FLOAT:  self.numericValue = @(*(float*)aSetting->value); break;
      case ST_STRING: self.stringValue = @((const char*)aSetting->value); break;
      case ST_PATH:   self.stringValue = @((const char*)aSetting->value); break;
      case ST_BOOL:   self.booleanValue = *(bool*)aSetting->value; break;
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
   _stringValue = stringValue ? stringValue : @"";
   
   if (_setting && (_setting->type == ST_STRING || _setting->type == ST_PATH))
      strlcpy(_setting->value, _stringValue.UTF8String, _setting->size);
}

// Input Binding
- (void)updateInputString
{
   self.stringValue = [NSString stringWithFormat:@"[KB:%s] [JS:%s] [AX:%s]", key_name_for_rk(BINDFOR(*_setting).key),
                                                                              get_button_name(_setting),
                                                                              get_axis_name(_setting)];
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
   config_file_t* conf = config_file_new(apple_platform.globalConfigFile.UTF8String);

   for (int i = 0; setting_data[i].type; i ++)
   {
      switch (setting_data[i].type)
      {
         case ST_BOOL:   config_get_bool  (conf, setting_data[i].name,  (bool*)setting_data[i].value); break;
         case ST_INT:    config_get_int   (conf, setting_data[i].name,   (int*)setting_data[i].value); break;
         case ST_FLOAT:  config_get_float (conf, setting_data[i].name, (float*)setting_data[i].value); break;
         case ST_PATH:   config_get_array (conf, setting_data[i].name,  (char*)setting_data[i].value, setting_data[i].size); break;
         case ST_STRING: config_get_array (conf, setting_data[i].name,  (char*)setting_data[i].value, setting_data[i].size); break;
         
         case ST_BIND:
         {
            input_config_parse_key       (conf, "input_player1", setting_data[i].name, setting_data[i].value);
            input_config_parse_joy_button(conf, "input_player1", setting_data[i].name, setting_data[i].value);
            input_config_parse_joy_axis  (conf, "input_player1", setting_data[i].name, setting_data[i].value);
            break;
         }
         
         case ST_HEX:    break;
         default:        break;
      }
   }
   config_file_free(conf);
}

- (void)windowWillClose:(NSNotification *)notification
{
   config_file_t* conf = config_file_new(apple_platform.globalConfigFile.UTF8String);
   conf = conf ? conf : config_file_new(0);
   
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
            config_set_string(conf, get_input_config_key(&setting_data[i], 0     ), key_name_for_rk(BINDFOR(setting_data[i]).key));
            config_set_string(conf, get_input_config_key(&setting_data[i], "btn" ), get_button_name(&setting_data[i]));
            config_set_string(conf, get_input_config_key(&setting_data[i], "axis"), get_axis_name(&setting_data[i]));
            break;
         }
         
         case ST_HEX:    break;
         default:        break;
      }
   }
   config_file_write(conf, apple_platform.globalConfigFile.UTF8String);
   config_file_free(conf);

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
               default:        break;
         }
         s.setting = setting;
         return s;
      }
   }
   
   return nil;
}

@end
