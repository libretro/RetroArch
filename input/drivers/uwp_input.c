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
   const input_device_driver_t *joypad;
} uwp_input_t;

static void uwp_input_poll(void *data)
{
   uwp_input_t *uwp = (uwp_input_t*)data;

   if (uwp && uwp->joypad)
      uwp->joypad->poll();

   uwp_input_next_frame();
}

static void uwp_input_free_input(void *data)
{
   uwp_input_t *uwp = (uwp_input_t*)data;

   if (!uwp)
      return;

   if (uwp->joypad)
      uwp->joypad->destroy();

   free(uwp);
}

static void *uwp_input_init(const char *joypad_driver)
{
   uwp_input_t *uwp     = (uwp_input_t*)calloc(1, sizeof(*uwp));
   if (!uwp)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_uwp);

   uwp->joypad = input_joypad_init_driver(joypad_driver, uwp);

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

static bool uwp_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   struct uwp_input *uwp = (struct uwp_input*)data;
   if (!uwp)
      return false;
   return input_joypad_set_rumble(uwp->joypad, port, effect, strength);
}

static const input_device_driver_t *uwp_input_get_joypad_driver(void *data)
{
   uwp_input_t *uwp = (uwp_input_t*)data;
   if (!uwp)
      return NULL;
   return uwp->joypad;
}

static void uwp_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool uwp_pressed_joypad(uwp_input_t *uwp,
   rarch_joypad_info_t joypad_info,
   const struct retro_keybind *binds,
   unsigned port, unsigned id)
{
   const struct retro_keybind *bind = &binds[id];

   /* First, process the keyboard bindings */
   if ((bind->key < RETROK_LAST) && uwp_keyboard_pressed(bind->key))
      if ((id == RARCH_GAME_FOCUS_TOGGLE) || !input_uwp.keyboard_mapping_blocked)
         return true;

   /* Then, process the joypad bindings */
   if (binds && binds[id].valid)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[id].joykey != NO_BTN)
         ? binds[id].joykey : joypad_info.auto_binds[id].joykey;
      const uint32_t joyaxis = (binds[id].joyaxis != AXIS_NONE)
         ? binds[id].joyaxis : joypad_info.auto_binds[id].joyaxis;

      if (uwp_mouse_state(port, bind->mbutton, false))
         return true;
      if ((uint16_t)joykey != NO_BTN && uwp->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
         return true;
      if (((float)abs(uwp->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
         return true;
   }

   return false;
}

static int16_t uwp_pressed_analog(uwp_input_t *uwp,
   rarch_joypad_info_t joypad_info,
   const struct retro_keybind *binds,
   unsigned port, unsigned idx, unsigned id)
{
   const struct retro_keybind *bind_minus, *bind_plus;
   int16_t pressed_minus = 0, pressed_plus = 0, pressed_keyboard;
   unsigned id_minus = 0, id_plus = 0;

   /* First, process the keyboard bindings */
   input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

   bind_minus = &binds[id_minus];
   bind_plus = &binds[id_plus];

   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   if ((bind_minus->key < RETROK_LAST) && uwp_keyboard_pressed(bind_minus->key))
      pressed_minus = -0x7fff;
   if ((bind_plus->key < RETROK_LAST) && uwp_keyboard_pressed(bind_plus->key))
      pressed_plus = 0x7fff;

   pressed_keyboard = pressed_plus + pressed_minus;
   if (pressed_keyboard != 0)
      return pressed_keyboard;

   /* Then, process the joypad bindings */
   return input_joypad_analog(uwp->joypad, joypad_info, port, idx, id, binds);
}

static int16_t uwp_input_state(void *data,
      rarch_joypad_info_t joypad_info,
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
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (uwp_pressed_joypad(
                        uwp, joypad_info, binds[port], port, i))
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
         {
            if (id < RARCH_BIND_LIST_END)
               if (uwp_pressed_joypad(uwp, joypad_info, binds[port], port, id))
                  return true;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return uwp_pressed_analog(uwp, joypad_info, binds[port], port, index, id);
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
   uwp_input_grab_mouse,
   NULL,
   uwp_input_set_rumble,
   uwp_input_get_joypad_driver,
   NULL,
   false
};
