/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#define IDI_ICON 1

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include <windows.h>
#include <commdlg.h>

#include <file/file_path.h>
#include "../ui_companion_driver.h"


typedef struct ui_companion_win32
{
   void *empty;
} ui_companion_win32_t;

bool win32_browser(
      HWND owner,
      char *filename,
      const char *extensions,
      const char *title,
      const char *initial_dir)
{
   OPENFILENAME ofn;

   memset((void*)&ofn, 0, sizeof(OPENFILENAME));

   ofn.lStructSize     = sizeof(OPENFILENAME);
   ofn.hwndOwner       = owner;
   ofn.lpstrFilter     = extensions;
   ofn.lpstrFile       = filename;
   ofn.lpstrTitle      = title;
   ofn.lpstrInitialDir = TEXT(initial_dir);
   ofn.lpstrDefExt     = "";
   ofn.nMaxFile        = PATH_MAX;
   ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

   if (!GetOpenFileName(&ofn))
      return false;

   return true;
}

static void ui_companion_win32_deinit(void *data)
{
   ui_companion_win32_t *handle = (ui_companion_win32_t*)data;

   if (handle)
      free(handle);
}

static void *ui_companion_win32_init(void)
{
   ui_companion_win32_t *handle = (ui_companion_win32_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   return handle;
}

static int ui_companion_win32_iterate(void *data, unsigned action)
{
   (void)data;
   (void)action;

   return 0;
}

static void ui_companion_win32_notify_content_loaded(void *data)
{
   (void)data;
}

static void ui_companion_win32_toggle(void *data)
{
   (void)data;
}

static void ui_companion_win32_event_command(void *data, enum event_command cmd)
{
   (void)data;
   (void)cmd;
}

static void ui_companion_win32_notify_list_pushed(void *data,
        file_list_t *list, file_list_t *menu_list)
{
    (void)data;
    (void)list;
    (void)menu_list;
}

const ui_companion_driver_t ui_companion_win32 = {
   ui_companion_win32_init,
   ui_companion_win32_deinit,
   ui_companion_win32_iterate,
   ui_companion_win32_toggle,
   ui_companion_win32_event_command,
   ui_companion_win32_notify_content_loaded,
   ui_companion_win32_notify_list_pushed,
   NULL,
   NULL,
   NULL,
   "win32",
};
