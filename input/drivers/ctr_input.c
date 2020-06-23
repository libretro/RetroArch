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
   const input_device_driver_t *joypad;
} ctr_input_t;

static void ctr_input_poll(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;

   if (ctr->joypad)
      ctr->joypad->poll();
}

static int16_t ctr_input_state(void *data,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   ctr_input_t *ctr                   = (ctr_input_t*)data;

   if (port >= MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS)
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
                  if (button_is_pressed(
                           ctr->joypad, joypad_info, binds[port], port, i))
                     ret |= (1 << i);
               }
            }

            return ret;
         }
         else
         {
            if (id < RARCH_BIND_LIST_END)
            {
               if (binds[port][id].valid)
               {
                  if (button_is_pressed(
                        ctr->joypad, joypad_info, binds[port], port, id))
                     return 1;
               }
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         break;
   }

   return 0;
}

static void ctr_input_free_input(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;

   if (ctr && ctr->joypad)
      ctr->joypad->destroy();

   free(data);
}

static void* ctr_input_init(const char *joypad_driver)
{
   ctr_input_t *ctr = (ctr_input_t*)calloc(1, sizeof(*ctr));
   if (!ctr)
      return NULL;

   ctr->joypad = input_joypad_init_driver(joypad_driver, ctr);

   return ctr;
}

static uint64_t ctr_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

static const input_device_driver_t *ctr_input_get_joypad_driver(void *data)
{
   ctr_input_t *ctr = (ctr_input_t*)data;
   if (ctr)
      return ctr->joypad;
   return NULL;
}

static void ctr_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool ctr_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

input_driver_t input_ctr = {
   ctr_input_init,
   ctr_input_poll,
   ctr_input_state,
   ctr_input_free_input,
   NULL,
   NULL,
   ctr_input_get_capabilities,
   "ctr",
   ctr_input_grab_mouse,
   NULL,
   ctr_input_set_rumble,
   ctr_input_get_joypad_driver,
   NULL,
   false
};
