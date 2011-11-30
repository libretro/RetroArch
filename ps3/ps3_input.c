/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "../driver.h"
#include "pad_input.h"
#include <stdint.h>
#include "../libsnes.hpp"

#include "gl_debug.h"

#include <stdlib.h>

static uint64_t state = 0;

static void ps3_input_poll(void *data)
{
   (void)data;
   state = cell_pad_input_poll_device(0);
}

static int16_t ps3_input_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   (void)data;
   (void)binds; // Hardcoded binds
   (void)index;

   if (device != SNES_DEVICE_JOYPAD)
      return 0;
   if (port == SNES_PORT_2)
      return 0;

   uint64_t button = 0;
   
   switch (id)
   {
      case SNES_DEVICE_ID_JOYPAD_A:
         button = CTRL_CIRCLE_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_B:
         button = CTRL_CROSS_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_X:
         button = CTRL_TRIANGLE_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_Y:
         button = CTRL_SQUARE_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_LEFT:
         button = CTRL_LEFT_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_RIGHT:
         button = CTRL_RIGHT_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_UP:
         button = CTRL_UP_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_DOWN:
         button = CTRL_DOWN_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_START:
         button = CTRL_START_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_SELECT:
         button = CTRL_SELECT_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_L:
         button = CTRL_L1_MASK;
         break;
      case SNES_DEVICE_ID_JOYPAD_R:
         button = CTRL_R1_MASK;
         break;
      default:
         button = 0;
   }

   return CTRL_MASK(state, button) ? 1 : 0;
}

static void ps3_free_input(void *data)
{
   free(data);
   cell_pad_input_deinit();
}

static void* ps3_input_init(void)
{
   cell_pad_input_init();
   return malloc(sizeof(void*));
}

const input_driver_t input_ps3 = {
   .init = ps3_input_init,
   .poll = ps3_input_poll,
   .input_state = ps3_input_state,
   .free = ps3_free_input
};
