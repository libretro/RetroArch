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

#ifndef __INPUT_TYPES__H
#define __INPUT_TYPES__H

typedef struct rarch_joypad_driver input_device_driver_t;
typedef struct input_keyboard_line input_keyboard_line_t;
typedef struct rarch_joypad_info rarch_joypad_info_t;
typedef struct input_driver input_driver_t;
typedef struct input_keyboard_ctx_wait input_keyboard_ctx_wait_t;
typedef struct {
   uint32_t data[8];
   uint16_t analogs[8];
   uint16_t analog_buttons[16];
} input_bits_t;
typedef struct joypad_connection joypad_connection_t;
typedef struct pad_connection_listener_interface pad_connection_listener_t;

#endif /* __INPUT_TYPES__H */
