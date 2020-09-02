/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Francisco Javier Trujillo Mata - fjtrujy
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include "../input_driver.h"

typedef struct ps2_input
{
   void *empty;
} ps2_input_t;

static int16_t ps2_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind **binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return joypad->state(
                  joypad_info, binds[port], port);

         if (binds[port][id].valid)
            if (button_is_pressed(joypad, joypad_info, binds[port],
                     port, id))
               return 1;
         break;
      case RETRO_DEVICE_ANALOG:
         break;
   }

   return 0;
}

static void ps2_input_free_input(void *data)
{
   free(data);
}

static void* ps2_input_initialize(const char *joypad_driver)
{
   ps2_input_t *ps2 = (ps2_input_t*)calloc(1, sizeof(*ps2));
   if (!ps2)
      return NULL;

   return ps2;
}

static uint64_t ps2_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_ps2 = {
   ps2_input_initialize,
   NULL,                         /* poll */
   ps2_input_state,
   ps2_input_free_input,
   NULL,
   NULL,
   ps2_input_get_capabilities,
   "ps2",
   NULL,                         /* grab_mouse */
   NULL 
};
