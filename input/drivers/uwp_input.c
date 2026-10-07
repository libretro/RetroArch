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

/* Which of @keys are down: bit n of @down for keys[n]. */
static void uwp_keys_down(void *data, unsigned port,
      const uint16_t *keys, const uint8_t *bind, unsigned count,
      uint32_t *down)
{
   unsigned i;
   (void)data;
   (void)port;
   (void)bind;
   for (i = 0; i < count; i++)
      if (uwp_keyboard_pressed(keys[i]))
         down[i >> 5] |= (1u << (i & 31));
}

/* What the port's mouse is holding, for the controls bound to its
 * buttons. This driver does not hand its mice to the frontend. */
static unsigned uwp_bind_mouse_buttons(void *data, unsigned port)
{
   unsigned held = 0;
   (void)data;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_LEFT, false))
      held |= INPUT_POINTER_LEFT;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_RIGHT, false))
      held |= INPUT_POINTER_RIGHT;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_MIDDLE, false))
      held |= INPUT_POINTER_MIDDLE;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_BUTTON_4, false))
      held |= INPUT_POINTER_BUTTON_4;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_BUTTON_5, false))
      held |= INPUT_POINTER_BUTTON_5;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_WHEELUP, false))
      held |= INPUT_POINTER_WHEEL_UP;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_WHEELDOWN, false))
      held |= INPUT_POINTER_WHEEL_DOWN;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP, false))
      held |= INPUT_POINTER_HWHEEL_UP;
   if (uwp_mouse_state(port, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN, false))
      held |= INPUT_POINTER_HWHEEL_DOWN;
   return held;
}

static int16_t uwp_input_state(
      void *data,
      const input_device_driver_t *joypad,
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
      /* The RetroPad's buttons, the hotkeys and a stick's axes, where
       * they are bound to keys or mouse buttons, are the frontend's to
       * answer: it asks uwp_keys_down() for the keys once a poll. */
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && uwp_keyboard_pressed(id);
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
   NULL,
   NULL,                         /* survives_video */
   uwp_keys_down,
   uwp_bind_mouse_buttons
};
