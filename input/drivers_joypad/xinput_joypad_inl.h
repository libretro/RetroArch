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

#ifndef __XINPUT_JOYPAD_INL_H
#define __XINPUT_JOYPAD_INL_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
static bool load_xinput_dll(void)
{
   const char *version = "1.4";
   /* Find the correct path to load the DLL from.
    * Usually this will be from the system directory,
    * but occasionally a user may wish to use a third-party
    * wrapper DLL (such as x360ce); support these by checking
    * the working directory first.
    *
    * No need to check for existance as we will be checking dylib_load's
    * success anyway.
    */

   g_xinput_dll = dylib_load("xinput1_4.dll");
   if (!g_xinput_dll)
   {
      g_xinput_dll = dylib_load("xinput1_3.dll");
      version = "1.3";
   }

   if (!g_xinput_dll)
   {
      RARCH_ERR("[XInput]: Failed to load XInput, ensure DirectX and controller drivers are up to date.\n");
      return false;
   }

   RARCH_LOG("[XInput]: Found XInput v%s.\n", version);
   return true;
}
#endif

static int32_t xinput_joypad_button_state(
      unsigned xuser, uint16_t btn_word,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir  = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      switch (hat_dir)
      {
         case HAT_UP_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_UP);
         case HAT_DOWN_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_DOWN);
         case HAT_LEFT_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_LEFT);
         case HAT_RIGHT_MASK:
            return (btn_word & XINPUT_GAMEPAD_DPAD_RIGHT);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < g_xinput_num_buttons)
      return (btn_word & button_index_to_bitmap_code[joykey]);
   return 0;
}

static int16_t xinput_joypad_axis_state(
      XINPUT_GAMEPAD *pad,
      unsigned port, uint32_t joyaxis)
{
   int16_t val         = 0;
   int     axis        = -1;
   bool is_neg         = false;
   bool is_pos         = false;
   /* triggers (axes 4,5) cannot be negative */
   if (AXIS_NEG_GET(joyaxis) <= 3)
   {
      axis             = AXIS_NEG_GET(joyaxis);
      is_neg           = true;
   }
   else if (AXIS_POS_GET(joyaxis) <= 5)
   {
      axis             = AXIS_POS_GET(joyaxis);
      is_pos           = true;
   }
   else
      return 0;

   switch (axis)
   {
      case 0:
         val = pad->sThumbLX;
         break;
      case 1:
         val = pad->sThumbLY;
         break;
      case 2:
         val = pad->sThumbRX;
         break;
      case 3:
         val = pad->sThumbRY;
         break;
      case 4:
         val = pad->bLeftTrigger  * 32767 / 255;
         break; /* map 0..255 to 0..32767 */
      case 5:
         val = pad->bRightTrigger * 32767 / 255;
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   /* Clamp to avoid overflow error. */
   else if (val == -32768)
      return -32767;
   return val;
}

#endif
