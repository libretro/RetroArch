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

RETRO_END_DECLS

#endif
