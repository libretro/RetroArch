/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "keyboard_event_android.h"

#define AKEYCODE_ASSIST 219

#define LAST_KEYCODE AKEYCODE_ASSIST 

#define MAX_KEYS ((LAST_KEYCODE + 7) / 8)

static uint8_t android_key_state[MAX_PADS][MAX_KEYS];

bool android_keyboard_input_pressed(unsigned key)
{
   return BIT_GET(android_key_state[0], key);
}

uint8_t *android_keyboard_state_get(unsigned port)
{
   return android_key_state[port];
}

void android_keyboard_free(void)
{
   unsigned i, j;

   for (i = 0; i < MAX_PADS; i++)
      for (j = 0; j < MAX_KEYS; j++)
         android_key_state[i][j] = 0;
}
