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

#ifndef _MENU_INPUT_LINE_CB_H
#define _MENU_INPUT_LINE_CB_H

#include "../input/input_common.h"
#include "../input/keyboard_line.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
   MENU_ACTION_UP,
   MENU_ACTION_DOWN,
   MENU_ACTION_LEFT,
   MENU_ACTION_RIGHT,
   MENU_ACTION_OK,
   MENU_ACTION_SEARCH,
   MENU_ACTION_TEST,
   MENU_ACTION_CANCEL,
   MENU_ACTION_REFRESH,
   MENU_ACTION_SELECT,
   MENU_ACTION_START,
   MENU_ACTION_MESSAGE,
   MENU_ACTION_SCROLL_DOWN,
   MENU_ACTION_SCROLL_UP,
   MENU_ACTION_TOGGLE,
   MENU_ACTION_NOOP
} menu_action_t;

enum menu_input_bind_mode
{
   MENU_INPUT_BIND_NONE,
   MENU_INPUT_BIND_SINGLE,
   MENU_INPUT_BIND_ALL,
};

void menu_input_key_event(bool down, unsigned keycode, uint32_t character,
      uint16_t key_modifiers);

void menu_input_key_start_line(const char *label,
      const char *label_setting, unsigned type, unsigned idx,
      input_keyboard_line_complete_t cb);

void menu_input_st_uint_callback(void *userdata, const char *str);
void menu_input_st_hex_callback(void *userdata, const char *str);

void menu_input_st_string_callback(void *userdata, const char *str);

void menu_input_st_cheat_callback(void *userdata, const char *str);

bool menu_input_poll_find_trigger(struct menu_bind_state *state,
      struct menu_bind_state *new_state);

int menu_input_bind_iterate(void);

int menu_input_bind_iterate_keyboard(void);

unsigned menu_input_frame(retro_input_t input, retro_input_t trigger_state);

void menu_input_post_iterate(int *ret, unsigned action);

void menu_input_search_start(void);

int menu_input_set_keyboard_bind_mode(void *data, enum menu_input_bind_mode type);

int menu_input_set_input_device_bind_mode(void *data, enum menu_input_bind_mode type);

#ifdef __cplusplus
}
#endif

#endif
