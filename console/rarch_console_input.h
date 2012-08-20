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
   DPAD_EMULATION_NONE = 0,
   DPAD_EMULATION_LSTICK,
   DPAD_EMULATION_RSTICK
};

enum
{
   RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_LEFT = RARCH_FIRST_ANALOG_BIND,
   RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_ANALOG_LEFT_DPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_ANALOG_RIGHT_DPAD_DOWN,
};

extern uint64_t rarch_default_keybind_lut[RARCH_FIRST_META_KEY];
extern char rarch_default_libretro_keybind_name_lut[RARCH_FIRST_META_KEY][32];

const char *rarch_input_find_platform_key_label(uint64_t joykey);

void rarch_input_set_default_keybinds(unsigned player);

void rarch_input_set_keybind(unsigned player, unsigned keybind_action, uint64_t default_retro_joypad_id);

void rarch_input_set_controls_default (const input_driver_t *input);
const char *rarch_input_get_default_keybind_name (unsigned id);

#endif
