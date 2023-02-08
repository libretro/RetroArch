/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdint.h>
#include <stdlib.h>

#include <retro_miscellaneous.h>

#include "../input_driver.h"
#include "../input_keymaps.h"
#include "../drivers_keyboard/keyboard_event_dos.h"

#define MAX_KEYS LAST_KEYCODE + 1

/* TODO/FIXME -
 * fix game focus toggle */

/* First ports are used to keeping track of gamepad states. Last port is used for keyboard state */
/* TODO/FIXME - static globals */
static uint16_t dos_key_state[DEFAULT_MAX_PADS+1][MAX_KEYS];

uint16_t *dos_keyboard_state_get(unsigned port)
{
   return dos_key_state[port];
}

static void dos_keyboard_free(void)
{
   unsigned i, j;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
      for (j = 0; j < MAX_KEYS; j++)
         dos_key_state[i][j] = 0;
}

static int16_t dos_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   if (port > 0)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
               {
                  if (id < RARCH_BIND_LIST_END)
                     if (dos_key_state[DOS_KEYBOARD_PORT]
                           [rarch_keysym_lut[binds[port][i].key]])
                        ret |= (1 << i);
               }
            }

            return ret;
         }

         if (binds[port][id].valid)
         {
            if (
                  (id < RARCH_BIND_LIST_END
                   && dos_key_state[DOS_KEYBOARD_PORT]
                   [rarch_keysym_lut[binds[port][id].key]])
               )
               return 1;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         if (id < RARCH_BIND_LIST_END)
            return (dos_key_state[DOS_KEYBOARD_PORT]
                  [rarch_keysym_lut[binds[port][id].key]]);
         break;
   }

   return 0;
}

static void dos_input_free_input(void *data)
{
   dos_keyboard_free();
}

static void* dos_input_init(const char *joypad_driver)
{
   input_keymaps_init_keyboard_lut(rarch_key_map_dos);

   return (void*)-1;
}

static uint64_t dos_input_get_capabilities(void *data)
{
   return UINT64_C(1) << RETRO_DEVICE_JOYPAD;
}

input_driver_t input_dos = {
   dos_input_init,
   NULL,                         /* poll */
   dos_input_state,
   dos_input_free_input,
   NULL,
   NULL,
   dos_input_get_capabilities,
   "dos",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
