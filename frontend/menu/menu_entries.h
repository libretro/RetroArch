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

#ifndef MENU_ENTRIES_H__
#define MENU_ENTRIES_H__

#include <stdlib.h>
#include "menu_common.h"
#include "../../file_list.h"
#include "../../settings_data.h"

void menu_entries_push(file_list_t *list,
      const char *path, const char *label, unsigned type,
      size_t directory_ptr);

int menu_entries_push_list(menu_handle_t *menu,
      file_list_t *list, const char *path,
      const char *label, unsigned menu_type);

void menu_entries_pop_list(file_list_t *list);

int menu_parse_check(const char *label, unsigned menu_type);

int menu_parse_and_resolve(file_list_t *list, file_list_t *menu_list);

void menu_entries_pop_stack(file_list_t *list, const char *needle);

void menu_flush_stack_type(file_list_t *list, unsigned final_type);
void menu_flush_stack_label(file_list_t *list, const char *needle);

int menu_entries_set_current_path_selection(
      rarch_setting_t *setting, const char *start_path,
      const char *label, unsigned type,
      unsigned action);

void *menu_entries_get_last_setting(const char *label, int index,
      rarch_setting_t *settings);

#endif
