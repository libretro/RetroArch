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

#ifndef CONSOLE_EXT_INPUT_H__
#define CONSOLE_EXT_INPUT_H__

#include "../driver.h"

enum keybind_set_id
{
   KEYBIND_NOACTION = 0,
   KEYBIND_DECREMENT,
   KEYBIND_INCREMENT,
   KEYBIND_DEFAULT
};

enum
{
   MODE_EMULATION = 0,
   MODE_MENU,
   MODE_EXIT
};

enum
{
   DPAD_EMULATION_NONE = 0,
   DPAD_EMULATION_LSTICK,
   DPAD_EMULATION_RSTICK
};

#ifdef _XBOX
#include "../input/rarch_xinput2.h"
#endif

#if defined(__CELLOS_LV2__)
#include "../ps3/ps3_input.h"
enum ps3_device_id
{
   PS3_DEVICE_ID_JOYPAD_CIRCLE = 0,
   PS3_DEVICE_ID_JOYPAD_CROSS,
   PS3_DEVICE_ID_JOYPAD_TRIANGLE,
   PS3_DEVICE_ID_JOYPAD_SQUARE,
   PS3_DEVICE_ID_JOYPAD_UP,
   PS3_DEVICE_ID_JOYPAD_DOWN,
   PS3_DEVICE_ID_JOYPAD_LEFT,
   PS3_DEVICE_ID_JOYPAD_RIGHT,
   PS3_DEVICE_ID_JOYPAD_SELECT,
   PS3_DEVICE_ID_JOYPAD_START,
   PS3_DEVICE_ID_JOYPAD_L1,
   PS3_DEVICE_ID_JOYPAD_L2,
   PS3_DEVICE_ID_JOYPAD_L3,
   PS3_DEVICE_ID_JOYPAD_R1,
   PS3_DEVICE_ID_JOYPAD_R2,
   PS3_DEVICE_ID_JOYPAD_R3,
   PS3_DEVICE_ID_LSTICK_LEFT,
   PS3_DEVICE_ID_LSTICK_RIGHT,
   PS3_DEVICE_ID_LSTICK_UP,
   PS3_DEVICE_ID_LSTICK_DOWN,
   PS3_DEVICE_ID_LSTICK_LEFT_DPAD,
   PS3_DEVICE_ID_LSTICK_RIGHT_DPAD,
   PS3_DEVICE_ID_LSTICK_UP_DPAD,
   PS3_DEVICE_ID_LSTICK_DOWN_DPAD,
   PS3_DEVICE_ID_RSTICK_LEFT,
   PS3_DEVICE_ID_RSTICK_RIGHT,
   PS3_DEVICE_ID_RSTICK_UP,
   PS3_DEVICE_ID_RSTICK_DOWN,
   PS3_DEVICE_ID_RSTICK_LEFT_DPAD,
   PS3_DEVICE_ID_RSTICK_RIGHT_DPAD,
   PS3_DEVICE_ID_RSTICK_UP_DPAD,
   PS3_DEVICE_ID_RSTICK_DOWN_DPAD,

   RARCH_LAST_PLATFORM_KEY
};

#elif defined(_XBOX)

enum xdk_device_id
{
   XDK_DEVICE_ID_JOYPAD_B = 0,
   XDK_DEVICE_ID_JOYPAD_A,
   XDK_DEVICE_ID_JOYPAD_Y,
   XDK_DEVICE_ID_JOYPAD_X,
   XDK_DEVICE_ID_JOYPAD_UP,
   XDK_DEVICE_ID_JOYPAD_DOWN,
   XDK_DEVICE_ID_JOYPAD_LEFT,
   XDK_DEVICE_ID_JOYPAD_RIGHT,
   XDK_DEVICE_ID_JOYPAD_BACK,
   XDK_DEVICE_ID_JOYPAD_START,
   XDK_DEVICE_ID_JOYPAD_LB,
   XDK_DEVICE_ID_JOYPAD_LEFT_TRIGGER,
   XDK_DEVICE_ID_LSTICK_THUMB,
   XDK_DEVICE_ID_JOYPAD_RB,
   XDK_DEVICE_ID_JOYPAD_RIGHT_TRIGGER,
   XDK_DEVICE_ID_RSTICK_THUMB,
   XDK_DEVICE_ID_LSTICK_LEFT,
   XDK_DEVICE_ID_LSTICK_RIGHT,
   XDK_DEVICE_ID_LSTICK_UP,
   XDK_DEVICE_ID_LSTICK_DOWN,
   XDK_DEVICE_ID_LSTICK_LEFT_DPAD,
   XDK_DEVICE_ID_LSTICK_RIGHT_DPAD,
   XDK_DEVICE_ID_LSTICK_UP_DPAD,
   XDK_DEVICE_ID_LSTICK_DOWN_DPAD,
   XDK_DEVICE_ID_RSTICK_LEFT,
   XDK_DEVICE_ID_RSTICK_RIGHT,
   XDK_DEVICE_ID_RSTICK_UP,
   XDK_DEVICE_ID_RSTICK_DOWN,
   XDK_DEVICE_ID_RSTICK_LEFT_DPAD,
   XDK_DEVICE_ID_RSTICK_RIGHT_DPAD,
   XDK_DEVICE_ID_RSTICK_UP_DPAD,
   XDK_DEVICE_ID_RSTICK_DOWN_DPAD,

   RARCH_LAST_PLATFORM_KEY
};
#elif defined(GEKKO)
#include <ogc/pad.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif
#endif

extern uint64_t rarch_default_keybind_lut[RARCH_FIRST_META_KEY];
extern char rarch_default_libretro_keybind_name_lut[RARCH_FIRST_META_KEY][256];

#endif
