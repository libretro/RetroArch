/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_driver.h"

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct ctr_input
{
   void *empty;
} ctr_input_t;

static int16_t ctr_input_state(
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
   if (port >= MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            return joypad->state(
                  joypad_info, binds[port], port);

         if (id < RARCH_BIND_LIST_END)
            if (binds[port][id].valid)
               if (button_is_pressed(
                        joypad, joypad_info, binds[port], port, id))
                  return 1;
         break;
      case RETRO_DEVICE_ANALOG:
         break;
   }

   return 0;
}

static void ctr_input_free_input(void *data)
{
   free(data);
}

static void* ctr_input_init(const char *joypad_driver)
{
   ctr_input_t *ctr = (ctr_input_t*)calloc(1, sizeof(*ctr));
   if (!ctr)
      return NULL;

   return ctr;
}

static uint64_t ctr_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_ctr = {
   ctr_input_init,
   NULL,                         /* poll */
   ctr_input_state,
   ctr_input_free_input,
   NULL,
   NULL,
   ctr_input_get_capabilities,
   "ctr",
   NULL,                         /* grab_mouse */
   NULL
};
