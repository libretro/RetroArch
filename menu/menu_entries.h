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

#ifndef MENU_ENTRIES_H__
#define MENU_ENTRIES_H__

#include <stdlib.h>
#include "menu.h"
#include <file/file_list.h>
#include "../settings_data.h"

#ifdef __cplusplus
extern "C" {
#endif

int menu_entries_parse_list(file_list_t *list, file_list_t *menu_list,
      const char *dir, const char *label, unsigned type,
      unsigned default_type_plain, const char *exts,
      rarch_setting_t *setting);

int menu_entries_deferred_push(file_list_t *list, file_list_t *menu_list);

/**
 * menu_entries_init:
 * @menu                     : Menu handle.
 *
 * Creates and initializes menu entries.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool menu_entries_init(menu_handle_t *menu);

int menu_entries_setting_set_flags(rarch_setting_t *setting);

int menu_entries_push_main_menu_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned menu_type);

int menu_entries_push_horizontal_menu_list(menu_handle_t *menu,
      file_list_t *list,
      const char *path, const char *label,
      unsigned menu_type);

#ifdef __cplusplus
}
#endif

#endif
