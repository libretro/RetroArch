/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "../../input/keyboard_line.h"

void menu_key_event(bool down, unsigned keycode, uint32_t character,
      uint16_t key_modifiers);

void menu_key_start_line(void *data, const char *label,
      const char *label_setting, input_keyboard_line_complete_t cb);

void st_uint_callback(void *userdata, const char *str);

void st_string_callback(void *userdata, const char *str);

void preset_filename_callback(void *userdata, const char *str);

void menu_poll_bind_get_rested_axes(struct menu_bind_state *state);

void menu_poll_bind_state(struct menu_bind_state *state);

bool menu_poll_find_trigger(struct menu_bind_state *state,
      struct menu_bind_state *new_state);

bool menu_custom_bind_keyboard_cb(void *data, unsigned code);

uint64_t menu_input(void);

#endif
