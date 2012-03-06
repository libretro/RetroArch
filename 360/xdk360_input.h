/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _XDK360_INPUT_H
#define _XDK360_INPUT_H

#define XINPUT_GAMEPAD_LSTICK_LEFT_MASK		(65536)
#define XINPUT_GAMEPAD_LSTICK_RIGHT_MASK	(131072)
#define XINPUT_GAMEPAD_LSTICK_UP_MASK		(262144)
#define XINPUT_GAMEPAD_LSTICK_DOWN_MASK		(524288)
#define XINPUT_GAMEPAD_RSTICK_LEFT_MASK		(1048576)
#define XINPUT_GAMEPAD_RSTICK_RIGHT_MASK	(2097152)
#define XINPUT_GAMEPAD_RSTICK_UP_MASK		(4194304)
#define XINPUT_GAMEPAD_RSTICK_DOWN_MASK		(8388608)
#define XINPUT_GAMEPAD_LEFT_TRIGGER			(16777216)
#define XINPUT_GAMEPAD_RIGHT_TRIGGER		(33554432)
#define DEADZONE							(16000)

extern void xdk360_input_init(void);
extern void xdk360_input_map_dpad_to_stick(uint32_t map_dpad_enum, uint32_t controller_id);

#endif
