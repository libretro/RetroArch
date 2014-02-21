/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RARCH_REWIND_H
#define __RARCH_REWIND_H

#include <stddef.h>
#include "boolean.h"

typedef struct state_manager state_manager_t;

state_manager_t *state_manager_new(size_t state_size, size_t buffer_size);
void state_manager_free(state_manager_t *state);
bool state_manager_pop(state_manager_t *state, const void **data);
void state_manager_push_where(state_manager_t *state, void **data);
void state_manager_push_do(state_manager_t *state);
void state_manager_capacity(state_manager_t *state, unsigned int *entries, size_t *bytes, bool *full);

#endif
