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

#ifndef _XDK360_XINPUT2_H
#define _XDK360_XINPUT2_H

enum {
   XINPUT_GAMEPAD_LSTICK_LEFT_MASK         = 1 << 16,
   XINPUT_GAMEPAD_LSTICK_RIGHT_MASK        = 1 << 17,
   XINPUT_GAMEPAD_LSTICK_UP_MASK           = 1 << 18,
   XINPUT_GAMEPAD_LSTICK_DOWN_MASK         = 1 << 19,
   XINPUT_GAMEPAD_RSTICK_LEFT_MASK         = 1 << 20,
   XINPUT_GAMEPAD_RSTICK_RIGHT_MASK        = 1 << 21,
   XINPUT_GAMEPAD_RSTICK_UP_MASK           = 1 << 22,
   XINPUT_GAMEPAD_RSTICK_DOWN_MASK         = 1 << 23,
   XINPUT_GAMEPAD_LEFT_TRIGGER             = 1 << 24,
   XINPUT_GAMEPAD_RIGHT_TRIGGER            = 1 << 25
};

#define DEADZONE                            (16000)

#ifdef _XBOX
extern void xdk360_input_map_dpad_to_stick(uint32_t map_dpad_enum, uint32_t controller_id);
#endif

#endif
