/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#ifndef _INPUT_REMAPPING_H
#define _INPUT_REMAPPING_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "input_defines.h"
#include "input_types.h"

RETRO_BEGIN_DECLS

/**
 * Loads a remap file from disk to memory.
 * 
 * @param data Path to config file.
 *
 * @return true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(void *data, const char *path);

/**
 * Saves remapping values to file.
 *
 * @param path Relative path to remapping file.
 *
 * @return true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save_file(const char *path);

bool input_remapping_remove_file(const char *path, 
                                 const char *dir_input_remapping);

/**
 * Caches any global configuration settings that should not be overwritten by
 * input remap changes made while content is running. Must be called on each
 * core init. 
 */
void input_remapping_cache_global_config(void);

/**
 * Sets flags to enable the restoration of global configuration settings from
 * the internal cache. Should be called independently from 
 * `input_remapping_cache_global_config()`.
 * Must be called:
 *   - Whenever content is loaded
 *   - Whenever a remap file is loaded 
 */
void input_remapping_enable_global_config_restore(void);

/**
 * Restores any global configuration settings that were cached on the last core
 * init if `input_remapping_enable_global_config_restore()` has been called.
 * Must be called on core deinitialization. 
 * 
 * @param clear_cache  If true, function becomes a NOOP until the next time
 *                     `input_remapping_cache_global_config()` and 
 *                     `input_remapping_enable_global_config_restore()` are
 *                     called.
 */
void input_remapping_restore_global_config(bool clear_cache);

/**
 * Must be called whenever `settings->uints.input_remap_ports` is modified.
 */
void input_remapping_update_port_map(void);

/**
 * Frees global->name.remapfile and sets these runloop_state flags to false: 
 * remaps_core_active, remaps_content_dir_active, and remaps_game_active.
 */
void input_remapping_deinit(void);

/**
 * Used to set the default mapping values within the `settings` struct
 * @param clear_cache  This value is passed to 
 *                     `input_remapping_restore_global_config()`. Please see
 *                     the documentation for that function for details.
 */
void input_remapping_set_defaults(bool clear_cache);

/**
 * Checks `input_config_bind_map` for the requested `input_bind_map`, and if
 * the bind has been registered, returns its base.
 * 
 * @param index
 * 
 * @return the contents of the meta field, or NULL if there is no matching bind
 */
const char *input_config_bind_map_get_base(unsigned bind_index);

/**
 * Checks `input_config_bind_map` for the requested `input_bind_map`, and if
 * the bind has been registered, returns the value of its meta binds field.
 *
 * @param index
 *
 * @return the contents of the meta field, or 0 if there is no matching bind
 */
unsigned input_config_bind_map_get_meta(unsigned bind_index);

/**
 * Checks `input_config_bind_map` for the requested `input_bind_map`, and if
 * the bind has been registered, returns a pointer to its description field.
 * 
 * @param index
 * 
 * @return the contents of the description field, or NULL if there is no
 *         matching bind
 */
const char *input_config_bind_map_get_desc(unsigned index);

/**
 * Checks `input_config_bind_map` for the requested `input_bind_map`, and if
 * the bind has been registered, returns the value of its retro_key field.
 * 
 * @param index
 * 
 * @return the value of the retro_key field, or 0 if there is no matching bind
 */
uint8_t input_config_bind_map_get_retro_key(unsigned index);

/**
 * Converts a retro_keybind to a human-readable string, optionally allowing a
 * fallback auto_bind to be used as the source for the string. 
 *
 * @param buf        A string which will be overwritten with the returned value
 * @param bind       A binding to convert to a string
 * @param auto_bind  A default binding which will be used after `bind`. Can be NULL.
 * @param size       The maximum length that will be written to `buf`
 */
void input_config_get_bind_string(void *settings_data,
      char *buf, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size);

/**
 * Parses the string representation of a retro_key struct
 * 
 * @param str String to parse.
 *
 * @return Key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str);

/**
 * Searches for a string among the "base" fields of the list of binds.
 * 
 * @param str String to search for among the binds
 *
 * @return Bind index value on success or RARCH_BIND_LIST_END if not found.
 **/
unsigned input_config_translate_str_to_bind_id(const char *str);

/**
 * Parse the bind data of an object of config_file_t.
 *
 * @param data  An object of type config_file_t. We assume it is passed as a
 *              void pointer like this to avoid including config_file.h.
 **/
void config_read_keybinds_conf(void *data);

/**
 * Apply autoconfig binds to the indicated control port.
 * 
 * @param port
 * @param data  An object of type config_file_t. We assume it is passed as a
 *              void pointer like this to avoid including config_file.h.
 */
void input_config_set_autoconfig_binds(unsigned port, void *data);

void input_mapper_reset(void *data);

RETRO_END_DECLS

#endif
