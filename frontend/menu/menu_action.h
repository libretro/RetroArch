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

#ifdef __cplusplus
extern "C" {
#endif

int menu_action_setting_boolean(
      rarch_setting_t *setting, unsigned action);

int menu_action_setting_fraction(
      rarch_setting_t *setting, unsigned action);

int menu_action_setting_unsigned_integer(
      rarch_setting_t *setting, unsigned action);

void menu_action_setting_driver(
      rarch_setting_t *setting, unsigned action);

int menu_action_set_current_string_based_on_label(
      const char *label, const char *str);

int menu_action_setting_set_current_string(
      rarch_setting_t *setting, const char *str);

int menu_action_setting_set_current_string_path(
      rarch_setting_t *setting, const char *dir, const char *path);

int menu_action_setting_set(unsigned type, const char *label,
      unsigned action);

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action);

#ifdef __cplusplus
}
#endif

#endif
