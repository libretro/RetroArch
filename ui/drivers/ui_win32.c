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

#ifndef _WIN32_IE
#define _WIN32_IE 0x0300
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
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../frontend/drivers/platform_win32.h"

#include "ui_win32.h"

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
      bool translated_accelerator = main_window.hwnd == msg.hwnd && TranslateAccelerator(msg.hwnd, window_accelerators, &msg) != 0;

      if (!translated_accelerator)
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
   }
}

static ui_application_t ui_application_win32 = {
   ui_application_win32_initialize,
   ui_application_win32_process_events,
   NULL,
   false,
   "win32"
};

static void* ui_window_win32_init(void)
{
   return NULL;
}

static void ui_window_win32_destroy(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   DestroyWindow(window->hwnd);
}

static void ui_window_win32_set_focused(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   SetFocus(window->hwnd);
}

static void ui_window_win32_set_visible(void *data,
        bool set_visible)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   ShowWindow(window->hwnd, set_visible ? SW_SHOWNORMAL : SW_HIDE);
}

static void ui_window_win32_set_title(void *data, char *buf)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
#ifdef LEGACY_WIN32
   char         *title_local = utf8_to_local_string_alloc(buf);
   SetWindowText(window->hwnd, title_local);
#else
   wchar_t      *title_local = utf8_to_utf16_string_alloc(buf);
   SetWindowTextW(window->hwnd, title_local);
#endif
   free(title_local);
}

void ui_window_win32_set_droppable(void *data, bool droppable)
{
   /* Minimum supported client: Windows XP, 
    * minimum supported server: Windows 2000 Server */
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   if (DragAcceptFiles_func)
      DragAcceptFiles_func(window->hwnd, droppable);
}

static bool ui_window_win32_focused(void *data)
{
   ui_window_win32_t *window = (ui_window_win32_t*)data;
   return (GetForegroundWindow() == window->hwnd);
}

static ui_window_t ui_window_win32 = {
   ui_window_win32_init,
   ui_window_win32_destroy,
   ui_window_win32_set_focused,
   ui_window_win32_set_visible,
   ui_window_win32_set_title,
   ui_window_win32_set_droppable,
   ui_window_win32_focused,
   "win32"
};

static enum ui_msg_window_response ui_msg_window_win32_response(
      ui_msg_window_state *state, UINT response)
{
	switch (response)
	{
	   case IDOK:
		return UI_MSG_RESPONSE_OK;
	   case IDCANCEL:
		return UI_MSG_RESPONSE_CANCEL;
	   case IDYES:
	    return UI_MSG_RESPONSE_YES;
	   case IDNO:
        return UI_MSG_RESPONSE_NO;
	   default:
		   break;
	}

	switch (state->buttons)
	{
	   case UI_MSG_WINDOW_OK:
		   return UI_MSG_RESPONSE_OK;
	   case UI_MSG_WINDOW_OKCANCEL:
		   return UI_MSG_RESPONSE_CANCEL;
	   case UI_MSG_WINDOW_YESNO:
		   return UI_MSG_RESPONSE_NO;
	   case UI_MSG_WINDOW_YESNOCANCEL:
		   return UI_MSG_RESPONSE_CANCEL;
	   default:
		   break;
	}

	return UI_MSG_RESPONSE_NA;
}

static UINT ui_msg_window_win32_buttons(ui_msg_window_state *state)
{
	switch (state->buttons)
	{
	   case UI_MSG_WINDOW_OK:
		   return MB_OK;
	   case UI_MSG_WINDOW_OKCANCEL:
		   return MB_OKCANCEL;
	   case UI_MSG_WINDOW_YESNO:
		   return MB_YESNO;
	   case UI_MSG_WINDOW_YESNOCANCEL:
		   return MB_YESNOCANCEL;
	}

	return 0;
}

static enum ui_msg_window_response ui_msg_window_win32_error(
      ui_msg_window_state *state)
{
   UINT flags = MB_ICONERROR | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state,
         MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

static enum ui_msg_window_response ui_msg_window_win32_information(
      ui_msg_window_state *state)
{
   UINT flags = MB_ICONINFORMATION | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state,
         MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

static enum ui_msg_window_response ui_msg_window_win32_question(
      ui_msg_window_state *state)
{
   UINT flags = MB_ICONQUESTION | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state,
         MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

static enum ui_msg_window_response ui_msg_window_win32_warning(
      ui_msg_window_state *state)
{
   UINT flags = MB_ICONWARNING | ui_msg_window_win32_buttons(state);
   return ui_msg_window_win32_response(state,
         MessageBoxA(NULL, (LPCSTR)state->text, (LPCSTR)state->title, flags));
}

ui_msg_window_t ui_msg_window_win32 = {
   ui_msg_window_win32_error,
   ui_msg_window_win32_information,
   ui_msg_window_win32_question,
   ui_msg_window_win32_warning,
   "win32"
};

static bool ui_browser_window_win32_core(
      ui_browser_window_state_t *state, bool save)
{
   OPENFILENAME ofn;
   bool            okay  = true;
   settings_t *settings  = config_get_ptr();
   bool video_fullscreen = settings->bools.video_fullscreen;

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
   ofn.Flags             =   OFN_FILEMUSTEXIST 
                           | OFN_HIDEREADONLY 
                           | OFN_NOCHANGEDIR;
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
   if (video_fullscreen)
      video_driver_show_mouse();

   if (!save && !GetOpenFileName(&ofn))
      okay = false;
   if (save && !GetSaveFileName(&ofn))
      okay = false;

   /* Full screen: Hide mouse after the file dialog */
   if (video_fullscreen)
      video_driver_hide_mouse();

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

static ui_browser_window_t ui_browser_window_win32 = {
   ui_browser_window_win32_open,
   ui_browser_window_win32_save,
   "win32"
};

static void ui_companion_win32_deinit(void *data) { } 
static void *ui_companion_win32_init(void) { return (void*)-1; }
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
   NULL, /* log_msg */
   NULL, /* is_active */
   &ui_browser_window_win32,
   &ui_msg_window_win32,
   &ui_window_win32,
   &ui_application_win32,
   "win32",
};
