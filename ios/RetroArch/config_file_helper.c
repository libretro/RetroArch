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

#include <string.h>
#import "config_file_helper.h"

config_file_t* ios_config_open_or_new(const char* path)
{
   config_file_t* result = config_file_new(path);
   return result ? result : config_file_new(0);
}

void ios_config_file_write(config_file_t* config, const char* path)
{
   if (config)
      config_file_write(config, path);
}

bool ios_config_get_bool(config_file_t* config, const char* name, bool default_)
{
   if (!config) return default_;
   
   bool result = default_;
   config_get_bool(config, name, &result);
   return result;
}

int ios_config_get_int(config_file_t* config, const char* name, int default_)
{
   if (!config) return default_;
   
   int result = default_;
   config_get_int(config, name, &result);
   return result;
}

unsigned ios_config_get_uint(config_file_t* config, const char* name, unsigned default_)
{
   if (!config) return default_;
   
   unsigned result = default_;
   config_get_uint(config, name, &result);
   return result;
}

double ios_config_get_double(config_file_t* config, const char* name, double default_)
{
   if (!config) return default_;
   
   double result = default_;
   config_get_double(config, name, &result);
   return result;
}

char* ios_config_get_string(config_file_t* config, const char* name, const char* default_)
{
   if (config)
   {
      char* result = 0;
      if (config_get_string(config, name, &result))
         return result;
   }
   
   return default_ ? strdup(default_) : 0;
}

void ios_config_set_string(config_file_t* config, const char* name, const char* value)
{
   if (!config)
      return;
   
   config_set_string(config, name, value);
}
