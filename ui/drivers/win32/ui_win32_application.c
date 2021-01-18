/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../ui_companion_driver.h"
#include "../ui_win32.h"

extern ui_window_win32_t main_window;
extern HACCEL window_accelerators;

static void* ui_application_win32_initialize(void)
{
   return NULL;
}

static void ui_application_win32_process_events(void)
{
   MSG msg;
   while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
   {
      bool translatedAccelerator = false;
      translatedAccelerator = main_window.hwnd == msg.hwnd && TranslateAccelerator(msg.hwnd, window_accelerators, &msg) != 0;

      if (!translatedAccelerator)
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
   }
}

ui_application_t ui_application_win32 = {
   ui_application_win32_initialize,
   ui_application_win32_process_events,
   NULL,
   false,
   "win32"
};
