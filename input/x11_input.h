/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef X11_INPUT_H__
#define X11_INPUT_H__

#include "../driver.h"

#include "SDL.h"
#include "../boolean.h"
#include "../general.h"
#include <stdint.h>
#include <stdlib.h>
#include "rarch_sdl_input.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

typedef struct x11_input
{
   sdl_input_t *sdl;

   bool inherit_disp;
   Display *display;
   Window win;

   char state[32];
   bool mouse_l, mouse_r, mouse_m;
   int mouse_x, mouse_y;
   int mouse_last_x, mouse_last_y;
} x11_input_t;

void x_input_set_disp_win(x11_input_t *x11, Display *dpy, Window win);

#endif

