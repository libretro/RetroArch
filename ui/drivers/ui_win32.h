/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _WIN32_UI
#define _WIN32_UI

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "../ui_companion_driver.h"

RETRO_BEGIN_DECLS

typedef struct ui_application_win32
{
   void *empty;
} ui_application_win32_t;

typedef struct ui_window_win32
{
   HWND hwnd;
} ui_window_win32_t;

extern VOID (WINAPI *DragAcceptFiles_func)(HWND, BOOL);

#ifdef HAVE_THREADS
/**
 * Asynchronous file-browser result messages.
 *
 * The file dialog runs on a worker thread so the main loop keeps
 * rendering.  When the dialog closes the thread posts one of these
 * messages to the main window.  LPARAM carries a heap-allocated
 * win32_browser_thread_data_t* that the handler must free.
 */
#define WM_BROWSER_OPEN_RESULT  (WM_USER + 0)
#define WM_BROWSER_CANCELLED    (WM_USER + 1)

enum win32_browser_mode
{
   WIN32_BROWSER_MODE_LOAD_CONTENT = 0,
   WIN32_BROWSER_MODE_LOAD_CORE,
   WIN32_BROWSER_MODE_SAVE
};

typedef struct
{
   /* Inputs -- copied before the thread starts. */
   char               filters[1024];
   char               title[PATH_MAX_LENGTH];
   char               startdir[DIR_MAX_LENGTH];
   HWND               owner;         /* main window, for PostMessage  */
   bool               is_save;       /* false = Open, true = Save     */
   enum win32_browser_mode mode;     /* what the caller wanted to do  */

   /* Output -- written by the worker thread. */
   char               path[PATH_MAX_LENGTH];
   bool               result;        /* true = user picked a file     */
} win32_browser_thread_data_t;
#endif /* HAVE_THREADS */

/* Win32 UI resource identifiers — used by the menu bar,
 * accelerator table, and the "Pick Core" dialog. */
enum
{
   IDR_MENU          = 101,
   IDR_PICKCORE      = 103,
   IDR_ACCELERATOR1,

   ID_M_LOAD_CONTENT = 40001,
   ID_CORELISTBOX,
   ID_M_RESET,
   ID_M_QUIT,
   ID_M_MENU_TOGGLE,
   ID_M_PAUSE_TOGGLE,
   ID_M_LOAD_CORE,
   ID_M_LOAD_STATE,
   ID_M_SAVE_STATE,
   ID_M_DISK_CYCLE,
   ID_M_DISK_NEXT,
   ID_M_DISK_PREV,
   ID_M_WINDOW_SCALE_1X,
   ID_M_WINDOW_SCALE_2X,
   ID_M_WINDOW_SCALE_3X,
   ID_M_WINDOW_SCALE_4X,
   ID_M_WINDOW_SCALE_5X,
   ID_M_WINDOW_SCALE_6X,
   ID_M_WINDOW_SCALE_7X,
   ID_M_WINDOW_SCALE_8X,
   ID_M_WINDOW_SCALE_9X,
   ID_M_WINDOW_SCALE_10X,
   ID_M_FULL_SCREEN,
   ID_M_MOUSE_GRAB,
   ID_M_STATE_INDEX_AUTO,
   ID_M_STATE_INDEX_0,
   ID_M_STATE_INDEX_1,
   ID_M_STATE_INDEX_2,
   ID_M_STATE_INDEX_3,
   ID_M_STATE_INDEX_4,
   ID_M_STATE_INDEX_5,
   ID_M_STATE_INDEX_6,
   ID_M_STATE_INDEX_7,
   ID_M_STATE_INDEX_8,
   ID_M_STATE_INDEX_9,
   ID_M_TAKE_SCREENSHOT,
   ID_M_MUTE_TOGGLE,
   ID_M_TOGGLE_DESKTOP
};

/* Functions moved from gfx/common/win32_common.c —
 * UI logic that the WndProc and window-setup code calls into. */
bool win32_load_content_from_gui(const char *szFilename);
bool win32_drag_query_file(HWND hwnd, WPARAM wparam);
LRESULT win32_menu_loop(HWND owner, WPARAM wparam);
#ifdef HAVE_MENU
void win32_localize_menu(HMENU menu);
void win32_menubar_rebuild(void);
#endif
#ifndef __WINRT__
HMENU win32_resources_create_menu(void);
int win32_resources_pick_core_dialog(HWND parent, DLGPROC dlg_proc);
#endif

RETRO_END_DECLS

#endif
