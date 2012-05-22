/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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
#include <xtl.h>
#include "../driver.h"
#include "../general.h"
#include "../libretro.h"
#include "../console/console_ext_input.h"
#include "xdk360_input.h"
#include "xdk360_video_general.h"
#include "shared.h"
#include "menu.h"

static uint64_t state[4];
static unsigned pads_connected;

static void xdk360_input_poll(void *data)
{
   (void)data;

   pads_connected = 0;

   for (unsigned i = 0; i < 4; i++)
   {
      XINPUT_STATE state_tmp;
      unsigned long retval = XInputGetState(i, &state_tmp);
      pads_connected += (retval == ERROR_DEVICE_NOT_CONNECTED) ? 0 : 1;
      state[i] = state_tmp.Gamepad.wButtons;
      state[i] |= ((state_tmp.Gamepad.sThumbLX < -DEADZONE))        << 16;
      state[i] |= ((state_tmp.Gamepad.sThumbLX > DEADZONE))         << 17;
      state[i] |= ((state_tmp.Gamepad.sThumbLY > DEADZONE))         << 18;
      state[i] |= ((state_tmp.Gamepad.sThumbLY < -DEADZONE))        << 19;
      state[i] |= ((state_tmp.Gamepad.sThumbRX < -DEADZONE))        << 20;
      state[i] |= ((state_tmp.Gamepad.sThumbRX > DEADZONE))         << 21;
      state[i] |= ((state_tmp.Gamepad.sThumbRY > DEADZONE))         << 22;
      state[i] |= ((state_tmp.Gamepad.sThumbRY < -DEADZONE))        << 23;
      state[i] |= ((state_tmp.Gamepad.bLeftTrigger > 128 ? 1 : 0))  << 24;
      state[i] |= ((state_tmp.Gamepad.bRightTrigger > 128 ? 1 : 0)) << 25;
   }
}

static int16_t xdk360_input_state(void *data, const struct snes_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   unsigned player = port;
   uint64_t button = binds[player][id].joykey;

   return (state[player] & button) ? 1 : 0;
}

static void xdk360_free_input(void *data)
{
   (void)data;
}

static void* xdk360_input_initialize(void)
{
   return (void*)-1;
}

void xdk360_input_init(void)
{
   for(unsigned i = 0; i < 4; i++)
      xdk360_input_map_dpad_to_stick(g_settings.input.dpad_emulation[i], i);
}

void xdk360_input_map_dpad_to_stick(uint32_t map_dpad_enum, uint32_t controller_id)
{
   switch(map_dpad_enum)
   {
      case DPAD_EMULATION_NONE:
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK360_DEVICE_ID_JOYPAD_UP].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK360_DEVICE_ID_JOYPAD_DOWN].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK360_DEVICE_ID_JOYPAD_LEFT].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK360_DEVICE_ID_JOYPAD_RIGHT].joykey;
	 break;
      case DPAD_EMULATION_LSTICK:
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK360_DEVICE_ID_LSTICK_UP_DPAD].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK360_DEVICE_ID_LSTICK_DOWN_DPAD].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK360_DEVICE_ID_LSTICK_LEFT_DPAD].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK360_DEVICE_ID_LSTICK_RIGHT_DPAD].joykey;
	 break;
      case DPAD_EMULATION_RSTICK:
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK360_DEVICE_ID_RSTICK_UP_DPAD].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK360_DEVICE_ID_RSTICK_DOWN_DPAD].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK360_DEVICE_ID_RSTICK_LEFT_DPAD].joykey;
	 g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK360_DEVICE_ID_RSTICK_RIGHT_DPAD].joykey;
	 break;
   }
}

void xdk360_input_loop(void)
{
	int custom_viewport_x_tmp, custom_viewport_y_tmp, custom_viewport_width_tmp,
		custom_viewport_height_tmp;

	XINPUT_STATE state;

	XInputGetState(0, &state);

	custom_viewport_x_tmp = g_console.custom_viewport_x;
	custom_viewport_y_tmp = g_console.custom_viewport_y;
	custom_viewport_width_tmp = g_console.custom_viewport_width;
	custom_viewport_height_tmp = g_console.custom_viewport_height;

	if(state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT || state.Gamepad.sThumbLX < -DEADZONE)
		g_console.custom_viewport_x -= 1;
	else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT || state.Gamepad.sThumbLX > DEADZONE)
		g_console.custom_viewport_x += 1;

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP || state.Gamepad.sThumbLY > DEADZONE)
		g_console.custom_viewport_y += 1;
	else if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN || state.Gamepad.sThumbLY < -DEADZONE) 
		g_console.custom_viewport_y -= 1;

	if (state.Gamepad.sThumbRX < -DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
		g_console.custom_viewport_width -= 1;
	else if (state.Gamepad.sThumbRX > DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
		g_console.custom_viewport_width += 1;

	if (state.Gamepad.sThumbRY > DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
		g_console.custom_viewport_height += 1;
	else if (state.Gamepad.sThumbRY < -DEADZONE || state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
		g_console.custom_viewport_height -= 1;

	if (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
	{
		g_console.custom_viewport_x = 0;
		g_console.custom_viewport_y = 0;
		g_console.custom_viewport_width = 1280; //FIXME: hardcoded
		g_console.custom_viewport_height = 720; //FIXME: hardcoded
	}
	if(state.Gamepad.wButtons & XINPUT_GAMEPAD_B)
	{
		g_console.input_loop = INPUT_LOOP_MENU;
	}
}

static bool xdk360_key_pressed(void *data, int key)
{
   (void)data;
   XINPUT_STATE state;
   bool retval;

   XInputGetState(0, &state);
   retval = false;

   switch(key)
   {
      case RARCH_FAST_FORWARD_HOLD_KEY:
         return ((state.Gamepad.sThumbRY < -DEADZONE) && !(state.Gamepad.bRightTrigger > 128));
      case RARCH_LOAD_STATE_KEY:
	 return ((state.Gamepad.sThumbRY > DEADZONE) && (state.Gamepad.bRightTrigger > 128));
      case RARCH_SAVE_STATE_KEY:
	 return ((state.Gamepad.sThumbRY < -DEADZONE) && (state.Gamepad.bRightTrigger > 128));
      case RARCH_STATE_SLOT_PLUS:
	 return ((state.Gamepad.sThumbRX > DEADZONE) && (state.Gamepad.bRightTrigger > 128));
      case RARCH_STATE_SLOT_MINUS:
	 return ((state.Gamepad.sThumbRX < -DEADZONE) && (state.Gamepad.bRightTrigger > 128));
      case RARCH_FRAMEADVANCE:
	 if(g_console.frame_advance_enable)
	 {
            g_console.menu_enable = true;
	    g_console.ingame_menu_enable = true;
	    g_console.mode_switch = MODE_MENU;
	 }
	 return false;
      case RARCH_REWIND:
	 return ((state.Gamepad.sThumbRY > DEADZONE) && !(state.Gamepad.bRightTrigger > 128));
      case RARCH_QUIT_KEY:
	 if(IS_TIMER_EXPIRED())
	 {
            uint32_t left_thumb_pressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
	    uint32_t right_thumb_pressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

	    g_console.menu_enable = right_thumb_pressed && left_thumb_pressed && IS_TIMER_EXPIRED();
	    g_console.ingame_menu_enable = right_thumb_pressed && !left_thumb_pressed;

	    if(g_console.menu_enable || (g_console.ingame_menu_enable && !g_console.menu_enable))
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

const input_driver_t input_xdk360 = 
{
   xdk360_input_initialize,
   xdk360_input_poll,
   xdk360_input_state,
   xdk360_key_pressed,
   xdk360_free_input,
   "xdk360"
};
