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
#include "ps3_input.h"
#include "ps3_video_psgl.h"
#include "../driver.h"
#include "../input/input_luts.h"
#include "../libsnes.hpp"
#include "../general.h"
#include "shared.h"

static uint64_t state[MAX_PADS];
static void ps3_input_poll(void *data)
{
   (void)data;
   for (unsigned i = 0; i < MAX_PADS; i++)
      state[i] = cell_pad_input_poll_device(i);
}

static int16_t ps3_input_state(void *data, const struct snes_keybind **binds,
      bool port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;

   unsigned pads_connected, player;
   uint64_t button;

   player = 0;
   pads_connected = cell_pad_input_pads_connected(); 

   if (device != SNES_DEVICE_JOYPAD)
      return 0;

   if (port == SNES_PORT_2)
   {
	   if(pads_connected < 2)
		   return 0;

	   player = 1;

	   if(device == SNES_DEVICE_MULTITAP)
	      player += index;
   }

   button = binds[player][id].joykey;

   return CTRL_MASK(state[player], button) ? 1 : 0;
}

static void ps3_free_input(void *data)
{
   (void)data;
}

static void* ps3_input_initialize(void)
{
   return (void*)-1;
}

void ps3_input_init(void)
{
   cell_pad_input_init();
   for(unsigned i = 0; i < MAX_PADS; i++)
   	ps3_input_map_dpad_to_stick(g_settings.input.dpad_emulation[i], i);
}

void ps3_input_map_dpad_to_stick(uint32_t map_dpad_enum, uint32_t controller_id)
{
	switch(map_dpad_enum)
	{
		case DPAD_EMULATION_NONE:
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_UP].joykey		= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_UP];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_DOWN].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_DOWN];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_LEFT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_LEFT];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_RIGHT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_JOYPAD_RIGHT];
			break;
		case DPAD_EMULATION_LSTICK:
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_UP].joykey		= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_UP_DPAD];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_DOWN].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_DOWN_DPAD];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_LEFT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_LEFT_DPAD];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_RIGHT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_LSTICK_RIGHT_DPAD];
			break;
		case DPAD_EMULATION_RSTICK:
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_UP].joykey		= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_UP_DPAD];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_DOWN].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_DOWN_DPAD];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_LEFT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_LEFT_DPAD];
			g_settings.input.binds[controller_id][SNES_DEVICE_ID_JOYPAD_RIGHT].joykey	= ssnes_platform_keybind_lut[PS3_DEVICE_ID_RSTICK_RIGHT_DPAD];
			break;
	}
}

static bool ps3_key_pressed(void *data, int key)
{
   (void)data;
   switch (key)
   {
      case SSNES_FAST_FORWARD_HOLD_KEY:
         return CTRL_RSTICK_DOWN(state[0]) && CTRL_R2(~state[0]);
      case SSNES_LOAD_STATE_KEY:
         return (CTRL_RSTICK_UP(state[0]) && CTRL_R2(state[0]));
      case SSNES_SAVE_STATE_KEY:
         return (CTRL_RSTICK_DOWN(state[0]) && CTRL_R2(state[0]));
      case SSNES_STATE_SLOT_PLUS:
         return (CTRL_RSTICK_RIGHT(state[0]) && CTRL_R2(state[0]));
      case SSNES_STATE_SLOT_MINUS:
         return (CTRL_RSTICK_LEFT(state[0]) && CTRL_R2(state[0]));
      case SSNES_FRAMEADVANCE:
      	if(g_console.frame_advance_enable)
	{
		g_console.menu_enable = true;
		g_console.ingame_menu_enable = true;
		g_console.mode_switch = MODE_MENU;
	}
	 return false;
      case SSNES_REWIND:
         return CTRL_RSTICK_UP(state[0]) && CTRL_R2(~state[0]);
      case SSNES_QUIT_KEY:
	 if(IS_TIMER_EXPIRED(g_console.timer_expiration_frame_count))
	 {
		 uint32_t r3_pressed = CTRL_R3(state[0]);
		 uint32_t l3_pressed = CTRL_L3(state[0]);
		 bool retval = false;
		 g_console.menu_enable = (r3_pressed && l3_pressed && IS_TIMER_EXPIRED(g_console.timer_expiration_frame_count));
		 g_console.ingame_menu_enable = r3_pressed && !l3_pressed;

		 if(g_console.menu_enable || (g_console.ingame_menu_enable && !g_console.menu_enable))
		 {
			 g_console.mode_switch = MODE_MENU;
			 SET_TIMER_EXPIRATION(g_console.control_timer_expiration_frame_count, 30);
			 retval = g_console.menu_enable;
		 }

		 retval = g_console.ingame_menu_enable ? g_console.ingame_menu_enable : g_console.menu_enable;
		 return retval;
	 }
      default:
         return false;
   }
}

const input_driver_t input_ps3 = {
   .init = ps3_input_initialize,
   .poll = ps3_input_poll,
   .input_state = ps3_input_state,
   .key_pressed = ps3_key_pressed,
   .free = ps3_free_input,
   .ident = "ps3",
};

