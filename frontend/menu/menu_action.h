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

#ifndef _MENU_ACTION_H
#define _MENU_ACTION_H

#include "../../settings_data.h"

void menu_common_setting_set_current_boolean(
      rarch_setting_t *setting, unsigned action);

void menu_common_setting_set_current_fraction(
      rarch_setting_t *setting, unsigned action);

void menu_common_setting_set_current_unsigned_integer(
      rarch_setting_t *setting, unsigned id, unsigned action);

#endif
