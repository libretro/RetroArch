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

#include "menu_navigation.h"
#include "menu_list.h"
#include "menu_setting.h"
#include "menu_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_entries
{
   /* Flagged when menu entries need to be refreshed */
   bool need_refresh;
   bool nonblocking_refresh;

   size_t begin;
   menu_list_t *menu_list;
   rarch_setting_t *list_settings;
   menu_navigation_t navigation;
} menu_entries_t;

void menu_entries_set_start(size_t i);

size_t menu_entries_get_start(void);

size_t menu_entries_get_end(void);

void menu_entries_get(size_t i, menu_entry_t *entry);

int menu_entries_get_title(char *title, size_t title_len);

bool menu_entries_show_back(void);

void menu_entries_get_core_title(char *title_msg, size_t title_msg_len);

menu_entries_t *menu_entries_get_ptr(void);

int menu_entries_refresh(unsigned action);

bool menu_entries_needs_refresh(void);

void menu_entries_set_refresh(void);

void menu_entries_unset_refresh(void);

void menu_entries_set_nonblocking_refresh(void);

void menu_entries_unset_nonblocking_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
