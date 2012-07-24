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

static uint64_t real_state[4];
HANDLE gamepads[4];
DWORD dwDeviceMask;
bool bInserted[4];
bool bRemoved[4];

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
      XINPUT_STATE state[4];
      XINPUT_CAPABILITIES caps[4];
      real_state[i] = 0;
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
         if(retval == ERROR_SUCCESS)
         {
            pads_connected++;
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B]) ? XINPUT1_GAMEPAD_B : 0);
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A]) ? XINPUT1_GAMEPAD_A : 0);
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y]) ? XINPUT1_GAMEPAD_Y : 0);
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X]) ? XINPUT1_GAMEPAD_X : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || (state[i].Gamepad.sThumbLX < -DEADZONE) ? XINPUT1_GAMEPAD_DPAD_LEFT : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || (state[i].Gamepad.sThumbLX > DEADZONE) ? XINPUT1_GAMEPAD_DPAD_RIGHT : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) || (state[i].Gamepad.sThumbLY > DEADZONE) ? XINPUT1_GAMEPAD_DPAD_UP : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)|| (state[i].Gamepad.sThumbLY < -DEADZONE) ? XINPUT1_GAMEPAD_DPAD_DOWN : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_START) ? XINPUT1_GAMEPAD_START : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? XINPUT1_GAMEPAD_BACK : 0);
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]) ? XINPUT1_GAMEPAD_LEFT_TRIGGER : 0);
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER]) ? XINPUT1_GAMEPAD_RIGHT_TRIGGER : 0);
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE]) ? XINPUT1_GAMEPAD_WHITE : 0);
            real_state[i] |= ((state[i].Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK]) ? XINPUT1_GAMEPAD_BLACK : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? XINPUT1_GAMEPAD_LEFT_THUMB : 0);
            real_state[i] |= ((state[i].Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? XINPUT1_GAMEPAD_RIGHT_THUMB : 0);
         }
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

   return (real_state[player] & button) ? 1 : 0;
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
