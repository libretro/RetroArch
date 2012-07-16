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

#include "../driver.h"
#include "../general.h"
#include "../libretro.h"
#include "xinput_xbox_input.h"

static XINPUT_STATE state[4];
HANDLE gamepads[4];
DWORD dwDeviceMask;
bool bInserted[4];
bool bRemoved[4];
XINPUT_CAPABILITIES caps[4];

#define DEADZONE (16000)

static unsigned pads_connected;

static void xinput_input_poll(void *data)
{
   (void)data;
   unsigned int dwInsertions, dwRemovals;
   
   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, reinterpret_cast<PDWORD>(&dwInsertions), reinterpret_cast<PDWORD>(&dwRemovals));

   pads_connected = 0;

   for (unsigned i = 0; i < 4; i++)
   {
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

         retval = XInputGetState(gamepads[i], &state[i]);
         pads_connected += (retval != ERROR_SUCCESS) ? 0 : 1;
      }
   }
}

static int16_t xinput_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   (void)data;
   unsigned player = port;
   DWORD button = binds[player][id].joykey;
   int16_t retval = 0;

   //Hardcoded binds (WIP)
   switch(id)
   {
      case RETRO_DEVICE_ID_JOYPAD_A:
         retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B]) ? 1 : 0;
         break;
      case RETRO_DEVICE_ID_JOYPAD_B:
			retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A]) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_X:
			retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y]) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_Y:
			retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X]) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_LEFT:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (state[player].Gamepad.sThumbLX < -DEADZONE) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_RIGHT:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || (state[player].Gamepad.sThumbLX > DEADZONE) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_UP:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) || (state[player].Gamepad.sThumbLY > DEADZONE) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_DOWN:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)|| (state[player].Gamepad.sThumbLY < -DEADZONE) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_START:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_START) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_SELECT:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_L:
			retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_R:
			retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER]) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_L2:
			retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE]) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_R2:
			retval = (state[player].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK]) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_L3:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? 1 : 0;
			break;
      case RETRO_DEVICE_ID_JOYPAD_R3:
			retval = (state[player].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? 1 : 0;
			break;
      default:
			break;
   }

   return retval;
}

static void xinput_input_free_input(void *data)
{
   (void)data;
}

static void* xinput_input_init(void)
{
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

   return (void*)-1;
}

static bool xinput_input_key_pressed(void *data, int key)
{
   (void)data;
   bool retval = false;

   return retval;
}

const input_driver_t input_xinput = 
{
   xinput_input_init,
   xinput_input_poll,
   xinput_input_state,
   xinput_input_key_pressed,
   xinput_input_free_input,
   "xinput"
};
