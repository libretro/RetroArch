/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef CORE_OPTIONS_H__
#define CORE_OPTIONS_H__

#include <stddef.h>

#include <boolean.h>
#include <string/string_list.h>

#include "libretro.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct core_option_manager core_option_manager_t;

/**
 * core_option_new:
 * @conf_path        : Filesystem path to write core option config file to.
 * @vars             : Pointer to variable array handle.
 *
 * Creates and initializes a core manager handle.
 *
 * Returns: handle to new core manager handle, otherwise NULL.
 **/
core_option_manager_t *core_option_new(const char *conf_path,
      const struct retro_variable *vars);

/**
 * core_option_updated:
 * @opt              : options manager handle
 *
 * Has a core option been updated?
 *
 * Returns: true (1) if a core option has been updated,
 * otherwise false (0).
 **/
bool core_option_updated(core_option_manager_t *opt);

/**
 * core_option_flush:
 * @opt              : options manager handle
 *
 * Writes core option key-pair values to file.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
bool core_option_flush(core_option_manager_t *opt);

/**
 * core_option_flush_game_specific:
 * @opt              : options manager handle
 * @path             : path for the core options file
 *
 * Writes core option key-pair values to a custom file.
 *
 * Returns: true (1) if core option values could be
 * successfully saved to disk, otherwise false (0).
 **/
bool core_option_flush_game_specific(
      core_option_manager_t *opt, char* path);

/**
 * core_option_free:
 * @opt              : options manager handle
 *
 * Frees core option manager handle.
 **/
void core_option_free(core_option_manager_t *opt);

void core_option_get(core_option_manager_t *opt, 
      struct retro_variable *var);

/**
 * core_option_size:
 * @opt              : options manager handle
 *
 * Gets total number of options.
 *
 * Returns: Total number of options.
 **/
size_t core_option_size(core_option_manager_t *opt);

/**
 * core_option_get_desc:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets description for an option.
 *
 * Returns: Description for an option.
 **/
const char *core_option_get_desc(core_option_manager_t *opt, 
      size_t idx);

/**
 * core_option_get_val:
 * @opt              : options manager handle
 * @idx              : idx identifier of the option
 *
 * Gets value for an option.
 *
 * Returns: Value for an option.
 **/
const char *core_option_get_val(core_option_manager_t *opt, 
      size_t idx);

/**
 * core_option_get_vals:
 * @opt                   : pointer to core option manager object.
 * @idx                   : idx of core option.
 *
 * Gets list of core option values from core option at index @idx.
 *
 * Returns: string list of core option values if successful, otherwise
 * NULL.
 **/
struct string_list *core_option_get_vals(
      core_option_manager_t *opt, size_t idx);

void core_option_set_val(core_option_manager_t *opt,
      size_t idx, size_t val_idx);

/**
 * core_option_next:
 * @opt                   : pointer to core option manager object.
 * @idx                   : idx of core option to be reset to defaults.
 *
 * Get next value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_next(core_option_manager_t *opt, size_t idx);

/**
 * core_option_prev:
 * @opt                   : pointer to core option manager object.
 * @idx                   : idx of core option to be reset to defaults.
 * Options wrap around.
 *
 * Get previous value for core option specified by @idx.
 * Options wrap around.
 **/
void core_option_prev(core_option_manager_t *opt, size_t idx);

/**
 * core_option_set_default:
 * @opt                   : pointer to core option manager object.
 * @idx                   : idx of core option to be reset to defaults.
 *
 * Reset core option specified by @idx and sets default value for option.
 **/
void core_option_set_default(core_option_manager_t *opt, size_t idx);

#ifdef __cplusplus
}
#endif

#endif

