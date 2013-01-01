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

#define MAX_PADS

static struct controller_data_s pad[MAX_PADS];
static void xenon360_input_poll(void *data)
{
   (void)data;
   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      usb_do_poll();
      get_controller_data(&pad[i], i);
   }
}

static int16_t xenon360_input_state(void *data, const struct retro_keybind **binds,
      bool port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   (void)binds;
   (void)index;

   if (device != SNES_DEVICE_JOYPAD)
      return 0;

   unsigned player = 0;
   if (port == SNES_PORT_2 && device == SNES_DEVICE_MULTITAP)
      player = index + 1;
   else if (port == SNES_PORT_2)
      player = 1;

   bool button;

   // Hardcoded binds.
   switch (id)
   {
      case SNES_DEVICE_ID_JOYPAD_A:
         button = pad[player].b;
         break;
      case SNES_DEVICE_ID_JOYPAD_B:
         button = pad[player].a;
         break;
      case SNES_DEVICE_ID_JOYPAD_X:
         button = pad[player].y;
         break;
      case SNES_DEVICE_ID_JOYPAD_Y:
         button = pad[player].x;
         break;
      case SNES_DEVICE_ID_JOYPAD_LEFT:
         button = pad[player].left;
         break;
      case SNES_DEVICE_ID_JOYPAD_RIGHT:
         button = pad[player].right;
         break;
      case SNES_DEVICE_ID_JOYPAD_UP:
         button = pad[player].up;
         break;
      case SNES_DEVICE_ID_JOYPAD_DOWN:
         button = pad[player].down;
         break;
      case SNES_DEVICE_ID_JOYPAD_START:
         button = pad[player].start;
         break;
      case SNES_DEVICE_ID_JOYPAD_SELECT:
         button = pad[player].select;
         break;
      case SNES_DEVICE_ID_JOYPAD_L:
         button = pad[player].lt;
         break;
      case SNES_DEVICE_ID_JOYPAD_R:
         button = pad[player].rt;
         break;
      default:
         button = false;
         break;
   }

   return button;
}

static void xenon360_free_input(void *data)
{
   (void)data;
}

static void* xenon360_input_init(void)
{
   return (void*)-1;
}

static bool xenon360_key_pressed(void *data, int key)
{
   (void)data;
   return false;
}

static void xenon360_input_set_default_keybind_lut(unsigned device, unsigned port)
{
   (void)device;
   (void)port;
}

static void xenon360_input_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;
   (void)map_dpad_enum;
   (void)controller_id;
}

const input_driver_t input_xenon360 = {
   .init = xenon360_input_init,
   .poll = xenon360_input_poll,
   .input_state = xenon360_input_state,
   .key_pressed = xenon360_key_pressed,
   .free = xenon360_free_input,
   .set_default_keybind_lut = xenon360_input_set_default_keybind_lut,
   .set_analog_dpad_mapping = xenon360_input_set_analog_dpad_mapping,
   .max_pads = MAX_PADS,
   .ident = "xenon360",
};

