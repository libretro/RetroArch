/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "../general.h"
#include "../driver.h"

static void *nullinput_input_init(void)
{
   return (void*)-1;
}

static void nullinput_input_poll(void *data)
{
   (void)data;
}

static int16_t nullinput_input_state(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)data;
   (void)retro_keybinds;
   (void)port;
   (void)device;
   (void)index;
   (void)id;

   return 0;
}

static bool nullinput_input_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;

   return false;
}

static void nullinput_input_free_input(void *data)
{
   (void)data;
}

#ifdef RARCH_CONSOLE
static void nullinput_set_default_keybind_lut(unsigned device, unsigned port)
{
   (void)device;
   (void)port;
}

static void nullinput_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;
   (void)map_dpad_enum;
   (void)controller_id;
}

static void nullinput_input_post_init(void) {}
#endif

const input_driver_t input_null = {
   nullinput_input_init,
   nullinput_input_poll,
   nullinput_input_state,
   nullinput_input_key_pressed,
   nullinput_input_free_input,
#ifdef RARCH_CONSOLE
   nullinput_set_default_keybind_lut,
   nullinput_set_analog_dpad_mapping,
   nullinput_input_post_init,
   2,
#endif
   "null",
};

