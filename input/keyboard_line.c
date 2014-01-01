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

#include "keyboard_line.h"
#include "../general.h"
#include "../driver.h"
#include <stddef.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

struct input_keyboard_line
{
   char *buffer;
   size_t ptr;
   size_t size;

   input_keyboard_line_complete_t cb;
   void *userdata;
};

void input_keyboard_line_free(input_keyboard_line_t *state)
{
   if (!state)
      return;

   free(state->buffer);
   free(state);
}

input_keyboard_line_t *input_keyboard_line_new(void *userdata,
      input_keyboard_line_complete_t cb)
{
   input_keyboard_line_t *state = (input_keyboard_line_t*)calloc(1, sizeof(*state));
   if (!state)
      return NULL;

   state->cb = cb;
   state->userdata = userdata;
   return state;
}

bool input_keyboard_line_event(input_keyboard_line_t *state, uint32_t character)
{
   // Treat extended chars as ? as we cannot support printable characters for unicode stuff.
   char c = character >= 128 ? '?' : character;
   if (c == '\r' || c == '\n')
   {
      state->cb(state->userdata, state->buffer);
      return true;
   }

   if (c == '\b')
   {
      if (state->ptr)
      {
         memmove(state->buffer + state->ptr - 1, state->buffer + state->ptr,
               state->size - state->ptr + 1);
         state->ptr--;
         state->size--;
      }
   }
   // Handle left/right here when suitable
   else if (isprint(c))
   {
      char *newbuf = (char*)realloc(state->buffer, state->size + 2);
      if (!newbuf)
         return false;

      memmove(newbuf + state->ptr + 1, newbuf + state->ptr, state->size - state->ptr + 1);
      newbuf[state->ptr] = c;
      state->ptr++;
      state->size++;
      newbuf[state->size] = '\0';

      state->buffer = newbuf;
   }

   return false;
}

const char **input_keyboard_line_get_buffer(const input_keyboard_line_t *state)
{
   return (const char**)&state->buffer;
}

static input_keyboard_line_t *g_keyboard_line;

const char **input_keyboard_start_line(void *userdata, input_keyboard_line_complete_t cb)
{
   if (g_keyboard_line)
      input_keyboard_line_free(g_keyboard_line);

   g_keyboard_line = input_keyboard_line_new(userdata, cb);
   // While reading keyboard line input, we have to block all hotkeys.
   driver.block_input = true;

   return input_keyboard_line_get_buffer(g_keyboard_line);
}

void input_keyboard_event(bool down, unsigned code, uint32_t character, uint16_t mod)
{
   if (g_keyboard_line)
   {
      if (input_keyboard_line_event(g_keyboard_line, character))
      {
         // Line is complete, can free it now.
         input_keyboard_line_free(g_keyboard_line);
         g_keyboard_line = NULL;

         // Unblock all hotkeys.
         driver.block_input = false;
      }
   }
   else if (g_extern.system.key_event)
      g_extern.system.key_event(down, code, character, mod);
}

