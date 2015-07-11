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

#include <file/file_list.h>

#include "../command_event.h"

#define BINDFOR(s) (*(&(s))->value.keybind)

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
   SD_FLAG_IS_DRIVER      = (1 << 6),
   SD_FLAG_EXIT           = (1 << 7),
   SD_FLAG_CMD_APPLY_AUTO = (1 << 8),
   SD_FLAG_IS_DEFERRED    = (1 << 9),
   SD_FLAG_BROWSER_ACTION = (1 << 10),
   SD_FLAG_ADVANCED       = (1 << 11)
};

enum setting_list_flags
{
   SL_FLAG_MAIN_MENU             =  (1 << 0),
   SL_FLAG_MAIN_MENU_SETTINGS    =  (1 << 1),
   SL_FLAG_DRIVER_OPTIONS        =  (1 << 2),
   SL_FLAG_CORE_OPTIONS          =  (1 << 3),
   SL_FLAG_CONFIGURATION_OPTIONS =  (1 << 4),
   SL_FLAG_REWIND_OPTIONS        =  (1 << 5),
   SL_FLAG_VIDEO_OPTIONS         =  (1 << 6),
   SL_FLAG_SHADER_OPTIONS        =  (1 << 7),
   SL_FLAG_FONT_OPTIONS          =  (1 << 8),
   SL_FLAG_AUDIO_OPTIONS         =  (1 << 9),
   SL_FLAG_INPUT_OPTIONS         =  (1 << 10),
   SL_FLAG_INPUT_HOTKEY_OPTIONS  =  (1 << 11),
   SL_FLAG_OVERLAY_OPTIONS       =  (1 << 12),
   SL_FLAG_MENU_OPTIONS          =  (1 << 13),
   SL_FLAG_MULTIMEDIA_OPTIONS    =  (1 << 14),
   SL_FLAG_UI_OPTIONS            =  (1 << 15),
   SL_FLAG_CORE_UPDATER_OPTIONS  =  (1 << 16),
   SL_FLAG_NETPLAY_OPTIONS       =  (1 << 17),
   SL_FLAG_USER_OPTIONS          =  (1 << 18),
   SL_FLAG_DIRECTORY_OPTIONS     =  (1 << 19),
   SL_FLAG_PRIVACY_OPTIONS       =  (1 << 20),
   SL_FLAG_PLAYLIST_OPTIONS      =  (1 << 21),
   SL_FLAG_MENU_BROWSER_OPTIONS  =  (1 << 22),
   SL_FLAG_PATCH_OPTIONS         =  (1 << 23),
   SL_FLAG_RECORDING_OPTIONS     =  (1 << 24),
   SL_FLAG_FRAME_THROTTLE_OPTIONS=  (1 << 25),
   SL_FLAG_LOGGING_OPTIONS       =  (1 << 26),
   SL_FLAG_SAVING_OPTIONS        =  (1 << 27),
   SL_FLAG_ALL                   =  (1 << 28),
   SL_FLAG_ALLOW_EMPTY_LIST      =  (1 << 29)
};

#define SL_FLAG_ALL_SETTINGS (SL_FLAG_ALL - SL_FLAG_MAIN_MENU)

typedef void (*change_handler_t               )(void *data);
typedef int  (*action_left_handler_t          )(void *data, bool wraparound);
typedef int  (*action_right_handler_t         )(void *data, bool wraparound);
typedef int  (*action_up_handler_t            )(void *data);
typedef int  (*action_down_handler_t          )(void *data);
typedef int  (*action_start_handler_t         )(void *data);
typedef int  (*action_iterate_handler_t       )(unsigned action);
typedef int  (*action_cancel_handler_t        )(void *data);
typedef int  (*action_ok_handler_t            )(void *data, bool wraparound);
typedef int  (*action_select_handler_t        )(void *data, bool wraparound);
typedef void (*get_string_representation_t    )(void *data, char *s, size_t len);

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
   enum setting_type    type;

   uint32_t             size;
   
   const char           *name;
   uint32_t             name_hash;
   const char           *short_description;
   const char           *group;
   const char           *subgroup;
   const char           *parent_group;
   const char           *values;

   uint32_t             index;
   uint32_t             index_offset;

   double               min;
   double               max;
   
   uint64_t             flags;
   
   change_handler_t              change_handler;
   change_handler_t              deferred_handler;
   change_handler_t              read_handler;
   action_start_handler_t        action_start;
   action_iterate_handler_t      action_iterate;
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
      int                        integer;
      unsigned int               unsigned_integer;
      float                      fraction;
      const char                 *string;
      const struct retro_keybind *keybind;
   } default_value;
   
   union
   {
      bool                 *boolean;
      int                  *integer;
      unsigned int         *unsigned_integer;
      float                *fraction;
      char                 *string;
      struct retro_keybind *keybind;
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

   unsigned          bind_type;
   unsigned          browser_selection_type;
   float             step;
   const char        *rounding_fraction;
   bool              enforce_minrange;
   bool              enforce_maxrange;
}  rarch_setting_t;



void menu_setting_apply_deferred(void);

int menu_setting_set_flags(rarch_setting_t *setting);

int menu_setting_generic(rarch_setting_t *setting, bool wraparound);

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
int setting_set_with_string_representation(
      rarch_setting_t* setting, const char *value);

/**
 * setting_get_string_representation:
 * @setting            : pointer to setting
 * @s                  : buffer to write contents of string representation to.
 * @len                : size of the buffer (@s)
 *
 * Get a setting value's string representation.
 **/
void setting_get_string_representation(void *data, char *s, size_t len);

/**
 * setting_get_description:
 * @label              : identifier label of setting
 * @s                  : output message 
 * @len                : size of @s
 *
 * Writes a 'Help' description message to @s if there is
 * one available based on the identifier label of the setting
 * (@label).
 *
 * Returns: 0 (always for now). TODO: make it handle -1 as well.
 **/
int setting_get_description(const char *label, char *s, size_t len);

/**
 * setting_get_label:
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
void setting_get_label(file_list_t *list, char *s,
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
rarch_setting_t* menu_setting_new(unsigned mask);

bool menu_setting_is_of_path_type(rarch_setting_t *setting);

bool menu_setting_is_of_general_type(rarch_setting_t *setting);

bool menu_setting_is_of_numeric_type(rarch_setting_t *setting);

bool menu_setting_is_of_enum_type(rarch_setting_t *setting);

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action, bool wraparound);

#ifdef __cplusplus
}
#endif

#endif
