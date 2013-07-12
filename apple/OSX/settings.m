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
   
   for (int i = 0; setting_data[i].type; i ++)
   {
      switch (setting_data[i].type)
      {
         case ST_GROUP:
         {
            thisGroup = [NSMutableArray array];
            objc_setAssociatedObject(thisGroup, "NAME", [NSString stringWithFormat:@"%s", setting_data[i].name], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
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
            objc_setAssociatedObject(thisSubGroup, "NAME", [NSString stringWithFormat:@"%s", setting_data[i].name], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
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
   [NSApplication.sharedApplication stopModal];
   [NSApplication.sharedApplication endSheet:_window returnCode:0];
   [_window orderOut:nil];
}


#pragma mark View Builders
- (NSView*)labelAccessoryFor:(NSString*)text onTable:(NSTableView*)table
{
   NSTextField* result = [table makeViewWithIdentifier:@"label" owner:self];
   if (result == nil)
   {
      result = [NSTextField new];
      result.bordered = NO;
      result.drawsBackground = NO;
      result.identifier = @"label";
   }
 
   result.stringValue = text;
   return result;
}

- (NSView*)booleanAccessoryFor:(const rarch_setting_t*)setting onTable:(NSTableView*)table
{
   NSButton* result = [table makeViewWithIdentifier:@"boolean" owner:self];
   
   if (!result)
   {
      result = [NSButton new];
      result.buttonType = NSSwitchButton;
      result.title = @"";
   }
   
   result.state = *(bool*)setting->value;
   return result;
}

#pragma mark Section Table
- (NSInteger)numberOfRowsInTableView:(NSTableView*)view
{
   return _settings.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
   return [self labelAccessoryFor:objc_getAssociatedObject(_settings[row], "NAME") onTable:tableView];
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

- (NSView *)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(NSTableColumn *)tableColumn item:(id)item
{
   if ([item isKindOfClass:[NSArray class]])
   {
      if ([tableColumn.identifier isEqualToString:@"title"])
         return [self labelAccessoryFor:objc_getAssociatedObject(item, "NAME") onTable:outlineView];
      else
         return [self labelAccessoryFor:[NSString stringWithFormat:@"%d items", (int)[item count]] onTable:outlineView];
   }
   else
   {
      const rarch_setting_t* setting = &setting_data[[item intValue]];

      if ([tableColumn.identifier isEqualToString:@"title"])
         return [self labelAccessoryFor:[NSString stringWithFormat:@"%s", setting->short_description] onTable:outlineView]; // < The outlineView will fill the value
      else if([tableColumn.identifier isEqualToString:@"accessory"])
      {
         switch (setting->type)
         {
            case ST_BOOL: return [self booleanAccessoryFor:setting onTable:outlineView];
         
            case ST_PATH:
            case ST_STRING:
               return [self labelAccessoryFor:[NSString stringWithFormat:@"%s", (const char*)setting->value] onTable:outlineView];
            
            case ST_INT:
               return [self labelAccessoryFor:[NSString stringWithFormat:@"%d", *(int*)setting->value] onTable:outlineView];

            case ST_FLOAT:
               return [self labelAccessoryFor:[NSString stringWithFormat:@"%f", *(float*)setting->value] onTable:outlineView];

            default: abort();
         }
      }
   }
   
   return nil;
}

@end
