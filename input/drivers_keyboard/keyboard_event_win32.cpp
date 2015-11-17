/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../../general.h"
#include "../keyboard_line.h"
#include "../../gfx/common/win32_common.h"
#include "../input_common.h"
#include "../input_keymaps.h"

LRESULT win32_handle_keyboard_event(HWND hwnd, UINT message,
		WPARAM wparam, LPARAM lparam)
{
   unsigned scancode = (lparam >> 16) & 0xff;
   unsigned keycode = input_keymaps_translate_keysym_to_rk(scancode);
   uint16_t mod     = 0;
   bool keydown     = true;

   if (GetKeyState(VK_SHIFT)   & 0x80)
      mod |= RETROKMOD_SHIFT;
   if (GetKeyState(VK_CONTROL) & 0x80)
      mod |=  RETROKMOD_CTRL;
   if (GetKeyState(VK_MENU)    & 0x80)
      mod |=  RETROKMOD_ALT;
   if (GetKeyState(VK_CAPITAL) & 0x81)
      mod |= RETROKMOD_CAPSLOCK;
   if (GetKeyState(VK_SCROLL)  & 0x81)
      mod |= RETROKMOD_SCROLLOCK;
   if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x80)
      mod |= RETROKMOD_META;

   switch (message)
   {
      /* Seems to be hard to synchronize 
       * WM_CHAR and WM_KEYDOWN properly.
       */
      case WM_CHAR:
         input_keyboard_event(keydown, RETROK_UNKNOWN, wparam, mod,
               RETRO_DEVICE_KEYBOARD);
         return TRUE;

      case WM_KEYUP:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_SYSKEYDOWN:
         /* Key released? */
         if (message == WM_KEYUP || message == WM_SYSKEYUP)
            keydown = false;

         /* DirectInput uses scancodes directly. */
         input_keyboard_event(keydown, keycode, 0, mod,
               RETRO_DEVICE_KEYBOARD);

         if (message == WM_SYSKEYDOWN)
         {
            switch (wparam)
            {
               case VK_F10:
               case VK_MENU:
               case VK_RSHIFT:
                  return 0;
               default:
                  break;
            }
         }
         else
            return 0;

         break;
   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}

