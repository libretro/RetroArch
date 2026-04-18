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
#define _WIN32_WINNT 0x0500 /* _WIN32_WINNT_WIN2K */
#endif

#ifndef _WIN32_IE
#define _WIN32_IE 0x0300
#endif

#include "../../gfx/common/win32_common.h"
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#ifdef HAVE_THREADS
#include <objbase.h>
#endif

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <encodings/utf.h>
#include <string/stdstring.h>
#include <compat/strl.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../ui_companion_driver.h"
#include "../../msg_hash.h"
#include "../../paths.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"
#include "../../frontend/drivers/platform_win32.h"

#include "ui_win32.h"

#include "../../core_info.h"
#include "../../tasks/task_content.h"
#include "../../input/input_keymaps.h"
#include <shellapi.h>
#include <ctype.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_THREADS
/* Which action the threaded file-browser should trigger.
 * Set by win32_browser(), read by ui_browser_window_win32_core(). */
static enum win32_browser_mode g_win32_browser_mode =
   WIN32_BROWSER_MODE_LOAD_CONTENT;
#endif

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

#ifdef HAVE_THREADS
/**
 * Threaded file-dialog.
 *
 * GetOpenFileName / GetSaveFileName are blocking calls that run their
 * own message loop.  When called on the main thread the RetroArch
 * window stops updating (video freezes, audio may stutter/stop).
 *
 * We avoid that by running the dialog on a short-lived worker thread.
 * The main thread continues its normal runloop_iterate() cycle.
 * When the dialog closes, the worker thread posts a WM_USER message
 * to the main window carrying a heap-allocated result so the main
 * thread can finish loading the content/core.
 *
 * Message constants, the mode enum, and the thread-data struct are
 * declared in ui_win32.h so that win32_common.c can handle the
 * async completion.
 */

static void ui_browser_window_win32_thread(void *userdata)
{
   OPENFILENAME ofn;
   win32_browser_thread_data_t *data =
      (win32_browser_thread_data_t *)userdata;
   bool ret = false;

   /* COM must be initialised per-thread for the shell dialogs. */
   CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

   memset(&ofn, 0, sizeof(ofn));
   ofn.lStructSize       = sizeof(OPENFILENAME);
   /* No owner → non-modal so the main window stays responsive. */
   ofn.hwndOwner         = NULL;
   ofn.hInstance         = NULL;
   ofn.lpstrFilter       = data->filters;
   ofn.lpstrCustomFilter = NULL;
   ofn.nMaxCustFilter    = 0;
   ofn.nFilterIndex      = 0;
   ofn.lpstrFile         = data->path;
   ofn.nMaxFile          = sizeof(data->path);
   ofn.lpstrFileTitle    = NULL;
   ofn.nMaxFileTitle     = 0;
   ofn.lpstrInitialDir   = data->startdir;
   ofn.lpstrTitle        = data->title;
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

   if (!data->is_save)
      ret = GetOpenFileName(&ofn) ? true : false;
   else
      ret = GetSaveFileName(&ofn) ? true : false;

   data->result = ret;

   /* Notify the main window.  LPARAM carries the heap pointer;
    * the main-thread handler is responsible for freeing it.     */
   PostMessage(data->owner, ret
         ? WM_BROWSER_OPEN_RESULT
         : WM_BROWSER_CANCELLED,
         (WPARAM)0, (LPARAM)data);

   CoUninitialize();
}

static bool ui_browser_window_win32_core(
      ui_browser_window_state_t *state, bool save)
{
   win32_browser_thread_data_t *td = NULL;
   settings_t *settings            = config_get_ptr();
   video_driver_state_t *video_st  = video_state_get_ptr();
   bool video_fullscreen           = settings->bools.video_fullscreen;
   sthread_t *thread               = NULL;

   td = (win32_browser_thread_data_t *)calloc(1, sizeof(*td));
   if (!td)
      return false;

   /* Copy the inputs into the heap block the thread will use. */
   if (state->filters)
      strlcpy(td->filters, state->filters, sizeof(td->filters));
   if (state->title)
      strlcpy(td->title, state->title, sizeof(td->title));
   if (state->startdir)
      strlcpy(td->startdir, state->startdir, sizeof(td->startdir));

   td->path[0] = '\0';
   if (state->path && *state->path)
      strlcpy(td->path, state->path, sizeof(td->path));

   td->owner    = (HWND)state->window;
   td->is_save  = save;
   td->mode     = g_win32_browser_mode;
   td->result   = false;

   /* Full Screen: Show mouse for the file dialog */
   if (video_fullscreen)
   {
      if (     video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, true);
   }

   thread = sthread_create(ui_browser_window_win32_thread, td);
   if (!thread)
   {
      free(td);
      return false;
   }

   /* Detach — the thread frees nothing; the main-thread WM_USER
    * handler owns *td and will free it after processing.          */
   sthread_detach(thread);

   /* Return false here: the caller must NOT act on state->path yet.
    * The real result arrives asynchronously via WM_BROWSER_OPEN_RESULT. */
   return false;
}
#else
/* Non-threaded fallback: run the file dialog synchronously on the
 * main thread.  This blocks the window while the dialog is open
 * (the original behavior before the threaded dialog was added). */
static bool ui_browser_window_win32_core(
      ui_browser_window_state_t *state, bool save)
{
   OPENFILENAME ofn;
   bool            ret   = true;
   settings_t *settings  = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
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
   {
      if (     video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, true);
   }

   if (!save && !GetOpenFileName(&ofn))
      ret = false;
   if (save && !GetSaveFileName(&ofn))
      ret = false;

   /* Full screen: Hide mouse after the file dialog */
   if (video_fullscreen)
   {
      if (     video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, false);
   }

   return ret;
}
#endif /* HAVE_THREADS */

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


/* ================================================================
 * UI FUNCTIONS — moved from gfx/common/win32_common.c
 *
 * These handle content loading, file browser dialogs, the Win32
 * menu bar, menu localisation, and the "Pick Core" dialog.
 * They live here because they are UI concerns, not window/gfx
 * infrastructure.
 * ================================================================ */

#if defined(_MSC_VER) && _MSC_VER <= 1200
#define INT_PTR_COMPAT int
#else
#define INT_PTR_COMPAT INT_PTR
#endif

static INT_PTR_COMPAT CALLBACK pick_core_proc(
      HWND hDlg, UINT message,
      WPARAM wParam, LPARAM lParam)
{
   size_t list_size;

   switch (message)
   {
      case WM_INITDIALOG:
         {
            const core_info_t *core_info     = NULL;
            core_info_list_t *core_info_list = NULL;
            /* Add items to list. */
            core_info_get_list(&core_info_list);
            core_info_list_get_supported_cores(core_info_list,
                  path_get(RARCH_PATH_CONTENT), &core_info, &list_size);
            if (list_size != 0)
            {
               size_t i;
               HWND hwndList = GetDlgItem(hDlg, ID_CORELISTBOX);
               for (i = 0; i < list_size; i++)
                  SendMessage(hwndList, LB_ADDSTRING, 0,
                        (LPARAM)core_info[i].display_name);
               /* Select the first item in the list */
               SendMessage(hwndList, LB_SETCURSEL, 0, 0);
               path_set(RARCH_PATH_CORE, core_info[0].path);
               SetFocus(hwndList);
            }
            return TRUE;
         }

      case WM_COMMAND:
         switch (LOWORD(wParam))
         {
            case IDOK:
            case IDCANCEL:
               EndDialog(hDlg, LOWORD(wParam));
               break;
            case ID_CORELISTBOX:
               switch (HIWORD(wParam))
               {
                  case LBN_SELCHANGE:
                     {
                        const core_info_t *core_info     = NULL;
                        core_info_list_t *core_info_list = NULL;
                        HWND hwndList = GetDlgItem(hDlg, ID_CORELISTBOX);
                        int lbItem    = (int)
                           SendMessage(hwndList, LB_GETCURSEL, 0, 0);

                        core_info_get_list(&core_info_list);
                        core_info_list_get_supported_cores(core_info_list,
                              path_get(RARCH_PATH_CONTENT), &core_info,
                              &list_size);
                        if (lbItem < 0 || (size_t)lbItem >= list_size)
                           break;
                        path_set(RARCH_PATH_CORE, core_info[lbItem].path);
                     }
                     break;
               }
               return TRUE;
         }
   }
   return FALSE;
}

bool win32_load_content_from_gui(const char *szFilename)
{
   /* poll list of current cores */
   core_info_list_t *core_info_list = NULL;

   core_info_get_list(&core_info_list);

   if (core_info_list)
   {
      size_t list_size;
      content_ctx_info_t content_info  = { 0 };
      const core_info_t *core_info     = NULL;
      core_info_list_get_supported_cores(core_info_list,
            (const char*)szFilename, &core_info, &list_size);

      if (list_size)
      {
         path_set(RARCH_PATH_CONTENT, szFilename);

         if (!path_is_empty(RARCH_PATH_CONTENT))
         {
            unsigned i;
            core_info_t *current_core = NULL;
            core_info_get_current_core(&current_core);

            /*we already have path for libretro core */
            for (i = 0; i < list_size; i++)
            {
               const core_info_t *info = (const core_info_t*)&core_info[i];

               if (string_is_equal(path_get(RARCH_PATH_CORE), info->path))
               {
                  /* Our previous core supports the current rom */
                  task_push_load_content_with_current_core_from_companion_ui(
                        NULL,
                        &content_info,
                        CORE_TYPE_PLAIN,
                        NULL, NULL);
                  return true;
               }
            }
         }

         /* Poll for cores for current rom since none exist. */
         if (list_size == 1)
         {
            /*pick core that only exists and is bound to work. Ish. */
            const core_info_t *info = (const core_info_t*)&core_info[0];

            if (info)
            {
               task_push_load_content_with_new_core_from_companion_ui(
                     info->path, NULL, NULL, NULL, NULL, &content_info, NULL, NULL);
               return true;
            }
         }
         else
         {
            bool            okay              = false;
            settings_t *settings              = config_get_ptr();
            bool video_is_fs                  = settings->bools.video_fullscreen;
            video_driver_state_t *video_st    = video_state_get_ptr();
            bool needs_cursor                 =    video_is_fs
                                               && video_st->poke
                                               && video_st->poke->show_mouse;

            if (needs_cursor)
               video_st->poke->show_mouse(video_st->data, true);

            /* Pick one core that could be compatible. */
            if (win32_resources_pick_core_dialog(
                     main_window.hwnd, pick_core_proc) == IDOK)
            {
               task_push_load_content_with_current_core_from_companion_ui(
                     NULL, &content_info, CORE_TYPE_PLAIN, NULL, NULL);
               okay = true;
            }

            if (needs_cursor)
               video_st->poke->show_mouse(video_st->data, false);

            return okay;
         }
      }
   }
   return false;
}

#ifdef LEGACY_WIN32
bool win32_drag_query_file(HWND hwnd, WPARAM wparam)
{
   if (DragQueryFile((HDROP)wparam, 0xFFFFFFFF, NULL, 0))
   {
      char szFilename[1024];
      szFilename[0]    = '\0';
      DragQueryFile((HDROP)wparam, 0, szFilename, sizeof(szFilename));
      return win32_load_content_from_gui(szFilename);
   }
   return false;
}
#else
bool win32_drag_query_file(HWND hwnd, WPARAM wparam)
{
   if (DragQueryFileW((HDROP)wparam, 0xFFFFFFFF, NULL, 0))
   {
      wchar_t wszFilename[4096];
      bool ret        = false;
      char *szFilename = NULL;
      wszFilename[0]   = L'\0';

      DragQueryFileW((HDROP)wparam, 0, wszFilename,
            sizeof(wszFilename) / sizeof(wszFilename[0]));
      szFilename = utf16_to_utf8_string_alloc(wszFilename);
      ret        = win32_load_content_from_gui(szFilename);
      if (szFilename)
         free(szFilename);
      if (ret)
         return true;
   }
   return false;
}
#endif

#ifdef HAVE_THREADS

static bool win32_browser(
      HWND owner,
      char *filename,
      size_t filename_size,
      const char *extensions,
      const char *title,
      const char *initial_dir,
      enum win32_browser_mode mode)
{
   bool result = false;
   const ui_browser_window_t *browser =
      ui_companion_driver_get_browser_window_ptr();

   if (browser)
   {
      ui_browser_window_state_t browser_state;

      /* These need to be big enough to hold the
       * path/name of any file the user may select. */
      char new_title[PATH_MAX];
      char new_file[PATH_MAX_LENGTH]; /* MAX_PATH-length path buffer */
      char new_dir[DIR_MAX_LENGTH];

      new_title[0] = '\0';
      new_file[0]  = '\0';
      new_dir[0]   = '\0';

      if (title && *title)
         strlcpy(new_title, title, sizeof(new_title));

      if (filename && *filename)
         strlcpy(new_file, filename, sizeof(new_file));

      if (initial_dir && *initial_dir)
         strlcpy(new_dir, initial_dir, sizeof(new_dir));

      /* OPENFILENAME.lpstrFilters is actually const,
       * so this cast should be safe */
      browser_state.filters  = (char*)extensions;
      browser_state.title    = new_title;
      browser_state.startdir = new_dir;
      browser_state.path     = new_file;
      browser_state.window   = owner;

      /* Stash the mode so the threaded dialog can tag its result. */
      g_win32_browser_mode   = mode;

      result = browser->open(&browser_state);

      /* With the threaded dialog, browser->open() spawns the thread
       * and returns false immediately.  The real result arrives via
       * WM_BROWSER_OPEN_RESULT.  The copy below is harmless but
       * will not contain anything useful in the threaded path. */
      if (filename && browser_state.path)
         strlcpy(filename, browser_state.path, filename_size);
   }

   return result;
}
#else
/* Non-threaded fallback: the dialog blocks the main thread and the
 * result is returned synchronously to the caller (old behavior). */
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
       * path/name of any file the user may select. */
      char new_title[PATH_MAX];
      char new_file[PATH_MAX_LENGTH];
      char new_dir[DIR_MAX_LENGTH];

      new_title[0] = '\0';
      new_file[0]  = '\0';
      new_dir[0]   = '\0';

      if (title && *title)
         strlcpy(new_title, title, sizeof(new_title));

      if (filename && *filename)
         strlcpy(new_file, filename, sizeof(new_file));

      if (initial_dir && *initial_dir)
         strlcpy(new_dir, initial_dir, sizeof(new_dir));

      /* OPENFILENAME.lpstrFilters is actually const,
       * so this cast should be safe */
      browser_state.filters  = (char*)extensions;
      browser_state.title    = new_title;
      browser_state.startdir = new_dir;
      browser_state.path     = new_file;
      browser_state.window   = owner;

      result = browser->open(&browser_state);

      /* browser->open() may update browser_state.path in-place;
       * copy the final path back to the caller's buffer. */
      if (filename && browser_state.path)
         strlcpy(filename, browser_state.path, filename_size);
   }

   return result;
}
#endif /* HAVE_THREADS */

LRESULT win32_menu_loop(HWND owner, WPARAM wparam)
{
   WPARAM mode            = wparam & 0xffff;

   switch (mode)
   {
      case ID_M_LOAD_CORE:
         {
#ifndef HAVE_THREADS
            content_ctx_info_t content_info;
#endif
            char win32_file[PATH_MAX_LENGTH] = {0};
            settings_t *settings    = config_get_ptr();
            char    *title_cp       = NULL;
            const char *extensions  = "Libretro core (.dll)\0*.dll\0All Files\0*.*\0\0";
            const char *title       = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST);
            const char *initial_dir = settings->paths.directory_libretro;

            wchar_t *title_wide     = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp             = utf16_to_utf8_string_alloc(title_wide);

#ifdef HAVE_THREADS
            /* Fire-and-forget: the dialog runs on a worker thread.
             * The actual core-load happens in WM_BROWSER_OPEN_RESULT. */
            win32_browser(owner, win32_file, sizeof(win32_file),
                     extensions, title_cp, initial_dir,
                     WIN32_BROWSER_MODE_LOAD_CORE);

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);
#else
            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
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
            content_info.argc        = 0;
            content_info.argv        = NULL;
            content_info.args        = NULL;
            content_info.environ_get = NULL;
            if (task_push_load_new_core(
                     win32_file, NULL,
                     &content_info,
                     CORE_TYPE_PLAIN,
                     NULL, NULL))
            {
#ifdef HAVE_MENU
               /* Force the main menu to rebuild so that entries which
                * depend on a loaded core (Start Core for contentless
                * cores, Unload Core, etc.) appear on the fly instead
                * of only after the next user-driven menu interaction. */
               struct menu_state *menu_st = menu_state_get_ptr();
               menu_st->flags            |=
                     MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                   | MENU_ST_FLAG_PREVENT_POPULATE;
#endif
            }
#endif
         }
         break;
      case ID_M_LOAD_CONTENT:
         {
            char win32_file[PATH_MAX_LENGTH] = {0};
            char *title_cp          = NULL;
            wchar_t *title_wide     = NULL;
            const char *extensions  = "All Files (*.*)\0*.*\0\0";
            const char *title       = msg_hash_to_str(
                  MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST);
            settings_t *settings    = config_get_ptr();
            const char *initial_dir = settings->paths.directory_menu_content;
#ifndef HAVE_THREADS
            bool browser            = true;
#endif

            /* Menubar accelerator hotkey is hijacked always, therefore must
             * press the keyboard event manually when blocking the accelerator. */
            if (     !settings->bools.ui_menubar_enable
                  || (!settings->bools.video_windowed_fullscreen && settings->bools.video_fullscreen))
            {
               input_keyboard_event(true, RETROK_o,
                     0, RETROK_LCTRL, RETRO_DEVICE_KEYBOARD);
               break;
            }

            /* Convert UTF8 to UTF16, then back to the
             * local code page.
             * This is needed for proper multi-byte
             * string display until Unicode is
             * fully supported.
             */
            title_wide = utf8_to_utf16_string_alloc(title);

            if (title_wide)
               title_cp = utf16_to_utf8_string_alloc(title_wide);

#ifdef HAVE_THREADS
            /* Fire-and-forget: the dialog runs on a worker thread.
             * The actual content-load happens in WM_BROWSER_OPEN_RESULT. */
            win32_browser(owner, win32_file, sizeof(win32_file),
                  extensions, title_cp, initial_dir,
                  WIN32_BROWSER_MODE_LOAD_CONTENT);

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);
#else
            browser = win32_browser(owner, win32_file, sizeof(win32_file),
                  extensions, title_cp, initial_dir);

            if (title_wide)
               free(title_wide);
            if (title_cp)
               free(title_cp);

            if (browser)
               win32_load_content_from_gui(win32_file);
#endif
         }
         break;
      case ID_M_RESET:
         command_event(CMD_EVENT_RESET, NULL);
         break;
      case ID_M_MUTE_TOGGLE:
         command_event(CMD_EVENT_AUDIO_MUTE_TOGGLE, NULL);
         break;
      case ID_M_MENU_TOGGLE:
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
         break;
      case ID_M_PAUSE_TOGGLE:
         command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
         break;
      case ID_M_LOAD_STATE:
         command_event(CMD_EVENT_LOAD_STATE, NULL);
         break;
      case ID_M_SAVE_STATE:
         command_event(CMD_EVENT_SAVE_STATE, NULL);
         break;
      case ID_M_DISK_CYCLE:
         command_event(CMD_EVENT_DISK_EJECT_TOGGLE, NULL);
         break;
      case ID_M_DISK_NEXT:
         command_event(CMD_EVENT_DISK_NEXT, NULL);
         break;
      case ID_M_DISK_PREV:
         command_event(CMD_EVENT_DISK_PREV, NULL);
         break;
      case ID_M_FULL_SCREEN:
         {
            /* Menubar accelerator hotkey is hijacked always, therefore must
             * press the keyboard event manually when blocking the accelerator. */
            settings_t *settings    = config_get_ptr();
            if (     !settings->bools.ui_menubar_enable
                  || (!settings->bools.video_windowed_fullscreen && settings->bools.video_fullscreen))
            {
               input_keyboard_event(true, RETROK_RETURN,
                     0, RETROK_LALT, RETRO_DEVICE_KEYBOARD);
               break;
            }
         }
         command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);
         break;
      case ID_M_MOUSE_GRAB:
         command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
         break;
      case ID_M_TAKE_SCREENSHOT:
         command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);
         break;
      case ID_M_QUIT:
         PostMessage(owner, WM_CLOSE, 0, 0);
         break;
      case ID_M_TOGGLE_DESKTOP:
         command_event(CMD_EVENT_UI_COMPANION_TOGGLE, NULL);
         break;
      default:
         if (mode >= ID_M_WINDOW_SCALE_1X && mode <= ID_M_WINDOW_SCALE_10X)
         {
            unsigned idx = (mode - (ID_M_WINDOW_SCALE_1X-1));
            retroarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);
            command_event(CMD_EVENT_RESIZE_WINDOWED_SCALE, NULL);
         }
         else if (mode == ID_M_STATE_INDEX_AUTO)
         {
            signed           idx = -1;
            settings_t *settings = config_get_ptr();
            configuration_set_int(
                  settings, settings->ints.state_slot, idx);
         }
         else if (mode >= (ID_M_STATE_INDEX_AUTO+1)
               && mode <= (ID_M_STATE_INDEX_AUTO+10))
         {
            signed           idx = (mode - (ID_M_STATE_INDEX_AUTO+1));
            settings_t *settings = config_get_ptr();
            configuration_set_int(
                  settings, settings->ints.state_slot, idx);
         }
         break;
   }

   return 0L;
}

#ifdef HAVE_MENU
/* Given a Win32 Resource ID, return a RetroArch menu ID (for renaming the menu item) */
static enum msg_hash_enums menu_id_to_label_enum(unsigned int menuId)
{
   switch (menuId)
   {
      case ID_M_LOAD_CONTENT:
         return MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST;
      case ID_M_RESET:
         return MENU_ENUM_LABEL_VALUE_RESTART_CONTENT;
      case ID_M_QUIT:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY;
      case ID_M_MENU_TOGGLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE;
      case ID_M_PAUSE_TOGGLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE;
      case ID_M_LOAD_CORE:
         return MENU_ENUM_LABEL_VALUE_CORE_LIST;
      case ID_M_LOAD_STATE:
         return MENU_ENUM_LABEL_VALUE_LOAD_STATE;
      case ID_M_SAVE_STATE:
         return MENU_ENUM_LABEL_VALUE_SAVE_STATE;
      case ID_M_DISK_CYCLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE;
      case ID_M_DISK_NEXT:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT;
      case ID_M_DISK_PREV:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV;
      case ID_M_FULL_SCREEN:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY;
      case ID_M_MOUSE_GRAB:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE;
      case ID_M_TAKE_SCREENSHOT:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT;
      case ID_M_MUTE_TOGGLE:
         return MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE;
      default:
         break;
   }

   return MSG_UNKNOWN;
}

/* Given a RetroArch menu ID, get its shortcut key (meta key) */
static unsigned int menu_id_to_meta_key(unsigned int menu_id)
{
   switch (menu_id)
   {
      case ID_M_RESET:
         return RARCH_RESET;
      case ID_M_QUIT:
         return RARCH_QUIT_KEY;
      case ID_M_MENU_TOGGLE:
         return RARCH_MENU_TOGGLE;
      case ID_M_PAUSE_TOGGLE:
         return RARCH_PAUSE_TOGGLE;
      case ID_M_LOAD_STATE:
         return RARCH_LOAD_STATE_KEY;
      case ID_M_SAVE_STATE:
         return RARCH_SAVE_STATE_KEY;
      case ID_M_DISK_CYCLE:
         return RARCH_DISK_EJECT_TOGGLE;
      case ID_M_DISK_NEXT:
         return RARCH_DISK_NEXT;
      case ID_M_DISK_PREV:
         return RARCH_DISK_PREV;
      case ID_M_FULL_SCREEN:
         return RARCH_FULLSCREEN_TOGGLE_KEY;
      case ID_M_MOUSE_GRAB:
         return RARCH_GRAB_MOUSE_TOGGLE;
      case ID_M_TAKE_SCREENSHOT:
         return RARCH_SCREENSHOT;
      case ID_M_MUTE_TOGGLE:
         return RARCH_MUTE;
      default:
         break;
   }

   return 0;
}

/* Given a short key (meta key), get its name as a string.
 * For named keys the return value points into the global
 * input_config_key_map table.  For single printable-ASCII
 * characters the name is written into the caller-supplied
 * buffer (buf, buf_size) and the return value points there.
 * Returns NULL when no name can be determined. */
static const char *win32_meta_key_to_name(unsigned int meta_key,
      char *buf, size_t buf_size)
{
   int i = 0;
   const struct retro_keybind* key = &input_config_binds[0][meta_key];
   int key_code                    = key->key;

   for (;;)
   {
      const struct input_key_map* entry = &input_config_key_map[i];
      if (!entry->str)
         break;
      if (entry->key == (enum retro_key)key_code)
         return entry->str;
      i++;
   }

   if (key_code >= 32 && key_code < 127 && buf_size >= 2)
   {
      buf[0] = (char)key_code;
      buf[1] = '\0';
      return buf;
   }
   return NULL;
}

/* Replaces Menu Item text with localized menu text,
 * and displays the current shortcut key */
void win32_localize_menu(HMENU menu)
{
#ifndef LEGACY_WIN32
   MENUITEMINFOW menu_item_info;
#else
   MENUITEMINFOA menu_item_info;
#endif
   int index = 0;

   for (;;)
   {
      enum msg_hash_enums label_enum;
      memset(&menu_item_info, 0, sizeof(menu_item_info));
      menu_item_info.cbSize     = sizeof(menu_item_info);
      menu_item_info.dwTypeData = NULL;
#if (WINVER >= 0x0500)
      menu_item_info.fMask      = MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
#else
      menu_item_info.fMask      =                            MIIM_ID | MIIM_STATE | MIIM_SUBMENU;
#endif

#ifndef LEGACY_WIN32
      if (!GetMenuItemInfoW(menu, index, true, &menu_item_info))
         break;
#else
      if (!GetMenuItemInfoA(menu, index, true, &menu_item_info))
         break;
#endif

      /* Recursion - call this on submenu items too */
      if (menu_item_info.hSubMenu)
         win32_localize_menu(menu_item_info.hSubMenu);

      label_enum = menu_id_to_label_enum(menu_item_info.wID);
      if (label_enum != MSG_UNKNOWN)
      {
         size_t final_len;
         size_t key_name_len;
#ifndef LEGACY_WIN32
         wchar_t* new_label_unicode = NULL;
#else
         char* new_label_ansi       = NULL;
#endif
         const char* new_label      = msg_hash_to_str(label_enum);
         unsigned int meta_key      = menu_id_to_meta_key(menu_item_info.wID);
         const char* new_label2     = new_label;
         const char* meta_key_name  = NULL;
         char* new_label_text       = NULL;
         char key_name_buf[2]       = {0};

         /* specific replacements:
            Load Content = "Ctrl+O"
            Fullscreen = "Alt+Enter" */
         if (label_enum ==
               MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST)
         {
            meta_key_name = "Ctrl+O";
            key_name_len  = STRLEN_CONST("Ctrl+O");
         }
         else if (label_enum ==
               MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY)
         {
            meta_key_name = "Alt+Enter";
            key_name_len  = STRLEN_CONST("Alt+Enter");
         }
         else if (meta_key != 0)
         {
            meta_key_name = win32_meta_key_to_name(meta_key,
                  key_name_buf, sizeof(key_name_buf));
            key_name_len  = meta_key_name ? strlen(meta_key_name) : 0;
         }

         /* Append localized name, tab character, and Shortcut Key */
         if (meta_key_name && string_is_not_equal(meta_key_name, "nul"))
         {
            size_t label_len = strlen(new_label);
            size_t buf_size  = label_len + key_name_len + 2;
            new_label_text   = (char*)malloc(buf_size);

            if (new_label_text)
            {
               size_t copy_len;
               new_label2              = new_label_text;
               copy_len                = strlcpy(new_label_text, new_label,
                     buf_size);
               new_label_text[  copy_len] = '\t';
               new_label_text[++copy_len] = '\0';
               strlcpy(new_label_text + copy_len, meta_key_name, buf_size - copy_len);
               /* Make first character of shortcut name uppercase */
               new_label_text[label_len + 1] = toupper(new_label_text[label_len + 1]);
            }
         }

#ifndef LEGACY_WIN32
         /* Convert string from UTF-8, then assign menu text */
         new_label_unicode         = utf8_to_utf16_string_alloc(new_label2);
         final_len                 = wcslen(new_label_unicode);
         menu_item_info.cch        = final_len;
         menu_item_info.dwTypeData = new_label_unicode;
         SetMenuItemInfoW(menu, index, true, &menu_item_info);
         free(new_label_unicode);
#else
         new_label_ansi            = utf8_to_local_string_alloc(new_label2);
         final_len                 = strlen(new_label_ansi);
         menu_item_info.cch        = final_len;
         menu_item_info.dwTypeData = new_label_ansi;
         SetMenuItemInfoA(menu, index, true, &menu_item_info);
         free(new_label_ansi);
#endif
         if (new_label_text)
            free(new_label_text);
      }
      index++;
   }
}
#endif

#ifndef __WINRT__
HMENU win32_resources_create_menu(void)
{
   HMENU menu_bar, file_menu, command_menu, window_menu;
   HMENU audio_menu, disk_menu, savestate_menu, stateindex_menu;
   HMENU scale_menu;

   menu_bar = CreateMenu();
   if (!menu_bar)
      return NULL;

   /* ---- File ---- */
   file_menu = CreatePopupMenu();
   AppendMenuA(file_menu, MF_STRING, ID_M_LOAD_CORE,    "Load Core...");
   AppendMenuA(file_menu, MF_STRING, ID_M_LOAD_CONTENT, "Load Content...");
   AppendMenuA(file_menu, MF_SEPARATOR, 0, NULL);
   AppendMenuA(file_menu, MF_STRING, ID_M_QUIT,         "Close");
   AppendMenuA(menu_bar,  MF_POPUP, (UINT_PTR)file_menu, "File");

   /* ---- Command ---- */
   command_menu = CreatePopupMenu();

   audio_menu = CreatePopupMenu();
   AppendMenuA(audio_menu, MF_STRING, ID_M_MUTE_TOGGLE, "Audio Mute Toggle");
   AppendMenuA(command_menu, MF_POPUP, (UINT_PTR)audio_menu, "Audio Options");

   disk_menu = CreatePopupMenu();
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_CYCLE, "Disk Eject Toggle");
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_PREV,  "Disk Previous");
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_NEXT,  "Disk Next");
   AppendMenuA(command_menu, MF_POPUP, (UINT_PTR)disk_menu, "Disk Options");

   savestate_menu = CreatePopupMenu();

   stateindex_menu = CreatePopupMenu();
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_AUTO, "Auto");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_0,    "0");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_1,    "1");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_2,    "2");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_3,    "3");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_4,    "4");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_5,    "5");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_6,    "6");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_7,    "7");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_8,    "8");
   AppendMenuA(stateindex_menu, MF_STRING, ID_M_STATE_INDEX_9,    "9");
   AppendMenuA(savestate_menu, MF_POPUP,
         (UINT_PTR)stateindex_menu, "State Index");

   AppendMenuA(savestate_menu, MF_STRING, ID_M_LOAD_STATE, "Load State");
   AppendMenuA(savestate_menu, MF_STRING, ID_M_SAVE_STATE, "Save State");
   AppendMenuA(command_menu, MF_POPUP,
         (UINT_PTR)savestate_menu, "Save State Options");

   AppendMenuA(command_menu, MF_STRING, ID_M_RESET,           "Reset");
   AppendMenuA(command_menu, MF_STRING, ID_M_PAUSE_TOGGLE,    "Pause Toggle");
   AppendMenuA(command_menu, MF_STRING, ID_M_MENU_TOGGLE,     "Menu Toggle");
   AppendMenuA(command_menu, MF_STRING, ID_M_TAKE_SCREENSHOT, "Take Screenshot");
   AppendMenuA(command_menu, MF_STRING, ID_M_MOUSE_GRAB,      "Mouse Grab Toggle");
   AppendMenuA(menu_bar, MF_POPUP, (UINT_PTR)command_menu, "Command");

   /* ---- Window ---- */
   window_menu = CreatePopupMenu();

   scale_menu = CreatePopupMenu();
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_1X,  "1x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_2X,  "2x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_3X,  "3x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_4X,  "4x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_5X,  "5x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_6X,  "6x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_7X,  "7x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_8X,  "8x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_9X,  "9x");
   AppendMenuA(scale_menu, MF_STRING, ID_M_WINDOW_SCALE_10X, "10x");
   AppendMenuA(window_menu, MF_POPUP, (UINT_PTR)scale_menu, "Windowed Scale");

#ifdef HAVE_QT
   AppendMenuA(window_menu, MF_STRING, ID_M_TOGGLE_DESKTOP,
         "Toggle Desktop Menu");
#endif
   AppendMenuA(window_menu, MF_STRING, ID_M_FULL_SCREEN,
         "Toggle Exclusive Full Screen");
   AppendMenuA(menu_bar, MF_POPUP, (UINT_PTR)window_menu, "Window");

   return menu_bar;
}

/* "PICK CORE" DIALOG  (replaces IDD_PICKCORE)
 * Builds DLGTEMPLATE + DLGITEMTEMPLATE in memory. */

static LPWORD align_dword(LPWORD ptr)
{
   ULONG_PTR ul = (ULONG_PTR)ptr;
   ul  = (ul + 3) & ~(ULONG_PTR)3;
   return (LPWORD)ul;
}

static LPWORD append_wstr(LPWORD ptr, const WCHAR *str)
{
   int len = (int)wcslen(str) + 1;
   memcpy(ptr, str, len * sizeof(WCHAR));
   return ptr + len;
}

int win32_resources_pick_core_dialog(HWND parent, DLGPROC dlg_proc)
{
   BYTE buf[2048];
   DLGTEMPLATE *dlg;
   LPWORD p;
   DLGITEMTEMPLATE *item;

   memset(buf, 0, sizeof(buf));
   dlg = (DLGTEMPLATE *)buf;

   dlg->style = DS_3DLOOK | DS_CENTER | DS_MODALFRAME | DS_SHELLFONT
              | WS_CAPTION | WS_VISIBLE | WS_POPUP | WS_SYSMENU;
   dlg->dwExtendedStyle = 0;
   dlg->cdit  = 4;
   dlg->x     = 0;
   dlg->y     = 0;
   dlg->cx    = 225;
   dlg->cy    = 118;

   p = (LPWORD)(dlg + 1);
   *p++ = 0;                              /* no menu       */
   *p++ = 0;                              /* default class */
   p = append_wstr(p, L"Pick Core");      /* caption       */
   *p++ = 8;                              /* font size     */
   p = append_wstr(p, L"Ms Shell Dlg");   /* font name     */

   /* Control 1: LTEXT (static label) */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | SS_LEFT;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 9;   item->y  = 12;
   item->cx = 160; item->cy = 17;
   item->id = 0;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0082;          /* STATIC class  */
   p = append_wstr(p,
         L"Please select a core to use for the content loaded.\n"
         L"Otherwise, press 'Cancel' to cancel loading.");
   *p++ = 0;                              /* no creation data */

   /* Control 2: DEFPUSHBUTTON "OK" */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 170; item->y  = 15;
   item->cx = 50;  item->cy = 14;
   item->id = IDOK;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0080;          /* BUTTON class  */
   p = append_wstr(p, L"OK");
   *p++ = 0;

   /* Control 3: PUSHBUTTON "Cancel" */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 170; item->y  = 32;
   item->cx = 50;  item->cy = 14;
   item->id = IDCANCEL;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0080;          /* BUTTON class  */
   p = append_wstr(p, L"Cancel");
   *p++ = 0;

   /* Control 4: LISTBOX (core list) */
   p    = align_dword(p);
   item = (DLGITEMTEMPLATE *)p;
   item->style           = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL
                         | LBS_NOINTEGRALHEIGHT | LBS_SORT | LBS_NOTIFY;
   item->dwExtendedStyle = WS_EX_LEFT;
   item->x  = 5;   item->y  = 55;
   item->cx = 214; item->cy = 60;
   item->id = ID_CORELISTBOX;
   p = (LPWORD)(item + 1);
   *p++ = 0xFFFF; *p++ = 0x0083;          /* LISTBOX class */
   *p++ = 0;                              /* empty title   */
   *p++ = 0;                              /* no creation data */

   return (int)DialogBoxIndirectParamW(
         GetModuleHandleW(NULL),
         dlg, parent, dlg_proc, 0);
}
#endif /* !__WINRT__ */

static void ui_companion_win32_deinit(void *data) { }
static void *ui_companion_win32_init(void) { return (void*)-1; }
static void ui_companion_win32_toggle(void *data, bool force) { }
static void ui_companion_win32_event_command(
      void *data, enum event_command cmd) { }

ui_companion_driver_t ui_companion_win32 = {
   ui_companion_win32_init,
   ui_companion_win32_deinit,
   ui_companion_win32_toggle,
   ui_companion_win32_event_command,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL, /* log_msg */
   NULL, /* is_active */
   NULL, /* get_app_icons */
   NULL, /* set_app_icon */
   NULL, /* get_app_icon_texture */
   &ui_browser_window_win32,
   &ui_msg_window_win32,
   &ui_window_win32,
   &ui_application_win32,
   "win32",
};
