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

#include "rewind.h"
#include "rewind-alcaro.h"
#include <stdlib.h>
#include <string.h>

struct state_manager {
	struct rewindstack * core;
	unsigned int state_size;
};

state_manager_t *state_manager_new(size_t state_size, size_t buffer_size, void *init_buffer)
{
	state_manager_t *state = (state_manager_t*)calloc(1, sizeof(*state));
	if (!state)
		return NULL;
	
	state->state_size=state_size;
	
	state->core=rewindstack_create(state_size, buffer_size);
	if (!state->core)
	{
		free(state);
		return NULL;
	}
	
	void* first_state=state->core->push_begin(state->core);
	memcpy(first_state, init_buffer, state_size);
	state->core->push_end(state->core);
}

void state_manager_free(state_manager_t *state)
{
	state->core->free(state->core);
	free(state);
}

bool state_manager_pop(state_manager_t *state, void **data)
{
	*data=(void*)state->core->pull(state->core);
	return (*data);
}

bool state_manager_push(state_manager_t *state, const void *data)
{
	void* next_state=state->core->push_begin(state->core);
	memcpy(next_state, data, state->state_size);
	state->core->push_end(state->core);
	return true;
}
