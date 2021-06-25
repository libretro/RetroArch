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

RETRO_BEGIN_DECLS

/**
 * input_remapping_load_file:
 * @data                     : Path to config file.
 *
 * Loads a remap file from disk to memory.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(void *data, const char *path);

/**
 * input_remapping_save_file:
 * @path                     : Path to remapping file (relative path).
 *
 * Saves remapping values to file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save_file(const char *path);

bool input_remapping_remove_file(const char *path,
      const char *dir_input_remapping);

/* Caches any global configuration settings that
 * should not be overwritten by input remap
 * changes made while content is running.
 * Must be called on each core init. */
void input_remapping_cache_global_config(void);
/* Sets flags to enable the restoration of
 * global configuration settings from the
 * internal cache. Should be called independently
 * from 'input_remapping_cache_global_config()'.
 * Must be called:
 * - Whenever content is loaded
 * - Whenever a remap file is loaded */
void input_remapping_enable_global_config_restore(void);
/* Restores any cached global configuration settings
 * *if* 'input_remapping_enable_global_config_restore()'
 * has been called.
 * Must be called on core deint.
 * If 'clear_cache' is true, function becomes a NOOP
 * until the next time input_remapping_cache_global_config()
 * and input_remapping_enable_global_config_restore()
 * are called. */
void input_remapping_restore_global_config(bool clear_cache);

void input_remapping_update_port_map(void);

void input_remapping_deinit(void);
void input_remapping_set_defaults(bool clear_cache);

RETRO_END_DECLS

#endif
