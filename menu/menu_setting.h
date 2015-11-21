/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef _MENU_SETTING_H
#define _MENU_SETTING_H

#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

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
   SD_FLAG_PATH_DIR       = (1 << 0),
   SD_FLAG_PATH_FILE      = (1 << 1),
   SD_FLAG_ALLOW_EMPTY    = (1 << 2),
   SD_FLAG_HAS_RANGE      = (1 << 3),
   SD_FLAG_ALLOW_INPUT    = (1 << 4),
   SD_FLAG_IS_DRIVER      = (1 << 5),
   SD_FLAG_EXIT           = (1 << 6),
   SD_FLAG_CMD_APPLY_AUTO = (1 << 7),
   SD_FLAG_BROWSER_ACTION = (1 << 8),
   SD_FLAG_ADVANCED       = (1 << 9)
};

enum setting_list_flags
{
   SL_FLAG_MAIN_MENU                                =  (1 << 0),
   SL_FLAG_SETTINGS                                 =  (1 << 1),
   SL_FLAG_SETTINGS_DRIVER_OPTIONS                  =  (1 << 2),
   SL_FLAG_SETTINGS_CORE_OPTIONS                    =  (1 << 3),
   SL_FLAG_SETTINGS_CONFIGURATION_OPTIONS           =  (1 << 4),
   SL_FLAG_SETTINGS_REWIND_OPTIONS                  =  (1 << 5),
   SL_FLAG_SETTINGS_VIDEO_OPTIONS                   =  (1 << 6),
   SL_FLAG_SETTINGS_SHADER_OPTIONS                  =  (1 << 7),
   SL_FLAG_SETTINGS_FONT_OPTIONS                    =  (1 << 8),
   SL_FLAG_SETTINGS_AUDIO_OPTIONS                   =  (1 << 9),
   SL_FLAG_SETTINGS_INPUT_OPTIONS                   =  (1 << 10),
   SL_FLAG_SETTINGS_INPUT_HOTKEY_OPTIONS            =  (1 << 11),
   SL_FLAG_SETTINGS_OVERLAY_OPTIONS                 =  (1 << 12),
   SL_FLAG_SETTINGS_MENU_OPTIONS                    =  (1 << 13),
   SL_FLAG_SETTINGS_MULTIMEDIA_OPTIONS              =  (1 << 14),
   SL_FLAG_SETTINGS_UI_OPTIONS                      =  (1 << 15),
   SL_FLAG_SETTINGS_CHEEVOS_OPTIONS                 =  (1 << 16),
   SL_FLAG_SETTINGS_CORE_UPDATER_OPTIONS            =  (1 << 17),
   SL_FLAG_SETTINGS_NETPLAY_OPTIONS                 =  (1 << 18),
   SL_FLAG_SETTINGS_USER_OPTIONS                    =  (1 << 19),
   SL_FLAG_SETTINGS_DIRECTORY_OPTIONS               =  (1 << 20),
   SL_FLAG_SETTINGS_PRIVACY_OPTIONS                 =  (1 << 21),
   SL_FLAG_SETTINGS_PLAYLIST_OPTIONS                =  (1 << 22),
   SL_FLAG_SETTINGS_MENU_BROWSER_OPTIONS            =  (1 << 23),
   SL_FLAG_SETTINGS_PATCH_OPTIONS                   =  (1 << 24),
   SL_FLAG_SETTINGS_RECORDING_OPTIONS               =  (1 << 25),
   SL_FLAG_SETTINGS_FRAME_THROTTLE_OPTIONS          =  (1 << 26),
   SL_FLAG_SETTINGS_LOGGING_OPTIONS                 =  (1 << 27),
   SL_FLAG_SETTINGS_SAVING_OPTIONS                  =  (1 << 28),
   SL_FLAG_SETTINGS_SUB_ACCOUNTS_OPTIONS            =  (1 << 29),
   SL_FLAG_SETTINGS_ALL                             =  (1 << 30),
   SL_FLAG_ALLOW_EMPTY_LIST                         =  (1 << 31)
};

typedef struct rarch_setting rarch_setting_t;

#define SL_FLAG_SETTINGS_GROUP_ALL (SL_FLAG_SETTINGS_ALL - SL_FLAG_MAIN_MENU)

#define START_GROUP(group_info, NAME, parent_group) \
do{ \
   group_info.name = NAME; \
   if (!(menu_settings_list_append(list, list_info, setting_group_setting (ST_GROUP, NAME, parent_group)))) return false; \
}while(0)

#define END_GROUP(list, list_info, parent_group) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_group_setting (ST_END_GROUP, 0, parent_group)))) return false; \
}while(0)

#define START_SUB_GROUP(list, list_info, NAME, group_info, subgroup_info, parent_group) \
do{ \
   subgroup_info.name = NAME; \
   if (!(menu_settings_list_append(list, list_info, setting_subgroup_setting (ST_SUB_GROUP, NAME, group_info, parent_group)))) return false; \
}while(0)

#define END_SUB_GROUP(list, list_info, parent_group) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_group_setting (ST_END_SUB_GROUP, 0, parent_group)))) return false; \
}while(0)

#define CONFIG_ACTION(NAME, SHORT, group_info, subgroup_info, parent_group) \
do{ \
   if (!menu_settings_list_append(list, list_info, setting_action_setting  (NAME, SHORT, group_info, subgroup_info, parent_group))) return false; \
}while(0)

#define CONFIG_BOOL(TARGET, NAME, SHORT, DEF, OFF, ON, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!menu_settings_list_append(list, list_info, setting_bool_setting  (NAME, SHORT, TARGET, DEF, OFF, ON, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))return false; \
}while(0)

#define CONFIG_INT(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_int_setting   (NAME, SHORT, &TARGET, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

#define CONFIG_UINT(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_uint_setting  (NAME, SHORT, &TARGET, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

#define CONFIG_FLOAT(TARGET, NAME, SHORT, DEF, ROUNDING, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_float_setting (NAME, SHORT, &TARGET, DEF, ROUNDING, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

#define CONFIG_PATH(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_string_setting(ST_PATH, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

#define CONFIG_DIR(TARGET, NAME, SHORT, DEF, EMPTY, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_string_setting(ST_DIR, NAME, SHORT, TARGET, sizeof(TARGET), DEF, EMPTY, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

#define CONFIG_STRING(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_string_setting(ST_STRING, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

#define CONFIG_STRING_OPTIONS(TARGET, NAME, SHORT, DEF, OPTS, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
  if (!(menu_settings_list_append(list, list_info, setting_string_setting_options(ST_STRING_OPTIONS, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", OPTS, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

#define CONFIG_HEX(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_hex_setting(NAME, SHORT, &TARGET, DEF, group_info, subgroup_info, parent_group, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}while(0)

/* Please strdup() NAME and SHORT */
#define CONFIG_BIND(TARGET, PLAYER, PLAYER_OFFSET, NAME, SHORT, DEF, group_info, subgroup_info, parent_group) \
do{ \
   if (!(menu_settings_list_append(list, list_info, setting_bind_setting  (NAME, SHORT, &TARGET, PLAYER, PLAYER_OFFSET, DEF, group_info, subgroup_info, parent_group)))) return false; \
}while(0)

int menu_setting_generic(rarch_setting_t *setting, bool wraparound);

int menu_setting_set_flags(rarch_setting_t *setting);

int menu_setting_set(unsigned type, const char *label,
      unsigned action, bool wraparound);

/**
 * menu_setting_find:
 * @name               : name of setting to search for
 *
 * Search for a setting with a specified name (@name).
 *
 * Returns: pointer to setting if found, NULL otherwise.
 **/
rarch_setting_t *menu_setting_find(const char *label);

/**
 * setting_set_with_string_representation:
 * @setting            : pointer to setting
 * @value              : value for the setting (string)
 *
 * Set a settings' value with a string. It is assumed
 * that the string has been properly formatted.
 **/
int menu_setting_set_with_string_representation(
      rarch_setting_t* setting, const char *value);

/**
 * setting_get_string_representation:
 * @setting            : pointer to setting
 * @s                  : buffer to write contents of string representation to.
 * @len                : size of the buffer (@s)
 *
 * Get a setting value's string representation.
 **/
void menu_setting_get_string_representation(void *data, char *s, size_t len);

/**
 * menu_setting_get_label:
 * @list               : File list on which to perform the search
 * @s                  : String for the type to be represented on-screen as
 *                       a label.
 * @len                : Size of @s.
 * @w                  : Width of the string (for text label representation
 *                       purposes in the menu display driver).
 * @type               : Identifier of setting.
 * @menu_label         : Menu Label identifier of setting.
 * @label              : Label identifier of setting.
 * @idx                : Index identifier of setting.
 *
 * Get associated label of a setting.
 **/
void menu_setting_get_label(void *data, char *s,
      size_t len, unsigned *w, unsigned type, 
      const char *menu_label, const char *label, unsigned idx);

void menu_setting_free(rarch_setting_t *list);

/**
 * setting_new:
 * @mask               : Bitmask of settings to include.
 *
 * Request a list of settings based on @mask.
 *
 * Returns: settings list composed of all requested
 * settings on success, otherwise NULL.
 **/
rarch_setting_t* menu_setting_new(void);

bool menu_setting_is_of_path_type(rarch_setting_t *setting);

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action, bool wraparound);

enum setting_type menu_setting_get_type(rarch_setting_t *setting);

enum setting_type menu_setting_get_browser_selection_type(rarch_setting_t *setting);

const char *menu_setting_get_values(rarch_setting_t *setting);

const char *menu_setting_get_name(rarch_setting_t *setting);

const char *menu_setting_get_short_description(rarch_setting_t *setting);

uint64_t menu_setting_get_flags(rarch_setting_t *setting);

const char *menu_setting_get_parent_group(rarch_setting_t *setting);

double menu_setting_get_min(rarch_setting_t *setting);

double menu_setting_get_max(rarch_setting_t *setting);

unsigned menu_setting_get_bind_type(rarch_setting_t *setting);

uint32_t menu_setting_get_index(rarch_setting_t *setting);

unsigned menu_setting_get_index_offset(rarch_setting_t *setting);

void *setting_get_ptr(rarch_setting_t *setting);

bool menu_setting_action_right(rarch_setting_t *setting, bool wraparound);

void menu_settings_list_increment(rarch_setting_t **list);

#ifdef __cplusplus
}
#endif

#endif
