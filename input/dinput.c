/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include "../general.h"
#include "../boolean.h"
#include "input_common.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

static LPDIRECTINPUT8 g_ctx;

struct dinput_joypad
{
   LPDIRECTINPUTDEVICE8 joypad;
   DIJOYSTATE2 joy_state;
};

static unsigned g_joypad_cnt;
static struct dinput_joypad g_pads[MAX_PLAYERS];

static void dinput_destroy(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].joypad)
      {
         IDirectInputDevice8_Unacquire(g_pads[i].joypad);
         IDirectInputDevice8_Release(g_pads[i].joypad);
      }
   }

   if (g_ctx)
      IDirectInput8_Release(g_ctx);

   g_ctx = NULL;
   g_joypad_cnt = 0;
   memset(g_pads, 0, sizeof(g_pads));
}

static BOOL CALLBACK enum_axes_cb(const DIDEVICEOBJECTINSTANCE *inst, void *p)
{
   LPDIRECTINPUTDEVICE8 joypad = (LPDIRECTINPUTDEVICE8)p;

   DIPROPRANGE range;
   memset(&range, 0, sizeof(range));
   range.diph.dwSize = sizeof(DIPROPRANGE);
   range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
   range.diph.dwHow = DIPH_BYID;
   range.diph.dwObj = inst->dwType;
   range.lMin = -32768;
   range.lMax = 32767;
   IDirectInputDevice8_SetProperty(joypad, DIPROP_RANGE, &range.diph);

   return DIENUM_CONTINUE;
}

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
   (void)p;
   if (g_joypad_cnt == MAX_PLAYERS)
      return DIENUM_STOP;

   LPDIRECTINPUTDEVICE8 *pad = &g_pads[g_joypad_cnt].joypad;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, inst->guidInstance, pad, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(g_ctx, &inst->guidInstance, pad, NULL)))
#endif
      return DIENUM_CONTINUE;

   IDirectInputDevice8_SetDataFormat(*pad, &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(*pad, (HWND)driver.video_window,
         DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(*pad, enum_axes_cb, 
         *pad, DIDFT_ABSAXIS);

   g_joypad_cnt++;

   return DIENUM_CONTINUE; 
}

static bool dinput_init(void)
{
   if (driver.display_type != RARCH_DISPLAY_WIN32)
   {
      RARCH_ERR("Cannot open DInput as no Win32 window is present.\n");
      return false;
   }

   CoInitialize(NULL);

#ifdef __cplusplus
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8,
      (void**)&g_ctx, NULL)))
#else
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8,
      (void**)&g_ctx, NULL)))
#endif
   {
      RARCH_ERR("Failed to init DirectInput.\n");
      goto error;
   }

   RARCH_LOG("Enumerating DInput devices ...\n");
   IDirectInput8_EnumDevices(g_ctx, DI8DEVCLASS_GAMECTRL, enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);
   RARCH_LOG("Done enumerating DInput devices ...\n");

   return true;

error:
   dinput_destroy();
   return NULL;
}

static bool dinput_button(unsigned port_num, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   const struct dinput_joypad *pad = &g_pads[port_num];

   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      unsigned hat = GET_HAT(joykey);
      
      unsigned elems = sizeof(pad->joy_state.rgdwPOV) / sizeof(pad->joy_state.rgdwPOV[0]);
      if (hat >= elems)
         return false;

      unsigned pov = pad->joy_state.rgdwPOV[hat];

      // Magic numbers I'm not sure where originate from.
      if (pov < 36000)
      {
         switch (GET_HAT_DIR(joykey))
         {
            case HAT_UP_MASK:
               return (pov >= 31500) || (pov <= 4500);
            case HAT_RIGHT_MASK:
               return (pov >= 4500) && (pov <= 13500);
            case HAT_DOWN_MASK:
               return (pov >= 13500) && (pov <= 22500);
            case HAT_LEFT_MASK:
               return (pov >= 22500) && (pov <= 31500);
         }
      }

      return false;
   }
   else
   {
      unsigned elems = sizeof(pad->joy_state.rgbButtons) / sizeof(pad->joy_state.rgbButtons[0]);

      if (joykey < elems)
         return pad->joy_state.rgbButtons[joykey];
   }

   return false;
}

static int16_t dinput_axis(unsigned port_num, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   const struct dinput_joypad *pad = &g_pads[port_num];

   int val = 0;

   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) <= 5)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 5)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0: val = pad->joy_state.lX; break;
      case 1: val = pad->joy_state.lY; break;
      case 2: val = pad->joy_state.lZ; break;
      case 3: val = pad->joy_state.lRx; break;
      case 4: val = pad->joy_state.lRy; break;
      case 5: val = pad->joy_state.lRz; break;
   }

   if (val < -0x7fff) // So abs() of -0x8000 can't mess us up.
      val = -0x7fff;

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void dinput_poll(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      struct dinput_joypad *pad = &g_pads[i];

      if (pad->joypad)
      {
         memset(&pad->joy_state, 0, sizeof(pad->joy_state));

         if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         {
            IDirectInputDevice8_Acquire(pad->joypad);
            continue;
         }

         IDirectInputDevice8_GetDeviceState(pad->joypad, sizeof(DIJOYSTATE2), &pad->joy_state);
      }
   }
}

const rarch_joypad_driver_t dinput_joypad = {
   dinput_init,
   dinput_destroy,
   dinput_button,
   dinput_axis,
   dinput_poll,
   "DInput",
};

