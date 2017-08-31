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

#include <boolean.h>

#include <file/file_path.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "input/input_driver.h"
#include "config.def.keybinds.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/widgets/menu_input_dialog.h"
#include "menu/widgets/menu_input_bind_dialog.h"
#endif

#include "configuration.h"
#include "config.def.h"
#include "setting_list.h"

rarch_setting_t setting_terminator_setting(void)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_NONE;
  
   result.size                      = 0;

   result.name                      = NULL;
   result.name_hash                 = 0;
   result.short_description         = NULL;
   result.group                     = NULL;
   result.subgroup                  = NULL;
   result.parent_group              = NULL;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = NULL;
   result.action_select             = NULL;
   result.get_string_representation = NULL;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = false;

   return result;
}

bool settings_list_append(rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   if (!list || !*list || !list_info)
      return false;

   if (list_info->index == list_info->size)
   {
      list_info->size *= 2;
      *list = (rarch_setting_t*)
         realloc(*list, sizeof(rarch_setting_t) * list_info->size);
      if (!*list)
         return false;
   }

   return true;
}

unsigned setting_get_bind_type(rarch_setting_t *setting)
{
   if (!setting)
      return 0;
   return setting->bind_type;
}

static int setting_bind_action_ok(void *data, bool wraparound)
{
   (void)wraparound; /* TODO/FIXME - handle this */

#ifdef HAVE_MENU
   /* TODO - get rid of menu dependency */
   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_SINGLE, data))
      return -1;
#endif
   return 0;
}

static int setting_int_action_right_default(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   double               max = 0.0f;
   
   if (!setting)
      return -1;

   max = setting->max;

   (void)wraparound; /* TODO/FIXME - handle this */

   *setting->value.target.integer =
      *setting->value.target.integer + setting->step;

   if (setting->enforce_maxrange)
   {
      if (*setting->value.target.integer > max)
      {
         settings_t *settings = config_get_ptr();
#ifdef HAVE_MENU
         double          min  = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.integer = min;
         else
#endif
            *setting->value.target.integer = max;
      }
   }

   return 0;
}

#ifdef HAVE_MENU
static int setting_bind_action_start(void *data)
{
   unsigned bind_type;
   struct retro_keybind *keybind   = NULL;
   rarch_setting_t *setting        = (rarch_setting_t*)data;
   struct retro_keybind *def_binds = (struct retro_keybind *)retro_keybinds_1;

   if (!setting)
      return -1;

   keybind = (struct retro_keybind*)setting->value.target.keybind;
   if (!keybind)
      return -1;

   keybind->joykey  = NO_BTN;
   keybind->joyaxis = AXIS_NONE;

   if (setting->index_offset)
      def_binds = (struct retro_keybind*)retro_keybinds_rest;

   bind_type    = setting_get_bind_type(setting);
   keybind->key = def_binds[bind_type - MENU_SETTINGS_BIND_BEGIN].key;

   return 0;
}
#endif

static void setting_get_string_representation_hex(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (setting)
      snprintf(s, len, "%08x",
            *setting->value.target.unsigned_integer);
}

static void setting_get_string_representation_uint(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (setting)
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
}

static int setting_uint_action_left_default(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   double               min = 0.0f;
   bool                 overflowed = false;
   
   if (!setting)
      return -1;

   min = setting->min;

   (void)wraparound; /* TODO/FIXME - handle this */


   overflowed = setting->step > *setting->value.target.unsigned_integer;

   if (!overflowed)
      *setting->value.target.unsigned_integer = *setting->value.target.unsigned_integer - setting->step;

   if (setting->enforce_minrange)
   {
      if (overflowed || *setting->value.target.unsigned_integer < min)
      {
         settings_t *settings = config_get_ptr();

#ifdef HAVE_MENU
      double           max = setting->max;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.unsigned_integer = max;
         else
#endif
            *setting->value.target.unsigned_integer = min;
      }
   }

   return 0;
}

static int setting_uint_action_right_default(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   double               max = 0.0f;
   
   if (!setting)
      return -1;

   max = setting->max;

   (void)wraparound; /* TODO/FIXME - handle this */


   *setting->value.target.unsigned_integer =
      *setting->value.target.unsigned_integer + setting->step;

   if (setting->enforce_maxrange)
   {
      if (*setting->value.target.unsigned_integer > max)
      {
         settings_t *settings = config_get_ptr();

#ifdef HAVE_MENU
         double           min = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.unsigned_integer = min;
         else
#endif
            *setting->value.target.unsigned_integer = max;
      }
   }

   return 0;
}

int setting_generic_action_ok_default(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   if (setting->cmd_trigger.idx != CMD_EVENT_NONE)
      setting->cmd_trigger.triggered = true;

   return 0;
}

static void setting_get_string_representation_int(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      snprintf(s, len, "%d", *setting->value.target.integer);
}

/**
 * setting_set_with_string_representation:
 * @setting            : pointer to setting
 * @value              : value for the setting (string)
 *
 * Set a settings' value with a string. It is assumed
 * that the string has been properly formatted.
 **/
int setting_set_with_string_representation(rarch_setting_t* setting,
      const char* value)
{
   double min, max;
   uint64_t flags;
   if (!setting || !value)
      return -1;

   min          = setting->min;
   max          = setting->max;
   flags        = setting->flags;

   switch (setting_get_type(setting))
   {
      case ST_INT:
         sscanf(value, "%d", setting->value.target.integer);
         if (flags & SD_FLAG_HAS_RANGE)
         {
            if (setting->enforce_minrange && *setting->value.target.integer < min)
               *setting->value.target.integer = min;
            if (setting->enforce_maxrange && *setting->value.target.integer > max)
            {
#ifdef HAVE_MENU
               settings_t *settings = config_get_ptr();
               if (settings && settings->bools.menu_navigation_wraparound_enable)
                  *setting->value.target.integer = min;
               else
#endif
                  *setting->value.target.integer = max;
            }
         }
         break;
      case ST_UINT:
         sscanf(value, "%u", setting->value.target.unsigned_integer);
         if (flags & SD_FLAG_HAS_RANGE)
         {
            if (setting->enforce_minrange && *setting->value.target.unsigned_integer < min)
               *setting->value.target.unsigned_integer = min;
            if (setting->enforce_maxrange && *setting->value.target.unsigned_integer > max)
            {
#ifdef HAVE_MENU
               settings_t *settings = config_get_ptr();
               if (settings && settings->bools.menu_navigation_wraparound_enable)
                  *setting->value.target.unsigned_integer = min;
               else
#endif
                  *setting->value.target.unsigned_integer = max;
            }
         }
         break;      
      case ST_FLOAT:
         sscanf(value, "%f", setting->value.target.fraction);
         if (flags & SD_FLAG_HAS_RANGE)
         {
            if (setting->enforce_minrange && *setting->value.target.fraction < min)
               *setting->value.target.fraction = min;
            if (setting->enforce_maxrange && *setting->value.target.fraction > max)
            {
#ifdef HAVE_MENU
               settings_t *settings = config_get_ptr();
               if (settings && settings->bools.menu_navigation_wraparound_enable)
                  *setting->value.target.fraction = min;
               else
#endif
                  *setting->value.target.fraction = max;
            }
         }
         break;
      case ST_PATH:
      case ST_DIR:
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_ACTION:
         strlcpy(setting->value.target.string, value, setting->size);
         break;
      case ST_BOOL:
         if (string_is_equal_fast(value, "true", 4))
            *setting->value.target.boolean = true;
         else if (string_is_equal_fast(value, "false", 5))
            *setting->value.target.boolean = false;
         break;
      default:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

static int setting_fraction_action_left_default(
      void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   
   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   *setting->value.target.fraction =
      *setting->value.target.fraction - setting->step;

   if (setting->enforce_minrange)
   {
      double min = setting->min;
      if (*setting->value.target.fraction < min)
         *setting->value.target.fraction = min;
   }

   return 0;
}

static int setting_fraction_action_right_default(
      void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   double               max = 0.0f;

   if (!setting)
      return -1;

   max = setting->max;

   (void)wraparound; /* TODO/FIXME - handle this */

   *setting->value.target.fraction = 
      *setting->value.target.fraction + setting->step;

   if (setting->enforce_maxrange)
   {
      if (*setting->value.target.fraction > max)
      {
#ifdef HAVE_MENU
         settings_t *settings = config_get_ptr();
         double          min  = setting->min;

         if (settings && settings->bools.menu_navigation_wraparound_enable)
            *setting->value.target.fraction = min;
         else
#endif
            *setting->value.target.fraction = max;
      }
   }

   return 0;
}

/**
 * setting_reset_setting:
 * @setting            : pointer to setting
 *
 * Reset a setting's value to its defaults.
 **/
static void setting_reset_setting(rarch_setting_t* setting)
{
   if (!setting)
      return;

   switch (setting_get_type(setting))
   {
      case ST_BOOL:
         *setting->value.target.boolean          = setting->default_value.boolean;
         break;
      case ST_INT:
         *setting->value.target.integer          = setting->default_value.integer;
         break;
      case ST_UINT:
         *setting->value.target.unsigned_integer = setting->default_value.unsigned_integer;
         break;
      case ST_FLOAT:
         *setting->value.target.fraction         = setting->default_value.fraction;
         break;
      case ST_BIND:
         *setting->value.target.keybind          = *setting->default_value.keybind;
         break;
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_PATH:
      case ST_DIR:
         if (setting->default_value.string)
         {
            if (setting_get_type(setting) == ST_STRING)
               setting_set_with_string_representation(setting, setting->default_value.string);
            else
               fill_pathname_expand_special(setting->value.target.string,
                     setting->default_value.string, setting->size);
         }
         break;
      default:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

int setting_generic_action_start_default(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   setting_reset_setting(setting);

   return 0;
}

static void setting_get_string_representation_default(void *data,
      char *s, size_t len)
{
   (void)data;
   strlcpy(s, "...", len);
}

/**
 * setting_get_string_representation_st_bool:
 * @setting            : pointer to setting
 * @s                  : string for the type to be represented on-screen as
 *                       a label.
 * @len                : size of @s
 *
 * Set a settings' label value. The setting is of type ST_BOOL.
 **/
static void setting_get_string_representation_st_bool(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(s, *setting->value.target.boolean ? setting->boolean.on_label :
            setting->boolean.off_label, len);
}


/**
 * setting_get_string_representation_st_float:
 * @setting            : pointer to setting
 * @s                  : string for the type to be represented on-screen as
 *                       a label.
 * @len                : size of @s
 *
 * Set a settings' label value. The setting is of type ST_FLOAT.
 **/
static void setting_get_string_representation_st_float(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      snprintf(s, len, setting->rounding_fraction,
            *setting->value.target.fraction);
}

static void setting_get_string_representation_st_dir(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(s,
            *setting->value.target.string ?
            setting->value.target.string : setting->dir.empty_path,
            len);
}

static void setting_get_string_representation_st_path(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      fill_short_pathname_representation(s, setting->value.target.string, len);
}

static void setting_get_string_representation_st_string(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(s, setting->value.target.string, len);
}

static void setting_get_string_representation_st_bind(void *data,
      char *s, size_t len)
{
   unsigned index_offset;
   rarch_setting_t *setting              = (rarch_setting_t*)data;
   const struct retro_keybind* keybind   = NULL;
   const struct retro_keybind* auto_bind = NULL;

   if (!setting)
      return;

   index_offset = setting->index_offset;
   keybind      = (const struct retro_keybind*)setting->value.target.keybind;
   auto_bind    = (const struct retro_keybind*)
      input_config_get_bind_auto(index_offset, keybind->id);

   input_config_get_bind_string(s, keybind, auto_bind, len);
}

static int setting_action_action_ok(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   if (setting->cmd_trigger.idx != CMD_EVENT_NONE)
      command_event(setting->cmd_trigger.idx, NULL);

   return 0;
}


/**
 * setting_action_setting:
 * @name               : Name of setting.
 * @short_description  : Short description of setting.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 *
 * Initializes a setting of type ST_ACTION.
 *
 * Returns: setting of type ST_ACTION.
 **/
static rarch_setting_t setting_action_setting(const char* name,
      const char* short_description,
      const char *group, const char *subgroup,
      const char *parent_group,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_ACTION;
  
   result.size                      = 0;

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_action_action_ok;
   result.action_select             = setting_action_action_ok;
   result.get_string_representation = &setting_get_string_representation_default;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_group_setting:
 * @type               : type of settting.
 * @name               : name of setting.
 *
 * Initializes a setting of type ST_GROUP.
 *
 * Returns: setting of type ST_GROUP.
 **/
static rarch_setting_t setting_group_setting(enum setting_type type, const char* name,
      const char *parent_group)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;
  
   result.size                      = 0;

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = name;
   result.group                     = NULL;
   result.subgroup                  = NULL;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = NULL;
   result.action_select             = NULL;
   result.get_string_representation = &setting_get_string_representation_default;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = false;

   return result;
}

/**
 * setting_float_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of float setting.
 * @default_value      : Default value (in float).
 * @rounding           : Rounding (for float-to-string representation).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_FLOAT.
 *
 * Returns: setting of type ST_FLOAT.
 **/
static rarch_setting_t setting_float_setting(const char* name,
      const char* short_description, float* target, float default_value,
      const char *rounding, const char *group, const char *subgroup,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_FLOAT;
  
   result.size                      = sizeof(float);

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_fraction_action_left_default;
   result.action_right              = setting_fraction_action_right_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_st_float;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = rounding;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.fraction     = target;
   result.original_value.fraction   = *target;
   result.default_value.fraction    = default_value;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_uint_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of unsigned integer setting.
 * @default_value      : Default value (in unsigned integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_UINT. 
 *
 * Returns: setting of type ST_UINT.
 **/
static rarch_setting_t setting_uint_setting(const char* name,
      const char* short_description, unsigned int* target,
      unsigned int default_value,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_UINT;
  
   result.size                      = sizeof(unsigned int);

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_uint_action_left_default;
   result.action_right              = setting_uint_action_right_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_uint;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.unsigned_integer   = target;
   result.original_value.unsigned_integer = *target;
   result.default_value.unsigned_integer  = default_value;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_hex_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of unsigned integer setting.
 * @default_value      : Default value (in unsigned integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_HEX.
 *
 * Returns: setting of type ST_HEX.
 **/
static rarch_setting_t setting_hex_setting(const char* name,
      const char* short_description, unsigned int* target,
      unsigned int default_value,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_HEX;
  
   result.size                      = sizeof(unsigned int);

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_hex;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.unsigned_integer   = target;
   result.original_value.unsigned_integer = *target;
   result.default_value.unsigned_integer  = default_value;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_bind_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bind setting.
 * @idx                : Index of bind setting.
 * @idx_offset         : Index offset of bind setting.
 * @default_value      : Default value (in bind format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 *
 * Initializes a setting of type ST_BIND. 
 *
 * Returns: setting of type ST_BIND.
 **/
static rarch_setting_t setting_bind_setting(const char* name,
      const char* short_description, struct retro_keybind* target,
      uint32_t idx, uint32_t idx_offset,
      const struct retro_keybind* default_value,
      const char *group, const char *subgroup,
      const char *parent_group,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_BIND;
  
   result.size                      = 0;

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = idx;
   result.index_offset              = idx_offset;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
#ifdef HAVE_MENU
   result.action_start              = setting_bind_action_start;
#else
   result.action_start              = NULL; 
#endif
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_bind_action_ok;
   result.action_select             = setting_bind_action_ok;
   result.get_string_representation = &setting_get_string_representation_st_bind;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.keybind      = target;
   result.default_value.keybind     = default_value;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

static int setting_int_action_left_default(void *data, bool wraparound)
{
   double min               = 0.0f;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   min               = setting->min;

   (void)wraparound; /* TODO/FIXME - handle this */

   if (*setting->value.target.integer != min)
      *setting->value.target.integer =
         *setting->value.target.integer - setting->step;

   if (setting->enforce_minrange)
   {
      if (*setting->value.target.integer < min)
         *setting->value.target.integer = min;
   }


   return 0;
}

static int setting_bool_action_ok_default(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   setting_set_with_string_representation(setting,
         *setting->value.target.boolean ? "false" : "true");

   return 0;
}

static int setting_bool_action_toggle_default(void *data, bool wraparound)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   (void)wraparound; /* TODO/FIXME - handle this */

   setting_set_with_string_representation(setting,
         *setting->value.target.boolean ? "false" : "true");

   return 0;
}

int setting_string_action_start_generic(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   setting->value.target.string[0] = '\0';

   return 0;
}

/**
 * setting_string_setting:
 * @type               : type of setting.
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of string setting.
 * @size               : Size of string setting.
 * @default_value      : Default value (in string format).
 * @empty              : TODO/FIXME: ???
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a string setting (of type @type). 
 *
 * Returns: String setting of type @type.
 **/
static rarch_setting_t setting_string_setting(enum setting_type type,
      const char* name, const char* short_description, char* target,
      unsigned size, const char* default_value, const char *empty,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler,
      change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;
  
   result.size                      = size;

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = NULL;
   result.action_select             = NULL;
   result.get_string_representation = &setting_get_string_representation_st_string;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.dir.empty_path            = empty;
   result.value.target.string       = target;
   result.default_value.string      = default_value;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   switch (type)
   {
      case ST_DIR:
         result.action_start              = setting_string_action_start_generic;
         result.browser_selection_type    = ST_DIR;
         result.get_string_representation = &setting_get_string_representation_st_dir;
         break;
      case ST_PATH:
         result.action_start              = setting_string_action_start_generic;
         result.browser_selection_type    = ST_PATH;
         result.get_string_representation = &setting_get_string_representation_st_path;
         break;
      default:
         break;
   }

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_string_setting_options:
 * @type               : type of settting.
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bind setting.
 * @size               : Size of string setting.
 * @default_value      : Default value.
 * @empty              : N/A.
 * @values             : Values, separated by a delimiter.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a string options list setting. 
 *
 * Returns: string option list setting.
 **/
static rarch_setting_t setting_string_setting_options(enum setting_type type,
      const char* name, const char* short_description, char* target,
      unsigned size, const char* default_value,
      const char *empty, const char *values,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
  rarch_setting_t result = setting_string_setting(type, name,
        short_description, target, size, default_value, empty, group,
        subgroup, parent_group, change_handler, read_handler,
        dont_use_enum_idx);

  result.parent_group    = parent_group;
  result.values          = values;
  return result;
}

/**
 * setting_subgroup_setting:
 * @type               : type of settting.
 * @name               : name of setting.
 * @parent_name        : group that the subgroup setting belongs to.
 *
 * Initializes a setting of type ST_SUBGROUP.
 *
 * Returns: setting of type ST_SUBGROUP.
 **/
static rarch_setting_t setting_subgroup_setting(enum setting_type type,
      const char* name, const char *parent_name, const char *parent_group,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = type;
  
   result.size                      = 0;

   result.name                      = name;
   result.name_hash                 = 0;
   result.short_description         = name;
   result.group                     = parent_name;
   result.parent_group              = parent_group;
   result.values                    = NULL;
   result.subgroup                  = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = NULL;
   result.read_handler              = NULL;
   result.action_start              = NULL;
   result.action_left               = NULL;
   result.action_right              = NULL;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = NULL;
   result.action_select             = NULL;
   result.get_string_representation = &setting_get_string_representation_default;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_bool_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bool setting.
 * @default_value      : Default value (in bool format).
 * @off                : String value for "Off" label.
 * @on                 : String value for "On"  label.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_BOOL.
 *
 * Returns: setting of type ST_BOOL.
 **/
static rarch_setting_t setting_bool_setting(const char* name,
      const char* short_description, bool* target, bool default_value,
      const char *off, const char *on,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_BOOL;
  
   result.size                      = sizeof(bool);

   result.name                      = name;
   result.name_hash                 = name ? msg_hash_calculate(name) : 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_bool_action_toggle_default;
   result.action_right              = setting_bool_action_toggle_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_bool_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_st_bool;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.boolean      = target;
   result.original_value.boolean    = *target;
   result.default_value.boolean     = default_value;
   result.boolean.off_label         = off;
   result.boolean.on_label          = on;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

/**
 * setting_int_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of signed integer setting.
 * @default_value      : Default value (in signed integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_INT. 
 *
 * Returns: setting of type ST_INT.
 **/
static rarch_setting_t setting_int_setting(const char* name,
      const char* short_description, int* target,
      int default_value,
      const char *group, const char *subgroup, const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler,
      bool dont_use_enum_idx)
{
   rarch_setting_t result;

   result.enum_idx                  = MSG_UNKNOWN;
   result.type                      = ST_INT;
  
   result.size                      = sizeof(int);

   result.name                      = name;
   result.name_hash                 = name ? msg_hash_calculate(name) : 0;
   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.parent_group              = parent_group;
   result.values                    = NULL;

   result.index                     = 0;
   result.index_offset              = 0;

   result.min                       = 0.0;
   result.max                       = 0.0;

   result.flags                     = 0;
   result.free_flags                = 0;

   result.change_handler            = change_handler;
   result.read_handler              = read_handler;
   result.action_start              = setting_generic_action_start_default;
   result.action_left               = setting_int_action_left_default;
   result.action_right              = setting_int_action_right_default;
   result.action_up                 = NULL;
   result.action_down               = NULL;
   result.action_cancel             = NULL;
   result.action_ok                 = setting_generic_action_ok_default;
   result.action_select             = setting_generic_action_ok_default;
   result.get_string_representation = &setting_get_string_representation_int;

   result.bind_type                 = 0;
   result.browser_selection_type    = ST_NONE;
   result.step                      = 0.0f;
   result.rounding_fraction         = NULL;
   result.enforce_minrange          = false;
   result.enforce_maxrange          = false;

   result.value.target.integer      = target;
   result.original_value.integer    = *target;
   result.default_value.integer     = default_value;

   result.cmd_trigger.idx           = CMD_EVENT_NONE;
   result.cmd_trigger.triggered     = false;

   result.dont_use_enum_idx_representation = dont_use_enum_idx;

   return result;
}

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
      uint32_t flags)
{
   rarch_setting_t value = setting_bool_setting(name, SHORT, target,
         default_value,
         msg_hash_to_str(off_enum_idx), msg_hash_to_str(on_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, true);

   if (!settings_list_append(list, list_info))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   if (flags != SD_FLAG_NONE)
      settings_data_list_current_add_flags(list, list_info, flags);
   return true;
}

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
      uint32_t flags)
{
   rarch_setting_t value = setting_bool_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target,
         default_value,
         msg_hash_to_str(off_enum_idx),
         msg_hash_to_str(on_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);

   if (!settings_list_append(list, list_info))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   if (flags != SD_FLAG_NONE)
      settings_data_list_current_add_flags(list, list_info, flags);

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif

   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_int_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif

   return true;
}

bool CONFIG_UINT_ALT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned int *target,
      const char *name, const char *SHORT,
      unsigned int default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group,
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_uint_setting(name, SHORT, target, default_value,
                  group_info->name,
                  subgroup_info->name, parent_group, change_handler, read_handler,
                  true);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_uint_setting  (
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name,
         subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif

   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_float_setting(msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx), target, default_value, rounding,
         group_info->name, subgroup_info->name, parent_group, change_handler, read_handler,
         false);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif

   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_string_setting(ST_PATH,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value, "",
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_EMPTY);
#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif
   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_string_setting(ST_DIR, 
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value,
         msg_hash_to_str(empty_enum_idx),
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler,
         false);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif
   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_string_setting(ST_STRING,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value, "",
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif
   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_string_setting_options(ST_STRING_OPTIONS,
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, (unsigned)len, default_value, "", values,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   if (!(settings_list_append(list, list_info)))
      return false;

   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif
   /* Request values to be freed later */
   settings_data_list_current_add_free_flags(list, list_info, SD_FREE_FLAG_VALUES);

   return true;
}

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
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t value = setting_hex_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         target, default_value,
         group_info->name, subgroup_info->name, parent_group,
         change_handler, read_handler, false);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif

   return true;
}

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
      const char *parent_group)
{
   rarch_setting_t value = setting_bind_setting(name, SHORT, target,
         player, player_offset, default_value,
         group_info->name, subgroup_info->name, parent_group,
         false);
   if (!(settings_list_append(list, list_info)))
      return false;

   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   /* Request name and short description to be freed later */
   settings_data_list_current_add_free_flags(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);

   return true;
}

bool CONFIG_BIND_ALT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      struct retro_keybind *target,
      uint32_t player, uint32_t player_offset,
      const char *name, const char *SHORT,
      const struct retro_keybind *default_value,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   rarch_setting_t value = setting_bind_setting(name, SHORT, target,
         player, player_offset, default_value,
         group_info->name, subgroup_info->name, parent_group,
         true);
   if (!(settings_list_append(list, list_info)))
      return false;

   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   /* Request name and short description to be freed later */
   settings_data_list_current_add_free_flags(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);

   return true;
}

bool CONFIG_ACTION_ALT(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *name, const char *SHORT,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   rarch_setting_t value = setting_action_setting(name, SHORT,
         group_info->name, subgroup_info->name, parent_group,
         true);

   if (!settings_list_append(list, list_info))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   return true;
}

bool CONFIG_ACTION(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum msg_hash_enums name_enum_idx,
      enum msg_hash_enums SHORT_enum_idx,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   rarch_setting_t value = setting_action_setting(
         msg_hash_to_str(name_enum_idx),
         msg_hash_to_str(SHORT_enum_idx),
         group_info->name,
         subgroup_info->name, parent_group,
         false);

   if (!settings_list_append(list, list_info))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;

#ifdef HAVE_MENU
   menu_settings_list_current_add_enum_idx(list, list_info, name_enum_idx);
   menu_settings_list_current_add_enum_value_idx(list, list_info, SHORT_enum_idx);
#endif

   return true;
}

bool START_GROUP(rarch_setting_t **list, rarch_setting_info_t *list_info,
      rarch_setting_group_info_t *group_info,
      const char *name, const char *parent_group)
{
   rarch_setting_t value = setting_group_setting (ST_GROUP, name, parent_group);
   group_info->name = name;
   if (!(settings_list_append(list, list_info)))
      return false;

   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   return true;
}

bool END_GROUP(rarch_setting_t **list, rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_t value = setting_group_setting (ST_END_GROUP, 0, parent_group);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   return true;
}

bool START_SUB_GROUP(rarch_setting_t **list,
      rarch_setting_info_t *list_info, const char *name,
      rarch_setting_group_info_t *group_info,
      rarch_setting_group_info_t *subgroup_info,
      const char *parent_group)
{
   rarch_setting_t value = setting_subgroup_setting (ST_SUB_GROUP,
         name, group_info->name, parent_group, false);

   subgroup_info->name = name;

   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   return true;
}

bool END_SUB_GROUP(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   rarch_setting_t value = setting_group_setting (ST_END_SUB_GROUP, 0, parent_group);
   if (!(settings_list_append(list, list_info)))
      return false;
   if (value.name)
      value.name_hash = msg_hash_calculate(value.name);
   (*list)[list_info->index++] = value;
   return true;
}

#ifdef HAVE_MENU
static void menu_input_st_string_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t *setting = NULL;
      const char        *label = menu_input_dialog_get_label_setting_buffer();

      if (!string_is_empty(label))
         setting = menu_setting_find(label);

      if (setting)
      {
         setting_set_with_string_representation(setting, str);
         menu_setting_generic(setting, false);
      }
   }

   menu_input_dialog_end();
}

static void menu_input_st_uint_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char        *label = menu_input_dialog_get_label_setting_buffer();
      rarch_setting_t *setting = menu_setting_find(label);

      setting_set_with_string_representation(setting, str);
   }

   menu_input_dialog_end();
}

static void menu_input_st_hex_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      const char        *label = menu_input_dialog_get_label_setting_buffer();
      rarch_setting_t *setting = menu_setting_find(label);

      if (setting)
      {
         unsigned *ptr = (unsigned*)setting_get_ptr(setting);
         if (str[0] == '#')
            str++;
         if (ptr)
            *ptr = (unsigned)strtoul(str, NULL, 16);
      }
   }

   menu_input_dialog_end();
}

static int setting_generic_action_ok_linefeed(void *data, bool wraparound)
{
   menu_input_ctx_line_t line;
   input_keyboard_line_complete_t cb = NULL;
   rarch_setting_t      *setting     = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   (void)wraparound;

   switch (setting_get_type(setting))
   {
      case ST_UINT:
         cb = menu_input_st_uint_cb;
         break;
      case ST_HEX:
         cb = menu_input_st_hex_cb;
         break;
      case ST_STRING:
      case ST_STRING_OPTIONS:
         cb = menu_input_st_string_cb;
         break;
      default:
         break;
   }

   line.label         = setting->short_description;
   line.label_setting = setting->name;
   line.type          = 0;
   line.idx           = 0;
   line.cb            = cb;

   if (!menu_input_dialog_start(&line))
      return -1;

   return 0;
}
#endif

static void setting_add_special_callbacks(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values)
{
   unsigned idx = list_info->index - 1;

   if (values & SD_FLAG_ALLOW_INPUT)
   {
#ifdef HAVE_MENU
      (*list)[idx].action_ok     = setting_generic_action_ok_linefeed;
      (*list)[idx].action_select = setting_generic_action_ok_linefeed;
#else
      (*list)[idx].action_ok     = NULL;
      (*list)[idx].action_select = NULL;
#endif

      switch ((*list)[idx].type)
      {
         case ST_UINT:
            (*list)[idx].action_cancel = NULL;
            break;
         case ST_HEX:
            (*list)[idx].action_cancel = NULL;
            break;
         case ST_STRING:
            (*list)[idx].action_start  = setting_string_action_start_generic;
            (*list)[idx].action_cancel = NULL;
            break;
         default:
            break;
      }
   }
}

void settings_data_list_current_add_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values)
{
   (*list)[list_info->index - 1].flags |= values;
   setting_add_special_callbacks(list, list_info, values);
}

void settings_data_list_current_add_free_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values)
{
   (*list)[list_info->index - 1].free_flags |= values;
}
