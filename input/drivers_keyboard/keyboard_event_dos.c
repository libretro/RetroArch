/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <retro_miscellaneous.h>

#include "keyboard_event_dos.h"
#include "../input_keymaps.h"

#define MAX_KEYS LAST_KEYCODE + 1

// First ports are used to keeping track of gamepad states. Last port is used for keyboard state
static uint16_t dos_key_state[MAX_PADS+1][MAX_KEYS];

bool dos_keyboard_port_input_pressed(const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      unsigned key = rarch_keysym_lut[bind->key];
      return dos_key_state[DOS_KEYBOARD_PORT][key];
   }
   return false;
}

bool dos_keyboard_input_pressed(unsigned key)
{
   unsigned keysym = rarch_keysym_lut[key];
   return dos_key_state[DOS_KEYBOARD_PORT][keysym];
}

uint16_t *dos_keyboard_state_get(unsigned port)
{
   return dos_key_state[port];
}

void dos_keyboard_free(void)
{
   unsigned i, j;

   for (i = 0; i < MAX_PADS; i++)
      for (j = 0; j < MAX_KEYS; j++)
         dos_key_state[i][j] = 0;
}
