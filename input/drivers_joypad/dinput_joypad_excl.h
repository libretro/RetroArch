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

/* ---------------------------------------------------------------------------
 * dinput_joypad_cleanup_pad
 *
 * Releases the DirectInput device and frees heap-allocated name strings
 * for the pad at the given index, then zeroes the slot.  Used both for
 * error roll-back during enumeration and for disconnect cleanup.
 * --------------------------------------------------------------------------*/
static void dinput_joypad_cleanup_pad(unsigned idx)
{
   struct dinput_joypad_data *pad = &g_pads[idx];

   if (pad->joypad)
   {
      IDirectInputDevice8_Unacquire(pad->joypad);
      IDirectInputDevice8_Release(pad->joypad);
   }

   if (pad->joy_name)
      free(pad->joy_name);
   if (pad->joy_friendly_name)
      free(pad->joy_friendly_name);

   ZeroMemory(pad, sizeof(*pad));
}


static void dinput_joypad_poll(void)
{
   int i;
   for (i = 0; i < MAX_USERS; i++)
   {
      HRESULT ret;
      struct dinput_joypad_data *pad = &g_pads[i];

      if (!pad->joypad)
         continue;

      /* Zero the entire state structure before polling.
       * This also correctly zeroes lZ, which the previous
       * field-by-field implementation accidentally omitted. */
      ZeroMemory(&pad->joy_state, sizeof(pad->joy_state));

      /* If Poll() fails, attempt to re-acquire and poll again.
       * If that also fails, skip this pad for this frame. */
      if (FAILED(IDirectInputDevice8_Poll(pad->joypad)))
         if (  FAILED(IDirectInputDevice8_Acquire(pad->joypad))
            || FAILED(IDirectInputDevice8_Poll(pad->joypad)))
            continue;

      ret = IDirectInputDevice8_GetDeviceState(pad->joypad,
            sizeof(DIJOYSTATE2), &pad->joy_state);

      if (ret == DIERR_INPUTLOST || ret == DIERR_NOTACQUIRED)
      {
         input_autoconfigure_disconnect(i, g_pads[i].joy_friendly_name);
         dinput_joypad_cleanup_pad(i);
      }
   }
}

/* ---------------------------------------------------------------------------
 * dinput_joypad_tchar_to_str
 *
 * Helper: safely converts a TCHAR string to a heap-allocated char* that
 * is valid in both ANSI and Unicode builds.
 *
 * Under ANSI builds (TCHAR == char) this is equivalent to strdup().
 * Under Unicode builds (TCHAR == wchar_t) the original code cast the
 * wide pointer directly to char*, silently producing garbage.  We use
 * WideCharToMultiByte() instead.
 *
 * Returns NULL on allocation failure; the caller must free() the result.
 * --------------------------------------------------------------------------*/
static char *dinput_joypad_tchar_to_str(const TCHAR *src)
{
#ifdef UNICODE
   int   len;
   char *out;

   if (!src)
      return NULL;

   /* Query required buffer size (includes NUL terminator). */
   len = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
   if (len <= 0)
      return NULL;

   out = (char*)malloc((size_t)len);
   if (!out)
      return NULL;

   WideCharToMultiByte(CP_UTF8, 0, src, -1, out, len, NULL, NULL);
   return out;
#else
   if (!src)
      return NULL;
   return strdup(src);
#endif
}

static BOOL CALLBACK enum_joypad_cb(const DIDEVICEINSTANCE *inst, void *p)
{
   LPDIRECTINPUTDEVICE8 *pad = NULL;

   (void)p; /* unused */

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

   /* Bug fixed: use the safe TCHAR helper instead of a raw cast that
    * would produce garbage under Unicode builds. */
   g_pads[g_joypad_cnt].joy_name          =
      dinput_joypad_tchar_to_str(inst->tszProductName);
   g_pads[g_joypad_cnt].joy_friendly_name =
      dinput_joypad_tchar_to_str(inst->tszInstanceName);

   /* VID is in the low 16 bits of guidProduct.Data1,
    * PID is in the high 16 bits. */
   g_pads[g_joypad_cnt].vid = (int32_t)(inst->guidProduct.Data1 & 0xFFFFu);
   g_pads[g_joypad_cnt].pid = (int32_t)(inst->guidProduct.Data1 >> 16);

   /* Set data format to extended joystick state.
    * If either SetDataFormat or SetCooperativeLevel fails the device
    * is unusable - release everything and move on to the next pad. */
   if (FAILED(IDirectInputDevice8_SetDataFormat(*pad, &c_dfDIJoystick2)))
   {
      dinput_joypad_cleanup_pad(g_joypad_cnt);
      return DIENUM_CONTINUE;
   }

   if (FAILED(IDirectInputDevice8_SetCooperativeLevel(*pad,
         (HWND)video_driver_window_get(),
         DISCL_EXCLUSIVE | DISCL_BACKGROUND)))
   {
      dinput_joypad_cleanup_pad(g_joypad_cnt);
      return DIENUM_CONTINUE;
   }

   IDirectInputDevice8_EnumObjects(*pad, enum_axes_cb,
         *pad, DIDFT_ABSAXIS);

   dinput_create_rumble_effects(&g_pads[g_joypad_cnt]);

   input_autoconfigure_connect(
         g_pads[g_joypad_cnt].joy_name,
         g_pads[g_joypad_cnt].joy_friendly_name,
         NULL,
         dinput_joypad.ident,
         g_joypad_cnt,
         g_pads[g_joypad_cnt].vid,
         g_pads[g_joypad_cnt].pid);

   g_joypad_cnt++;
   return DIENUM_CONTINUE;
}

static void *dinput_joypad_init(void *data)
{
   if (!dinput_init_context())
      return NULL;
   /* Zero the pad array; dinput_joypad_destroy() uses memset() on the same
    * region, so we stay consistent and avoid partial-initialisation bugs. */
   ZeroMemory(g_pads, sizeof(g_pads));
   IDirectInput8_EnumDevices(g_dinput_ctx, DI8DEVCLASS_GAMECTRL,
         enum_joypad_cb, NULL, DIEDFL_ATTACHEDONLY);
   return (void*)-1;
}

#endif /* __DINPUT_JOYPAD_EXCL_INL_H */
