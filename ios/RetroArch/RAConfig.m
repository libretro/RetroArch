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
   if (_config)
      config_file_free(_config);
}

- (void)writeToFile:(NSString*)path
{
   if (_config)
      config_file_write(_config, [path UTF8String]);
}

- (int)getIntNamed:(NSString*)name withDefault:(int)def
{
   int result = def;
   
   if (_config)
      config_get_int(_config, [name UTF8String], &result);
   
   return result;
}

- (unsigned)getUintNamed:(NSString*)name withDefault:(unsigned)def
{
   unsigned result = def;
   
   if (_config)
      config_get_uint(_config, [name UTF8String], &result);
   
   return result;
}

- (NSString*)getStringNamed:(NSString*)name withDefault:(NSString*)def
{
   NSString* result = def;
   
   if (_config)
   {
      char* data = 0;
      if (config_get_string(_config, [name UTF8String], &data))
         result = [NSString stringWithUTF8String:data];
      free(data);
   }
   
   return result;
}

- (void)putIntNamed:(NSString*)name value:(int)value
{
   if (_config)
      config_set_int(_config, [name UTF8String], value);
}

- (void)putStringNamed:(NSString*)name value:(NSString*)value
{
   if (_config)
      config_set_string(_config, [name UTF8String], [value UTF8String]);
}

@end
