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
#include "../../configuration.h"
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
   SetThreadUILanguage(win32_get_langid_from_retro_lang(*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE)));
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

static bool win32_browser(
      HWND owner,
      char *filename,
      size_t filename_size,
      const char *extensions,
      const char *title,
      const char *initial_dir)
{
   bool result = false;
   const ui_browser_window_t *browser =
      ui_companion_driver_get_browser_window_ptr();

   if (browser)
   {
      ui_browser_window_state_t browser_state;

      /* These need to be big enough to hold the
       * path/name of any file the user may select.
       * FIXME: We should really handle the
       * error case when this isn't big enough. */
      char new_title[PATH_MAX];
      char new_file[32768];

      new_title[0] = '\0';
      new_file[0] = '\0';

      if (!string_is_empty(title))
         strlcpy(new_title, title, sizeof(new_title));

      if (filename && *filename)
         strlcpy(new_file, filename, sizeof(new_file));

      /* OPENFILENAME.lpstrFilters is actually const,
       * so this cast should be safe */
      browser_state.filters  = (char*)extensions;
      browser_state.title    = new_title;
      browser_state.startdir = strdup(initial_dir);
      browser_state.path     = new_file;
      browser_state.window   = owner;

      result = browser->open(&browser_state);

      if (filename && browser_state.path)
         strlcpy(filename, browser_state.path, filename_size);

      free(browser_state.startdir);
   }

   return result;
}

LRESULT win32_menu_loop(HWND owner, WPARAM wparam)
{
   WPARAM mode         = wparam & 0xffff;
   enum event_command cmd         = CMD_EVENT_NONE;
   bool do_wm_close     = false;
   settings_t *settings = config_get_ptr();

   switch (mode)
   {
      case ID_M_LOAD_CORE:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            char    *title_cp       = NULL;
            size_t converted        = 0;
            const char *extensions  = "Libretro core (.dll)\0*.dll\0All Files\0*.*\0\0";
            const char *title       = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST);
            const char *initial_dir = settings->paths.directory_libretro;

            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
            wchar_t *title_wide     = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp             = utf16_to_utf8_string_alloc(title_wide);

            if (!win32_browser(owner, win32_file, sizeof(win32_file),
                     extensions, title_cp, initial_dir))
            {
               if (title_wide)
                  free(title_wide);
               if (title_cp)
                  free(title_cp);
               break;
            }

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);
            path_set(RARCH_PATH_CORE, win32_file);
            cmd         = CMD_EVENT_LOAD_CORE;
         }
         break;
      case ID_M_LOAD_CONTENT:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            char *title_cp          = NULL;
            size_t converted        = 0;
            const char *extensions  = "All Files (*.*)\0*.*\0\0";
            const char *title       = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST);
            const char *initial_dir = settings->paths.directory_menu_content;

            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
            wchar_t *title_wide     = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp             = utf16_to_utf8_string_alloc(title_wide);

            if (!win32_browser(owner, win32_file, sizeof(win32_file),
                     extensions, title_cp, initial_dir))
            {
               if (title_wide)
                  free(title_wide);
               if (title_cp)
                  free(title_cp);
               break;
            }

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);
            win32_load_content_from_gui(win32_file);
         }
         break;
      case ID_M_RESET:
         cmd = CMD_EVENT_RESET;
         break;
      case ID_M_MUTE_TOGGLE:
         cmd = CMD_EVENT_AUDIO_MUTE_TOGGLE;
         break;
      case ID_M_MENU_TOGGLE:
         cmd = CMD_EVENT_MENU_TOGGLE;
         break;
      case ID_M_PAUSE_TOGGLE:
         cmd = CMD_EVENT_PAUSE_TOGGLE;
         break;
      case ID_M_LOAD_STATE:
         cmd = CMD_EVENT_LOAD_STATE;
         break;
      case ID_M_SAVE_STATE:
         cmd = CMD_EVENT_SAVE_STATE;
         break;
      case ID_M_DISK_CYCLE:
         cmd = CMD_EVENT_DISK_EJECT_TOGGLE;
         break;
      case ID_M_DISK_NEXT:
         cmd = CMD_EVENT_DISK_NEXT;
         break;
      case ID_M_DISK_PREV:
         cmd = CMD_EVENT_DISK_PREV;
         break;
      case ID_M_FULL_SCREEN:
         cmd = CMD_EVENT_FULLSCREEN_TOGGLE;
         break;
      case ID_M_MOUSE_GRAB:
         cmd = CMD_EVENT_GRAB_MOUSE_TOGGLE;
         break;
      case ID_M_TAKE_SCREENSHOT:
         cmd = CMD_EVENT_TAKE_SCREENSHOT;
         break;
      case ID_M_QUIT:
         do_wm_close = true;
         break;
      case ID_M_TOGGLE_DESKTOP:
         cmd = CMD_EVENT_UI_COMPANION_TOGGLE;
         break;
      default:
         if (mode >= ID_M_WINDOW_SCALE_1X && mode <= ID_M_WINDOW_SCALE_10X)
         {
            unsigned idx = (mode - (ID_M_WINDOW_SCALE_1X-1));
            rarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);
            cmd = CMD_EVENT_RESIZE_WINDOWED_SCALE;
         }
         else if (mode == ID_M_STATE_INDEX_AUTO)
         {
            signed idx = -1;
            configuration_set_int(
                  settings, settings->ints.state_slot, idx);
         }
         else if (mode >= (ID_M_STATE_INDEX_AUTO+1)
               && mode <= (ID_M_STATE_INDEX_AUTO+10))
         {
            signed idx = (mode - (ID_M_STATE_INDEX_AUTO+1));
            configuration_set_int(
                  settings, settings->ints.state_slot, idx);
         }
         break;
   }

   if (cmd != CMD_EVENT_NONE)
      command_event(cmd, NULL);

   if (do_wm_close)
      PostMessage(owner, WM_CLOSE, 0, 0);

   return 0L;
}

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

static void ui_companion_win32_notify_content_loaded(void *data)
{
   (void)data;
}

static void ui_companion_win32_toggle(void *data, bool force)
{
   (void)data;
   (void)force;
}

static void ui_companion_win32_event_command(
      void *data, enum event_command cmd)
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
