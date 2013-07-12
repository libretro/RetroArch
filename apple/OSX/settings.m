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

@interface RASettingsDelegate : NSObject<NSTableViewDataSource>
@end

@implementation RASettingsDelegate
{
   NSWindow IBOutlet* _window;
   NSScrollView IBOutlet* _scroller;
   NSTableView IBOutlet* _table;
   
   NSMutableArray* _groups;
}

- (void)awakeFromNib
{
   _groups = [NSMutableArray array];

   NSMatrix* mtx = nil;
   NSMutableArray* subGroups = nil;

   for (int i = 0; setting_data[i].type; i ++)
   {
      const rarch_setting_t* s = &setting_data[i];

      if (s->type == ST_GROUP)
      {
         subGroups = [NSMutableArray array];
         objc_setAssociatedObject(subGroups, "NAME", [NSString stringWithFormat:@"%s", s->name], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      }
      else if(s->type == ST_END_GROUP)
      {
         NSView* view = [NSView new];
         uint32_t height = 0;
         
         for (NSMatrix* mtx in subGroups)
            height += mtx.frame.size.height + 20;
         
         view.frameSize = CGSizeMake(_scroller.frame.size.width, height);
         
         for (NSMatrix* mtx in subGroups)
         {
            mtx.frameOrigin = CGPointMake(0, height - mtx.frame.size.height);
            height -= mtx.frame.size.height + 20;
            
            NSBox* box = [[NSBox alloc] initWithFrame:mtx.frame];
            box.title = objc_getAssociatedObject(mtx, "NAME");
            box.contentView = mtx;
            [view addSubview:box];
         }

         [_groups addObject:view];
         objc_setAssociatedObject(view, "NAME", objc_getAssociatedObject(subGroups, "NAME"), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
         subGroups = nil;
      }
      else if (s->type == ST_SUB_GROUP)
      {
         mtx = [[NSMatrix alloc] initWithFrame:CGRectMake(0, 0, 480, 480)
                                 mode:NSHighlightModeMatrix
                                 prototype:nil
                                 numberOfRows:0
                                 numberOfColumns:2];
         mtx.cellSize = NSMakeSize(240, 20);
         objc_setAssociatedObject(mtx, "NAME", [NSString stringWithFormat:@"%s", s->name], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
      }
      else if (s->type == ST_END_SUB_GROUP)
      {
         [mtx sizeToCells];
         [subGroups addObject:mtx];
         mtx = nil;
      }
      else
      {
         NSTextFieldCell* label = [[NSTextFieldCell alloc] initTextCell:[NSString stringWithFormat:@"%s", s->short_description]];
         id accessory = nil;
      
         switch (s->type)
         {
            case ST_BOOL:
            {
               accessory = [NSButtonCell new];
            
               [accessory setButtonType:NSSwitchButton];
               [accessory setState:*(bool*)s->value];
               [accessory setTitle:@""];
               break;
            }
         
            case ST_STRING:
            case ST_PATH:
            case ST_INT:
            case ST_FLOAT:
            {
               accessory = [NSTextFieldCell new];
            
               if (s->type == ST_INT)        [accessory setIntValue:*(int32_t*)s->value];
               else if (s->type == ST_FLOAT) [accessory setFloatValue:*(float*)s->value];
               else                          [accessory setTitle:[NSString stringWithFormat:@"%s", (const char*)s->value]];
               break;
            }
         
            default: abort();
         }
         
         [mtx addRowWithCells:[NSArray arrayWithObjects:label, accessory, nil]];
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

- (NSInteger)numberOfRowsInTableView:(NSTableView*)view
{
   return _groups.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
   NSTextField* result = [tableView makeViewWithIdentifier:@"category" owner:self];
   if (result == nil)
   {
      result = [[NSTextField alloc] initWithFrame:CGRectMake(0, 0, 100, 10)];
      result.bordered = NO;
      result.drawsBackground = NO;
      result.identifier = @"category";
   }
 
   result.stringValue = objc_getAssociatedObject(_groups[row], "NAME");
   return result;
}

- (void)tableViewSelectionDidChange:(NSNotification *)aNotification
{
   _scroller.documentView = _groups[_table.selectedRow];
}

@end
