/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __SETTING_LIST_H
#define __SETTING_LIST_H

#include <boolean.h>

#include <retro_common_api.h>

#include "command.h"
#include "msg_hash.h"

RETRO_BEGIN_DECLS

enum setting_type
{
   ST_NONE = 0,
   ST_ACTION,
   ST_BOOL,
   ST_INT,
   ST_UINT,
   ST_FLOAT,
   ST_PATH,
   ST_DIR,
   ST_STRING,
   ST_STRING_OPTIONS,
   ST_HEX,
   ST_BIND,
   ST_GROUP,
   ST_SUB_GROUP,
   ST_END_GROUP,
   ST_END_SUB_GROUP
};

enum setting_flags
{
   SD_FLAG_NONE           = 0,
   SD_FLAG_PATH_DIR       = (1 << 0),
   SD_FLAG_PATH_FILE      = (1 << 1),
   SD_FLAG_ALLOW_EMPTY    = (1 << 2),
   SD_FLAG_HAS_RANGE      = (1 << 3),
   SD_FLAG_ALLOW_INPUT    = (1 << 4),
   SD_FLAG_IS_DRIVER      = (1 << 5),
   SD_FLAG_EXIT           = (1 << 6),
   SD_FLAG_CMD_APPLY_AUTO = (1 << 7),
   SD_FLAG_BROWSER_ACTION = (1 << 8),
   SD_FLAG_ADVANCED       = (1 << 9),
   SD_FLAG_LAKKA_ADVANCED = (1 << 10)
};

enum settings_free_flags
{
   SD_FREE_FLAG_VALUES    = (1 << 0),
   SD_FREE_FLAG_NAME      = (1 << 1),
   SD_FREE_FLAG_SHORT     = (1 << 2)
};

typedef struct rarch_setting rarch_setting_t;
typedef struct rarch_setting_info rarch_setting_info_t;
typedef struct rarch_setting_group_info rarch_setting_group_info_t;

typedef void (*change_handler_t               )(void *data);
typedef int  (*action_left_handler_t          )(void *data, bool wraparound);
typedef int  (*action_right_handler_t         )(void *data, bool wraparound);
typedef int  (*action_up_handler_t            )(void *data);
typedef int  (*action_down_handler_t          )(void *data);
typedef int  (*action_start_handler_t         )(void *data);
typedef int  (*action_cancel_handler_t        )(void *data);
typedef int  (*action_ok_handler_t            )(void *data, bool wraparound);
typedef int  (*action_select_handler_t        )(void *data, bool wraparound);
typedef void (*get_string_representation_t    )(void *data, char *s, size_t len);

struct rarch_setting_group_info
{
   const char *name;
};

struct rarch_setting
{
   bool                 dont_use_enum_idx_representation;
   bool                 enforce_minrange;
   bool                 enforce_maxrange;
   
   const char           *rounding_fraction;
   const char           *name;
   const char           *short_description;
   const char           *group;
   const char           *subgroup;
   const char           *parent_group;
   const char           *values;

   uint8_t              index;
   uint8_t              index_offset;

   unsigned             bind_type;
   uint32_t             size;
   uint32_t             name_hash;

   float                step;

   double               min;
   double               max;
   
   uint64_t             flags;
   uint64_t             free_flags;
   
   change_handler_t              change_handler;
   change_handler_t              read_handler;
   action_start_handler_t        action_start;
   action_left_handler_t         action_left;
   action_right_handler_t        action_right;
   action_up_handler_t           action_up;
   action_down_handler_t         action_down;
   action_cancel_handler_t       action_cancel;
   action_ok_handler_t           action_ok;
   action_select_handler_t       action_select;
   get_string_representation_t   get_string_representation;

   union
   {
      bool                       boolean;
      const char                 *string;
      int                        integer;
      unsigned int               unsigned_integer;
      float                      fraction;
      const struct retro_keybind *keybind;
   } default_value;
   
   struct
   {
      union
      {
         bool                 *boolean;
         char                 *string;
         int                  *integer;
         unsigned int         *unsigned_integer;
         float                *fraction;
         struct retro_keybind *keybind;
      } target;
   } value;

   union
   {
      bool           boolean;
      int            integer;
      unsigned int   unsigned_integer;
      float          fraction;
   } original_value;

   struct
   {
      const char     *empty_path;
   } dir;

   struct
   {
      enum           event_command idx;
      bool           triggered;
   } cmd_trigger;

   struct
   {
      const char     *off_label;
      const char     *on_label;
   } boolean;

   enum setting_type    browser_selection_type;
   enum msg_hash_enums  enum_idx;
   enum msg_hash_enums  enum_value_idx;
   enum setting_type    type;

};

struct rarch_setting_info
{
   int index;
   int size;
};

bool START_GROUP(rarch_setting_t **list, rarch_setting_info_t *list_info,
      rarch_setting_group_info_t *group_info,
      const char *name, const char *parent_group);

bool END_GROUP(rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group);

bool START_SUB_GROUP(rarch_setting_t **list,
      rarch_setting_info_t *list_info, const char *name,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group);

bool END_SUB_GROUP(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group);

bool CONFIG_ACTION_ALT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *name, const char *SHORT,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group);

bool CONFIG_ACTION(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group);

bool CONFIG_BOOL_ALT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      bool *target,
      const char *name, const char *SHORT,
      bool default_value,
      enum msg_hash_enums off_enum_idx,
      enum msg_hash_enums on_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler,
      change_handler_t read_handler,
      uint32_t flags);

bool CONFIG_BOOL(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      bool *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      bool default_value,
      enum msg_hash_enums off_enum_idx,
      enum msg_hash_enums on_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler,
      change_handler_t read_handler,
      uint32_t flags);

bool CONFIG_INT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      int *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_UINT_ALT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned int *target,
      const char *name, const char *SHORT,
      unsigned int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_UINT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned int *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      unsigned int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_FLOAT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      float *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      float default_value, const char *rounding,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_PATH(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_DIR(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      enum msg_hash_enums empty_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_STRING(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_STRING_OPTIONS(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      char *target, size_t len,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      const char *default_value, const char *values,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

bool CONFIG_HEX(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned int *target,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      unsigned int default_value, 
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler);

/* Please strdup() NAME and SHORT */
bool CONFIG_BIND(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      struct retro_keybind *target,
      uint32_t player, uint32_t player_offset,
      const char *name, const char *SHORT,
      const struct retro_keybind *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group);

bool CONFIG_BIND_ALT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      struct retro_keybind *target,
      uint32_t player, uint32_t player_offset,
      const char *name, const char *SHORT,
      const struct retro_keybind *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group);

/**
 * setting_set_with_string_representation:
 * @setting            : pointer to setting
 * @value              : value for the setting (string)
 *
 * Set a settings' value with a string. It is assumed
 * that the string has been properly formatted.
 **/
int setting_set_with_string_representation(
      rarch_setting_t* setting, const char *value);

unsigned setting_get_bind_type(rarch_setting_t *setting);

int setting_string_action_start_generic(void *data);

int setting_generic_action_ok_default(void *data, bool wraparound);

int setting_generic_action_start_default(void *data);

void settings_data_list_current_add_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values);

void settings_data_list_current_add_free_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values);

#define setting_get_type(setting) ((setting) ? setting->type : ST_NONE)

RETRO_END_DECLS

#endif
