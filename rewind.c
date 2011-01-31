/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "rewind.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

struct state_manager
{
   uint64_t *buffer;
   size_t buf_size;
   uint32_t *tmp_state;
   uint32_t *scratch_buf;
   size_t top_ptr;
   size_t bottom_ptr;
   size_t state_size;
};

state_manager_t *state_manager_new(size_t state_size, size_t buffer_size, void *init_buffer)
{
   if (buffer_size <= state_size * 2 + 1) // Need a sufficient buffer size.
      return NULL;

   state_manager_t *state = calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   state->top_ptr = 1;
   state->state_size = (state_size + 3) >> 2; // Multiple of 4.
   state->buf_size = (buffer_size + 7) >> 3; // Multiple of 8.
   if (!(state->buffer = calloc(1, state->buf_size * sizeof(uint64_t))))
      goto error;
   if (!(state->tmp_state = calloc(1, state->state_size * sizeof(uint32_t))))
      goto error;
   if (!(state->scratch_buf = calloc(1, state->state_size * sizeof(uint32_t))))
      goto error;

   memcpy(state->tmp_state, init_buffer, state_size);

   return state;

error:
   if (state)
   {
      free(state->buffer);
      free(state->tmp_state);
      free(state->scratch_buf);
      free(state);
   }
   return NULL;
}

void state_manager_free(state_manager_t *state)
{
   free(state->buffer);
   free(state->tmp_state);
   free(state->scratch_buf);
   free(state);
}

bool state_manager_pop(state_manager_t *state, void **data)
{ 
   *data = state->tmp_state;

   if (state->top_ptr == 0)
      state->top_ptr = state->buf_size - 1;
   else
      state->top_ptr--;

   if (state->top_ptr == state->bottom_ptr) // Our stack is completely empty... :v
   {
      state->top_ptr = (state->top_ptr + 1) % state->buf_size;
      print_status(state);
      return false;
   }

   while (state->buffer[state->top_ptr])
   {
      // Apply the xor patch.
      uint32_t addr = state->buffer[state->top_ptr] >> 32;
      uint32_t xor = state->buffer[state->top_ptr] & 0xFFFFFFFFU;
      state->tmp_state[addr] ^= xor;

      if (state->top_ptr == 0)
         state->top_ptr = state->buf_size - 1;
      else
         state->top_ptr--;
   }

   if (state->top_ptr == state->bottom_ptr) // Our stack is completely empty... :v
   {
      state->top_ptr = (state->top_ptr + 1) % state->buf_size;
      print_status(state);
      return true;
   }

   print_status(state);
   return true;
}

static void reassign_bottom(state_manager_t *state)
{
   state->bottom_ptr = (state->top_ptr + 1) % state->buf_size;
   while (state->buffer[state->bottom_ptr]) // Skip ahead until we find the first 0 (boundary for state delta).
      state->bottom_ptr = (state->bottom_ptr + 1) % state->buf_size;
}

static void generate_delta(state_manager_t *state, const void *data, bool aligned)
{
   bool crossed = false;
   const uint32_t *old_state = state->tmp_state;
   uint32_t *new_state = aligned ? (uint32_t*)data : state->scratch_buf;
   if (!aligned)
      memcpy(new_state, data, state->state_size * sizeof(uint32_t)); // If not guaranteed to be aligned, we need to make sure it is.

   state->buffer[state->top_ptr++] = 0; // For each separate delta, we have a 0 value sentinel in between.
   if (state->top_ptr == state->bottom_ptr) // Check if top_ptr and bottom_ptr crossed eachother, which means we need to delete old cruft.
      crossed = true;

   for (uint64_t i = 0; i < state->state_size; i++)
   {
      uint64_t xor = old_state[i] ^ new_state[i];

      // If the data differs (xor != 0), we push that xor on the stack with index and xor. This can be reversed by reapplying the xor.
      // This, if states don't really differ much, we'll save lots of space :) Hopefully this will work really well with save states.
      if (xor)
      {
         state->buffer[state->top_ptr] = (i << 32) | xor;
         state->top_ptr = (state->top_ptr + 1) % state->buf_size;

         if (state->top_ptr == state->bottom_ptr)
            crossed = true;
      }
   }

   if (crossed)
      reassign_bottom(state);
}

bool state_manager_push(state_manager_t *state, const void *data, bool aligned)
{
   generate_delta(state, data, aligned);
   memcpy(state->tmp_state, data, state->state_size * sizeof(uint32_t));

   print_status(state);

   return true;
}

