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

static void* const associated_name_tag = (void*)&associated_name_tag;

@interface RAInputBinder : NSWindow
{
   NSTimer* _timer;
   const rarch_setting_t* _setting;
}

@property (nonatomic, retain) NSTimer* timer;
@property (nonatomic, assign) const rarch_setting_t* setting;
@end

@implementation RAInputBinder

@synthesize timer = _timer;
@synthesize setting = _setting;

- (void)dealloc
{
   [_timer release];
   [super dealloc];
}

- (void)runForSetting:(const rarch_setting_t*)setting onWindow:(NSWindow*)window
{
   self.setting = setting;
 
   self.timer = [NSTimer timerWithTimeInterval:.1f target:self selector:@selector(checkBind:) userInfo:nil repeats:YES];
   [[NSRunLoop currentRunLoop] addTimer:self.timer forMode:NSModalPanelRunLoopMode];
   
   [NSApp beginSheet:self modalForWindow:window modalDelegate:nil didEndSelector:nil contextInfo:nil];
}

- (IBAction)goAway:(id)sender
{
   [self.timer invalidate];
   self.timer = nil;
   
   [NSApp endSheet:self];
   [self orderOut:nil];
}

- (void)checkBind:(NSTimer*)send
{
   int32_t value = 0;
   int32_t index = _setting->index ? _setting->index - 1 : 0;
   
   if ((value = apple_input_find_any_key()))
      BINDFOR(*_setting).key = input_translate_keysym_to_rk(value);
   else if ((value = apple_input_find_any_button(index)) >= 0)
      BINDFOR(*_setting).joykey = value;
   else if ((value = apple_input_find_any_axis(index)))
      BINDFOR(*_setting).joyaxis = (value > 0) ? AXIS_POS(value - 1) : AXIS_NEG(abs(value) - 1);
   else
      return;
   
   [self goAway:self];
}

// Stop the annoying sound when pressing a key
- (void)keyDown:(NSEvent*)theEvent
{
}

@end


@interface RASettingsDelegate : NSObject<NSTableViewDataSource,   NSTableViewDelegate,
                                         NSOutlineViewDataSource, NSOutlineViewDelegate,
                                         NSWindowDelegate>
{
   RAInputBinder* _binderWindow;
   NSButtonCell* _booleanCell;
   NSTextFieldCell* _binderCell;
   NSTableView* _table;
   NSOutlineView* _outline;
   NSMutableArray* _settings;
   NSMutableArray* _currentGroup;
}

@property (nonatomic, retain) RAInputBinder IBOutlet* binderWindow;
@property (nonatomic, retain) NSButtonCell IBOutlet* booleanCell;
@property (nonatomic, retain) NSTextFieldCell IBOutlet* binderCell;
@property (nonatomic, retain) NSTableView IBOutlet* table;
@property (nonatomic, retain) NSOutlineView IBOutlet* outline;
@property (nonatomic, retain) NSMutableArray* settings;
@property (nonatomic, retain) NSMutableArray* currentGroup;
@end

@implementation RASettingsDelegate

@synthesize binderWindow = _binderWindow;
@synthesize booleanCell = _booleanCell;
@synthesize binderCell = _binderCell;
@synthesize table = _table;
@synthesize outline = _outline;
@synthesize settings = _settings;
@synthesize currentGroup = _currentGroup;

- (void)dealloc
{
   [_binderWindow release];
   [_booleanCell release];
   [_binderCell release];
   [_table release];
   [_outline release];
   [_settings release];
   [_currentGroup release];
   
   [super dealloc];
}

- (void)awakeFromNib
{
   NSMutableArray* thisGroup = nil;
   NSMutableArray* thisSubGroup = nil;
   self.settings = [NSMutableArray array];

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
            if (thisGroup)
               [self.settings addObject:thisGroup];
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
            if (thisSubGroup)
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
   
   setting_data_load_config_path(setting_data_get_list(), [apple_platform.globalConfigFile UTF8String]);
}

- (void)windowWillClose:(NSNotification *)notification
{
   setting_data_save_config_path(setting_data_get_list(), [apple_platform.globalConfigFile UTF8String]);
   [NSApp stopModal];
}

#pragma mark Section Table
- (NSInteger)numberOfRowsInTableView:(NSTableView*)view
{
   return [self.settings count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
   return objc_getAssociatedObject([self.settings objectAtIndex:row], associated_name_tag);
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
   self.currentGroup = [self.settings objectAtIndex:[self.table selectedRow]];
   [self.outline reloadData];
}

#pragma mark Setting Outline
- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
   return (item == nil) ? [self.currentGroup count] : [item count];
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
   return (item == nil) ? [self.currentGroup objectAtIndex:index] : [item objectAtIndex:index];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
   return [item isKindOfClass:[NSArray class]];
}

- (BOOL)validateProposedFirstResponder:(NSResponder*)responder forEvent:(NSEvent*)event
{
    return YES;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
   if (!tableColumn)
      return nil;

   if ([item isKindOfClass:[NSArray class]])
   {
      if ([[tableColumn identifier] isEqualToString:@"left"])
         return objc_getAssociatedObject(item, associated_name_tag);
      else
         return @"";
   }
   else
   {
      const rarch_setting_t* setting_data = setting_data_get_list();
      const rarch_setting_t* setting = &setting_data[[item intValue]];
      char buffer[PATH_MAX];
      
      if ([[tableColumn identifier] isEqualToString:@"left"])
         return BOXSTRING(setting->short_description);
      else
      {
         switch (setting->type)
         {
            case ST_BOOL: return BOXINT(*setting->value.boolean);
            default:      return BOXSTRING(setting_data_get_string_representation(setting, buffer, sizeof(buffer)));
         }
      }
   }
}

- (NSCell*)outlineView:(NSOutlineView *)outlineView dataCellForTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
   if (!tableColumn)
      return nil;
   
   if ([item isKindOfClass:[NSArray class]])
      return [tableColumn dataCell];
   
   if ([[tableColumn identifier] isEqualToString:@"left"])
      return [tableColumn dataCell];

   const rarch_setting_t* setting_data = setting_data_get_list();
   const rarch_setting_t* setting = &setting_data[[item intValue]];

   switch (setting->type)
   {
      case ST_BOOL: return self.booleanCell;
      case ST_BIND: return self.binderCell;
      default:      return [tableColumn dataCell];
   }
}

- (IBAction)outlineViewClicked:(id)sender
{
   if ([self.outline clickedColumn] == 1)
   {
      id item = [self.outline itemAtRow:[self.outline clickedRow]];
      
      if ([item isKindOfClass:[NSNumber class]])
      {
         const rarch_setting_t* setting_data = setting_data_get_list();
         const rarch_setting_t* setting = &setting_data[[item intValue]];
   
         switch (setting->type)
         {
            case ST_BOOL: *setting->value.boolean = !*setting->value.boolean; return;
            case ST_BIND: [self.binderWindow runForSetting:setting onWindow:[self.outline window]]; return;
            default: return;
         }
      }
   }
}

- (void)controlTextDidEndEditing:(NSNotification*)notification
{
   if ([notification object] == self.outline)
   {
      NSText* editor = [[notification userInfo] objectForKey:@"NSFieldEditor"];
      
      id item = [self.outline itemAtRow:[self.outline selectedRow]];

      if ([item isKindOfClass:[NSNumber class]])
      {
         const rarch_setting_t* setting_data = setting_data_get_list();
         const rarch_setting_t* setting = &setting_data[[item intValue]];
         
         setting_data_set_with_string_representation(setting, [editor.string UTF8String]);
      }
   }
}

@end
