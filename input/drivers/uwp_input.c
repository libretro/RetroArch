/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2019 - Krzysztof Haładyn
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

#include <uwp/uwp_func.h>

#include "../input_driver.h"

/* TODO: Add support for multiple mice and multiple touch */

static void uwp_input_free_input(void *data) { }
static void *uwp_input_init(const char *a)
{
   input_keymaps_init_keyboard_lut(rarch_key_map_uwp);

   return (void*)-1;
}

static uint64_t uwp_input_get_capabilities(void *data)
{
   return
           (1 << RETRO_DEVICE_JOYPAD)
         | (1 << RETRO_DEVICE_MOUSE)
         | (1 << RETRO_DEVICE_KEYBOARD)
         | (1 << RETRO_DEVICE_POINTER)
         | (1 << RETRO_DEVICE_ANALOG);
}

static int16_t uwp_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned index,
      unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (     
                           ((binds[port][i].key < RETROK_LAST) 
                            && uwp_keyboard_pressed(binds[port][i].key))
                        )
                        ret |= (1 << i);
                  }
               }
            }

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
               {
                  if (uwp_mouse_state(port,
                           binds[port][i].mbutton, false))
                     ret |= (1 << i);
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if ((binds[port][id].key < RETROK_LAST) 
                     && uwp_keyboard_pressed(binds[port][id].key)
                     && ((id == RARCH_GAME_FOCUS_TOGGLE) || 
                        !keyboard_mapping_blocked)
                     )
                  return 1;
               else if (uwp_mouse_state(port,
                        binds[port][id].mbutton, false))
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            int16_t ret           = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(index, id, id_minus, id_plus);

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key < RETROK_LAST)
            {
               if (uwp_keyboard_pressed(id_plus_key))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key < RETROK_LAST)
            {
               if (uwp_keyboard_pressed(id_minus_key))
                  ret += -0x7fff;
            }

            return ret;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && uwp_keyboard_pressed(id);

      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         return uwp_mouse_state(port, id, device == RARCH_DEVICE_MOUSE_SCREEN);

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return uwp_pointer_state(index, id, device == RARCH_DEVICE_POINTER_SCREEN);
   }

   return 0;
}

input_driver_t input_uwp = {
   uwp_input_init,
   uwp_input_next_frame,         /* poll       */
   uwp_input_state,
   uwp_input_free_input,
   NULL,
   NULL,
   uwp_input_get_capabilities,
   "uwp",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
