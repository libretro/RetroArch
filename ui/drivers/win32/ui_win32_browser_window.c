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

#include "../../gfx/common/win32_common.h"
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include "../../ui_companion_driver.h"
#include "../../configuration.h"

static bool ui_browser_window_win32_core(ui_browser_window_state_t *state, bool save)
{
   bool okay = false;
   settings_t *settings = config_get_ptr();
   OPENFILENAME ofn;

   ofn.lStructSize       = sizeof(OPENFILENAME);
   ofn.hwndOwner         = (HWND)state->window;
   ofn.hInstance         = NULL;
   ofn.lpstrFilter       = state->filters; /* actually const */
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter    = 0;
   ofn.nFilterIndex      = 0;
   ofn.lpstrFile         = state->path;
   ofn.nMaxFile          = PATH_MAX;
   ofn.lpstrFileTitle    = NULL;
   ofn.nMaxFileTitle     = 0;
   ofn.lpstrInitialDir   = state->startdir;
   ofn.lpstrTitle        = state->title;
   ofn.Flags             = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
   ofn.nFileOffset       = 0;
   ofn.nFileExtension    = 0;
   ofn.lpstrDefExt       = "";
   ofn.lCustData         = 0;
   ofn.lpfnHook          = NULL;
   ofn.lpTemplateName    = NULL;
#if (_WIN32_WINNT >= 0x0500)
   ofn.pvReserved        = NULL;
   ofn.dwReserved        = 0;
   ofn.FlagsEx           = 0;
#endif

   /* Full Screen: Show mouse for the file dialog */
   if (settings->bools.video_fullscreen)
   {
      video_driver_show_mouse();
   }

   okay = true;
   if (!save && !GetOpenFileName(&ofn))
      okay = false;
   if (save && !GetSaveFileName(&ofn))
      okay = false;

   /* Full screen: Hide mouse after the file dialog */
   if (settings->bools.video_fullscreen)
   {
      video_driver_hide_mouse();
   }

   return okay;
}

static bool ui_browser_window_win32_open(ui_browser_window_state_t *state)
{
   return ui_browser_window_win32_core(state, false);
}

static bool ui_browser_window_win32_save(ui_browser_window_state_t *state)
{
   return ui_browser_window_win32_core(state, true);
}

ui_browser_window_t ui_browser_window_win32 = {
   ui_browser_window_win32_open,
   ui_browser_window_win32_save,
   "win32"
};
