/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef _SETTINGS_LIST_H
#define _SETTINGS_LIST_H

#include <stdint.h>
#include <stdlib.h>
#include "boolean.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef void (*change_handler_t)(void *data);

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


bool settings_list_append(rarch_setting_t **list,
      rarch_setting_info_t *list_info, rarch_setting_t value);

void settings_list_current_add_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values);

void settings_list_current_add_range(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      float min, float max, float step,
      bool enforce_minrange_enable, bool enforce_maxrange_enable);

void settings_list_current_add_values(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *values);

void settings_list_current_add_cmd(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values);

void settings_info_list_free(rarch_setting_info_t *list_info);

void settings_list_free(rarch_setting_t *list);

rarch_setting_info_t *settings_info_list_new(void);

rarch_setting_t *settings_list_new(unsigned size);

#ifdef __cplusplus
}
#endif

#endif
