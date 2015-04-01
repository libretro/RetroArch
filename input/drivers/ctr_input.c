/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
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

#include "../../general.h"
#include "../../driver.h"

static void *ctrinput_input_init(void)
{
   return (void*)-1;
}

static void ctrinput_input_poll(void *data)
{
   (void)data;
}

static int16_t ctrinput_input_state(void *data,
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

static bool ctrinput_input_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;

   return false;
}

static void ctrinput_input_free_input(void *data)
{
   (void)data;
}

static uint64_t ctrinput_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);

   return caps;
}

static bool ctrinput_set_sensor_state(void *data,
      unsigned port, enum retro_sensor_action action, unsigned event_rate)
{
   return false;
}

static void ctrinput_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool ctrinput_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

input_driver_t input_ctr = {
   ctrinput_input_init,
   ctrinput_input_poll,
   ctrinput_input_state,
   ctrinput_input_key_pressed,
   ctrinput_input_free_input,
   ctrinput_set_sensor_state,
   NULL,
   ctrinput_get_capabilities,
   "ctr",
   ctrinput_grab_mouse,
   ctrinput_set_rumble,
};
