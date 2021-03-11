/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _MENU_SETTING_H
#define _MENU_SETTING_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_common_api.h>

#include <lists/file_list.h>

#include "../setting_list.h"

RETRO_BEGIN_DECLS

enum setting_list_flags
{
   SL_FLAG_MAIN_MENU                                =  (1 << 0),
   SL_FLAG_SETTINGS                                 =  (1 << 1),
   SL_FLAG_SETTINGS_DRIVER_OPTIONS                  =  (1 << 2),
   SL_FLAG_SETTINGS_CORE_OPTIONS                    =  (1 << 3),
   SL_FLAG_SETTINGS_CONFIGURATION_OPTIONS           =  (1 << 4),
   SL_FLAG_SETTINGS_REWIND_OPTIONS                  =  (1 << 5),
   SL_FLAG_SETTINGS_VIDEO_OPTIONS                   =  (1 << 6),
   SL_FLAG_SETTINGS_SHADER_OPTIONS                  =  (1 << 7),
   SL_FLAG_SETTINGS_FONT_OPTIONS                    =  (1 << 8),
   SL_FLAG_SETTINGS_AUDIO_OPTIONS                   =  (1 << 9),
   SL_FLAG_SETTINGS_INPUT_OPTIONS                   =  (1 << 10),
   SL_FLAG_SETTINGS_INPUT_HOTKEY_OPTIONS            =  (1 << 11),
   SL_FLAG_SETTINGS_OVERLAY_OPTIONS                 =  (1 << 12),
   SL_FLAG_SETTINGS_MENU_OPTIONS                    =  (1 << 13),
   SL_FLAG_SETTINGS_MULTIMEDIA_OPTIONS              =  (1 << 14),
   SL_FLAG_SETTINGS_UI_OPTIONS                      =  (1 << 15),
   SL_FLAG_SETTINGS_CHEEVOS_OPTIONS                 =  (1 << 16),
   SL_FLAG_SETTINGS_CORE_UPDATER_OPTIONS            =  (1 << 17),
   SL_FLAG_SETTINGS_NETPLAY_OPTIONS                 =  (1 << 18),
   SL_FLAG_SETTINGS_USER_OPTIONS                    =  (1 << 19),
   SL_FLAG_SETTINGS_DIRECTORY_OPTIONS               =  (1 << 20),
   SL_FLAG_SETTINGS_PRIVACY_OPTIONS                 =  (1 << 21),
   SL_FLAG_SETTINGS_PLAYLIST_OPTIONS                =  (1 << 22),
   SL_FLAG_SETTINGS_MENU_BROWSER_OPTIONS            =  (1 << 23),
   SL_FLAG_SETTINGS_PATCH_OPTIONS                   =  (1 << 24),
   SL_FLAG_SETTINGS_RECORDING_OPTIONS               =  (1 << 25),
   SL_FLAG_SETTINGS_FRAME_THROTTLE_OPTIONS          =  (1 << 26),
   SL_FLAG_SETTINGS_LOGGING_OPTIONS                 =  (1 << 27),
   SL_FLAG_SETTINGS_SAVING_OPTIONS                  =  (1 << 28),
   SL_FLAG_SETTINGS_SUB_ACCOUNTS_OPTIONS            =  (1 << 29),
   SL_FLAG_SETTINGS_ALL                             =  (1 << 30)
};

#define SL_FLAG_SETTINGS_GROUP_ALL (SL_FLAG_SETTINGS_ALL - SL_FLAG_MAIN_MENU)

int menu_setting_generic(rarch_setting_t *setting, size_t idx, bool wraparound);

int menu_setting_set(unsigned type, unsigned action, bool wraparound);

/**
 * menu_setting_find:
 * @name               : name of setting to search for
 *
 * Search for a setting with a specified name (@name).
 *
 * Returns: pointer to setting if found, NULL otherwise.
 **/
rarch_setting_t *menu_setting_find(const char *label);

rarch_setting_t *menu_setting_find_enum(enum msg_hash_enums enum_idx);

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action, bool wraparound);

enum setting_type menu_setting_get_browser_selection_type(
      rarch_setting_t *setting);

void setting_generic_handle_change(rarch_setting_t *setting);

/**
 * menu_setting_new:
 * @mask               : Bitmask of settings to include.
 *
 * Request a list of settings based on @mask.
 *
 * Returns: settings list composed of all requested
 * settings on success, otherwise NULL.
 **/
rarch_setting_t *menu_setting_new(void);

void menu_setting_free(rarch_setting_t *setting);

RETRO_END_DECLS

#endif
