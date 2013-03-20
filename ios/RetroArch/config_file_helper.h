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

#ifndef __IOS_RARCH_CONFIG_H__
#define __IOS_RARCH_CONFIG_H__

#include "conf/config_file.h"

bool ios_config_get_bool(config_file_t* config, const char* name, bool default_);
unsigned ios_config_get_uint(config_file_t* config, const char* name, unsigned default_);
double ios_config_get_double(config_file_t* config, const char* name, double default_);

// You must free the result, even if it returns default_!
char* ios_config_get_string(config_file_t* config, const char* name, const char* default_);

void ios_config_set_string(config_file_t* config, const char* name, const char* value);

#endif

