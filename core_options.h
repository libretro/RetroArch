/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "boolean.h"
#include "libretro.h"
#include <stddef.h>

typedef struct core_option_manager core_option_manager_t;

core_option_manager_t *core_option_new(const char *conf_path, const struct retro_variable *vars);
bool core_option_updated(core_option_manager_t *opt);
void core_option_flush(core_option_manager_t *opt);
void core_option_free(core_option_manager_t *opt);

void core_option_get(core_option_manager_t *opt, struct retro_variable *var);

// Returns total number of options.
size_t core_option_size(core_option_manager_t *opt);

// Gets description and current value for an option.
const char *core_option_get_desc(core_option_manager_t *opt, size_t index);
const char *core_option_get_val(core_option_manager_t *opt, size_t index);

// Cycles through options for an option. Options wrap around.
void core_option_next(core_option_manager_t *opt, size_t index);
void core_option_prev(core_option_manager_t *opt, size_t index);

// Sets default val for an option.
void core_option_set_default(core_option_manager_t *opt, size_t index);

#endif

