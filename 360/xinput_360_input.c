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

#define MAX_PADS 4

#include "../driver.h"
#include "../general.h"
#include "../libretro.h"
#include "xinput_360_input.h"

static uint64_t state[MAX_PADS];
static unsigned pads_connected;

const struct platform_bind platform_keys[] = {
   { XINPUT_GAMEPAD_B, "B button" },
   { XINPUT_GAMEPAD_A, "A button" },
   { XINPUT_GAMEPAD_Y, "Y button" },
   { XINPUT_GAMEPAD_X, "X button" },
   { XINPUT_GAMEPAD_DPAD_UP, "D-Pad Up" },
   { XINPUT_GAMEPAD_DPAD_DOWN, "D-Pad Down" },
   { XINPUT_GAMEPAD_DPAD_LEFT, "D-Pad Left" },
   { XINPUT_GAMEPAD_DPAD_RIGHT, "D-Pad Right" },
   { XINPUT_GAMEPAD_BACK, "Back button" },
   { XINPUT_GAMEPAD_START, "Start button" },
   { XINPUT_GAMEPAD_LEFT_SHOULDER, "Left Shoulder" },
   { XINPUT_GAMEPAD_LEFT_TRIGGER, "Left Trigger" },
   { XINPUT_GAMEPAD_LEFT_THUMB, "Left Thumb" },
   { XINPUT_GAMEPAD_RIGHT_SHOULDER, "Right Shoulder" },
   { XINPUT_GAMEPAD_RIGHT_TRIGGER, "Right Trigger" },
   { XINPUT_GAMEPAD_RIGHT_THUMB, "Right Thumb" },
   { XINPUT_GAMEPAD_LSTICK_LEFT_MASK, "LStick Left" },
   { XINPUT_GAMEPAD_LSTICK_RIGHT_MASK, "LStick Right" },
   { XINPUT_GAMEPAD_LSTICK_UP_MASK, "LStick Up" },
   { XINPUT_GAMEPAD_LSTICK_DOWN_MASK, "LStick Down" },
   { XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_LSTICK_LEFT_MASK, "LStick D-Pad Left" },
   { XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_LSTICK_RIGHT_MASK, "LStick D-Pad Right" },
   { XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_LSTICK_UP_MASK, "LStick D-Pad Up" },
   { XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_LSTICK_DOWN_MASK, "LStick D-Pad Down" },
   { XINPUT_GAMEPAD_RSTICK_LEFT_MASK, "RStick Left" },
   { XINPUT_GAMEPAD_RSTICK_RIGHT_MASK, "RStick Right" },
   { XINPUT_GAMEPAD_RSTICK_UP_MASK, "RStick Up" },
   { XINPUT_GAMEPAD_RSTICK_DOWN_MASK, "RStick Down" },
   { XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_RSTICK_LEFT_MASK, "RStick D-Pad Left" },
   { XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_RSTICK_RIGHT_MASK, "RStick D-Pad Right" },
   { XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_RSTICK_UP_MASK, "RStick D-Pad Up" },
   { XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_RSTICK_DOWN_MASK, "RStick D-Pad Down" },
};

const unsigned int platform_keys_size = sizeof(platform_keys);

static void xinput_input_poll(void *data)
{
   (void)data;

   pads_connected = 0;

   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      XINPUT_STATE state_tmp;
      unsigned long retval;
      {
         retval = XInputGetState(i, &state_tmp);
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
}

static int16_t xinput_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   unsigned player = port;
   uint64_t button = binds[player][id].joykey;

   return (state[player] & button) ? 1 : 0;
}

static void xinput_input_free_input(void *data)
{
   (void)data;
}

static void xinput_input_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;
   
   switch(map_dpad_enum)
   {
      case DPAD_EMULATION_NONE:
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_UP].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_DOWN].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_LEFT].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_RIGHT].joykey;
         break;
      case DPAD_EMULATION_LSTICK:
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_UP_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_DOWN_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_LEFT_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_RIGHT_DPAD].joykey;
         break;
      case DPAD_EMULATION_RSTICK:
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_UP_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_DOWN_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_LEFT_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_RIGHT_DPAD].joykey;
         break;
   }
}

static void* xinput_input_init(void)
{
   return (void*)-1;
}

#define STUB_DEVICE 0

static void xinput_input_post_init(void)
{
   for(unsigned i = 0; i < MAX_PADS; i++)
      xinput_input_set_analog_dpad_mapping(STUB_DEVICE, g_settings.input.dpad_emulation[i], i);
}

static bool xinput_input_key_pressed(void *data, int key)
{
   (void)data;
   bool retval = false;
   XINPUT_STATE state;
   xdk_d3d_video_t *d3d = (xdk_d3d_video_t*)driver.video_data;

   XInputGetState(0, &state);

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
	 if(IS_TIMER_EXPIRED(d3d))
	 {
            uint32_t left_thumb_pressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
	    uint32_t right_thumb_pressed = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);

	    g_console.menu_enable = right_thumb_pressed && left_thumb_pressed && IS_TIMER_EXPIRED(d3d);
	    g_console.ingame_menu_enable = right_thumb_pressed && !left_thumb_pressed;

	    if(g_console.menu_enable || (g_console.ingame_menu_enable && !g_console.menu_enable))
	    {
               g_console.mode_switch = MODE_MENU;
	       SET_TIMER_EXPIRATION(d3d, 30);
	       retval = g_console.menu_enable;
	    }

	    retval = g_console.ingame_menu_enable ? g_console.ingame_menu_enable : g_console.menu_enable;
	 }
   }

   return retval;
}

static void xinput_set_default_keybind_lut(unsigned device, unsigned port)
{
   (void)device;
   (void)port;

   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_B]		= platform_keys[XDK_DEVICE_ID_JOYPAD_A].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_Y]		= platform_keys[XDK_DEVICE_ID_JOYPAD_X].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_SELECT]	= platform_keys[XDK_DEVICE_ID_JOYPAD_BACK].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_START]	= platform_keys[XDK_DEVICE_ID_JOYPAD_START].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_UP]		= platform_keys[XDK_DEVICE_ID_JOYPAD_UP].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_DOWN]	= platform_keys[XDK_DEVICE_ID_JOYPAD_DOWN].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_LEFT]	= platform_keys[XDK_DEVICE_ID_JOYPAD_LEFT].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_RIGHT]	= platform_keys[XDK_DEVICE_ID_JOYPAD_RIGHT].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_A]		= platform_keys[XDK_DEVICE_ID_JOYPAD_B].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_X]		= platform_keys[XDK_DEVICE_ID_JOYPAD_Y].joykey;
#if defined(_XBOX1)
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_L]      = platform_keys[XDK_DEVICE_ID_JOYPAD_LEFT_TRIGGER].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_R]      = platform_keys[XDK_DEVICE_ID_JOYPAD_RIGHT_TRIGGER].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_L2]     = platform_keys[XDK_DEVICE_ID_JOYPAD_LB].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_R2]     = platform_keys[XDK_DEVICE_ID_JOYPAD_RB].joykey;
#elif defined(_XBOX360)
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_L]      = platform_keys[XDK_DEVICE_ID_JOYPAD_LB].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_R]      = platform_keys[XDK_DEVICE_ID_JOYPAD_RB].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_L2]     = platform_keys[XDK_DEVICE_ID_JOYPAD_LEFT_TRIGGER].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_R2]     = platform_keys[XDK_DEVICE_ID_JOYPAD_RIGHT_TRIGGER].joykey;
#endif
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_L3]     = platform_keys[XDK_DEVICE_ID_LSTICK_THUMB].joykey;
   rarch_default_keybind_lut[RETRO_DEVICE_ID_JOYPAD_R3]     = platform_keys[XDK_DEVICE_ID_RSTICK_THUMB].joykey;
}

const input_driver_t input_xinput = 
{
   xinput_input_init,
   xinput_input_poll,
   xinput_input_state,
   xinput_input_key_pressed,
   xinput_input_free_input,
   xinput_set_default_keybind_lut,
   xinput_input_set_analog_dpad_mapping,
   xinput_input_post_init,
   MAX_PADS,
   "xinput"
};
