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

#ifndef KEYBOARD_LINE_H__
#define KEYBOARD_LINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../boolean.h"
#include "../libretro.h"
#include <stdint.h>

// Keyboard line reader. Handles textual input in a direct fashion.
typedef struct input_keyboard_line input_keyboard_line_t;

// Calls back after return is pressed with the completed line.
// line can be NULL.
typedef void (*input_keyboard_line_complete_t)(void *userdata, const char *line);

input_keyboard_line_t *input_keyboard_line_new(void *userdata,
      input_keyboard_line_complete_t cb);

// Called on every keyboard character event.
bool input_keyboard_line_event(input_keyboard_line_t *state, uint32_t character);

// Returns pointer to string. The underlying buffer can be reallocated at any time (or be NULL), but the pointer to it remains constant throughout the objects lifetime.
const char **input_keyboard_line_get_buffer(const input_keyboard_line_t *state);
void input_keyboard_line_free(input_keyboard_line_t *state);

// Keyboard event utils. Called by drivers when keyboard events are fired.
// This interfaces with the global driver struct and libretro callbacks.
void input_keyboard_event(bool down, unsigned code, uint32_t character, uint16_t mod);
const char **input_keyboard_start_line(void *userdata, input_keyboard_line_complete_t cb);

#ifdef __cplusplus
}
#endif

#endif

