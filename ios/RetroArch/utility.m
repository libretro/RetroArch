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

#include <sys/stat.h>

#include "general.h"
#include "file.h"
#import "views.h"

void ios_display_alert(NSString* message, NSString* title)
{
   UIAlertView* alert = [[UIAlertView alloc] initWithTitle:title ? title : @"RetroArch"
                                             message:message
                                             delegate:nil
                                             cancelButtonTitle:@"OK"
                                             otherButtonTitles:nil];
   [alert show];
}

// Little nudge to prevent stale values when reloading the confg file
void ios_clear_config_hack()
{
   g_extern.block_config_read = false;
   memset(g_settings.input.overlay, 0, sizeof(g_settings.input.overlay));
   memset(g_settings.video.shader_path, 0, sizeof(g_settings.video.shader_path));
}

// Fetch a value from a config file, returning defaultValue if the value is not present
NSString* ios_get_value_from_config(config_file_t* config, NSString* name, NSString* defaultValue)
{
   char* data = 0;
   if (config)
      config_get_string(config, [name UTF8String], &data);
   
   NSString* result = data ? [NSString stringWithUTF8String:data] : defaultValue;
   free(data);
   return result;
}

// Ensures a directory exists and has correct permissions
bool path_make_and_check_directory(const char* path, mode_t mode, int amode)
{
   if (!path_is_directory(path) && mkdir(path, mode) != 0)
      return false;
   
   return access(path, amode) == 0;
}

// Simple class to reduce code duplication for fixed table views
@implementation RATableViewController

- (id)initWithStyle:(UITableViewStyle)style
{
   self = [super initWithStyle:style];
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

@end
