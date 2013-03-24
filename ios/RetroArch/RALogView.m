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

#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>

static NSMutableArray* g_messages;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void ios_add_log_message(const char* format, ...)
{
   pthread_mutex_lock(&g_lock);
   
   g_messages = g_messages ? g_messages : [NSMutableArray array];

   char buffer[512];

   va_list args;
   va_start(args, format);
   vsnprintf(buffer, 512, format, args);
   va_end(args);
   [g_messages addObject:[NSString stringWithUTF8String: buffer]];
   
   pthread_mutex_unlock(&g_lock);
}

@implementation RALogView

- (RALogView*)init
{
   self = [super initWithStyle:UITableViewStyleGrouped];
   [self setTitle:@"RetroArch Diagnostic Log"];
   return self;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView
{
   return 1;
}

- (NSString*)tableView:(UITableView*)tableView titleForHeaderInSection:(NSInteger)section
{
   return @"Messages";
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
   pthread_mutex_lock(&g_lock);
   NSInteger count = g_messages ? g_messages.count : 0;
   pthread_mutex_unlock(&g_lock);
   
   return count;
}

- (UITableViewCell*)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
   UITableViewCell* cell = [self.tableView dequeueReusableCellWithIdentifier:@"message"];
   cell = cell ? cell : [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault reuseIdentifier:@"message"];

   pthread_mutex_lock(&g_lock);
   cell.textLabel.text = g_messages[indexPath.row];
   pthread_mutex_unlock(&g_lock);

   return cell;
}

@end
