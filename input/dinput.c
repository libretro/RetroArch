/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "ssnes_dinput.h"
#include "SDL.h"
#include "SDL_syswm.h"
#include "../boolean.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "../gfx/sdlwrap.h"

void sdl_dinput_free(sdl_dinput_t *di)
{
   if (di)
   {
      for (unsigned i = 0; i < MAX_PLAYERS; i++)
      {
         if (di->joypad[i])
         {
            IDirectInputDevice8_Unacquire(di->joypad[i]);
            IDirectInputDevice8_Release(di->joypad[i]);
         }
      }

      if (di->ctx)
         IDirectInput8_Release(di->ctx);

      free(di);
   }
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
   sdl_dinput_t *di = (sdl_dinput_t*)p;

   unsigned active = 0;
   unsigned n;
   for (n = 0; n < MAX_PLAYERS; n++)
   {
      if (!di->joypad[n] && g_settings.input.joypad_map[n] == (int)di->joypad_cnt)
         break;

      if (di->joypad[n])
         active++;
   }

   if (active == MAX_PLAYERS) return DIENUM_STOP;

   di->joypad_cnt++;
   if (n == MAX_PLAYERS)
      return DIENUM_CONTINUE;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(di->ctx, inst->guidInstance, &di->joypad[n], NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(di->ctx, &inst->guidInstance, &di->joypad[n], NULL)))
#endif
      return DIENUM_CONTINUE;

   IDirectInputDevice8_SetDataFormat(di->joypad[n], &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(di->joypad[n], di->hWnd,
         DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(di->joypad[n], enum_axes_cb, 
         di->joypad[n], DIDFT_ABSAXIS);

   return DIENUM_CONTINUE; 
}

sdl_dinput_t* sdl_dinput_init(void)
{
   sdl_dinput_t *di = (sdl_dinput_t*)calloc(1, sizeof(*di));
   if (!di)
      return NULL;

   CoInitialize(NULL);

   SDL_SysWMinfo info;
   SDL_VERSION(&info.version);

   if (!sdlwrap_get_wm_info(&info))
   {
      SSNES_ERR("Failed to get SysWM info.\n");
      goto error;
   }

#if SDL_MODERN
   di->hWnd = info.info.win.window;
#else
   di->hWnd = info.window;
#endif

#ifdef __cplusplus
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8,
      (void**)&di->ctx, NULL)))
#else
   if (FAILED(DirectInput8Create(
      GetModuleHandle(NULL), DIRECTINPUT_VERSION, &IID_IDirectInput8,
      (void**)&di->ctx, NULL)))
#endif
   {
      SSNES_ERR("Failed to init DirectInput.\n");
      goto error;
   }

   SSNES_LOG("Enumerating DInput devices ...\n");
   IDirectInput8_EnumDevices(di->ctx, DI8DEVCLASS_GAMECTRL, enum_joypad_cb, di, DIEDFL_ATTACHEDONLY);
   SSNES_LOG("Done enumerating DInput devices ...\n");

   return di;

error:
   sdl_dinput_free(di);
   return NULL;
}

static bool dinput_joykey_pressed(sdl_dinput_t *di, unsigned port_num, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      unsigned hat = GET_HAT(joykey);
      
      unsigned elems = sizeof(di->joy_state[0].rgdwPOV) / sizeof(di->joy_state[0].rgdwPOV[0]);
      if (hat >= elems)
         return false;

      unsigned pov = di->joy_state[port_num].rgdwPOV[hat];

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
      unsigned elems = sizeof(di->joy_state[0].rgbButtons) / sizeof(di->joy_state[0].rgbButtons[0]);

      if (joykey < elems)
         return di->joy_state[port_num].rgbButtons[joykey];
   }

   return false;
}

static bool dinput_joyaxis_pressed(sdl_dinput_t *di, unsigned port_num, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return false;

   int min = -32678 * g_settings.input.axis_threshold;
   int max = 32677 * g_settings.input.axis_threshold;

   switch (AXIS_NEG_GET(joyaxis))
   {
      case 0:
         return di->joy_state[port_num].lX <= min;
      case 1:
         return di->joy_state[port_num].lY <= min;
      case 2:
         return di->joy_state[port_num].lZ <= min;
      case 3:
         return di->joy_state[port_num].lRx <= min;
      case 4:
         return di->joy_state[port_num].lRy <= min;
      case 5:
         return di->joy_state[port_num].lRz <= min;
   }

   switch (AXIS_POS_GET(joyaxis))
   {
      case 0:
         return di->joy_state[port_num].lX >= max;
      case 1:
         return di->joy_state[port_num].lY >= max;
      case 2:
         return di->joy_state[port_num].lZ >= max;
      case 3:
         return di->joy_state[port_num].lRx >= max;
      case 4:
         return di->joy_state[port_num].lRy >= max;
      case 5:
         return di->joy_state[port_num].lRz >= max;
   }

   return false;
}

bool sdl_dinput_pressed(sdl_dinput_t *di, unsigned port_num, const struct snes_keybind *key)
{
   if (di->joypad[port_num] == NULL)
      return false;
   if (dinput_joykey_pressed(di, port_num, key->joykey))
      return true;
   if (dinput_joyaxis_pressed(di, port_num, key->joyaxis))
      return true;

   return false;
}

void sdl_dinput_poll(sdl_dinput_t *di)
{
   memset(di->joy_state, 0, sizeof(di->joy_state));
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (di->joypad[i])
      {
         if (FAILED(IDirectInputDevice8_Poll(di->joypad[i])))
         {
            IDirectInputDevice8_Acquire(di->joypad[i]);
            continue;
         }

         IDirectInputDevice8_GetDeviceState(di->joypad[i], sizeof(DIJOYSTATE2), &di->joy_state[i]);
      }
   }
}

