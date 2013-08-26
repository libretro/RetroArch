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

struct settings fake_settings;
struct global fake_extern;

static const void* associated_name_tag = (void*)&associated_name_tag;

@interface RASettingCell : NSTableCellView
@property (strong) NSString* stringValue;
@property (nonatomic) IBOutlet NSNumber* numericValue;
@property (nonatomic) bool booleanValue;
@property (nonatomic) const rarch_setting_t* setting;
@end

@implementation RASettingCell
- (void)setSetting:(const rarch_setting_t *)aSetting
{
   _setting = aSetting;
   
   switch (aSetting->type)
   {
      case ST_INT:    self.numericValue = @(*(int*)aSetting->value); break;
      case ST_FLOAT:  self.numericValue = @(*(float*)aSetting->value); break;
      case ST_STRING: self.stringValue = @((const char*)aSetting->value); break;
      case ST_PATH:   self.stringValue = @((const char*)aSetting->value); break;
      case ST_BOOL:   self.booleanValue = *(bool*)aSetting->value; break;
   }
}

- (IBAction)doBrowse:(id)sender
{
   NSOpenPanel* panel = [NSOpenPanel new];
   [panel runModal];
   
   if (panel.URLs.count == 1)
      self.stringValue = panel.URL.path;
}

- (IBAction)valueChanged:(id)sender
{
   printf("GABOR\n");
}

@end

@protocol RASettingView
@property const rarch_setting_t* setting;
@end

@interface RASettingsDelegate : NSObject<NSTableViewDataSource,   NSTableViewDelegate,
                                         NSOutlineViewDataSource, NSOutlineViewDelegate>
@end

@implementation RASettingsDelegate
{
   NSWindow IBOutlet* _window;
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
   
   [NSApplication.sharedApplication beginSheet:_window modalForWindow:RetroArch_OSX.get->window modalDelegate:nil didEndSelector:nil contextInfo:nil];
   [NSApplication.sharedApplication runModalForWindow:_window];
}

- (IBAction)close:(id)sender
{
#if 0
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
         case ST_HEX:    break;
         default:        break;
      }
   }
   config_file_free(conf);
#endif

   [NSApplication.sharedApplication stopModal];
   [NSApplication.sharedApplication endSheet:_window returnCode:0];
   [_window orderOut:nil];
}

- (void)readConfigFile:(const char*)path
{
   config_file_t* conf = config_file_new(path);
   if (conf)
   {
      for (int i = 0; setting_data[i].type; i ++)
      {
         switch (setting_data[i].type)
         {
            case ST_BOOL:   config_get_bool (conf, setting_data[i].name,  (bool*)setting_data[i].value); break;
            case ST_INT:    config_get_int  (conf, setting_data[i].name,   (int*)setting_data[i].value); break;
            case ST_FLOAT:  config_get_float(conf, setting_data[i].name, (float*)setting_data[i].value); break;
            case ST_PATH:   config_get_array(conf, setting_data[i].name,  (char*)setting_data[i].value, setting_data[i].size); break;
            case ST_STRING: config_get_array(conf, setting_data[i].name,  (char*)setting_data[i].value, setting_data[i].size); break;
            case ST_HEX:    break;
            default:        break;
         }
      }

      config_file_free(conf);
   }
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
         }
         s.setting = setting;
         return s;
      }
   }
   
   return nil;
}

@end
