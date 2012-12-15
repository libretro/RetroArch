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

#ifdef _XBOX
#include <xtl.h>
#endif

#define MAX_PADS 4

#include "../driver.h"
#include "../general.h"
#include "../libretro.h"
#include "xdk_xinput_input.h"

static uint64_t state[MAX_PADS];
unsigned pads_connected;

#ifdef _XBOX1
HANDLE gamepads[MAX_PADS];
DWORD dwDeviceMask;
bool bInserted[MAX_PADS];
bool bRemoved[MAX_PADS];
#endif

const struct platform_bind platform_keys[] = {
   { XINPUT1_GAMEPAD_B, "B button" },
   { XINPUT1_GAMEPAD_A, "A button" },
   { XINPUT1_GAMEPAD_Y, "Y button" },
   { XINPUT1_GAMEPAD_X, "X button" },
   { XINPUT1_GAMEPAD_DPAD_UP, "D-Pad Up" },
   { XINPUT1_GAMEPAD_DPAD_DOWN, "D-Pad Down" },
   { XINPUT1_GAMEPAD_DPAD_LEFT, "D-Pad Left" },
   { XINPUT1_GAMEPAD_DPAD_RIGHT, "D-Pad Right" },
   { XINPUT1_GAMEPAD_BACK, "Back button" },
   { XINPUT1_GAMEPAD_START, "Start button" },
#if defined(_XBOX1)
   { XINPUT1_GAMEPAD_WHITE, "White button" },
#elif defined(_XBOX360)
   { XINPUT1_GAMEPAD_WHITE, "Right shoulder" },
#endif
   { XINPUT1_GAMEPAD_LEFT_TRIGGER, "Left Trigger" },
   { XINPUT1_GAMEPAD_LEFT_THUMB, "Left Thumb" },
#if defined(_XBOX1)
   { XINPUT1_GAMEPAD_BLACK, "Black button" },
#elif defined(_XBOX360)
   { XINPUT1_GAMEPAD_BLACK, "Left shoulder" },
#endif
   { XINPUT1_GAMEPAD_RIGHT_TRIGGER, "Right Trigger" },
   { XINPUT1_GAMEPAD_RIGHT_THUMB, "Right Thumb" },
   { XINPUT1_GAMEPAD_LSTICK_LEFT_MASK, "LStick Left" },
   { XINPUT1_GAMEPAD_LSTICK_RIGHT_MASK, "LStick Right" },
   { XINPUT1_GAMEPAD_LSTICK_UP_MASK, "LStick Up" },
   { XINPUT1_GAMEPAD_LSTICK_DOWN_MASK, "LStick Down" },
   { XINPUT1_GAMEPAD_DPAD_LEFT | XINPUT1_GAMEPAD_LSTICK_LEFT_MASK, "LStick D-Pad Left" },
   { XINPUT1_GAMEPAD_DPAD_RIGHT | XINPUT1_GAMEPAD_LSTICK_RIGHT_MASK, "LStick D-Pad Right" },
   { XINPUT1_GAMEPAD_DPAD_UP | XINPUT1_GAMEPAD_LSTICK_UP_MASK, "LStick D-Pad Up" },
   { XINPUT1_GAMEPAD_DPAD_DOWN | XINPUT1_GAMEPAD_LSTICK_DOWN_MASK, "LStick D-Pad Down" },
   { XINPUT1_GAMEPAD_RSTICK_LEFT_MASK, "RStick Left" },
   { XINPUT1_GAMEPAD_RSTICK_RIGHT_MASK, "RStick Right" },
   { XINPUT1_GAMEPAD_RSTICK_UP_MASK, "RStick Up" },
   { XINPUT1_GAMEPAD_RSTICK_DOWN_MASK, "RStick Down" },
   { XINPUT1_GAMEPAD_DPAD_LEFT | XINPUT1_GAMEPAD_RSTICK_LEFT_MASK, "RStick D-Pad Left" },
   { XINPUT1_GAMEPAD_DPAD_RIGHT | XINPUT1_GAMEPAD_RSTICK_RIGHT_MASK, "RStick D-Pad Right" },
   { XINPUT1_GAMEPAD_DPAD_UP | XINPUT1_GAMEPAD_RSTICK_UP_MASK, "RStick D-Pad Up" },
   { XINPUT1_GAMEPAD_DPAD_DOWN | XINPUT1_GAMEPAD_RSTICK_DOWN_MASK, "RStick D-Pad Down" },
};

const unsigned int platform_keys_size = sizeof(platform_keys);

static void xdk_input_poll(void *data)
{
   (void)data;
   
   pads_connected = 0;

#if defined(_XBOX1)
   unsigned int dwInsertions, dwRemovals;
   
   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, reinterpret_cast<PDWORD>(&dwInsertions), reinterpret_cast<PDWORD>(&dwRemovals));

   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      XINPUT_CAPABILITIES caps[MAX_PADS];
      (void)caps;
      state[i] = 0;
      // handle removed devices
      bRemoved[i] = (dwRemovals & (1<<i)) ? true : false;

      if(bRemoved[i])
      {
         // if the controller was removed after XGetDeviceChanges but before
         // XInputOpen, the device handle will be NULL
         if(gamepads[i])
            XInputClose(gamepads[i]);

          gamepads[i] = NULL;
      }

      // handle inserted devices
      bInserted[i] = (dwInsertions & (1<<i)) ? true : false;

      if(bInserted[i])
      {
         XINPUT_POLLING_PARAMETERS m_pollingParameters;
         m_pollingParameters.fAutoPoll = TRUE;
         m_pollingParameters.fInterruptOut = TRUE;
         m_pollingParameters.bInputInterval = 8;
         m_pollingParameters.bOutputInterval = 8;
         gamepads[i] = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, NULL);
      }
	  
      if (gamepads[i])
      {
         unsigned long retval;

         // if the controller is removed after XGetDeviceChanges but before
         // XInputOpen, the device handle will be NULL
         XINPUT_STATE state_tmp;
         retval = XInputGetState(gamepads[i], &state_tmp);
         if(retval == ERROR_SUCCESS)
         {
            pads_connected++;
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B]) ? XINPUT1_GAMEPAD_B : 0);
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A]) ? XINPUT1_GAMEPAD_A : 0);
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y]) ? XINPUT1_GAMEPAD_Y : 0);
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X]) ? XINPUT1_GAMEPAD_X : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ? XINPUT1_GAMEPAD_DPAD_LEFT : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? XINPUT1_GAMEPAD_DPAD_RIGHT : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ? XINPUT1_GAMEPAD_DPAD_UP : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ? XINPUT1_GAMEPAD_DPAD_DOWN : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbLX < -DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_LEFT_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbLX > DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_RIGHT_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbLY > DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_UP_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbLY < -DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_DOWN_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbRX < -DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_LEFT_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbRX > DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_RIGHT_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbRY > DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_UP_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.sThumbRY < -DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_DOWN_MASK : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_START) ? XINPUT1_GAMEPAD_START : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? XINPUT1_GAMEPAD_BACK : 0);
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]) ? XINPUT1_GAMEPAD_LEFT_TRIGGER : 0);
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER]) ? XINPUT1_GAMEPAD_RIGHT_TRIGGER : 0);
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE]) ? XINPUT1_GAMEPAD_WHITE : 0);
            state[i] |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK]) ? XINPUT1_GAMEPAD_BLACK : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? XINPUT1_GAMEPAD_LEFT_THUMB : 0);
            state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? XINPUT1_GAMEPAD_RIGHT_THUMB : 0);
         }
      }
   }
#elif defined(_XBOX360)
   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      XINPUT_STATE state_tmp;
      unsigned long retval;
      {
         retval = XInputGetState(i, &state_tmp);
         pads_connected += (retval == ERROR_DEVICE_NOT_CONNECTED) ? 0 : 1;
         state[i] = 0;
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? XINPUT1_GAMEPAD_B : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? XINPUT1_GAMEPAD_A : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? XINPUT1_GAMEPAD_Y : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? XINPUT1_GAMEPAD_X : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ? XINPUT1_GAMEPAD_DPAD_LEFT : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? XINPUT1_GAMEPAD_DPAD_RIGHT : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ? XINPUT1_GAMEPAD_DPAD_UP : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ? XINPUT1_GAMEPAD_DPAD_DOWN : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbLX < -DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_LEFT_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbLX > DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_RIGHT_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbLY > DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_UP_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbLY < -DEADZONE) ? XINPUT1_GAMEPAD_LSTICK_DOWN_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbRX < -DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_LEFT_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbRX > DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_RIGHT_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbRY > DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_UP_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.sThumbRY < -DEADZONE) ? XINPUT1_GAMEPAD_RSTICK_DOWN_MASK : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_START) ? XINPUT1_GAMEPAD_START : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? XINPUT1_GAMEPAD_BACK : 0);
         state[i] |= ((state_tmp.Gamepad.bLeftTrigger > 128) ? XINPUT1_GAMEPAD_LEFT_TRIGGER : 0);
         state[i] |= ((state_tmp.Gamepad.bRightTrigger > 128) ? XINPUT1_GAMEPAD_RIGHT_TRIGGER : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? XINPUT1_GAMEPAD_WHITE : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? XINPUT1_GAMEPAD_BLACK : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? XINPUT1_GAMEPAD_LEFT_THUMB : 0);
         state[i] |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? XINPUT1_GAMEPAD_RIGHT_THUMB : 0);
      }
   }
#endif

   g_extern.lifecycle_state &= ~((1ULL << RARCH_QUIT_KEY) | (1ULL << RARCH_RMENU_TOGGLE) | (1ULL << RARCH_RMENU_QUICKMENU_TOGGLE));

   if(IS_TIMER_EXPIRED(0))
   {
      if((state[0] & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (state[0] & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      {
         g_extern.lifecycle_state |= (1ULL << RARCH_RMENU_TOGGLE);
         g_extern.lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
      }
      if(!(state[0] & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (state[0] & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      {
         g_extern.lifecycle_state |= (1ULL << RARCH_RMENU_QUICKMENU_TOGGLE);
         g_extern.lifecycle_state |= (1ULL << RARCH_QUIT_KEY);
      }
   }
}

static int16_t xdk_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   unsigned player = port;
   uint64_t button = binds[player][id].joykey;

   return (state[player] & button) ? 1 : 0;
}

static void xdk_input_free_input(void *data)
{
   (void)data;
}

static void xdk_set_default_keybind_lut(unsigned device, unsigned port)
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

static void xdk_input_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;
   
   switch(map_dpad_enum)
   {
      case DPAD_EMULATION_NONE:
		 g_settings.input.dpad_emulation[controller_id] = DPAD_EMULATION_NONE;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_UP].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_DOWN].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_LEFT].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK_DEVICE_ID_JOYPAD_RIGHT].joykey;
         break;
      case DPAD_EMULATION_LSTICK:
		 g_settings.input.dpad_emulation[controller_id] = DPAD_EMULATION_LSTICK;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_UP_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_DOWN_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_LEFT_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK_DEVICE_ID_LSTICK_RIGHT_DPAD].joykey;
         break;
      case DPAD_EMULATION_RSTICK:
		 g_settings.input.dpad_emulation[controller_id] = DPAD_EMULATION_RSTICK;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_UP_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_DOWN_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_LEFT_DPAD].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[XDK_DEVICE_ID_RSTICK_RIGHT_DPAD].joykey;
         break;
   }
}

static void *xdk_input_init(void)
{
#ifdef _XBOX1
   XInitDevices(0, NULL);

   dwDeviceMask = XGetDevices(XDEVICE_TYPE_GAMEPAD);
   
   //Check the device status
   switch(XGetDeviceEnumerationStatus())
   {
      case XDEVICE_ENUMERATION_IDLE:
         RARCH_LOG("Input state status: XDEVICE_ENUMERATION_IDLE\n");
			break;
      case XDEVICE_ENUMERATION_BUSY:
			RARCH_LOG("Input state status: XDEVICE_ENUMERATION_BUSY\n");
			break;
   }

   while(XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY) {}
#endif

   return (void*)-1;
}

#define STUB_DEVICE 0

static void xdk_input_post_init(void)
{
   for(unsigned i = 0; i < MAX_PADS; i++)
      xdk_input_set_analog_dpad_mapping(STUB_DEVICE, g_settings.input.dpad_emulation[i], i);
}

static bool xdk_input_key_pressed(void *data, int key)
{
   (void)data;

   if(g_extern.lifecycle_state & (1ULL << key))
      return true;
   
   switch(key)
   {
      case RARCH_FAST_FORWARD_HOLD_KEY:
         return ((state[0] & XINPUT1_GAMEPAD_RSTICK_DOWN_MASK) && !(state[0] & XINPUT1_GAMEPAD_RIGHT_TRIGGER));
      case RARCH_LOAD_STATE_KEY:
         return ((state[0] & XINPUT1_GAMEPAD_RSTICK_UP_MASK) && (state[0] & XINPUT1_GAMEPAD_RIGHT_TRIGGER));
      case RARCH_SAVE_STATE_KEY:
         return ((state[0] & XINPUT1_GAMEPAD_RSTICK_DOWN_MASK) && (state[0] & XINPUT1_GAMEPAD_RIGHT_TRIGGER));
      case RARCH_STATE_SLOT_PLUS:
         return ((state[0] & XINPUT1_GAMEPAD_RSTICK_RIGHT_MASK) && (state[0] & XINPUT1_GAMEPAD_RIGHT_TRIGGER));
      case RARCH_STATE_SLOT_MINUS:
         return ((state[0] & XINPUT1_GAMEPAD_RSTICK_LEFT_MASK) && (state[0] & XINPUT1_GAMEPAD_RIGHT_TRIGGER));
      case RARCH_REWIND:
         return ((state[0] & XINPUT1_GAMEPAD_RSTICK_UP_MASK) && !(state[0] & XINPUT1_GAMEPAD_RIGHT_TRIGGER));
      default:
         return false;
   }

   return false;
}

const input_driver_t input_xinput = 
{
   xdk_input_init,
   xdk_input_poll,
   xdk_input_state,
   xdk_input_key_pressed,
   xdk_input_free_input,
   xdk_set_default_keybind_lut,
   xdk_input_set_analog_dpad_mapping,
   xdk_input_post_init,
   MAX_PADS,
   "xinput"
};
