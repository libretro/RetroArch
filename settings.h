/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <file/file_list.h>
#include <file/config_file.h>
#include <retro_miscellaneous.h>
#include "settings_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BINDFOR(s) (*(&(s))->value.keybind)

/**
 * setting_reset_setting:
 * @setting            : pointer to setting
 *
 * Reset a setting's value to its defaults.
 **/
void setting_reset_setting(rarch_setting_t* setting);

/**
 * setting_reset:
 * @settings           : pointer to settings
 *
 * Reset all settings to their default values.
 **/
void setting_reset(rarch_setting_t* settings);

/**
 * setting_reset:
 * @settings           : pointer to settings
 * @name               : name of setting to search for
 *
 * Search for a setting with a specified name (@name).
 *
 * Returns: pointer to setting if found, NULL otherwise.
 **/
rarch_setting_t* setting_find_setting(rarch_setting_t* settings,
      const char* name);

/**
 * setting_set_with_string_representation:
 * @setting            : pointer to setting
 * @value              : value for the setting (string)
 *
 * Set a settings' value with a string. It is assumed
 * that the string has been properly formatted.
 **/
void setting_set_with_string_representation(
      rarch_setting_t* setting, const char* value);

/**
 * setting_get_string_representation:
 * @setting            : pointer to setting
 * @buf                : buffer to write contents of string representation to.
 * @sizeof_buf         : size of the buffer (@buf)
 *
 * Get a setting value's string representation.
 **/
void setting_get_string_representation(void *data,
      char* buf, size_t sizeof_buf);

/**
 * setting_action_setting:
 * @name               : name of setting.
 * @short_description  : short description of setting.
 * @group              : group that the setting belongs to.
 * @subgroup           : subgroup that the setting belongs to.
 *
 * Initializes a setting of type ST_ACTION.
 *
 * Returns: setting of type ST_ACTION.
 **/
rarch_setting_t setting_action_setting(const char *name,
      const char *short_description,
      const char *group,
      const char *subgroup);

/**
 * setting_group_setting:
 * @type               : type of settting.
 * @name               : name of setting.
 *
 * Initializes a setting of type ST_GROUP.
 *
 * Returns: setting of type ST_GROUP.
 **/
rarch_setting_t setting_group_setting(enum setting_type type,
      const char* name);

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
rarch_setting_t setting_subgroup_setting(enum setting_type type,
      const char* name, const char *parent_name);

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
rarch_setting_t setting_bool_setting(const char* name,
      const char* description, bool* target, bool default_value,
      const char *off, const char *on, const char * group,
      const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler);

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
rarch_setting_t setting_int_setting(const char* name,
      const char* description, int* target, int default_value,
      const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

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
rarch_setting_t setting_uint_setting(const char* name,
      const char* description, unsigned int* target,
      unsigned int default_value, const char *group,
      const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler);

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
rarch_setting_t setting_hex_setting(const char* name,
      const char* short_description, unsigned int* target,
      unsigned int default_value, const char *group,
      const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler);

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
rarch_setting_t setting_float_setting(const char* name,
      const char* description, float* target, float default_value,
      const char *rounding, const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

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
rarch_setting_t setting_string_setting(enum setting_type type,
      const char* name, const char* description, char* target,
      unsigned size, const char* default_value, const char *empty,
      const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

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
rarch_setting_t setting_bind_setting(const char* name,
      const char* description, struct retro_keybind* target, uint32_t idx,
      uint32_t idx_offset,
      const struct retro_keybind* default_value, const char *group,
      const char *subgroup);
    
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
rarch_setting_t setting_string_setting_options(enum setting_type type,
     const char* name, const char* short_description, char* target,
     unsigned size, const char* default_value, const char *empty, const char *values,
     const char *group, const char *subgroup, change_handler_t change_handler,
     change_handler_t read_handler);

/**
 * setting_get_description:
 * @label              : identifier label of setting
 * @msg                : output message 
 * @sizeof_msg         : size of @msg
 *
 * Writes a 'Help' description message to @msg if there is
 * one available based on the identifier label of the setting
 * (@label).
 *
 * Returns: 0 (always for now). TODO: make it handle -1 as well.
 **/
int setting_get_description(const char *label, char *msg,
      size_t msg_sizeof);

#ifdef HAVE_MENU
/**
 * setting_get_label:
 * @list               : File list on which to perform the search
 * @type_str           : String for the type to be represented on-screen as
 *                       a label.
 * @type_str_size      : Size of @type_str
 * @w                  : Width of the string (for text label representation
 *                       purposes in the menu display driver).
 * @type               : Identifier of setting.
 * @menu_label         : Menu Label identifier of setting.
 * @label              : Label identifier of setting.
 * @idx                : Index identifier of setting.
 *
 * Get associated label of a setting.
 **/
void setting_get_label(file_list_t *list, char *type_str,
      size_t type_str_size, unsigned *w, unsigned type, 
      const char *menu_label, const char *label, unsigned idx);
#endif

/**
 * setting_new:
 * @mask               : Bitmask of settings to include.
 *
 * Request a list of settings based on @mask.
 *
 * Returns: settings list composed of all requested
 * settings on success, otherwise NULL.
 **/
rarch_setting_t* setting_new(unsigned mask);

bool setting_is_of_path_type(rarch_setting_t *setting);

#ifdef __cplusplus
}
#endif

#endif
