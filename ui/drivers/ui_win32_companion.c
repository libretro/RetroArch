/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2026 - libretro team
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

/* Native Win32 desktop companion ("win32" companion UI driver).
 *
 * Pure C89, classic Win32 + common controls available on Windows 95/98
 * (comctl32 list box / list view / status bar, ANSI entry points only).
 * All model logic lives in ui/companion/companion_core; this file is
 * windowing, controls, layout and event wiring.
 *
 * The window lives on the main thread and is dispatched by the platform
 * driver's message pump (ui_win32.c pumps every window of the thread),
 * so nothing here blocks the RetroArch runloop: the only per-frame work
 * is a time-budgeted companion_core_iterate() from the iterate hook. */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#ifdef _MSC_VER
#pragma comment( lib, "comctl32" )
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#ifndef _WIN32_IE
#define _WIN32_IE 0x0300
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include <compat/strl.h>
#include <compat/msvc.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../command.h"
#include "../../configuration.h"
#include "../../gfx/video_driver.h"
#include "../../input/input_driver.h"
#include "../../retroarch.h"
#include "../../gfx/common/win32_common.h"

#include "../ui_companion_driver.h"
#include "../companion/companion_core.h"
#include "ui_win32.h"

#ifndef IDI_ICON
#define IDI_ICON 1
#endif

/* MSVC 6 headers predate UINT_PTR. */
#if defined(_MSC_VER) && _MSC_VER <= 1200
#define UINT_PTR_COMPAT UINT
#else
#define UINT_PTR_COMPAT UINT_PTR
#endif

#define COMPANION_WIN32_CLASS      "RetroArchCompanion"
#define COMPANION_WIN32_TITLE      "RetroArch"
#define COMPANION_WIN32_ITER_US    2000
#define COMPANION_WIN32_PANE_W     200
#define COMPANION_WIN32_MIN_W      480
#define COMPANION_WIN32_MIN_H      320

/* Control / command IDs. Kept clear of the ID_M_* range in ui_win32.h. */
enum
{
   IDC_CW_PLAYLISTS  = 50001,
   IDC_CW_ENTRIES,
   IDC_CW_STATUS,
   IDM_CW_LOAD_CORE  = 50101,
   IDM_CW_LOAD_CONTENT,
   IDM_CW_REFRESH,
   IDM_CW_CLOSE,
   IDM_CW_QUIT,
   IDM_CW_START_CORE,
   IDM_CW_RUN
};

typedef struct ui_companion_win32_wimp
{
   companion_core_t *core;
   HWND hwnd;
   HWND playlists;   /* LISTBOX  */
   HWND entries;     /* SysListView32, report view */
   HWND status;      /* msctls_statusbar32 */
   bool class_registered;
} ui_companion_win32_wimp_t;

/* One driver instance; the WNDPROC needs to find it. */
static ui_companion_win32_wimp_t *g_win32_wimp = NULL;

/* --- Presentation helpers -------------------------------------------- */

static void cw_status_set(ui_companion_win32_wimp_t *w, const char *msg)
{
   if (w && w->status)
      SendMessageA(w->status, SB_SETTEXTA, 0, (LPARAM)(msg ? msg : ""));
}

static void cw_playlists_rebuild(ui_companion_win32_wimp_t *w)
{
   size_t i, n;
   if (!w || !w->playlists)
      return;

   SendMessageA(w->playlists, LB_RESETCONTENT, 0, 0);
   n = companion_core_playlist_count(w->core);
   for (i = 0; i < n; i++)
   {
      const char *name = companion_core_playlist_name(w->core, i);
      SendMessageA(w->playlists, LB_ADDSTRING, 0,
            (LPARAM)(name ? name : ""));
   }
}

static void cw_entries_rebuild(ui_companion_win32_wimp_t *w)
{
   size_t i, n;
   LVITEMA item;
   char buf[64];

   if (!w || !w->entries)
      return;

   SendMessageA(w->entries, LVM_DELETEALLITEMS, 0, 0);
   n = companion_core_entry_count(w->core);

   /* Bulk insert without per-item repaint. */
   SendMessageA(w->entries, WM_SETREDRAW, FALSE, 0);
   for (i = 0; i < n; i++)
   {
      const struct playlist_entry *e = companion_core_entry(w->core, i);
      if (!e)
         continue;

      memset(&item, 0, sizeof(item));
      item.mask     = LVIF_TEXT | LVIF_PARAM;
      item.iItem    = (int)i;
      item.iSubItem = 0;
      item.lParam   = (LPARAM)i;
      item.pszText  = (LPSTR)(!string_is_empty(e->label)
            ? e->label
            : (e->path ? e->path : ""));
      SendMessageA(w->entries, LVM_INSERTITEMA, 0, (LPARAM)&item);

      item.mask     = LVIF_TEXT;
      item.iSubItem = 1;
      item.pszText  = (LPSTR)(!string_is_empty(e->core_name)
            ? e->core_name : "");
      SendMessageA(w->entries, LVM_SETITEMA, 0, (LPARAM)&item);
   }
   SendMessageA(w->entries, WM_SETREDRAW, TRUE, 0);

   snprintf(buf, sizeof(buf), "%u entries", (unsigned)n);
   cw_status_set(w, buf);
}

static void cw_layout(ui_companion_win32_wimp_t *w)
{
   RECT rc, sb;
   int status_h = 0;

   if (!w || !w->hwnd)
      return;

   GetClientRect(w->hwnd, &rc);

   if (w->status)
   {
      SendMessageA(w->status, WM_SIZE, 0, 0);
      GetWindowRect(w->status, &sb);
      status_h = sb.bottom - sb.top;
   }

   if (w->playlists)
      MoveWindow(w->playlists, 0, 0, COMPANION_WIN32_PANE_W,
            rc.bottom - status_h, TRUE);
   if (w->entries)
      MoveWindow(w->entries, COMPANION_WIN32_PANE_W, 0,
            rc.right - COMPANION_WIN32_PANE_W, rc.bottom - status_h, TRUE);
}

/* --- companion_core -> Win32 callbacks -------------------------------- */

static void cw_on_playlists_changed(void *ud)
{
   cw_playlists_rebuild((ui_companion_win32_wimp_t*)ud);
}

static void cw_on_playlist_changed(void *ud)
{
   cw_entries_rebuild((ui_companion_win32_wimp_t*)ud);
}

static void cw_on_status_message(void *ud, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   cw_status_set((ui_companion_win32_wimp_t*)ud, msg);
}

static void cw_on_notify_refresh(void *ud)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)ud;
   if (w)
      companion_core_refresh_playlists(w->core);
}

static const companion_callbacks_t cw_callbacks = {
   cw_on_playlists_changed,
   cw_on_playlist_changed,
   cw_on_status_message,
   NULL, /* on_log_message */
   cw_on_notify_refresh
};

/* --- Window procedure ------------------------------------------------- */

static void cw_select_playlist(ui_companion_win32_wimp_t *w)
{
   LRESULT sel = SendMessageA(w->playlists, LB_GETCURSEL, 0, 0);
   if (sel == LB_ERR)
      return;
   if (companion_core_select_playlist(w->core, (size_t)sel))
      cw_status_set(w, "Loading playlist...");
}

static void cw_run_selected(ui_companion_win32_wimp_t *w)
{
   LRESULT idx = SendMessageA(w->entries, LVM_GETNEXTITEM,
         (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
   if (idx < 0)
      return;
   if (companion_core_request_load_entry(w->core, (size_t)idx))
      ShowWindow(w->hwnd, SW_HIDE);
}

static LRESULT CALLBACK cw_wndproc(HWND hwnd, UINT msg,
      WPARAM wparam, LPARAM lparam)
{
   ui_companion_win32_wimp_t *w = g_win32_wimp;

   switch (msg)
   {
      case WM_SIZE:
         cw_layout(w);
         return 0;

      case WM_GETMINMAXINFO:
         {
            MINMAXINFO *mmi      = (MINMAXINFO*)lparam;
            mmi->ptMinTrackSize.x = COMPANION_WIN32_MIN_W;
            mmi->ptMinTrackSize.y = COMPANION_WIN32_MIN_H;
         }
         return 0;

      case WM_CLOSE:
         /* Closing the companion never quits RetroArch. */
         ShowWindow(hwnd, SW_HIDE);
         return 0;

      case WM_COMMAND:
         if (!w)
            break;
         switch (LOWORD(wparam))
         {
            case IDC_CW_PLAYLISTS:
               if (HIWORD(wparam) == LBN_SELCHANGE)
                  cw_select_playlist(w);
               return 0;
            case IDM_CW_LOAD_CORE:
            case IDM_CW_LOAD_CONTENT:
               /* Reuse the platform driver's dialog flow exactly as the
                * main window menu does. */
               win32_menu_loop(main_window.hwnd,
                     LOWORD(wparam) == IDM_CW_LOAD_CORE
                     ? ID_M_LOAD_CORE : ID_M_LOAD_CONTENT);
               return 0;
            case IDM_CW_START_CORE:
               companion_core_request_load(w->core, NULL, NULL);
               return 0;
            case IDM_CW_RUN:
               cw_run_selected(w);
               return 0;
            case IDM_CW_REFRESH:
               companion_core_refresh_playlists(w->core);
               return 0;
            case IDM_CW_CLOSE:
               ShowWindow(hwnd, SW_HIDE);
               return 0;
            case IDM_CW_QUIT:
               command_event(CMD_EVENT_QUIT, NULL);
               return 0;
            default:
               break;
         }
         break;

      case WM_NOTIFY:
         if (w && ((NMHDR*)lparam)->idFrom == IDC_CW_ENTRIES)
         {
            switch (((NMHDR*)lparam)->code)
            {
               case NM_DBLCLK:
               case NM_RETURN:
                  cw_run_selected(w);
                  return 0;
               default:
                  break;
            }
         }
         break;

      default:
         break;
   }

   return DefWindowProcA(hwnd, msg, wparam, lparam);
}

/* --- Window construction ---------------------------------------------- */

static HMENU cw_build_menu(void)
{
   HMENU bar  = CreateMenu();
   HMENU file = CreatePopupMenu();
   HMENU view = CreatePopupMenu();

   AppendMenuA(file, MF_STRING, IDM_CW_LOAD_CORE,    "Load &Core...");
   AppendMenuA(file, MF_STRING, IDM_CW_LOAD_CONTENT, "&Load Content...");
   AppendMenuA(file, MF_STRING, IDM_CW_START_CORE,   "&Start Core");
   AppendMenuA(file, MF_SEPARATOR, 0, NULL);
   AppendMenuA(file, MF_STRING, IDM_CW_CLOSE,        "&Close Window");
   AppendMenuA(file, MF_STRING, IDM_CW_QUIT,         "E&xit RetroArch");

   AppendMenuA(view, MF_STRING, IDM_CW_RUN,          "&Run Selected\tEnter");
   AppendMenuA(view, MF_STRING, IDM_CW_REFRESH,      "Re&fresh Playlists\tF5");

   AppendMenuA(bar, MF_POPUP, (UINT_PTR_COMPAT)file, "&File");
   AppendMenuA(bar, MF_POPUP, (UINT_PTR_COMPAT)view, "&View");
   return bar;
}

static bool cw_create_window(ui_companion_win32_wimp_t *w)
{
   WNDCLASSA wc;
   LVCOLUMNA col;
   HINSTANCE inst = GetModuleHandleA(NULL);

   InitCommonControls();

   memset(&wc, 0, sizeof(wc));
   wc.style         = CS_HREDRAW | CS_VREDRAW;
   wc.lpfnWndProc   = cw_wndproc;
   wc.hInstance     = inst;
   wc.hCursor       = LoadCursorA(NULL, MAKEINTRESOURCEA(32512)); /* IDC_ARROW */
   wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
   wc.lpszClassName = COMPANION_WIN32_CLASS;
   wc.hIcon         = LoadIconA(inst, MAKEINTRESOURCEA(IDI_ICON));

   if (!RegisterClassA(&wc))
      return false;
   w->class_registered = true;

   w->hwnd = CreateWindowExA(0, COMPANION_WIN32_CLASS, COMPANION_WIN32_TITLE,
         WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
         CW_USEDEFAULT, CW_USEDEFAULT, 800, 520,
         NULL, cw_build_menu(), inst, NULL);
   if (!w->hwnd)
      return false;

   w->playlists = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
         WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_PLAYLISTS, inst, NULL);

   w->entries = CreateWindowExA(WS_EX_CLIENTEDGE, "SysListView32", "",
         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_ENTRIES, inst, NULL);

   w->status = CreateWindowExA(0, "msctls_statusbar32", "",
         WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_STATUS, inst, NULL);

   if (!w->playlists || !w->entries || !w->status)
      return false;

   /* Full-row select is an IE3+ extended style; harmless where absent. */
   SendMessageA(w->entries, LVM_SETEXTENDEDLISTVIEWSTYLE,
         LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

   memset(&col, 0, sizeof(col));
   col.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
   col.pszText  = (LPSTR)"Name";
   col.cx       = 380;
   col.iSubItem = 0;
   SendMessageA(w->entries, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
   col.pszText  = (LPSTR)"Core";
   col.cx       = 180;
   col.iSubItem = 1;
   SendMessageA(w->entries, LVM_INSERTCOLUMNA, 1, (LPARAM)&col);

   cw_layout(w);
   return true;
}

/* --- Driver entry points ---------------------------------------------- */

static void *ui_companion_win32_wimp_init(void)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)
      calloc(1, sizeof(*w));
   if (!w)
      return NULL;

   g_win32_wimp = w;
   w->core      = companion_core_new(&cw_callbacks, w);

   if (!w->core || !cw_create_window(w))
   {
      if (w->hwnd)
         DestroyWindow(w->hwnd);
      if (w->class_registered)
         UnregisterClassA(COMPANION_WIN32_CLASS, GetModuleHandleA(NULL));
      companion_core_free(w->core);
      free(w);
      g_win32_wimp = NULL;
      return NULL;
   }

   companion_core_refresh_playlists(w->core);
   return w;
}

static void ui_companion_win32_wimp_deinit(void *data)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   if (!w)
      return;
   if (w->hwnd)
      DestroyWindow(w->hwnd);
   if (w->class_registered)
      UnregisterClassA(COMPANION_WIN32_CLASS, GetModuleHandleA(NULL));
   companion_core_free(w->core);
   if (g_win32_wimp == w)
      g_win32_wimp = NULL;
   free(w);
}

static void ui_companion_win32_wimp_toggle(void *data, bool force)
{
   ui_companion_win32_wimp_t *w   = (ui_companion_win32_wimp_t*)data;
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   bool ui_companion_toggle       = settings->bools.ui_companion_toggle;
   bool video_fullscreen          = settings->bools.video_fullscreen;
   bool mouse_grabbed             = (input_state_get_ptr()->flags
         & INP_FLAG_GRAB_MOUSE_STATE) ? true : false;

   if (!w || !w->hwnd)
      return;
   if (!(ui_companion_toggle || force))
      return;

   /* Same hand-off as the Qt companion: release the mouse, show the
    * cursor and leave fullscreen so the window is reachable. */
   if (mouse_grabbed)
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   if (video_st && video_st->poke && video_st->poke->show_mouse)
      video_st->poke->show_mouse(video_st->data, true);
   if (video_fullscreen)
      command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);

   ShowWindow(w->hwnd, SW_SHOW);
   SetForegroundWindow(w->hwnd);
}

static void ui_companion_win32_wimp_iterate(void *data)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   if (w)
      companion_core_iterate(w->core, COMPANION_WIN32_ITER_US);
}

static void ui_companion_win32_wimp_event_command(void *data,
      enum event_command cmd)
{
   (void)data;
   (void)cmd;
}

static void ui_companion_win32_wimp_notify_refresh(void *data)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   if (w)
      companion_core_notify_refresh(w->core);
}

static void ui_companion_win32_wimp_msg_queue_push(void *data,
      const char *msg, unsigned priority, unsigned duration, bool flush)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   if (w)
      companion_core_status_message(w->core, msg, priority, duration, flush);
}

static void *ui_companion_win32_wimp_get_main_window(void *data)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   return w ? (void*)w->hwnd : NULL;
}

static bool ui_companion_win32_wimp_is_active(void *data)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   return w && w->hwnd && IsWindowVisible(w->hwnd);
}

ui_companion_driver_t ui_companion_wimp_win32 = {
   ui_companion_win32_wimp_init,
   ui_companion_win32_wimp_deinit,
   ui_companion_win32_wimp_toggle,
   ui_companion_win32_wimp_iterate,
   ui_companion_win32_wimp_event_command,
   ui_companion_win32_wimp_notify_refresh,
   ui_companion_win32_wimp_msg_queue_push,
   NULL, /* render_messagebox */
   ui_companion_win32_wimp_get_main_window,
   NULL, /* log_msg */
   ui_companion_win32_wimp_is_active,
   NULL, /* get_app_icons */
   NULL, /* set_app_icon */
   NULL, /* get_app_icon_texture */
   NULL, /* browser_window: platform driver's */
   NULL, /* msg_window:     platform driver's */
   NULL, /* window:         platform driver's */
   NULL, /* application:    pumped by the platform driver */
   "win32",
};
