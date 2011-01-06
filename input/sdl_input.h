/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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

#ifndef __SSNES_SDL_INPUT_H
#define __SSNES_SDL_INPUT_H

#include <SDL/SDL.h>
typedef struct sdl_input
{
   SDL_Joystick *joysticks[2];
   unsigned num_axes[2];
   unsigned num_buttons[2];
   unsigned num_joysticks;

   // A video driver could pre-init with the SDL driver and have it handle resizing events...
   bool *quitting;
   bool *should_resize;
   unsigned *new_width;
   unsigned *new_height;
} sdl_input_t;

#endif
