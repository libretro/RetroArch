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

#import "RAConfig.h"
#include "conf/config_file.h"

@implementation RAConfig
{
   config_file_t* _config;
}

- (id)initWithPath:(NSString*)path
{
   _config = config_file_new([path UTF8String]);
   _config = _config ? _config : config_file_new(0);
   return self;
}

- (void)dealloc
{
   config_file_free(_config);
}

- (void)writeToFile:(NSString*)path
{
   config_file_write(_config, [path UTF8String]);
}

- (bool)getBoolNamed:(NSString*)name withDefault:(bool)def
{
   bool result = def;
   config_get_bool(_config, [name UTF8String], &result);
   return result;
}

- (int)getIntNamed:(NSString*)name withDefault:(int)def
{
   int result = def;
   config_get_int(_config, [name UTF8String], &result);
   return result;
}

- (unsigned)getUintNamed:(NSString*)name withDefault:(unsigned)def
{
   unsigned result = def;
   config_get_uint(_config, [name UTF8String], &result);
   return result;
}

- (double)getDoubleNamed:(NSString*)name withDefault:(double)def
{
   double result = def;
   config_get_double(_config, [name UTF8String], &result);
   return result;
}

- (NSString*)getStringNamed:(NSString*)name withDefault:(NSString*)def
{
   NSString* result = def;
   
   char* data = 0;
   if (config_get_string(_config, [name UTF8String], &data))
      result = [NSString stringWithUTF8String:data];
   free(data);
  
   return result;
}

- (void)putIntNamed:(NSString*)name value:(int)value
{
   config_set_int(_config, [name UTF8String], value);
}

- (void)putStringNamed:(NSString*)name value:(NSString*)value
{
   config_set_string(_config, [name UTF8String], [value UTF8String]);
}

@end
