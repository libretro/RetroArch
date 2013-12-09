/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "../../input/input_common.h"
#include "win32_common.h"

LRESULT win32_handle_keyboard_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
   switch (message)
   {
      // Seems to be hard to synchronize WM_CHAR and WM_KEYDOWN properly.
      case WM_CHAR:
      {
         if (g_extern.system.key_event)
            g_extern.system.key_event(true, RETROK_UNKNOWN, wparam, 0); // TODO: Mod state
         return TRUE;
      }

      case WM_KEYDOWN:
      {
         // DirectInput uses scancodes directly.
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         if (g_extern.system.key_event)
            g_extern.system.key_event(true, keycode, 0, 0); // TODO: Mod state
         return 0;
      }

      case WM_KEYUP:
      {
         // DirectInput uses scancodes directly.
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         if (g_extern.system.key_event)
            g_extern.system.key_event(false, keycode, 0, 0); // TODO: Mod state
         return 0;
      }

      case WM_SYSKEYUP:
      {
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         if (g_extern.system.key_event)
            g_extern.system.key_event(false, keycode, 0, 0); // TODO: Mod state
         return 0;
      }

      case WM_SYSKEYDOWN:
      {
         unsigned scancode = (lparam >> 16) & 0xff;
         unsigned keycode = input_translate_keysym_to_rk(scancode);
         if (g_extern.system.key_event)
            g_extern.system.key_event(true, keycode, 0, 0); // TODO: Mod state

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

