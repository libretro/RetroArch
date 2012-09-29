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

#include "input_common.h"
#include <string.h>
#include <stdlib.h>

#include "../general.h"
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

static const rarch_joypad_driver_t *joypad_drivers[] = {
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#ifdef HAVE_SDL
   &sdl_joypad,
#endif
#ifdef __linux
   &linuxraw_joypad,
#endif
};

const rarch_joypad_driver_t *input_joypad_find_driver(const char *ident)
{
   for (unsigned i = 0; i < sizeof(joypad_drivers) / sizeof(joypad_drivers[0]); i++)
   {
      if (strcmp(ident, joypad_drivers[i]->ident) == 0)
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n", joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

const rarch_joypad_driver_t *input_joypad_init_first(void)
{
   for (unsigned i = 0; i < sizeof(joypad_drivers) / sizeof(joypad_drivers[0]); i++)
   {
      if (joypad_drivers[i]->init())
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n", joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

void input_joypad_poll(const rarch_joypad_driver_t *driver)
{
   if (driver)
      driver->poll();
}

bool input_joypad_pressed(const rarch_joypad_driver_t *driver,
      unsigned port, const struct retro_keybind *key)
{
   if (!driver)
      return false;

   int joy_index = g_settings.input.joypad_map[port];
   if (joy_index < 0 || joy_index >= MAX_PLAYERS)
      return false;

   if (!key->valid)
      return false;

   if (driver->button(joy_index, key->joykey))
      return true;

   int16_t axis = driver->axis(joy_index, key->joyaxis);
   float scaled_axis = (float)abs(axis) / 0x8000;
   return scaled_axis > g_settings.input.axis_threshold;
}

int16_t input_joypad_analog(const rarch_joypad_driver_t *driver,
      unsigned port, unsigned index, unsigned id, const struct retro_keybind *binds)
{
   if (!driver)
      return 0;

   int joy_index = g_settings.input.joypad_map[port];
   if (joy_index < 0)
      return 0;

   unsigned id_minus = 0;
   unsigned id_plus  = 0;
   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   const struct retro_keybind *bind_minus = &binds[id_minus];
   const struct retro_keybind *bind_plus  = &binds[id_plus];
   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   int16_t pressed_minus = abs(driver->axis(joy_index, bind_minus->joyaxis));
   int16_t pressed_plus  = abs(driver->axis(joy_index, bind_plus->joyaxis));

   int16_t res = pressed_plus - pressed_minus;

   if (res != 0)
      return res;

   int16_t digital_left  = driver->button(joy_index, bind_minus->joykey) ? -0x7fff : 0;
   int16_t digital_right = driver->button(joy_index, bind_plus->joykey)  ?  0x7fff : 0;
   return digital_right + digital_left;
}

int16_t input_joypad_axis_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned axis)
{
   if (!driver)
      return 0;

   return driver->axis(joypad, AXIS_POS(axis)) + driver->axis(joypad, AXIS_NEG(axis));
}

bool input_joypad_button_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned button)
{
   if (!driver)
      return false;

   return driver->button(joypad, button);
}

bool input_joypad_hat_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned hat_dir, unsigned hat)
{
   if (!driver)
      return false;

   return driver->button(joypad, HAT_MAP(hat, hat_dir));
}

