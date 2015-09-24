/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef _MENU_INPUT_H
#define _MENU_INPUT_H

#include "../input/keyboard_line.h"
#include "../libretro.h"

typedef uint64_t retro_input_t;

#ifdef __cplusplus
extern "C" {
#endif
 
#ifndef MAX_USERS
#define MAX_USERS 16
#endif

typedef enum menu_action
{
   MENU_ACTION_UP,
   MENU_ACTION_DOWN,
   MENU_ACTION_LEFT,
   MENU_ACTION_RIGHT,
   MENU_ACTION_OK,
   MENU_ACTION_SEARCH,
   MENU_ACTION_SCAN,
   MENU_ACTION_CANCEL,
   MENU_ACTION_INFO,
   MENU_ACTION_SELECT,
   MENU_ACTION_START,
   MENU_ACTION_MESSAGE,
   MENU_ACTION_SCROLL_DOWN,
   MENU_ACTION_SCROLL_UP,
   MENU_ACTION_TOGGLE,
   MENU_ACTION_NOOP
} menu_action_t;

enum mouse_action
{
   MOUSE_ACTION_NONE = 0,
   MOUSE_ACTION_BUTTON_L,
   MOUSE_ACTION_BUTTON_L_TOGGLE,
   MOUSE_ACTION_BUTTON_L_SET_NAVIGATION,
   MOUSE_ACTION_BUTTON_R,
   MOUSE_ACTION_WHEEL_UP,
   MOUSE_ACTION_WHEEL_DOWN
};

enum menu_input_action
{
   MENU_INPUT_POINTER_AXIS_X = 0,
   MENU_INPUT_POINTER_AXIS_Y,
   MENU_INPUT_POINTER_DELTA_AXIS_X,
   MENU_INPUT_POINTER_DELTA_AXIS_Y,
   MENU_INPUT_POINTER_DRAGGED,
   MENU_INPUT_MOUSE_AXIS_X,
   MENU_INPUT_MOUSE_AXIS_Y,
   MENU_INPUT_MOUSE_SCROLL_UP,
   MENU_INPUT_MOUSE_SCROLL_DOWN
};

enum menu_input_pointer_acceleration
{
   MENU_INPUT_PTR_ACCELERATION  = 0,
   MENU_INPUT_PTR_ACCELERATION_1,
   MENU_INPUT_PTR_ACCELERATION_2 
};

enum menu_input_pointer_type
{
   MENU_INPUT_PTR_TYPE_POINTER = 0,
   MENU_INPUT_PTR_TYPE_MOUSE
};

enum menu_input_bind_mode
{
   MENU_INPUT_BIND_NONE,
   MENU_INPUT_BIND_SINGLE,
   MENU_INPUT_BIND_ALL
};

typedef struct menu_input menu_input_t;

void menu_input_key_event(bool down, unsigned keycode, uint32_t character,
      uint16_t key_modifiers);

void menu_input_key_start_line(const char *label,
      const char *label_setting, unsigned type, unsigned idx,
      input_keyboard_line_complete_t cb);

bool menu_input_key_displayed(void);

const char *menu_input_key_get_buff(void);

const char *menu_input_key_get_label(void);

void menu_input_st_uint_callback(void *userdata, const char *str);
void menu_input_st_hex_callback(void *userdata, const char *str);

void menu_input_st_string_callback(void *userdata, const char *str);

void menu_input_st_cheat_callback(void *userdata, const char *str);

int menu_input_bind_iterate(char *s, size_t len);

unsigned menu_input_frame(retro_input_t input, retro_input_t trigger_state);

void menu_input_post_iterate(int *ret, unsigned action);

void menu_input_search_start(void);

int menu_input_set_keyboard_bind_mode(void *data, enum menu_input_bind_mode type);

int menu_input_set_input_device_bind_mode(void *data, enum menu_input_bind_mode type);

void menu_input_set_binds(unsigned min, unsigned max);

void menu_input_set_pointer(enum menu_input_pointer_type type, unsigned val);

int16_t menu_input_pressed(enum menu_input_action axis);

void menu_input_set_acceleration(enum menu_input_pointer_acceleration accel, float val);

float menu_input_get_acceleration(enum menu_input_pointer_acceleration accel);

void menu_input_free(void *data);

bool menu_input_init(void *data);

#ifdef __cplusplus
}
#endif

#endif
