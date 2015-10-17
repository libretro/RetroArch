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

#ifndef __MENU_ENTRIES_H__
#define __MENU_ENTRIES_H__

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

#include "menu_setting.h"
#include "menu_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_entries menu_entries_t;

void menu_entries_set_start(size_t i);

size_t menu_entries_get_start(void);

size_t menu_entries_get_end(void);

void menu_entries_get(size_t i, menu_entry_t *entry);

int menu_entries_get_title(char *title, size_t title_len);

bool menu_entries_show_back(void);

int menu_entries_get_core_title(char *title_msg, size_t title_msg_len);

rarch_setting_t *menu_setting_get_ptr(void);

bool menu_entries_needs_refresh(void);

void menu_entries_set_refresh(bool nonblocking);

void menu_entries_unset_refresh(bool nonblocking);

file_list_t *menu_entries_get_selection_buf_ptr(void);

file_list_t *menu_entries_get_menu_stack_ptr(void);

void menu_entries_push(file_list_t *list, const char *path, const char *label,
      unsigned type, size_t directory_ptr, size_t entry_idx);

bool menu_entries_init(void *data);

void menu_entries_free(void);

void menu_entries_free_list(menu_entries_t *entries);

void menu_entries_new_list(menu_entries_t *entries, unsigned flags);

void menu_entries_get_last_stack(const char **path, const char **label,
      unsigned *file_type, size_t *entry_idx);

#ifdef __cplusplus
}
#endif

#endif
