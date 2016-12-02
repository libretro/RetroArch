/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <encodings/win32.h>

#include <windows.h>

#include "../../ui_companion_driver.h"

static bool ui_browser_window_win32_core(ui_browser_window_state_t *state, bool save)
{
   OPENFILENAME ofn = {};
   bool success = true;
#ifdef UNICODE
   size_t filters_size = (state->filters ? strlen(state->filters) : 0) + 1;
   size_t path_size = strlen(state->path) + 1;
   size_t title_size = strlen(state->title) + 1;
   size_t startdir_size = strlen(state->startdir) + 1;

   wchar_t *filters_wide = (wchar_t*)calloc(filters_size, 2);
   wchar_t *path_wide = (wchar_t*)calloc(path_size, 2);
   wchar_t *title_wide = (wchar_t*)calloc(title_size, 2);
   wchar_t *startdir_wide = (wchar_t*)calloc(startdir_size, 2);

   if (state->filters)
      MultiByteToWideChar(CP_UTF8, 0, state->filters, -1, filters_wide, filters_size);
   if (state->title)
      MultiByteToWideChar(CP_UTF8, 0, state->title, -1, title_wide, title_size);
   if (state->path)
      MultiByteToWideChar(CP_UTF8, 0, state->path, -1, path_wide, path_size);
   if (state->startdir)
      MultiByteToWideChar(CP_UTF8, 0, state->startdir, -1, startdir_wide, startdir_size);

   ofn.lpstrFilter     = filters_wide;
   ofn.lpstrFile       = path_wide;
   ofn.lpstrTitle      = title_wide;
   ofn.lpstrInitialDir = startdir_wide;
#else
   ofn.lpstrFilter     = state->filters;
   ofn.lpstrFile       = state->path;
   ofn.lpstrTitle      = state->title;
   ofn.lpstrInitialDir = state->startdir;
#endif
   ofn.lStructSize     = sizeof(OPENFILENAME);
   ofn.hwndOwner       = (HWND)state->window;
   ofn.lpstrDefExt     = TEXT("");
   ofn.nMaxFile        = PATH_MAX;
   ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

   if ( save && !GetOpenFileName(&ofn))
      success = false;
   if (!save && !GetSaveFileName(&ofn))
      success = false;

#ifdef UNICODE
   if (filters_wide)
      free(filters_wide);
   if (title_wide)
      free(title_wide);
   if (path_wide)
      free(path_wide);
   if (startdir_wide)
      free(startdir_wide);
#endif

   return success;
}

static bool ui_browser_window_win32_open(ui_browser_window_state_t *state)
{
   return ui_browser_window_win32_core(state, false);
}

static bool ui_browser_window_win32_save(ui_browser_window_state_t *state)
{
   return ui_browser_window_win32_core(state, true);
}

const ui_browser_window_t ui_browser_window_win32 = {
   ui_browser_window_win32_open,
   ui_browser_window_win32_save,
   "win32"
};
