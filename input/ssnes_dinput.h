/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

#ifndef __SSNES_DINPUT_H
#define __SSNES_DINPUT_H

#include <dinput.h>

#include "general.h"
typedef struct sdl_dinput
{
   HWND hWnd;

   IDirectInput8 *ctx;
   IDirectInputDevice8 *keyboard;
   uint8_t di_state[256];

   IDirectInputDevice8 *joypad[MAX_PLAYERS];
   unsigned joypad_cnt;
   DIJOYSTATE2 joy_state[MAX_PLAYERS];

   bool *quitting;
   bool *should_resize;
   unsigned *new_width;
   unsigned *new_height;

   int16_t mouse_x, mouse_y, mouse_l, mouse_r, mouse_m;
} sdl_dinput_t;

#endif
