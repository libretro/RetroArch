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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <input/input.h>
#include <usb/usbmain.h>

#include <libretro.h>

#include "../../config.def.h"

#include "../input_driver.h"

/* TODO/FIXME - add joypad driver */

/* TODO/FIXME - static global variable */
static uint64_t state[DEFAULT_MAX_PADS];

static void xenon360_input_poll(void *data)
{
   unsigned i;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      struct controller_data_s pad;
      uint64_t *cur_state;

      usb_do_poll();
      get_controller_data(&pad, i);

      cur_state   = &state[i];

      *cur_state |= pad.b     ? RETRO_DEVICE_ID_JOYPAD_A      : 0;
      *cur_state |= pad.a     ? RETRO_DEVICE_ID_JOYPAD_B      : 0;
      *cur_state |= pad.y     ? RETRO_DEVICE_ID_JOYPAD_X      : 0;
      *cur_state |= pad.x     ? RETRO_DEVICE_ID_JOYPAD_Y      : 0;
      *cur_state |= pad.left  ? RETRO_DEVICE_ID_JOYPAD_LEFT   : 0;
      *cur_state |= pad.right ? RETRO_DEVICE_ID_JOYPAD_RIGHT  : 0;
      *cur_state |= pad.up    ? RETRO_DEVICE_ID_JOYPAD_UP     : 0;
      *cur_state |= pad.down  ? RETRO_DEVICE_ID_JOYPAD_DOWN   : 0;
      *cur_state |= pad.start ? RETRO_DEVICE_ID_JOYPAD_START  : 0;
      *cur_state |= pad.back  ? RETRO_DEVICE_ID_JOYPAD_SELECT : 0;
      *cur_state |= pad.lt    ? RETRO_DEVICE_ID_JOYPAD_L      : 0;
      *cur_state |= pad.rt    ? RETRO_DEVICE_ID_JOYPAD_R      : 0;
   }
}

static void xenon360_input_free_input(void *data) { }
static void* xenon360_input_init(const char *a) { return (void*)-1; }

static uint64_t xenon360_input_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD);
}

input_driver_t input_xenon360 = {
   xenon360_input_init,
   xenon360_input_poll,
   NULL,                            /* input_state */
   xenon360_input_free_input,
   NULL,
   NULL,
   NULL,
   xenon360_input_get_capabilities,
   "xenon360",
   NULL,                            /* grab_mouse */
   NULL
};
