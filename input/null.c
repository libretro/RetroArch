/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

static void *null_input_init(void)
{
   return (void*)-1;
}

static void null_input_poll(void *data)
{
   (void)data;
}

static int16_t null_input_state(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)data;
   (void)retro_keybinds;
   (void)port;
   (void)device;
   (void)index;
   (void)id;

   return 0;
}

static bool null_input_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;

   return false;
}

static void null_input_free(void *data)
{
   (void)data;
}

#ifdef RARCH_CONSOLE
static void null_set_default_keybind_lut(unsigned device)
{
   (void)device;
}

static void null_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;
   (void)map_dpad_enum;
   (void)controller_id;
}

static void null_input_post_init(void) {}
#endif

const input_driver_t input_null = {
   null_input_init,
   null_input_poll,
   null_input_state,
   null_input_key_pressed,
   null_input_free,
#ifdef RARCH_CONSOLE
   null_set_default_keybind_lut,
   null_set_analog_dpad_mapping,
   null_input_post_init,
   2,
#endif
   "null",
};

