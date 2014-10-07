/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#ifndef __RARCH_SETTINGS_DATA_H__
#define __RARCH_SETTINGS_DATA_H__

#include <stdint.h>
#include "conf/config_file.h"
#include "miscellaneous.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*change_handler_t)(void *data);

#define BINDFOR(s) (*(&(s))->value.keybind)

enum setting_type
{
   ST_NONE = 0,
   ST_BOOL,
   ST_INT,
   ST_UINT,
   ST_FLOAT,
   ST_PATH,
   ST_DIR,
   ST_STRING,
   ST_HEX,
   ST_BIND,
   ST_GROUP,
   ST_SUB_GROUP,
   ST_END_GROUP,
   ST_END_SUB_GROUP
};
                    
enum setting_flags
{
   SD_FLAG_PATH_DIR       = (1 << 0),
   SD_FLAG_PATH_FILE      = (1 << 1),
   SD_FLAG_ALLOW_EMPTY    = (1 << 2),
   SD_FLAG_VALUE_DESC     = (1 << 3),
   SD_FLAG_HAS_RANGE      = (1 << 4),
   SD_FLAG_ALLOW_INPUT    = (1 << 5),
   SD_FLAG_PUSH_ACTION    = (1 << 6),
   SD_FLAG_IS_DRIVER      = (1 << 7),
   SD_FLAG_EXIT           = (1 << 8),
   SD_FLAG_CMD_APPLY_AUTO = (1 << 9),
};

typedef struct rarch_setting_info
{
   int index;
   int size;
} rarch_setting_info_t;

typedef struct rarch_setting_group_info
{
   const char *name;
} rarch_setting_group_info_t;

typedef struct rarch_setting
{
   enum setting_type type;

   const char *name;
   uint32_t size;
   
   const char* short_description;
   const char* group;
   const char* subgroup;

   uint32_t index;

   double min;
   double max;
   
   const char* values;
   uint64_t flags;
   
   change_handler_t change_handler;
   change_handler_t read_handler;
   
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

   struct
   {
      const char *empty_path;
   } dir;

   struct
   {
      unsigned idx;
      bool triggered;
   } cmd_trigger;

   struct
   {
      const char *off_label;
      const char *on_label;
   } boolean;

   float step;
   const char *rounding_fraction;
   bool enforce_minrange;
   bool enforce_maxrange;
}  rarch_setting_t;

void setting_data_reset_setting(rarch_setting_t* setting);
void setting_data_reset(rarch_setting_t* settings);

bool setting_data_load_config_path(rarch_setting_t* settings,
      const char* path);
bool setting_data_save_config(rarch_setting_t* settings,
      config_file_t* config);

rarch_setting_t* setting_data_find_setting(rarch_setting_t* settings,
      const char* name);

void setting_data_set_with_string_representation(
      rarch_setting_t* setting, const char* value);
void setting_data_get_string_representation(rarch_setting_t* setting,
      char* buf, size_t sizeof_buf);

/* List building helper functions. */
rarch_setting_t setting_data_group_setting(enum setting_type type,
      const char* name);

rarch_setting_t setting_data_subgroup_setting(enum setting_type type,
      const char* name, const char *parent_name);

rarch_setting_t setting_data_bool_setting(const char* name,
      const char* description, bool* target, bool default_value,
      const char *off, const char *on, const char * group,
      const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler);

rarch_setting_t setting_data_int_setting(const char* name,
      const char* description, int* target, int default_value,
      const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

rarch_setting_t setting_data_uint_setting(const char* name,
      const char* description, unsigned int* target,
      unsigned int default_value, const char *group,
      const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler);

rarch_setting_t setting_data_float_setting(const char* name,
      const char* description, float* target, float default_value,
      const char *rounding, const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

rarch_setting_t setting_data_string_setting(enum setting_type type,
      const char* name, const char* description, char* target,
      unsigned size, const char* default_value, const char *empty,
      const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

rarch_setting_t setting_data_bind_setting(const char* name,
      const char* description, struct retro_keybind* target, uint32_t index,
      const struct retro_keybind* default_value, const char *group,
      const char *subgroup);

int setting_data_get_description(const char *label, char *msg,
      size_t msg_sizeof);

#ifdef HAVE_MENU
rarch_setting_t* setting_data_get_mainmenu(bool regenerate);

void setting_data_get_label(char *type_str,
      size_t type_str_size, unsigned *w, unsigned type, 
      const char *menu_label, const char *label, unsigned index);
#endif
rarch_setting_t* setting_data_get_list(bool need_refresh);

#ifdef __cplusplus
}
#endif

#endif
