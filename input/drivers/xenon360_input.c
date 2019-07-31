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

/* TODO/FIXME -
 * fix game focus toggle */

static uint64_t state[DEFAULT_MAX_PADS];

static void xenon360_input_poll(void *data)
{
   (void)data;
   for (unsigned i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      struct controller_data_s pad;
      usb_do_poll();
      get_controller_data(&pad, i);

      uint64_t *cur_state = &state[i];

      *cur_state |= pad.b ? RETRO_DEVICE_ID_JOYPAD_A : 0;
      *cur_state |= pad.a ? RETRO_DEVICE_ID_JOYPAD_B : 0;
      *cur_state |= pad.y ? RETRO_DEVICE_ID_JOYPAD_X : 0;
      *cur_state |= pad.x ? RETRO_DEVICE_ID_JOYPAD_Y : 0;
      *cur_state |= pad.left ? RETRO_DEVICE_ID_JOYPAD_LEFT : 0;
      *cur_state |= pad.right ? RETRO_DEVICE_ID_JOYPAD_RIGHT : 0;
      *cur_state |= pad.up ? RETRO_DEVICE_ID_JOYPAD_UP : 0;
      *cur_state |= pad.down ? RETRO_DEVICE_ID_JOYPAD_DOWN : 0;
      *cur_state |= pad.start ? RETRO_DEVICE_ID_JOYPAD_START : 0;
      *cur_state |= pad.back ? RETRO_DEVICE_ID_JOYPAD_SELECT : 0;
      *cur_state |= pad.lt ? RETRO_DEVICE_ID_JOYPAD_L : 0;
      *cur_state |= pad.rt ? RETRO_DEVICE_ID_JOYPAD_R : 0;
   }
}

static int16_t xenon360_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      bool port, unsigned device,
      unsigned idx, unsigned id)
{
   uint64_t button            = binds[port][id].joykey;

   if (port >= DEFAULT_MAX_PADS)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (state[port] & binds[port][i].joykey)
                  ret |= (1 << i);
            }

            return ret;
         }
         else
            if (state[port] & binds[port][id].joykey)
               return true;
         break;
      default:
         break;
   }

   return 0;
}

static void xenon360_input_free_input(void *data)
{
   (void)data;
}

static void* xenon360_input_init(const char *joypad_driver)
{
   return (void*)-1;
}

static uint64_t xenon360_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);

   return caps;
}

static void xenon360_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool xenon360_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

input_driver_t input_xenon360 = {
   xenon360_input_init,
   xenon360_input_poll,
   xenon360_input_state,
   xenon360_input_free_input,
   NULL,
   NULL,
   NULL,
   xenon360_input_get_capabilities,
   "xenon360",
   xenon360_input_grab_mouse,
   NULL,
   xenon360_input_set_rumble,
   NULL,
   false
};
