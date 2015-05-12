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
#include "../settings.h"
#ifdef HAVE_LIBRETRODB
#include "menu_database.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

void menu_entries_cbs_init_bind_toggle(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label);

int menu_entries_setting_set_flags(rarch_setting_t *setting);

#ifdef __cplusplus
}
#endif

#endif
