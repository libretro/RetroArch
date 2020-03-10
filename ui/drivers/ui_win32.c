/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifdef _MSC_VER
#pragma comment( lib, "comctl32" )
#endif

#define IDI_ICON 1

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 //_WIN32_WINNT_WIN2K
#endif

#include "../../gfx/common/win32_common.h"
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <encodings/utf.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "../ui_companion_driver.h"
#include "../../msg_hash.h"
#include "../../driver.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../frontend/drivers/platform_win32.h"

#include "ui_win32.h"

typedef struct ui_companion_win32
{
   void *empty;
} ui_companion_win32_t;

#ifndef __WINRT__
bool win32_window_init(WNDCLASSEX *wndclass,
      bool fullscreen, const char *class_name)
{
#if _WIN32_WINNT >= 0x0501
   /* Use the language set in the config for the menubar... also changes the console language. */
   SetThreadUILanguage(win32_get_langid_from_retro_lang((enum retro_language)*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)));
#endif
   wndclass->cbSize        = sizeof(WNDCLASSEX);
   wndclass->style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
   wndclass->hInstance     = GetModuleHandle(NULL);
   wndclass->hCursor       = LoadCursor(NULL, IDC_ARROW);
   wndclass->lpszClassName = (class_name != NULL) ? class_name : msg_hash_to_str(MSG_PROGRAM);
   wndclass->hIcon         = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));
   wndclass->hIconSm       = (HICON)LoadImage(GetModuleHandle(NULL),
         MAKEINTRESOURCE(IDI_ICON), IMAGE_ICON, 16, 16, 0);
   if (!fullscreen)
      wndclass->hbrBackground = (HBRUSH)COLOR_WINDOW;

   if (class_name != NULL)
      wndclass->style         |= CS_CLASSDC;

   if (!RegisterClassEx(wndclass))
      return false;

   return true;
}
#endif

static void ui_companion_win32_deinit(void *data)
{
   ui_companion_win32_t *handle = (ui_companion_win32_t*)data;

   if (handle)
      free(handle);
}

static void *ui_companion_win32_init(void)
{
   ui_companion_win32_t *handle = (ui_companion_win32_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   return handle;
}

static void ui_companion_win32_notify_content_loaded(void *data) { }
static void ui_companion_win32_toggle(void *data, bool force) { }
static void ui_companion_win32_event_command(
      void *data, enum event_command cmd) { }
static void ui_companion_win32_notify_list_pushed(void *data,
        file_list_t *list, file_list_t *menu_list) { }

ui_companion_driver_t ui_companion_win32 = {
   ui_companion_win32_init,
   ui_companion_win32_deinit,
   ui_companion_win32_toggle,
   ui_companion_win32_event_command,
   ui_companion_win32_notify_content_loaded,
   ui_companion_win32_notify_list_pushed,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   &ui_browser_window_win32,
   &ui_msg_window_win32,
   &ui_window_win32,
   &ui_application_win32,
   "win32",
};
