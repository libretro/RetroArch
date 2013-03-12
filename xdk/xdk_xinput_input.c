/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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
#define DEADZONE (16000)

#include "../driver.h"
#include "../general.h"
#include "../libretro.h"


static uint64_t state[MAX_PADS];
unsigned pads_connected;

#ifdef _XBOX1
static HANDLE gamepads[MAX_PADS];
static DWORD dwDeviceMask;
static bool bInserted[MAX_PADS];
static bool bRemoved[MAX_PADS];
#endif

const struct platform_bind platform_keys[] = {
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_B), "A button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_Y), "X button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT), "Back button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_START), "Start button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_UP), "D-Pad Up" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN), "D-Pad Down" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT), "D-Pad Left" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT), "D-Pad Right" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_A), "B button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_X), "Y button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L), "Left trigger" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R), "Right trigger" },
#if defined(_XBOX360)
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L2), "Left shoulder" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R2), "Right shoulder" },
#elif defined(_XBOX1)
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L2), "Black button" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R2), "White button" },
#endif
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_L3), "Left thumb" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_R3), "Right thumb" },
   { (1ULL << RARCH_TURBO_ENABLE), "Turbo button (Unmapped)" },
   { (1ULL << RARCH_ANALOG_LEFT_X_PLUS), "LStick Left" },
   { (1ULL << RARCH_ANALOG_LEFT_X_MINUS), "LStick Right" },
   { (1ULL << RARCH_ANALOG_LEFT_Y_PLUS), "LStick Up" },
   { (1ULL << RARCH_ANALOG_LEFT_Y_MINUS), "LStick Down" },
   { (1ULL << RARCH_ANALOG_RIGHT_X_PLUS), "RStick Left" },
   { (1ULL << RARCH_ANALOG_RIGHT_X_MINUS), "RStick Right" },
   { (1ULL << RARCH_ANALOG_RIGHT_Y_PLUS), "RStick Up" },
   { (1ULL << RARCH_ANALOG_RIGHT_Y_MINUS), "RStick Down" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT), "LStick D-Pad Left" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT), "LStick D-Pad Right" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP), "LStick D-Pad Up" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) | (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN), "LStick D-Pad Down" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_LEFT), "RStick D-Pad Left" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) | (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_RIGHT), "RStick D-Pad Right" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) | (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP), "RStick D-Pad Up" },
   { (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) | (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN), "RStick D-Pad Down" },
};

const unsigned int platform_keys_size = sizeof(platform_keys);

static void xdk_input_poll(void *data)
{
   (void)data;

#if defined(_XBOX1)
   unsigned int dwInsertions, dwRemovals;

   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, reinterpret_cast<PDWORD>(&dwInsertions), reinterpret_cast<PDWORD>(&dwRemovals));

   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      XINPUT_CAPABILITIES caps[MAX_PADS];
      (void)caps;
      // handle removed devices
      bRemoved[i] = (dwRemovals & (1<<i)) ? true : false;

      if(bRemoved[i])
      {
         // if the controller was removed after XGetDeviceChanges but before
         // XInputOpen, the device handle will be NULL
         if(gamepads[i])
            XInputClose(gamepads[i]);

         gamepads[i] = NULL;
         state[i] = 0;
         pads_connected--;
      }

      // handle inserted devices
      bInserted[i] = (dwInsertions & (1<<i)) ? true : false;

      if(bInserted[i])
      {
         XINPUT_POLLING_PARAMETERS m_pollingParameters;
         m_pollingParameters.fAutoPoll = FALSE;
         m_pollingParameters.fInterruptOut = TRUE;
         m_pollingParameters.bInputInterval = 8;
         m_pollingParameters.bOutputInterval = 8;
         gamepads[i] = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, NULL);
         pads_connected++;
      }

      if (gamepads[i])
      {
         // if the controller is removed after XGetDeviceChanges but before
         // XInputOpen, the device handle will be NULL
         XINPUT_STATE state_tmp;

         if (XInputPoll(gamepads[i]) != ERROR_SUCCESS)
            continue;

         if(XInputGetState(gamepads[i], &state_tmp) != ERROR_SUCCESS)
            continue;

         uint64_t *state_cur = &state[i];
         bool dpad_emulation = (g_settings.input.dpad_emulation[i] != DPAD_EMULATION_NONE);

         *state_cur = 0;
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0);
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0);
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0);
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0);
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0);
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0);
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0);
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0);
         if (dpad_emulation)
         {
            *state_cur |= ((state_tmp.Gamepad.sThumbLX < -DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT): 0);
            *state_cur |= ((state_tmp.Gamepad.sThumbLX > DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT) : 0);
            *state_cur |= ((state_tmp.Gamepad.sThumbLY > DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP) : 0);
            *state_cur |= ((state_tmp.Gamepad.sThumbLY < -DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN) : 0);
            *state_cur |= ((state_tmp.Gamepad.sThumbRX < -DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_LEFT) : 0);
            *state_cur |= ((state_tmp.Gamepad.sThumbRX > DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_RIGHT) : 0);
            *state_cur |= ((state_tmp.Gamepad.sThumbRY > DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP) : 0);
            *state_cur |= ((state_tmp.Gamepad.sThumbRY < -DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN) : 0);
         }
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0);
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0);
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0);
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0);
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0);
         *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0);
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0);
         *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0);
      }
   }
#elif defined(_XBOX360)
   for (unsigned i = 0; i < MAX_PADS; i++)
   {
      XINPUT_STATE state_tmp;
      pads_connected += (XInputGetState(i, &state_tmp) == ERROR_DEVICE_NOT_CONNECTED) ? 0 : 1;

      uint64_t *state_cur = &state[i];
      bool dpad_emulation = (g_settings.input.dpad_emulation[i] != DPAD_EMULATION_NONE);

      *state_cur = 0;
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0);

      if (dpad_emulation)
      {
         *state_cur |= ((state_tmp.Gamepad.sThumbLX < -DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_X_DPAD_LEFT) : 0);
         *state_cur |= ((state_tmp.Gamepad.sThumbLX > DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_X_DPAD_RIGHT) : 0);
         *state_cur |= ((state_tmp.Gamepad.sThumbLY > DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_UP) : 0);
         *state_cur |= ((state_tmp.Gamepad.sThumbLY < -DEADZONE) ? (1ULL << RARCH_ANALOG_LEFT_Y_DPAD_DOWN) : 0);
         *state_cur |= ((state_tmp.Gamepad.sThumbRX < -DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_LEFT) : 0);
         *state_cur |= ((state_tmp.Gamepad.sThumbRX > DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_RIGHT) : 0);
         *state_cur |= ((state_tmp.Gamepad.sThumbRY > DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP) : 0);
         *state_cur |= ((state_tmp.Gamepad.sThumbRY < -DEADZONE) ? (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN) : 0);
      }
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0);
      *state_cur |= ((state_tmp.Gamepad.bLeftTrigger > 128) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0);
      *state_cur |= ((state_tmp.Gamepad.bRightTrigger > 128) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0);
   }
#endif

   uint64_t *state_p1 = &state[0];
   uint64_t *lifecycle_state = &g_extern.lifecycle_state;
   bool dpad_emulation = (g_settings.input.dpad_emulation[0] != DPAD_EMULATION_NONE);

   *lifecycle_state &= ~(
         (1ULL << RARCH_FAST_FORWARD_HOLD_KEY) | 
         (1ULL << RARCH_LOAD_STATE_KEY) | 
         (1ULL << RARCH_SAVE_STATE_KEY) | 
         (1ULL << RARCH_STATE_SLOT_PLUS) | 
         (1ULL << RARCH_STATE_SLOT_MINUS) | 
         (1ULL << RARCH_REWIND) |
         (1ULL << RARCH_QUIT_KEY) |
         (1ULL << RARCH_MENU_TOGGLE) |
         (1ULL << RARCH_MENU_QUICKMENU_TOGGLE));

   if (dpad_emulation)
   {
      if ((*state_p1 & (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN)) && !(*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2)))
         *lifecycle_state |= (1ULL << RARCH_FAST_FORWARD_HOLD_KEY);
      if ((*state_p1 & (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2)))
         *lifecycle_state |= (1ULL << RARCH_LOAD_STATE_KEY);
      if ((*state_p1 & (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_DOWN)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2)))
         *lifecycle_state |= (1ULL << RARCH_SAVE_STATE_KEY);
      if ((*state_p1 & (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_RIGHT)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2)))
         *lifecycle_state |= (1ULL << RARCH_STATE_SLOT_PLUS);
      if ((*state_p1 & (1ULL << RARCH_ANALOG_RIGHT_X_DPAD_LEFT)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2)))
         *lifecycle_state |= (1ULL << RARCH_STATE_SLOT_MINUS);
      if ((*state_p1 & (1ULL << RARCH_ANALOG_RIGHT_Y_DPAD_UP)) && !(*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R2)))
         *lifecycle_state |= (1ULL << RARCH_REWIND);
   }

   if (!(g_extern.frame_count < g_extern.delay_timer[0]))
   {
      if((*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
         *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);
      if(!(*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      {
         *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);
         *lifecycle_state |= (1ULL << RARCH_MENU_QUICKMENU_TOGGLE);
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

   for (int i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      g_settings.input.default_binds[i] = platform_keys[i].joykey;
}

static void xdk_input_set_analog_dpad_mapping(unsigned device, unsigned map_dpad_enum, unsigned controller_id)
{
   (void)device;

   switch(map_dpad_enum)
   {
      case DPAD_EMULATION_NONE:
         g_settings.input.dpad_emulation[controller_id] = DPAD_EMULATION_NONE;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[RETRO_DEVICE_ID_JOYPAD_UP].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[RETRO_DEVICE_ID_JOYPAD_DOWN].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[RETRO_DEVICE_ID_JOYPAD_LEFT].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey;
         break;
      case DPAD_EMULATION_LSTICK:
         g_settings.input.dpad_emulation[controller_id] = DPAD_EMULATION_LSTICK;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[RARCH_ANALOG_LEFT_Y_DPAD_UP].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[RARCH_ANALOG_LEFT_Y_DPAD_DOWN].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[RARCH_ANALOG_LEFT_X_DPAD_LEFT].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[RARCH_ANALOG_LEFT_X_DPAD_RIGHT].joykey;
         break;
      case DPAD_EMULATION_RSTICK:
         g_settings.input.dpad_emulation[controller_id] = DPAD_EMULATION_RSTICK;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_UP].joykey	= platform_keys[RARCH_ANALOG_RIGHT_Y_DPAD_UP].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_DOWN].joykey	= platform_keys[RARCH_ANALOG_RIGHT_Y_DPAD_DOWN].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_LEFT].joykey	= platform_keys[RARCH_ANALOG_RIGHT_X_DPAD_LEFT].joykey;
         g_settings.input.binds[controller_id][RETRO_DEVICE_ID_JOYPAD_RIGHT].joykey	= platform_keys[RARCH_ANALOG_RIGHT_X_DPAD_RIGHT].joykey;
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

static void xdk_input_post_init(void)
{
   for(unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      xdk_set_default_keybind_lut(0, i);
      rarch_input_set_default_keybinds(i);
   }

   for(unsigned i = 0; i < MAX_PADS; i++)
      xdk_input_set_analog_dpad_mapping(0, g_settings.input.dpad_emulation[i], i);
}

static bool xdk_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key));
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
