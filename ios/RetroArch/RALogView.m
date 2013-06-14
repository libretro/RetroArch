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

#import "views.h"

static NSMutableArray* g_messages;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

#ifndef HAVE_DEBUG_DIAGLOG
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
#else
static FILE* old_stdout;
static FILE* old_stderr;
static FILE* log_stream;

static int stdout_write(void* mem_buffer, const char* data, int len)
{
   pthread_mutex_lock(&g_lock);
   
   g_messages = g_messages ? g_messages : [NSMutableArray array];

   char buffer[len + 10];
   strncpy(buffer, data, len);
   buffer[len] = 0;
   
   [g_messages addObject:[NSString stringWithFormat:@"%s", buffer]];
   
   pthread_mutex_unlock(&g_lock);
   
   return len;
}

void ios_log_init()
{
   if (!log_stream)
   {
      old_stdout = stdout;
      old_stderr = stderr;

      log_stream = fwopen(0, stdout_write);
      setvbuf(log_stream, 0, _IOLBF, 0);
      
      stdout = log_stream;
      stderr = log_stream;
   }
}

void ios_log_quit()
{
   if (log_stream)
   {
      stdout = old_stdout;
      stderr = old_stderr;
      fclose(log_stream);
      log_stream = 0;
   }
}
#endif

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
