/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>
#include "../boolean.h"
#include "../general.h"

// Piggyback joypad driver for SDL.

typedef struct sdl_dinput
{
   HWND hWnd;
   LPDIRECTINPUT8 ctx;
   LPDIRECTINPUTDEVICE8 joypad[MAX_PLAYERS];
   unsigned joypad_cnt;
   DIJOYSTATE2 joy_state[MAX_PLAYERS];
} sdl_dinput_t;

sdl_dinput_t *sdl_dinput_init(void);
void sdl_dinput_free(sdl_dinput_t *di);

bool sdl_dinput_pressed(sdl_dinput_t *di, unsigned port_num, 
      const struct snes_keybind *key);

void sdl_dinput_poll(sdl_dinput_t *di);


#endif
