/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
   S_DELAY_0 = 0,
   S_DELAY_1 = 1,
   S_DELAY_45 = 45,
   S_DELAY_90  = 90,
   S_DELAY_180 = 180,
   S_DELAY_270 = 270
};

enum
{
   S_ASPECT_RATIO_DECREMENT = 0,
   S_ASPECT_RATIO_INCREMENT,
   S_AUDIO_MUTE,
   S_AUDIO_CONTROL_RATE_DECREMENT,
   S_AUDIO_CONTROL_RATE_INCREMENT,
   S_FRAME_ADVANCE,
   S_HW_TEXTURE_FILTER,
   S_HW_TEXTURE_FILTER_2,
   S_OVERSCAN_DECREMENT,
   S_OVERSCAN_INCREMENT,
   S_RESOLUTION_PREVIOUS,
   S_RESOLUTION_NEXT,
   S_ROTATION_DECREMENT,
   S_ROTATION_INCREMENT,
   S_REWIND,
   S_SAVESTATE_DECREMENT,
   S_SAVESTATE_INCREMENT,
   S_SCALE_ENABLED,
   S_SCALE_FACTOR_DECREMENT,
   S_SCALE_FACTOR_INCREMENT,
   S_THROTTLE,
   S_TRIPLE_BUFFERING
};

enum
{
   S_DEF_ASPECT_RATIO = 0,
   S_DEF_AUDIO_MUTE,
   S_DEF_AUDIO_CONTROL_RATE,
   S_DEF_HW_TEXTURE_FILTER,
   S_DEF_HW_TEXTURE_FILTER_2,
   S_DEF_OVERSCAN,
   S_DEF_ROTATION,
   S_DEF_THROTTLE,
   S_DEF_TRIPLE_BUFFERING,
   S_DEF_SAVE_STATE,
   S_DEF_SCALE_ENABLED,
   S_DEF_SCALE_FACTOR
};

enum
{
   S_MSG_CACHE_PARTITION = 0,
   S_MSG_CHANGE_CONTROLS,
   S_MSG_EXTRACTED_ZIPFILE,
   S_MSG_LOADING_ROM,
   S_MSG_DIR_LOADING_ERROR,
   S_MSG_ROM_LOADING_ERROR,
   S_MSG_NOT_IMPLEMENTED,
   S_MSG_RESIZE_SCREEN,
   S_MSG_RESTART_RARCH,
   S_MSG_SELECT_LIBRETRO_CORE,
   S_MSG_SELECT_SHADER,
   S_MSG_SHADER_LOADING_SUCCEEDED
};

enum
{
   S_LBL_ASPECT_RATIO = 0,
   S_LBL_RARCH_VERSION,
   S_LBL_ROTATION,
   S_LBL_SHADER,
   S_LBL_SHADER_2,
   S_LBL_SCALE_FACTOR,
   S_LBL_LOAD_STATE_SLOT,
   S_LBL_SAVE_STATE_SLOT,
   S_LBL_ZIP_EXTRACT,
};

void rmenu_settings_set(unsigned setting);
void rmenu_settings_set_default(unsigned setting);
void rmenu_settings_msg(unsigned setting, unsigned delay);

void rmenu_settings_create_menu_item_label(char * str, unsigned setting, size_t size);
void rmenu_settings_create_menu_item_label_w(wchar_t *strwbuf, unsigned setting, size_t size);

#endif
