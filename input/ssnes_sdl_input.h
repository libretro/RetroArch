/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef __SSNES_SDL_INPUT_H
#define __SSNES_SDL_INPUT_H

#include "SDL.h"
#include "../general.h"

#ifdef HAVE_DINPUT
#include "ssnes_dinput.h"
#endif

typedef struct sdl_input
{
#ifdef HAVE_DINPUT
   sdl_dinput_t *di;
#else
   SDL_Joystick *joysticks[MAX_PLAYERS];
   unsigned num_axes[MAX_PLAYERS];
   unsigned num_buttons[MAX_PLAYERS];
   unsigned num_hats[MAX_PLAYERS];
   unsigned num_joysticks;
#endif

   bool use_keyboard;

   int16_t mouse_x, mouse_y;
   int16_t mouse_l, mouse_r, mouse_m;
} sdl_input_t;

#endif
