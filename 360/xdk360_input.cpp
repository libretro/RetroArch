/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include "../general.h"
#include "../libsnes.hpp"
#include "xdk360_video.h"
#include "shared.h"

static XINPUT_STATE state[4];
static unsigned pads_connected;

#define DEADZONE (16000)

static void xdk360_input_poll(void *data)
{
   (void)data;

   pads_connected = 0;

   for (unsigned i = 0; i < 4; i++)
   {
      unsigned long retval = XInputGetState(i, &state[i]);
	  pads_connected += (retval == ERROR_DEVICE_NOT_CONNECTED) ? 0 : 1;
   }
}

static int16_t xdk360_input_state(void *data, const struct snes_keybind **binds,
      bool port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   unsigned player;
   uint64_t button;

   player = 0;

   if (device != SNES_DEVICE_JOYPAD)
      return 0;

   if (port == SNES_PORT_2)
   {
	   if(pads_connected < 2)
		   return 0;

	   player = 1;

	   if (device == SNES_DEVICE_MULTITAP)
			player += index;
   }

   button = binds[player][id].joykey;

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
   XInputGetState(0, &state[0]);
   bool retval;

   retval = false;

   switch(key)
   {
	   case SSNES_FAST_FORWARD_HOLD_KEY:
		   return ((state[0].Gamepad.sThumbRY < -DEADZONE) && !(state[0].Gamepad.bRightTrigger > 128));
	   case SSNES_LOAD_STATE_KEY:
		   return ((state[0].Gamepad.sThumbRY > DEADZONE) && (state[0].Gamepad.bRightTrigger > 128));
	   case SSNES_SAVE_STATE_KEY:
		   return ((state[0].Gamepad.sThumbRY < -DEADZONE) && (state[0].Gamepad.bRightTrigger > 128));
	   case SSNES_STATE_SLOT_PLUS:
		   return ((state[0].Gamepad.sThumbRX > DEADZONE) && (state[0].Gamepad.bRightTrigger > 128));
	   case SSNES_STATE_SLOT_MINUS:
		   return ((state[0].Gamepad.sThumbRX < -DEADZONE) && (state[0].Gamepad.bRightTrigger > 128));
	   case SSNES_FRAMEADVANCE:
		   if(g_console.frame_advance_enable)
		   {
			   g_console.menu_enable = true;
			   g_console.ingame_menu_enable = true;
			   g_console.mode_switch = MODE_EMULATION;
		   }
		   return g_console.frame_advance_enable;
	   case SSNES_REWIND:
		   return ((state[0].Gamepad.sThumbRY > DEADZONE) && !(state[0].Gamepad.bRightTrigger > 128));
		case SSNES_QUIT_KEY:
			if(IS_TIMER_EXPIRED())
			{
				uint32_t left_thumb_pressed = (state[0].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
				uint32_t right_thumb_pressed = (state[0].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

				g_console.menu_enable = right_thumb_pressed && left_thumb_pressed && IS_TIMER_EXPIRED();
				g_console.ingame_menu_enable = right_thumb_pressed && !left_thumb_pressed;
			
				if(g_console.menu_enable || (g_console.ingame_menu_enable
				&& !g_console.menu_enable))
				{
					g_console.mode_switch = MODE_MENU;
					SET_TIMER_EXPIRATION(30);
					retval = g_console.menu_enable;
				}

				retval = g_console.ingame_menu_enable ? g_console.ingame_menu_enable : g_console.menu_enable;
			}
   }

   return retval;
}

const input_driver_t input_xdk360 = {
	xdk360_input_init,
	xdk360_input_poll,
	xdk360_input_state,
	xdk360_key_pressed,
	xdk360_free_input,
	"xdk360"};
