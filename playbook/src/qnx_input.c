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

#include "../../general.h"
#include "../../driver.h"

static void *qnx_input_init(void)
{
   return (void*)-1;
}

static void qnx_input_poll(void *data)
{
   (void)data;
}

static int16_t qnx_input_state(void *data, const struct retro_keybind **retro_keybinds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   (void)data;
   (void)retro_keybinds;
   (void)port;
   (void)device;
   (void)index;
   (void)id;

   return 0;
}

static bool qnx_input_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;

   return false;
}

static void qnx_input_free_input(void *data)
{
   (void)data;
}

static void qnx_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   (void)data;
   (void)device;
   (void)port;
   (void)id;
   (void)keybind_action;
}

const input_driver_t input_qnx = {
	qnx_input_init,
	qnx_input_poll,
	qnx_input_state,
	qnx_input_key_pressed,
	qnx_input_free_input,
	qnx_set_keybinds,
    "qnx_input",
};

