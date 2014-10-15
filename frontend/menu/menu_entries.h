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

#ifdef __cplusplus
extern "C" {
#endif

void menu_entries_push(file_list_t *list,
      const char *path, const char *label, unsigned type,
      size_t directory_ptr);

int menu_entries_parse_list(file_list_t *list, file_list_t *menu_list,
      const char *dir, const char *label, unsigned type,
      unsigned default_type_plain, const char *exts);

void menu_entries_pop_list(file_list_t *list);

int menu_entries_deferred_push(file_list_t *list, file_list_t *menu_list);

void menu_entries_pop_stack(file_list_t *list, const char *needle);

void menu_flush_stack_type(file_list_t *list, unsigned final_type);
void menu_flush_stack_label(file_list_t *list, const char *needle);

bool menu_entries_init(menu_handle_t *menu);

void entries_refresh(file_list_t *list);

void menu_build_scroll_indices(file_list_t *list);

#ifdef __cplusplus
}
#endif

#endif
