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

#ifndef __APPLE_RARCH_SETTING_DATA_H__
#define __APPLE_RARCH_SETTING_DATA_H__

#include "general.h"

enum setting_type { ST_NONE, ST_BOOL, ST_INT, ST_UINT, ST_FLOAT, ST_PATH, ST_STRING, ST_HEX, ST_BIND,
                    ST_GROUP, ST_SUB_GROUP, ST_END_GROUP, ST_END_SUB_GROUP };
                    
enum setting_features
{
   SD_FEATURE_ALWAYS = 0,
   SD_FEATURE_MULTI_DRIVER = 1,
   SD_FEATURE_VIDEO_MODE = 2,
   SD_FEATURE_WINDOW_MANAGER = 4,
   SD_FEATURE_SHADERS = 8,
   SD_FEATURE_VSYNC = 16,
   SD_FEATURE_AUDIO_DEVICE = 32
};

enum setting_flags
{
   SD_FLAG_PATH_DIR = 1,
   SD_FLAG_PATH_FILE = 2,
   SD_FLAG_ALLOW_EMPTY = 4,
   SD_FLAG_VALUE_DESC = 8,
   SD_FLAG_HAS_RANGE = 16
};

typedef struct
{
   enum setting_type type;

   const char* name;
   uint32_t size;
   
   const char* short_description;

   uint32_t index;

   double min;
   double max;
   
   const char* values;
   uint64_t flags;
   
   union
   {
      bool boolean;
      int integer;
      unsigned int unsigned_integer;
      float fraction;
      const char* string;
      const struct retro_keybind* keybind;
   } default_value;
   
   union
   {
      bool* boolean;
      int* integer;
      unsigned int* unsigned_integer;
      float* fraction;
      char* string;
      struct retro_keybind* keybind;
   } value;
}  rarch_setting_t;

#define BINDFOR(s) (*(&s)->value.keybind)


void setting_data_reset_setting(const rarch_setting_t* setting);
void setting_data_reset(const rarch_setting_t* settings);

bool setting_data_load_config_path(const rarch_setting_t* settings, const char* path);
bool setting_data_load_config(const rarch_setting_t* settings, config_file_t* config);
bool setting_data_save_config_path(const rarch_setting_t* settings, const char* path);
bool setting_data_save_config(const rarch_setting_t* settings, config_file_t* config);

const rarch_setting_t* setting_data_find_setting(const rarch_setting_t* settings, const char* name);

void setting_data_set_with_string_representation(const rarch_setting_t* setting, const char* value);
const char* setting_data_get_string_representation(const rarch_setting_t* setting, char* buffer, size_t length);

// List building helper functions
rarch_setting_t setting_data_group_setting(enum setting_type type, const char* name);
rarch_setting_t setting_data_bool_setting(const char* name, const char* description, bool* target, bool default_value);
rarch_setting_t setting_data_int_setting(const char* name, const char* description, int* target, int default_value);
rarch_setting_t setting_data_uint_setting(const char* name, const char* description, unsigned int* target, unsigned int default_value);
rarch_setting_t setting_data_float_setting(const char* name, const char* description, float* target, float default_value);
rarch_setting_t setting_data_string_setting(enum setting_type type, const char* name, const char* description, char* target, unsigned size, const char* default_value);
rarch_setting_t setting_data_bind_setting(const char* name, const char* description, struct retro_keybind* target, uint32_t index,
                                    const struct retro_keybind* default_value);

// These functions operate only on RetroArch's main settings list
void setting_data_load_current();
const rarch_setting_t* setting_data_get_list();

// Keyboard
#include "keycode.h"

#endif
