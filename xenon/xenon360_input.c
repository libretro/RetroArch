/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "../driver.h"
#include "../libretro.h"

#include <input/input.h>
#include <usb/usbmain.h>

#define MAX_PADS 4

static uint64_t state[MAX_PADS];

static void xenon360_input_poll(void *data)
{
   (void)data;
   for (unsigned i = 0; i < MAX_PADS; i++)
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

static int16_t xenon360_input_state(void *data, const struct retro_keybind **binds,
      bool port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   (void)index;
   unsigned player = port;
   uint64_t button = binds[player][id].joykey;
   int16_t retval = 0;

   if(player < MAX_PADS)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            retval = (state[player] & button) ? 1 : 0;
            break;
      }
   }

   return retval;
}

static void xenon360_input_free_input(void *data)
{
   (void)data;
}

static void xenon360_input_set_default_keybinds(unsigned device, unsigned port, unsigned id)
{
   (void)device;
   (void)id;

   for (unsigned i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
   {
      g_settings.input.binds[port][i].id = i;
      g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
   }

   g_settings.input.dpad_emulation[port] = DPAD_EMULATION_LSTICK;
}

static void xenon360_input_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;
   (void)map_dpad_enum;
   (void)controller_id;
}

static void* xenon360_input_init(void)
{
   for(unsigned i = 0; i < MAX_PLAYERS; i++)
      xenon360_input_set_default_keybinds(0, i, 0);

   for(unsigned i = 0; i < MAX_PADS; i++)
      xenon360_input_set_analog_dpad_mapping(0, g_settings.input.dpad_emulation[i], i);

   return (void*)-1;
}

static bool xenon360_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key));
}


const input_driver_t input_xenon360 = {
   .init = xenon360_input_init,
   .poll = xenon360_input_poll,
   .input_state = xenon360_input_state,
   .key_pressed = xenon360_input_key_pressed,
   .free = xenon360_input_free_input,
   .set_default_keybinds = xenon360_input_set_default_keybinds,
   .set_analog_dpad_mapping = xenon360_input_set_analog_dpad_mapping,
   .ident = "xenon360",
};
