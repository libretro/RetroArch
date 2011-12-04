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

static uint64_t state[5];
static void ps3_input_poll(void *data)
{
   (void)data;
   for (unsigned i = 0; i < 5; i++)
      state[i] = cell_pad_input_poll_device(i);
}

static int16_t ps3_input_state(void *data, const struct snes_keybind **binds,
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

   // Hardcoded binds.
   uint64_t button;
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

   return CTRL_MASK(state[player], button) ? 1 : 0;
}

static void ps3_free_input(void *data)
{
   (void)data;
   cell_pad_input_deinit();
}

static void* ps3_input_init(void)
{
   cell_pad_input_init();
   return (void*)-1;
}

static bool ps3_key_pressed(void *data, int key)
{
   (void)data;
   switch(key)
   {
      case SSNES_FAST_FORWARD_HOLD_KEY:
         return CTRL_RSTICK_UP(state[0]);
      case SSNES_LOAD_STATE_KEY:
         return (CTRL_L2(state[0]) && CTRL_R3(state[0]));
      case SSNES_SAVE_STATE_KEY:
         return (CTRL_R2(state[0]) && CTRL_R3(state[0]));
      case SSNES_STATE_SLOT_PLUS:
         return (CTRL_RSTICK_RIGHT(state[0]) && CTRL_R2(state[0]));
      case SSNES_STATE_SLOT_MINUS:
         return (CTRL_RSTICK_LEFT(state[0]) && CTRL_R2(state[0]));
      case SSNES_REWIND:
         return CTRL_RSTICK_DOWN(state[0]);
      default:
         break;
   }

   return false;
}

const input_driver_t input_ps3 = {
   .init = ps3_input_init,
   .poll = ps3_input_poll,
   .input_state = ps3_input_state,
   .key_pressed = ps3_key_pressed,
   .free = ps3_free_input,
   .ident = "ps3",
};
