/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2020 - Daniel De Matteis
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

#ifndef __DINPUT_JOYPAD_EXCL_INL_H
#define __DINPUT_JOYPAD_EXCL_INL_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <windowsx.h>
#include <dinput.h>
#include <mmsystem.h>

static void dinput_joypad_poll(void)
{
   unsigned i;
   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;
      HRESULT ret;
      struct dinput_joypad_data *pad  = &g_pads[i];
      if (!pad || !pad->joypad)
         continue;

      pad->joy_state.lX               = 0;
      pad->joy_state.lY               = 0;
      pad->joy_state.lRx              = 0;
      pad->joy_state.lRy              = 0;
      pad->joy_state.lRz              = 0;
      pad->joy_state.rglSlider[0]     = 0;
      pad->joy_state.rglSlider[1]     = 0;
      pad->joy_state.rgdwPOV[0]       = 0;
      pad->joy_state.rgdwPOV[1]       = 0;
      pad->joy_state.rgdwPOV[2]       = 0;
      pad->joy_state.rgdwPOV[3]       = 0;
      for (j = 0; j < 128; j++)
         pad->joy_state.rgbButtons[j] = 0;

      pad->joy_state.lVX              = 0;
      pad->joy_state.lVY              = 0;
      pad->joy_state.lVZ              = 0;
      pad->joy_state.lVRx             = 0;
      pad->joy_state.lVRy             = 0;
      pad->joy_state.lVRz             = 0;
      pad->joy_state.rglVSlider[0]    = 0;
      pad->joy_state.rglVSlider[1]    = 0;
      pad->joy_state.lAX              = 0;
      pad->joy_state.lAY              = 0;
      pad->joy_state.lAZ              = 0;
      pad->joy_state.lARx             = 0;
      pad->joy_state.lARy             = 0;
      pad->joy_state.lARz             = 0;
      pad->joy_state.rglASlider[0]    = 0;
      pad->joy_state.rglASlider[1]    = 0;
      pad->joy_state.lFX              = 0;
      pad->joy_state.lFY              = 0;
      pad->joy_state.lFZ              = 0;
      pad->joy_state.lFRx             = 0;
      pad->joy_state.lFRy             = 0;
      pad->joy_state.lFRz             = 0;
      pad->joy_state.rglFSlider[0]    = 0;
      pad->joy_state.rglFSlider[1]    = 0;

      /* If this fails, something *really* bad must have happened. */
      if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         if (
                  FAILED(IDirectInputDevice8_Acquire(pad->joypad))
               || FAILED(IDirectInputDevice8_Poll(pad->joypad))
            )
            continue;

      ret = IDirectInputDevice8_GetDeviceState(pad->joypad,
            sizeof(DIJOYSTATE2), &pad->joy_state);

      if (ret == DIERR_INPUTLOST || ret == DIERR_NOTACQUIRED)
         input_autoconfigure_disconnect(i, g_pads[i].joy_friendly_name);
   }
}

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
   LPDIRECTINPUTDEVICE8 *pad = NULL;
   if (g_joypad_cnt == MAX_USERS)
      return DIENUM_STOP;

   pad = &g_pads[g_joypad_cnt].joypad;

#ifdef __cplusplus
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_ctx, inst->guidInstance, pad, NULL)))
#else
   if (FAILED(IDirectInput8_CreateDevice(
               g_dinput_ctx, &inst->guidInstance, pad, NULL)))
#endif
      return DIENUM_CONTINUE;

   g_pads[g_joypad_cnt].joy_name          = 
      strdup((const char*)inst->tszProductName);
   g_pads[g_joypad_cnt].joy_friendly_name = 
      strdup((const char*)inst->tszInstanceName);

   /* there may be more useful info in the GUID,
    * so leave this here for a while */
#if 0
   printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
   inst->guidProduct.Data1,
   inst->guidProduct.Data2,
   inst->guidProduct.Data3,
   inst->guidProduct.Data4[0],
   inst->guidProduct.Data4[1],
   inst->guidProduct.Data4[2],
   inst->guidProduct.Data4[3],
   inst->guidProduct.Data4[4],
   inst->guidProduct.Data4[5],
   inst->guidProduct.Data4[6],
   inst->guidProduct.Data4[7]);
#endif

   g_pads[g_joypad_cnt].vid = inst->guidProduct.Data1 % 0x10000;
   g_pads[g_joypad_cnt].pid = inst->guidProduct.Data1 / 0x10000;

   /* Set data format to simple joystick */
   IDirectInputDevice8_SetDataFormat(*pad, &c_dfDIJoystick2);
   IDirectInputDevice8_SetCooperativeLevel(*pad,
         (HWND)video_driver_window_get(),
         DISCL_EXCLUSIVE | DISCL_BACKGROUND);

   IDirectInputDevice8_EnumObjects(*pad, enum_axes_cb,
         *pad, DIDFT_ABSAXIS);

   dinput_create_rumble_effects(&g_pads[g_joypad_cnt]);

   input_autoconfigure_connect(
         g_pads[g_joypad_cnt].joy_name,
         g_pads[g_joypad_cnt].joy_friendly_name,
         dinput_joypad.ident,
         g_joypad_cnt,
         g_pads[g_joypad_cnt].vid,
         g_pads[g_joypad_cnt].pid);

   g_joypad_cnt++;
   return DIENUM_CONTINUE;
}

static void *dinput_joypad_init(void *data)
{
   unsigned i;

   if (!dinput_init_context())
      return NULL;

   for (i = 0; i < MAX_USERS; ++i)
   {
      g_pads[i].joy_name          = NULL;
      g_pads[i].joy_friendly_name = NULL;
   }

   IDirectInput8_EnumDevices(g_dinput_ctx, DI8DEVCLASS_GAMECTRL,
         enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);

   return (void*)-1;
}

#endif
