/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef CONSOLE_SETTINGS_H
#define CONSOLE_SETTINGS_H

enum
{
   S_DELAY_180 = 180,
   S_DELAY_270 = 270,
} delays;

enum
{
   S_FRAME_ADVANCE = 0,
   S_HW_TEXTURE_FILTER,
   S_HW_TEXTURE_FILTER_2,
   S_OVERSCAN_DECREMENT,
   S_OVERSCAN_INCREMENT,
   S_QUIT,
   S_RETURN_TO_DASHBOARD,
   S_RETURN_TO_GAME,
   S_RETURN_TO_LAUNCHER,
   S_RETURN_TO_MENU,
   S_REWIND,
   S_ROTATION_DECREMENT,
   S_ROTATION_INCREMENT,
   S_SAVESTATE_DECREMENT,
   S_SAVESTATE_INCREMENT,
   S_SCALE_ENABLED,
   S_SCALE_FACTOR_DECREMENT,
   S_SCALE_FACTOR_INCREMENT,
   S_THROTTLE,
   S_TRIPLE_BUFFERING,
} changed_settings;

enum
{
   S_DEF_HW_TEXTURE_FILTER = 0,
   S_DEF_HW_TEXTURE_FILTER_2,
   S_DEF_OVERSCAN,
   S_DEF_THROTTLE,
   S_DEF_TRIPLE_BUFFERING,
   S_DEF_SAVE_STATE,
   S_DEF_SCALE_ENABLED,
   S_DEF_SCALE_FACTOR,
} default_settings;

enum
{
   S_MSG_CACHE_PARTITION = 0,
   S_MSG_CHANGE_CONTROLS,
   S_MSG_EXTRACTED_ZIPFILE,
   S_MSG_NOT_IMPLEMENTED,
   S_MSG_RESIZE_SCREEN,
   S_MSG_RESTART_RARCH,
   S_MSG_SELECT_LIBRETRO_CORE,
   S_MSG_SELECT_SHADER,
   S_MSG_SHADER_LOADING_SUCCEEDED,
};

void rarch_settings_change (unsigned setting);
void rarch_settings_default (unsigned setting);
void rarch_settings_msg(unsigned setting, unsigned delay);

#endif
