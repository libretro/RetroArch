/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include "../../input/keyboard_line.h"
#include "win32_common.h"
#include "../../input/input_common.h"

LRESULT win32_handle_keyboard_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
   uint16_t mod = 0;
   mod |= (GetKeyState(VK_SHIFT)   & 0x80) ? RETROKMOD_SHIFT : 0;
   mod |= (GetKeyState(VK_CONTROL) & 0x80) ? RETROKMOD_CTRL : 0;
   mod |= (GetKeyState(VK_MENU)    & 0x80) ? RETROKMOD_ALT : 0;
   mod |= (GetKeyState(VK_CAPITAL) & 0x81) ? RETROKMOD_CAPSLOCK : 0;
   mod |= (GetKeyState(VK_SCROLL)  & 0x81) ? RETROKMOD_SCROLLOCK : 0;
   mod |= ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x80) ? RETROKMOD_META : 0;

   switch (message)
   {
      // Seems to be hard to synchronize WM_CHAR and WM_KEYDOWN properly.
      case WM_CHAR:
      {
         input_keyboard_event(true, RETROK_UNKNOWN, wparam, mod);
         return TRUE;
      }

      case WM_KEYDOWN:
      {
         // DirectInput uses scancodes directly.
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         input_keyboard_event(true, keycode, 0, mod);
         return 0;
      }

      case WM_KEYUP:
      {
         // DirectInput uses scancodes directly.
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         input_keyboard_event(false, keycode, 0, mod);
         return 0;
      }

      case WM_SYSKEYUP:
      {
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         input_keyboard_event(false, keycode, 0, mod);
         return 0;
      }

      case WM_SYSKEYDOWN:
      {
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         input_keyboard_event(true, keycode, 0, mod);

         switch (wparam)
         {
            case VK_F10:
            case VK_MENU:
            case VK_RSHIFT:
               return 0;
         }
         break;
      }
   }

   return DefWindowProc(hwnd, message, wparam, lparam);
}

