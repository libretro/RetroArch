/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#ifndef _MENU_GENERIC_H
#define _MENU_GENERIC_H

#include <boolean.h>

#include "../menu_driver.h"
#include "../menu_input.h"
#include "../menu_entries.h"

enum action_iterate_type
{
   ITERATE_TYPE_DEFAULT = 0,
   ITERATE_TYPE_HELP,
   ITERATE_TYPE_INFO,
   ITERATE_TYPE_BIND
};

int generic_menu_iterate(void *data, void *userdata, enum menu_action action);

bool generic_menu_init_list(void *data);

int generic_menu_entry_action(void *userdata, menu_entry_t *entry, size_t i, enum menu_action action);

#endif
