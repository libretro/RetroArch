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

#ifndef INPUT_KEYBOARD_H__
#define INPUT_KEYBOARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <boolean.h>
#include "../libretro.h"
#include <stdint.h>

/* Keyboard line reader. Handles textual input in a direct fashion. */
typedef struct input_keyboard_line input_keyboard_line_t;

/** Line complete callback. 
 * Calls back after return is pressed with the completed line.
 * Line can be NULL.
 **/
typedef void (*input_keyboard_line_complete_t)(void *userdata,
      const char *line);

typedef bool (*input_keyboard_press_t)(void *userdata, unsigned code);

/**
 * input_keyboard_line_new:
 * @userdata                 : Userdata.
 * @cb                       : Callback function.
 *
 * Creates and initializes input keyboard line handle.
 * Also sets callback function for keyboard line handle
 * to provided callback @cb.
 *
 * Returns: keyboard handle on success, otherwise NULL.
 **/
input_keyboard_line_t *input_keyboard_line_new(void *userdata,
      input_keyboard_line_complete_t cb);

/**
 * input_keyboard_line_event:
 * @state                    : Input keyboard line handle.
 * @character                : Inputted character.
 *
 * Called on every keyboard character event.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool input_keyboard_line_event(input_keyboard_line_t *state,
      uint32_t character);

/**
 * input_keyboard_line_get_buffer:
 * @state                    : Input keyboard line handle.
 *
 * Gets the underlying buffer of the keyboard line.
 *
 * The underlying buffer can be reallocated at any time 
 * (or be NULL), but the pointer to it remains constant 
 * throughout the objects lifetime.
 *
 * Returns: pointer to string.
 **/
const char **input_keyboard_line_get_buffer(
      const input_keyboard_line_t *state);

/**
 * input_keyboard_line_free:
 * @state                    : Input keyboard line handle.
 *
 * Frees input keyboard line handle.
 **/
void input_keyboard_line_free(input_keyboard_line_t *state);

/**
 * input_keyboard_event:
 * @down                     : Keycode was pressed down?
 * @code                     : Keycode.
 * @character                : Character inputted.
 * @mod                      : TODO/FIXME: ???
 *
 * Keyboard event utils. Called by drivers when keyboard events are fired.
 * This interfaces with the global driver struct and libretro callbacks.
 **/
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device);

/**
 * input_keyboard_start_line:
 * @userdata                 : Userdata.
 * @cb                       : Line complete callback function.
 *
 * Sets function pointer for keyboard line handle.
 *
 * Returns: underlying buffer returned by 
 * input_keyboard_line_get_buffer().
 **/
const char **input_keyboard_start_line(void *userdata,
      input_keyboard_line_complete_t cb);

/**
 * input_keyboard_wait_keys:
 * @userdata                 : Userdata.
 * @cb                       : Callback function.
 *
 * Waits for keys to be pressed (used for binding keys in the menu).
 * Callback returns false when all polling is done.
 **/
void input_keyboard_wait_keys(void *userdata, input_keyboard_press_t cb);

/**
 * input_keyboard_wait_keys_cancel:
 *
 * Cancels function callback set by input_keyboard_wait_keys().
 **/
void input_keyboard_wait_keys_cancel(void);

#ifdef __cplusplus
}
#endif

#endif

