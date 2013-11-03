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

typedef struct xdk_input
{
   uint64_t state[MAX_PADS];
#ifdef _XBOX1
   HANDLE gamepads[MAX_PADS];
   DWORD dwDeviceMask;
   bool bInserted[MAX_PADS];
   bool bRemoved[MAX_PADS];
#endif
} xdk_input_t;


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
};

static void xdk_input_poll(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;

#if defined(_XBOX1)
   unsigned int dwInsertions, dwRemovals;
   XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, reinterpret_cast<PDWORD>(&dwInsertions), reinterpret_cast<PDWORD>(&dwRemovals));
#endif

   for (unsigned i = 0; i < MAX_PADS; i++)
   {
#ifdef _XBOX1
      XINPUT_CAPABILITIES caps[MAX_PADS];
      (void)caps;
      // handle removed devices
      xdk->bRemoved[i] = (dwRemovals & (1<<i)) ? true : false;

      if(xdk->bRemoved[i])
      {
         // if the controller was removed after XGetDeviceChanges but before
         // XInputOpen, the device handle will be NULL
         if(xdk->gamepads[i])
            XInputClose(xdk->gamepads[i]);

         xdk->gamepads[i] = NULL;
         xdk->state[i] = 0;
      }

      // handle inserted devices
      xdk->bInserted[i] = (dwInsertions & (1<<i)) ? true : false;

      if(xdk->bInserted[i])
      {
         XINPUT_POLLING_PARAMETERS m_pollingParameters;
         m_pollingParameters.fAutoPoll = FALSE;
         m_pollingParameters.fInterruptOut = TRUE;
         m_pollingParameters.bInputInterval = 8;
         m_pollingParameters.bOutputInterval = 8;
         xdk->gamepads[i] = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, NULL);
      }

      if (!xdk->gamepads[i])
         continue;

      // if the controller is removed after XGetDeviceChanges but before
      // XInputOpen, the device handle will be NULL
#endif

      XINPUT_STATE state_tmp;

#if defined(_XBOX1)
      if (XInputPoll(xdk->gamepads[i]) != ERROR_SUCCESS)
         continue;

      if (XInputGetState(xdk->gamepads[i], &state_tmp) != ERROR_SUCCESS)
         continue;
#elif defined(_XBOX360)
      if (XInputGetState(i, &state_tmp) == ERROR_DEVICE_NOT_CONNECTED)
         continue;
#endif

      uint64_t *state_cur = &xdk->state[i];

      *state_cur = 0;
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0);

#if defined(_XBOX1)
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_B]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_A]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_Y]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_X]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0);
      *state_cur |= ((state_tmp.Gamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK]) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0);
#elif defined(_XBOX360)
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0);
      *state_cur |= ((state_tmp.Gamepad.bLeftTrigger > 128) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0);
      *state_cur |= ((state_tmp.Gamepad.bRightTrigger > 128) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0);
#endif
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0);
      *state_cur |= ((state_tmp.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0);
   }

   uint64_t *state_p1 = &xdk->state[0];
   uint64_t *lifecycle_state = &g_extern.lifecycle_state;

   *lifecycle_state &= ~(
         (1ULL << RARCH_FAST_FORWARD_HOLD_KEY) | 
         (1ULL << RARCH_LOAD_STATE_KEY) | 
         (1ULL << RARCH_SAVE_STATE_KEY) | 
         (1ULL << RARCH_STATE_SLOT_PLUS) | 
         (1ULL << RARCH_STATE_SLOT_MINUS) | 
         (1ULL << RARCH_REWIND) |
         (1ULL << RARCH_QUIT_KEY) |
         (1ULL << RARCH_MENU_TOGGLE));

   if((*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);
}

static int16_t xdk_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   xdk_input_t *xdk = (xdk_input_t*)data;
   unsigned player = port;
   uint64_t button = binds[player][id].joykey;

   return (xdk->state[player] & button) ? 1 : 0;
}

static void xdk_input_free_input(void *data)
{
   (void)data;
}

static void xdk_input_set_keybinds(void *data, unsigned device,
      unsigned port, unsigned id, unsigned keybind_action)
{
   uint64_t *key = &g_settings.input.binds[port][id].joykey;
   uint64_t joykey = *key;
   size_t arr_size = sizeof(platform_keys) / sizeof(platform_keys[0]);

   (void)device;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BIND))
      *key = g_settings.input.binds[port][id].def_joykey;

   if (keybind_action & (1ULL << KEYBINDS_ACTION_SET_DEFAULT_BINDS))
   {
      for (int i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         g_settings.input.binds[port][i].id = i;
         g_settings.input.binds[port][i].def_joykey = platform_keys[i].joykey;
         g_settings.input.binds[port][i].joykey = g_settings.input.binds[port][i].def_joykey;
      }
   }

   if (keybind_action & (1ULL << KEYBINDS_ACTION_GET_BIND_LABEL))
   {
      struct platform_bind *ret = (struct platform_bind*)data;

      if (ret->joykey == NO_BTN)
         strlcpy(ret->desc, "No button", sizeof(ret->desc));
      else
      {
         for (size_t i = 0; i < arr_size; i++)
         {
            if (platform_keys[i].joykey == ret->joykey)
            {
               strlcpy(ret->desc, platform_keys[i].desc, sizeof(ret->desc));
               return;
            }
         }
         strlcpy(ret->desc, "Unknown", sizeof(ret->desc));
      }
   }
}

static void *xdk_input_init(void)
{
   xdk_input_t *xdk = (xdk_input_t*)calloc(1, sizeof(*xdk));
   if (!xdk)
      return NULL;

#ifdef _XBOX1
   XInitDevices(0, NULL);

   xdk->dwDeviceMask = XGetDevices(XDEVICE_TYPE_GAMEPAD);

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

   return xdk;
}

static bool xdk_input_key_pressed(void *data, int key)
{
   return (g_extern.lifecycle_state & (1ULL << key));
}

static uint64_t xdk_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);

   return caps;
}

// FIXME - are we sure about treating low frequency motor as the "strong" motor? Does it apply for Xbox too?

static bool xdk_input_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   xdk_input_t *xdk = (xdk_input_t*)calloc(1, sizeof(*xdk));
   (void)xdk;
   bool val = false;

  
#if defined(_XBOX360)
   XINPUT_VIBRATION rumble_state;

   if (effect == RETRO_RUMBLE_STRONG)
      rumble_state.wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      rumble_state.wRightMotorSpeed = strength;
   val = XInputSetState(port, &rumble_state) == ERROR_SUCCESS;
#elif defined(_XBOX1)
   XINPUT_FEEDBACK rumble_state;

   if (effect == RETRO_RUMBLE_STRONG)
      rumble_state.Rumble.wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      rumble_state.Rumble.wRightMotorSpeed = strength;
   val = XInputSetState(xdk->gamepads[port], port, &rumble_state) == ERROR_SUCCESS;
#endif
}

const input_driver_t input_xinput = 
{
   xdk_input_init,
   xdk_input_poll,
   xdk_input_state,
   xdk_input_key_pressed,
   xdk_input_free_input,
   xdk_input_set_keybinds,
   NULL,
   xdk_input_get_capabilities,
   "xinput",

   NULL,
   xdk_input_set_rumble,
   NULL,
};
