/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Michael Lelli
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

#include <stdlib.h>

typedef struct rwebinput_state
{
   char keys[32];
   int mouse_x;
   int mouse_y;
   char mouse_l;
   char mouse_r;
} rwebinput_state_t;

int RWebInputInit(void);
rwebinput_state_t *RWebInputPoll(int context);
void RWebInputDestroy(int context);
