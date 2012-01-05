/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *  Copyright (C) 2011 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>
#include <xtl.h>
#include "../driver.h"
#include "../libsnes.hpp"

static XINPUT_STATE state[5];

static void xdk360_input_poll(void *data)
{
   (void)data;
   ZeroMemory(&state, sizeof(XINPUT_STATE));

   for (unsigned i = 0; i < 5; i++)
      XInputGetState(i, &state[i]);
}

static int16_t xdk360_input_state(void *data, const struct snes_keybind **binds,
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
         button = XINPUT_GAMEPAD_B;
         break;
      case SNES_DEVICE_ID_JOYPAD_B:
         button = XINPUT_GAMEPAD_A;
         break;
      case SNES_DEVICE_ID_JOYPAD_X:
         button = XINPUT_GAMEPAD_Y;
         break;
      case SNES_DEVICE_ID_JOYPAD_Y:
         button = XINPUT_GAMEPAD_X; 
         break;
      case SNES_DEVICE_ID_JOYPAD_LEFT:
         button = XINPUT_GAMEPAD_DPAD_LEFT;
         break;
      case SNES_DEVICE_ID_JOYPAD_RIGHT:
         button = XINPUT_GAMEPAD_DPAD_RIGHT;
         break;
      case SNES_DEVICE_ID_JOYPAD_UP:
         button = XINPUT_GAMEPAD_DPAD_UP;
         break;
      case SNES_DEVICE_ID_JOYPAD_DOWN:
         button = XINPUT_GAMEPAD_DPAD_DOWN;
         break;
      case SNES_DEVICE_ID_JOYPAD_START:
         button = XINPUT_GAMEPAD_START;
         break;
      case SNES_DEVICE_ID_JOYPAD_SELECT:
         button = XINPUT_GAMEPAD_BACK;
         break;
      case SNES_DEVICE_ID_JOYPAD_L:
         button = XINPUT_GAMEPAD_LEFT_SHOULDER;
         break;
      case SNES_DEVICE_ID_JOYPAD_R:
         button = XINPUT_GAMEPAD_RIGHT_SHOULDER;
         break;
      default:
         button = 0;
   }

   return (state[player].Gamepad.wButtons & button) ? 1 : 0;
}

static void xdk360_free_input(void *data)
{
   (void)data;
}

static void* xdk360_input_init(void)
{
   return (void*)-1;
}

static bool xdk360_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;

   return false;
}

const input_driver_t input_xdk360 = {
	xdk360_input_init,
	xdk360_input_poll,
	xdk360_input_state,
	xdk360_key_pressed,
	xdk360_free_input,
	"xdk360"};
