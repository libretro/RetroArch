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

#ifndef MENU_ENTRIES_CBS_H__
#define MENU_ENTRIES_CBS_H__

#include <stdlib.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FIXME - Externs, refactor */
extern size_t hack_shader_pass;
extern unsigned rdb_entry_start_game_selection_ptr;
#ifdef HAVE_NETWORKING
extern char core_updater_path[PATH_MAX_LENGTH];
#endif

void menu_entries_common_load_content(bool persist);

int menu_entries_common_is_settings_entry(const char *label);

void menu_entries_cbs_init_bind_get_string_representation(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1);

void menu_entries_cbs_init_bind_start(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1);

void menu_entries_cbs_init_bind_cancel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1);

void menu_entries_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label);

void menu_entries_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1);

int deferred_push_content_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type);

void menu_entries_cbs_init(void *data,
      const char *path, const char *label,
      unsigned type, size_t idx);

#ifdef __cplusplus
}
#endif

#endif
