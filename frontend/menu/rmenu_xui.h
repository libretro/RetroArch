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

#ifndef _RMENU_XUI_H_
#define _RMENU_XUI_H_

#include "menu_common.h"

enum
{
   SETTING_EMU_REWIND_ENABLED = 0,
   SETTING_EMU_REWIND_GRANULARITY,
   SETTING_EMU_SHOW_INFO_MSG,
   SETTING_EMU_SHOW_DEBUG_INFO_MSG,
   SETTING_GAMMA_CORRECTION_ENABLED,
   SETTING_HW_TEXTURE_FILTER,
   SETTING_ENABLE_SRAM_PATH,
   SETTING_ENABLE_STATE_PATH,
};

enum
{
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_B = 0,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_Y,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_SELECT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_START,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_UP,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_DOWN,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_LEFT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_RIGHT,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_A,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_X,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L2,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R2,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_L3,
   SETTING_CONTROLS_RETRO_DEVICE_ID_JOYPAD_R3,
   SETTING_CONTROLS_DPAD_EMULATION,
   SETTING_CONTROLS_DEFAULT_ALL
};

enum
{
   INPUT_LOOP_NONE = 0,
   INPUT_LOOP_MENU,
   INPUT_LOOP_RESIZE_MODE,
   INPUT_LOOP_FILEBROWSER
};

bool menu_iterate_xui(void);

#endif
