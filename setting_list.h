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
   ST_SIZE,
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

enum ui_setting_type
{
   ST_UI_TYPE_NONE = 0,
   ST_UI_TYPE_CHECKBOX,
   ST_UI_TYPE_UINT_COLOR_BUTTON,
   ST_UI_TYPE_UINT_SPINBOX,
   ST_UI_TYPE_UINT_COMBOBOX,
   ST_UI_TYPE_UINT_RADIO_BUTTONS,
   ST_UI_TYPE_FLOAT_COLOR_BUTTON,
   ST_UI_TYPE_FLOAT_SPINBOX,
   ST_UI_TYPE_FLOAT_SLIDER_AND_SPINBOX,
   ST_UI_TYPE_SIZE_SPINBOX,
   ST_UI_TYPE_BIND_BUTTON,
   ST_UI_TYPE_DIRECTORY_SELECTOR,
   ST_UI_TYPE_FILE_SELECTOR,
   ST_UI_TYPE_FONT_SELECTOR,
   ST_UI_TYPE_STRING_COMBOBOX,
   ST_UI_TYPE_STRING_LINE_EDIT,
   ST_UI_TYPE_PASSWORD_LINE_EDIT,
   ST_UI_TYPE_LAST
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

typedef void (*change_handler_t               )(rarch_setting_t *data);
typedef int  (*action_left_handler_t          )(rarch_setting_t *data, bool wraparound);
typedef int  (*action_right_handler_t         )(rarch_setting_t *setting, bool wraparound);
typedef int  (*action_up_handler_t            )(rarch_setting_t *setting);
typedef int  (*action_down_handler_t          )(rarch_setting_t *setting);
typedef int  (*action_start_handler_t         )(rarch_setting_t *setting);
typedef int  (*action_cancel_handler_t        )(rarch_setting_t *setting);
typedef int  (*action_ok_handler_t            )(rarch_setting_t *setting, bool wraparound);
typedef int  (*action_select_handler_t        )(rarch_setting_t *setting, bool wraparound);
typedef void (*get_string_representation_t    )(rarch_setting_t *setting, char *s, size_t len);

struct rarch_setting_group_info
{
   const char *name;
};

struct rarch_setting
{
   enum ui_setting_type ui_type;
   enum setting_type    browser_selection_type;
   enum msg_hash_enums  enum_idx;
   enum msg_hash_enums  enum_value_idx;
   enum setting_type    type;

   bool                 dont_use_enum_idx_representation;
   bool                 enforce_minrange;
   bool                 enforce_maxrange;

   uint8_t              index;
   uint32_t             index_offset;
   int16_t               offset_by;

   unsigned             bind_type;
   uint32_t             size;

   float                step;

   uint64_t             flags;
   uint64_t             free_flags;

   double               min;
   double               max;

   const char           *rounding_fraction;
   const char           *name;
   const char           *short_description;
   const char           *group;
   const char           *subgroup;
   const char           *parent_group;
   const char           *values;

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
      size_t                     sizet;
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
         size_t               *sizet;
      } target;
   } value;

   union
   {
      bool           boolean;
      int            integer;
      unsigned int   unsigned_integer;
      float          fraction;
      size_t         sizet;
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
};

struct rarch_setting_info
{
   int index;
   int size;
};

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

int setting_string_action_start_generic(rarch_setting_t *setting);

int setting_generic_action_ok_default(rarch_setting_t *setting, bool wraparound);

int setting_generic_action_start_default(rarch_setting_t *setting);

void settings_data_list_current_add_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values);

void settings_data_list_current_add_free_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values);

void setting_get_string_representation_size_in_mb(rarch_setting_t *setting,
      char *s, size_t len);

int setting_uint_action_right_with_refresh(rarch_setting_t *setting, bool wraparound);

int setting_uint_action_left_with_refresh(rarch_setting_t *setting, bool wraparound) ;

int setting_uint_action_left_default(rarch_setting_t *setting, bool wraparound);
int setting_uint_action_right_default(rarch_setting_t *setting, bool wraparound);
void setting_get_string_representation_uint(rarch_setting_t *setting, char *s, size_t len);
void setting_get_string_representation_hex_and_uint(rarch_setting_t *setting, char *s, size_t len);
#define setting_get_type(setting) ((setting) ? setting->type : ST_NONE)

RETRO_END_DECLS

#endif
