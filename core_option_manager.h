/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef CORE_OPTION_MANAGER_H__
#define CORE_OPTION_MANAGER_H__

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <lists/string_list.h>
#include <lists/nested_list.h>
#include <file/config_file.h>
#include <libretro.h>

#include <retro_miscellaneous.h>

RETRO_BEGIN_DECLS

struct core_option
{
   char *desc;
   char *desc_categorized;
   char *info;
   char *info_categorized;
   char *key;
   char *category_key;
   struct string_list *vals;
   struct string_list *val_labels;
   /* opt_idx: option index, used for internal
    * bookkeeping */
   size_t opt_idx;
   /* default_index, index: correspond to
    * option *value* indices */
   size_t default_index;
   size_t index;
   uint32_t key_hash;
   bool visible;
};

struct core_category
{
   char *key;
   char *desc;
   char *info;
   uint32_t key_hash;
};

/* TODO/FIXME: This struct should be made
 * 'private', with restricted access to its
 * members via interface functions. This
 * requires significant refactoring... */
struct core_option_manager
{
   config_file_t *conf;
   char conf_path[PATH_MAX_LENGTH];

   struct core_category *cats;
   struct core_option *opts;
   nested_list_t *option_map;

   size_t cats_size;
   size_t size;

   bool updated;
};

typedef struct core_option_manager core_option_manager_t;

/*********************/
/* Option Conversion */
/*********************/

/**
 * core_option_manager_convert_v1:
 *
 * @options_v1 : an array of retro_core_option_definition
 *               structs
 *
 * Converts an array of core option v1 definitions into
 * a v2 core options struct. Returned pointer must be
 * freed using core_option_manager_free_converted().
 *
 * Returns: Valid pointer to a new v2 core options struct
 * if successful, otherwise NULL.
 **/
struct retro_core_options_v2 *core_option_manager_convert_v1(
      const struct retro_core_option_definition *options_v1);

/**
 * core_option_manager_convert_v1_intl:
 *
 * @options_v1_intl : pointer to a retro_core_options_intl
 *                    struct
 *
 * Converts a v1 'international' core options definition
 * struct into a v2 core options struct. Returned pointer
 * must be freed using core_option_manager_free_converted().
 *
 * Returns: Valid pointer to a new v2 core options struct
 * if successful, otherwise NULL.
 **/
struct retro_core_options_v2 *core_option_manager_convert_v1_intl(
      const struct retro_core_options_intl *options_v1_intl);

/**
 * core_option_manager_convert_v2_intl:
 *
 * @options_v2_intl : pointer to a retro_core_options_v2_intl
 *                    struct
 *
 * Converts a v2 'international' core options struct
 * into a regular v2 core options struct. Returned pointer
 * must be freed using core_option_manager_free_converted().
 *
 * Returns: Valid pointer to a new v2 core options struct
 * if successful, otherwise NULL.
 **/
struct retro_core_options_v2 *core_option_manager_convert_v2_intl(
      const struct retro_core_options_v2_intl *options_v2_intl);

/**
 * core_option_manager_free_converted:
 *
 * @options_v2 : pointer to a retro_core_options_v2
 *               struct
 *
 * Frees the pointer returned by any
 * core_option_manager_convert_*() function.
 **/
void core_option_manager_free_converted(
      struct retro_core_options_v2 *options_v2);

/**************************************/
/* Initialisation / De-Initialisation */
/**************************************/

/**
 * core_option_manager_new_vars:
 *
 * @conf_path     : Filesystem path to write core option
 *                  config file to
 * @src_conf_path : Filesystem path from which to load
 *                  initial config settings.
 * @vars          : Pointer to core option variable array
 *                  handle
 *
 * Legacy version of core_option_manager_new().
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle if successful,
 * otherwise NULL.
 **/
core_option_manager_t *core_option_manager_new_vars(
      const char *conf_path, const char *src_conf_path,
      const struct retro_variable *vars);

/**
 * core_option_manager_new:
 *
 * @conf_path     : Filesystem path to write core option
 *                  config file to
 * @src_conf_path : Filesystem path from which to load
 *                  initial config settings.
 * @options_v2    : Pointer to retro_core_options_v2 struct
 * @categorized   : Flag specifying whether core option
 *                  category information should be read
 *                  from @options_v2
 *
 * Creates and initializes a core manager handle. Parses
 * information from a retro_core_options_v2 struct.
 * If @categorized is false, all option category
 * assignments will be ignored.
 *
 * Returns: handle to new core manager handle if successful,
 * otherwise NULL.
 **/
core_option_manager_t *core_option_manager_new(
      const char *conf_path, const char *src_conf_path,
      const struct retro_core_options_v2 *options_v2,
      bool categorized);

/**
 * core_option_manager_free:
 *
 * @opt : options manager handle
 *
 * Frees specified core options manager handle.
 **/
void core_option_manager_free(core_option_manager_t *opt);

/********************/
/* Category Getters */
/********************/

/**
 * core_option_manager_get_category_desc:
 *
 * @opt : options manager handle
 * @key : core option category id string
 *
 * Fetches the 'description' text of the core option
 * category identified by @key (used as the
 * category label in the menu).
 *
 * Returns: description string (menu label) of the
 * specified option category if successful,
 * otherwise NULL.
 **/
const char *core_option_manager_get_category_desc(core_option_manager_t *opt,
      const char *key);

/**
 * core_option_manager_get_category_info:
 *
 * @opt : options manager handle
 * @key : core option category id string
 *
 * Fetches the 'info' text of the core option
 * category identified by @key (used as the category
 * sublabel in the menu).
 *
 * Returns: information string (menu sublabel) of
 * the specified option category if successful,
 * otherwise NULL.
 **/
const char *core_option_manager_get_category_info(core_option_manager_t *opt,
      const char *key);

/**
 * core_option_manager_get_category_visible:
 *
 * @opt : options manager handle
 * @key : core option category id string
 *
 * Queries whether the core option category
 * identified by @key should be displayed in
 * the frontend menu. (A category is deemed to
 * be visible if at least one of the options
 * in the category is visible)
 *
 * @return true if option category should be
 * displayed by the frontend, otherwise false.
 **/
bool core_option_manager_get_category_visible(core_option_manager_t *opt,
      const char *key);

/******************/
/* Option Getters */
/******************/

/**
 * core_option_manager_get_idx:
 *
 * @opt : options manager handle
 * @key : core option key string (variable to query
 *        in RETRO_ENVIRONMENT_GET_VARIABLE)
 * @idx : index of core option corresponding
 *        to @key
 *
 * Fetches the index of the core option identified
 * by the specified @key.
 *
 * @return true if option matching the specified
 * key was found, otherwise false.
 **/
bool core_option_manager_get_idx(core_option_manager_t *opt,
      const char *key, size_t *idx);

/**
 * core_option_manager_get_val_idx:
 *
 * @opt     : options manager handle
 * @idx     : core option index
 * @val     : string representation of the
 *            core option value
 * @val_idx : index of core option value
 *            corresponding to @val
 *
 * Fetches the index of the core option value
 * identified by the specified core option @idx
 * and @val string.
 *
 * Returns: true if option value matching the
 * specified option index and value string
 * was found, otherwise false.
 **/
bool core_option_manager_get_val_idx(core_option_manager_t *opt,
      size_t idx, const char *val, size_t *val_idx);

/**
 * core_option_manager_get_desc:
 *
 * @opt         : options manager handle
 * @idx         : core option index
 * @categorized : flag specifying whether to
 *                fetch the categorised description
 *                or the legacy fallback
 *
 * Fetches the 'description' of the core option at
 * index @idx (used as the option label in the menu).
 * If menu has option category support, @categorized
 * should be true. (At present, only the Qt interface
 * requires @categorized to be false)
 *
 * Returns: description string (menu label) of the
 * specified option if successful, otherwise NULL.
 **/
const char *core_option_manager_get_desc(core_option_manager_t *opt,
      size_t idx, bool categorized);

/**
 * core_option_manager_get_info:
 *
 * @opt         : options manager handle
 * @idx         : core option index
 * @categorized : flag specifying whether to
 *                fetch the categorised information
 *                or the legacy fallback
 *
 * Fetches the 'info' text of the core option at
 * index @idx (used as the option sublabel in the
 * menu). If menu has option category support,
 * @categorized should be true. (At present, only
 * the Qt interface requires @categorized to be false)
 *
 * Returns: information string (menu sublabel) of the
 * specified option if successful, otherwise NULL.
 **/
const char *core_option_manager_get_info(core_option_manager_t *opt,
      size_t idx, bool categorized);

/**
 * core_option_manager_get_val:
 *
 * @opt : options manager handle
 * @idx : core option index
 *
 * Fetches the string representation of the current
 * value of the core option at index @idx.
 *
 * Returns: core option value string if successful,
 * otherwise NULL.
 **/
const char *core_option_manager_get_val(core_option_manager_t *opt,
      size_t idx);

/**
 * core_option_manager_get_val_label:
 *
 * @opt : options manager handle
 * @idx : core option index
 *
 * Fetches the 'label' text (used for display purposes
 * in the menu) for the current value of the core
 * option at index @idx.
 *
 * Returns: core option value label string if
 * successful, otherwise NULL.
 **/
const char *core_option_manager_get_val_label(core_option_manager_t *opt,
      size_t idx);

/**
 * core_option_manager_get_visible:
 *
 * @opt : options manager handle
 * @idx : core option index
 *
 * Queries whether the core option at index @idx
 * should be displayed in the frontend menu.
 *
 * Returns: true if option should be displayed by
 * the frontend, otherwise false.
 **/
bool core_option_manager_get_visible(core_option_manager_t *opt,
      size_t idx);

/******************/
/* Option Setters */
/******************/

/**
 * core_option_manager_set_val:
 *
 * @opt          : options manager handle
 * @idx          : core option index
 * @val_idx      : index of the value to set
 * @refresh_menu : flag specifying whether menu
 *                 should be refreshed if changes
 *                 to option visibility are detected
 *
 * Sets the core option at index @idx to the
 * option value corresponding to @val_idx.
 * After setting the option value, a request
 * will be made for the core to update the
 * in-menu visibility of all options; if
 * visibility changes are detected and
 * @refresh_menu is true, the menu will be
 * redrawn.
 **/
void core_option_manager_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx, bool refresh_menu);

/**
 * core_option_manager_adjust_val:
 *
 * @opt          : options manager handle
 * @idx          : core option index
 * @adjustment   : offset to apply from current
 *                 value index
 * @refresh_menu : flag specifying whether menu
 *                 should be refreshed if changes
 *                 to option visibility are detected
 *
 * Modifies the value of the core option at
 * index @idx by incrementing the current option
 * value index by @adjustment.
 * After setting the option value, a request
 * will be made for the core to update the
 * in-menu visibility of all options; if
 * visibility changes are detected and
 * @refresh_menu is true, the menu will be
 * redrawn.
 **/
void core_option_manager_adjust_val(core_option_manager_t* opt,
      size_t idx, int adjustment, bool refresh_menu);

/**
 * core_option_manager_set_default:
 *
 * @opt          : options manager handle
 * @idx          : core option index
 * @refresh_menu : flag specifying whether menu
 *                 should be refreshed if changes
 *                 to option visibility are detected
 *
 * Resets the core option at index @idx to
 * its default value.
 * After setting the option value, a request
 * will be made for the core to update the
 * in-menu visibility of all options; if
 * visibility changes are detected and
 * @refresh_menu is true, the menu will be
 * redrawn.
 **/
void core_option_manager_set_default(core_option_manager_t *opt,
      size_t idx, bool refresh_menu);

/**
 * core_option_manager_set_visible:
 *
 * @opt     : options manager handle
 * @key     : core option key string (variable to query
 *            in RETRO_ENVIRONMENT_GET_VARIABLE)
 * @visible : flag specifying whether option should
 *            be shown in the menu
 *
 * Sets the in-menu visibility of the core option
 * identified by the specified @key.
 **/
void core_option_manager_set_visible(core_option_manager_t *opt,
      const char *key, bool visible);

/**********************/
/* Configuration File */
/**********************/

/**
 * core_option_manager_flush:
 *
 * @opt  : options manager handle
 * @conf : configuration file handle
 *
 * Writes all core option key-pair values from the
 * specified core option manager handle to the
 * specified configuration file struct.
 **/
void core_option_manager_flush(core_option_manager_t *opt,
      config_file_t *conf);

RETRO_END_DECLS

#endif
