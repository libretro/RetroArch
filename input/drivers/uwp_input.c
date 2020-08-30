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

// TODO: Add support for multiple mice and multiple touch

typedef struct uwp_input
{
   void *empty;
} uwp_input_t;

static void uwp_input_poll(void *data)
{
   uwp_input_next_frame();
}

static void uwp_input_free_input(void *data)
{
   uwp_input_t *uwp = (uwp_input_t*)data;

   if (!uwp)
      return;

   free(uwp);
}

static void *uwp_input_init(const char *joypad_driver)
{
   uwp_input_t *uwp     = (uwp_input_t*)calloc(1, sizeof(*uwp));
   if (!uwp)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_uwp);

   return uwp;
}

static uint64_t uwp_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static bool uwp_input_set_rumble(
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   if (joypad)
      return input_joypad_set_rumble(joypad, port, effect, strength);
   return false;
}

static int16_t uwp_pressed_analog(uwp_input_t *uwp,
   rarch_joypad_info_t *joypad_info,
   const struct retro_keybind *binds,
   unsigned port, unsigned idx, unsigned id)
{
   const struct retro_keybind *bind_minus, *bind_plus;
   int16_t pressed_minus = 0, pressed_plus = 0;
   unsigned id_minus = 0, id_plus = 0;

   /* First, process the keyboard bindings */
   input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

   bind_minus = &binds[id_minus];
   bind_plus = &binds[id_plus];

   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   if ((bind_minus->key < RETROK_LAST) 
         && uwp_keyboard_pressed(bind_minus->key))
      pressed_minus = -0x7fff;
   if ((bind_plus->key < RETROK_LAST) 
         && uwp_keyboard_pressed(bind_plus->key))
      pressed_plus = 0x7fff;

   return pressed_plus + pressed_minus;
}

static int16_t uwp_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   uwp_input_t *uwp           = (uwp_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = joypad->state(
                  joypad_info, binds[port], port);

            if (!input_uwp.keyboard_mapping_blocked)
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
         else
         {
            if (id < RARCH_BIND_LIST_END)
            {
               if (binds[port][id].valid)
               {
                  if (button_is_pressed(joypad, joypad_info,
                           binds[port], port, id))
                     return 1;
                  else if ((binds[port][id].key < RETROK_LAST) 
                        && uwp_keyboard_pressed(binds[port][id].key)
                        && ((id == RARCH_GAME_FOCUS_TOGGLE) || 
                           !input_uwp.keyboard_mapping_blocked)
                        )
                     return 1;
                  else if (uwp_mouse_state(port,
                           binds[port][id].mbutton, false))
                     return 1;
               }
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return uwp_pressed_analog(uwp, joypad_info, binds[port], port, index, id);
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
   uwp_input_poll,
   uwp_input_state,
   uwp_input_free_input,
   NULL,
   NULL,
   uwp_input_get_capabilities,
   "uwp",
   NULL,                         /* grab_mouse */
   NULL,
   uwp_input_set_rumble,
   false
};
