/*  RetroArch - A frontend for libretro.
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

#ifndef MENU_ENTRY_H__
#define MENU_ENTRY_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum menu_entry_type
{
   MENU_ENTRY_ACTION = 0,
   MENU_ENTRY_BOOL,
   MENU_ENTRY_INT,
   MENU_ENTRY_UINT,
   MENU_ENTRY_FLOAT,
   MENU_ENTRY_PATH,
   MENU_ENTRY_DIR,
   MENU_ENTRY_STRING,
   MENU_ENTRY_HEX,
   MENU_ENTRY_BIND,
   MENU_ENTRY_ENUM,
};

void get_core_title(char *title_msg, size_t title_msg_len);

rarch_setting_t *get_menu_entry_setting(uint32_t i);

enum menu_entry_type get_menu_entry_type(uint32_t i);

const char *get_menu_entry_label(uint32_t i);

uint32_t menu_entry_bool_value_get(uint32_t i);

void menu_entry_bool_value_set(uint32_t i, uint32_t new_val);

struct string_list *menu_entry_enum_values(uint32_t i);

void menu_entry_enum_value_set_with_string(uint32_t i, const char *s);

int32_t menu_entry_bind_index(uint32_t i);

void menu_entry_bind_key_set(uint32_t i, int32_t value);

void menu_entry_bind_joykey_set(uint32_t i, int32_t value);

void menu_entry_bind_joyaxis_set(uint32_t i, int32_t value);

void menu_entry_pathdir_selected(uint32_t i);

uint32_t menu_entry_pathdir_allow_empty(uint32_t i);

uint32_t menu_entry_pathdir_for_directory(uint32_t i);

const char *menu_entry_pathdir_value_get(uint32_t i);

void menu_entry_pathdir_value_set(uint32_t i, const char *s);

const char *menu_entry_pathdir_extensions(uint32_t i);

void menu_entry_reset(uint32_t i);

void menu_entry_value_get(uint32_t i, char *s, size_t len);

void menu_entry_value_set(uint32_t i, const char *s);

uint32_t menu_entry_num_has_range(uint32_t i);

float menu_entry_num_min(uint32_t i);

float menu_entry_num_max(uint32_t i);

uint32_t menu_select_entry(uint32_t i);

#ifdef __cplusplus
}
#endif

#endif
