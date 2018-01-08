/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "../input_driver.h"
#include "../../verbosity.h"

static void *nullinput_input_init(const char *joypad_driver)
{
   RARCH_ERR("Using the null input driver. RetroArch will ignore you.");
   return (void*)-1;
}

static void nullinput_input_poll(void *data)
{
   (void)data;
}

static int16_t nullinput_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **retro_keybinds, unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   (void)data;
   (void)retro_keybinds;
   (void)port;
   (void)device;
   (void)idx;
   (void)id;

   return 0;
}

static void nullinput_input_free_input(void *data)
{
   (void)data;
}

static uint64_t nullinput_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);

   return caps;
}

static bool nullinput_set_sensor_state(void *data,
      unsigned port, enum retro_sensor_action action, unsigned event_rate)
{
   return false;
}

static void nullinput_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool nullinput_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

static bool nullinput_keyboard_mapping_is_blocked(void *data)
{
   (void)data;

   return false;
}


input_driver_t input_null = {
   nullinput_input_init,
   nullinput_input_poll,
   nullinput_input_state,
   nullinput_input_free_input,
   nullinput_set_sensor_state,
   NULL,
   nullinput_get_capabilities,
   "null",
   nullinput_grab_mouse,
   NULL,
   nullinput_set_rumble,
   NULL,
   NULL,
   nullinput_keyboard_mapping_is_blocked,
   NULL,
};
