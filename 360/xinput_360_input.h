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

#define DEADZONE                            (16000)

extern void xdk360_input_map_dpad_to_stick(uint32_t map_dpad_enum, uint32_t controller_id);

#endif
