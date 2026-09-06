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
#pragma comment( lib, "shell32" )
#pragma comment( lib, "ole32" )
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
#include <shlobj.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <formats/image.h>
#include <lists/string_list.h>
#include <compat/msvc.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../command.h"
#include "../../configuration.h"
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
#define COMPANION_WIN32_CORES_CLASS "RetroArchCompanionCores"
#define COMPANION_WIN32_TITLE      "RetroArch"
#define COMPANION_WIN32_ITER_US    2000
#define COMPANION_WIN32_PANE_W     200   /* initial playlist pane width */
#define COMPANION_WIN32_PANE_MIN   100
#define COMPANION_WIN32_SPLIT_W    5     /* draggable gap between panes */
#define COMPANION_WIN32_MIN_W      480
#define COMPANION_WIN32_MIN_H      320
#define COMPANION_WIN32_LOG_H      120   /* log pane height when shown */
#define COMPANION_WIN32_INFO_W     280   /* core-info pane width when shown */
#define COMPANION_WIN32_THUMB      128   /* icon-view thumbnail edge, px */
/* Icon view: one thumbnail decoded per frame from the iterate hook, so a
 * playlist of any size never costs more than one file decode per frame. */
/* The log edit is trimmed from the front when it passes this, in one
 * cut, so appends stay O(line) instead of the control's O(text). */
#define COMPANION_WIN32_LOG_MAX    (256 * 1024)

/* Pre-Win98 SDKs lack these; the messages themselves date from 95. */
#ifndef WM_ENTERMENULOOP
#define WM_ENTERMENULOOP 0x0211
#endif
#ifndef WM_EXITMENULOOP
#define WM_EXITMENULOOP 0x0212
#endif
#ifndef WM_ENTERSIZEMOVE
#define WM_ENTERSIZEMOVE 0x0231
#endif
#ifndef WM_EXITSIZEMOVE
#define WM_EXITSIZEMOVE 0x0232
#endif

/* Control / command IDs. Kept clear of the ID_M_* range in ui_win32.h. */
enum
{
   IDC_CW_PLAYLISTS  = 50001,
   IDC_CW_ENTRIES,
   IDC_CW_STATUS,
   IDC_CW_LOG,
   IDC_CW_INFO,       /* core information pane: list view */
   IDC_CW_CORES,      /* Load Core window: list view */
   IDC_CW_CORES_OK,
   IDC_CW_CORES_CANCEL,
   IDM_CW_LOAD_CORE  = 50101,
   IDM_CW_LOAD_CONTENT,
   IDM_CW_REFRESH,
   IDM_CW_CLOSE,
   IDM_CW_QUIT,
   IDM_CW_START_CORE,
   IDM_CW_RUN,
   IDM_CW_DELETE_ENTRY,
   IDM_CW_ASSOC_DETECT,
   IDM_CW_SCAN_DIR,
   IDM_CW_TOGGLE_LOG,
   IDM_CW_TOGGLE_INFO,
   IDM_CW_VIEW_LIST,
   IDM_CW_VIEW_ICONS,
   /* IDM_CW_ASSOC_BASE + i selects installed core i as the playlist's
    * default core; keep a wide gap after it. */
   IDM_CW_ASSOC_BASE = 51000,
   IDM_CW_ASSOC_MAX  = 59999
};

typedef struct ui_companion_win32_wimp
{
   companion_core_t *core;
   HWND hwnd;
   HWND playlists;   /* LISTBOX  */
   HWND entries;     /* SysListView32, report view */
   HWND status;      /* msctls_statusbar32 */
   HWND log;         /* read-only multiline EDIT, hidden by default */
   bool log_visible;
   HWND info;        /* core information: SysListView32 key / value */
   bool info_visible;
   /* Core the info pane currently describes; the pane follows the
    * running core from the iterate hook (only the shader commands are
    * forwarded to companions, so a load/unload is not an event here). */
   char info_core[PATH_MAX_LENGTH];
   /* Icon (grid) view: thumbnails in a 32-bit image list, index 0 the
    * placeholder; pending items decode one per frame while the view
    * is showing. */
   HIMAGELIST thumbs;
   size_t thumb_next;      /* next entry to decode */
   size_t thumb_count;     /* entries in the current list */
   bool icon_view;
   /* Load Core window (non-modal: a DialogBox would run its own loop). */
   HWND cores_hwnd;
   HWND cores_list;
   bool cores_class_registered;
   /* Playlist a context menu was opened on (a list box does not move
    * its selection on right-click); (size_t)-1 = use the selection. */
   size_t ctx_playlist;
   /* Splitter between the playlist pane and the entries. */
   int pane_w;
   bool splitting;
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

/* --- Icon view thumbnails ---------------------------------------------- */

/* A 32-bit BGRA DIB of @w x @h from @bits (ARGB8888 as image_texture
 * decodes when supports_rgba is false - the Windows byte order). */
static HBITMAP cw_dib_from_argb(const uint32_t *bits, int w, int h)
{
   BITMAPINFO bmi;
   void *dst = NULL;
   HBITMAP bmp;

   memset(&bmi, 0, sizeof(bmi));
   bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
   bmi.bmiHeader.biWidth       = w;
   bmi.bmiHeader.biHeight      = -h; /* top-down */
   bmi.bmiHeader.biPlanes      = 1;
   bmi.bmiHeader.biBitCount    = 32;
   bmi.bmiHeader.biCompression = BI_RGB;

   bmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &dst, NULL, 0);
   if (bmp && dst)
      memcpy(dst, bits, (size_t)w * h * 4);
   return bmp;
}

/* Letterbox @img into a square of THUMB px, nearest-neighbour: cheap,
 * and thumbnails are viewed at that size. */
static HBITMAP cw_thumb_bitmap(const struct texture_image *img, uint32_t bg)
{
   uint32_t *buf;
   HBITMAP bmp;
   int sw, sh, ox, oy, x, y;
   const int T = COMPANION_WIN32_THUMB;

   if (!img->pixels || !img->width || !img->height)
      return NULL;
   buf = (uint32_t*)malloc((size_t)T * T * sizeof(uint32_t));
   if (!buf)
      return NULL;

   if (img->width >= img->height)
   {
      sw = T;
      sh = (int)((unsigned)T * img->height / img->width);
   }
   else
   {
      sh = T;
      sw = (int)((unsigned)T * img->width / img->height);
   }
   if (sw < 1) sw = 1;
   if (sh < 1) sh = 1;
   ox = (T - sw) / 2;
   oy = (T - sh) / 2;

   for (y = 0; y < T; y++)
   {
      uint32_t *row = buf + (size_t)y * T;
      if (y < oy || y >= oy + sh)
      {
         for (x = 0; x < T; x++)
            row[x] = bg;
         continue;
      }
      {
         const uint32_t *src = img->pixels
            + (size_t)((y - oy) * img->height / sh) * img->width;
         for (x = 0; x < T; x++)
            row[x] = (x < ox || x >= ox + sw)
               ? bg
               : src[(x - ox) * img->width / sw] | 0xff000000u;
      }
   }

   bmp = cw_dib_from_argb(buf, T, T);
   free(buf);
   return bmp;
}

static uint32_t cw_sys_color_argb(int index)
{
   DWORD c = GetSysColor(index); /* 0x00BBGGRR */
   return 0xff000000u | ((c & 0xff) << 16) | (c & 0xff00) | ((c >> 16) & 0xff);
}

/* New image list for the current entry list: placeholder at 0, every
 * item pointed at it, decoding restarted from the top. */
static void cw_thumbs_reset(ui_companion_win32_wimp_t *w, size_t count)
{
   HBITMAP placeholder;
   uint32_t *bits;
   const int T = COMPANION_WIN32_THUMB;

   if (w->thumbs)
   {
      SendMessageA(w->entries, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)NULL);
      ImageList_Destroy(w->thumbs);
      w->thumbs = NULL;
   }
   w->thumb_next  = 0;
   w->thumb_count = count;

   /* ILC_COLOR32 is honoured from comctl32 4.71; older ones pick a lower
    * depth themselves, which is fine for a thumbnail. */
   w->thumbs = ImageList_Create(T, T, ILC_COLOR32, (int)(count > 0 ? count : 1) + 1, 16);
   if (!w->thumbs)
      return;

   bits = (uint32_t*)malloc((size_t)T * T * sizeof(uint32_t));
   if (bits)
   {
      int i;
      uint32_t bg = cw_sys_color_argb(COLOR_BTNFACE);
      for (i = 0; i < T * T; i++)
         bits[i] = bg;
      placeholder = cw_dib_from_argb(bits, T, T);
      free(bits);
      if (placeholder)
      {
         ImageList_Add(w->thumbs, placeholder, NULL);
         DeleteObject(placeholder);
      }
   }
   SendMessageA(w->entries, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)w->thumbs);
}

/* Decode the next pending thumbnail (one per call). Returns false when
 * nothing is pending. */
static bool cw_thumb_step(ui_companion_win32_wimp_t *w)
{
   char path[PATH_MAX_LENGTH];
   char db_name[NAME_MAX_LENGTH];
   struct texture_image img;
   const struct playlist_entry *e;
   HBITMAP bmp = NULL;
   int idx = 0;
   size_t i;

   if (!w->thumbs || w->thumb_next >= w->thumb_count)
      return false;

   i = w->thumb_next++;
   e = companion_core_entry(w->core, i);
   if (!e)
      return true;

   /* Playlist name without .lpl, as the repository is laid out. */
   strlcpy(db_name, e->db_name ? e->db_name : "", sizeof(db_name));
   path_remove_extension(db_name);

   memset(&img, 0, sizeof(img));
   if (companion_core_thumbnail_path(w->core, db_name, COMPANION_THUMB_BOXART,
            !string_is_empty(e->label) ? e->label : path_basename(e->path),
            e->path, path, sizeof(path))
         && image_texture_load(&img, path))
   {
      bmp = cw_thumb_bitmap(&img, cw_sys_color_argb(COLOR_WINDOW));
      image_texture_free(&img);
   }
   if (bmp)
   {
      idx = ImageList_Add(w->thumbs, bmp, NULL);
      DeleteObject(bmp);
      if (idx > 0)
      {
         LVITEMA item;
         memset(&item, 0, sizeof(item));
         item.mask   = LVIF_IMAGE;
         item.iItem  = (int)i;
         item.iImage = idx;
         SendMessageA(w->entries, LVM_SETITEMA, 0, (LPARAM)&item);
      }
   }
   return true;
}

static void cw_set_icon_view(ui_companion_win32_wimp_t *w, bool icons)
{
   LONG style;
   if (!w || !w->entries)
      return;
   w->icon_view = icons;
   style  = GetWindowLongA(w->entries, GWL_STYLE);
   style &= ~(LVS_TYPEMASK);
   style |= icons ? LVS_ICON : LVS_REPORT;
   SetWindowLongA(w->entries, GWL_STYLE, style);
   if (icons)
      SendMessageA(w->entries, LVM_SETICONSPACING, 0,
            MAKELPARAM(COMPANION_WIN32_THUMB + 24, COMPANION_WIN32_THUMB + 40));
   InvalidateRect(w->entries, NULL, TRUE);
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
   cw_thumbs_reset(w, n);

   /* Bulk insert without per-item repaint. */
   SendMessageA(w->entries, WM_SETREDRAW, FALSE, 0);
   for (i = 0; i < n; i++)
   {
      const struct playlist_entry *e = companion_core_entry(w->core, i);
      if (!e)
         continue;

      memset(&item, 0, sizeof(item));
      item.mask     = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE;
      item.iItem    = (int)i;
      item.iSubItem = 0;
      item.iImage   = 0; /* placeholder until decoded */
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

   /* Keep the pane inside the window as it is resized. */
   if (w->pane_w > rc.right - COMPANION_WIN32_PANE_MIN - COMPANION_WIN32_SPLIT_W)
      w->pane_w = rc.right - COMPANION_WIN32_PANE_MIN - COMPANION_WIN32_SPLIT_W;
   if (w->pane_w < COMPANION_WIN32_PANE_MIN)
      w->pane_w = COMPANION_WIN32_PANE_MIN;

   {
      int log_h   = (w->log_visible && w->log) ? COMPANION_WIN32_LOG_H : 0;
      int info_w  = (w->info_visible && w->info) ? COMPANION_WIN32_INFO_W : 0;
      int list_h  = rc.bottom - status_h - log_h;
      int entry_x = w->pane_w + COMPANION_WIN32_SPLIT_W;
      int entry_w = rc.right - entry_x - info_w;
      if (list_h < 0)
         list_h = 0;
      if (entry_w < COMPANION_WIN32_PANE_MIN)
      {
         /* Too narrow for all three: the info pane yields. */
         info_w  = 0;
         entry_w = rc.right - entry_x;
      }

      if (w->playlists)
         MoveWindow(w->playlists, 0, 0, w->pane_w, list_h, TRUE);
      if (w->entries)
         MoveWindow(w->entries, entry_x, 0, entry_w, list_h, TRUE);
      if (w->info)
         MoveWindow(w->info, entry_x + entry_w, 0, info_w, list_h, TRUE);
      if (w->log)
         MoveWindow(w->log, 0, list_h, rc.right, log_h, TRUE);
   }
}

/* Core information pane: the rows companion_core_core_info_rows()
 * produces for the running core, as a two-column list. Rebuilt when the
 * pane is shown and when the core changes. */
static void cw_info_fill(ui_companion_win32_wimp_t *w)
{
   struct string_list *keys, *values;
   size_t i;
   LVITEMA item;

   if (!w || !w->info || !w->info_visible)
      return;

   keys   = string_list_new();
   values = string_list_new();
   if (!keys || !values)
   {
      string_list_free(keys);
      string_list_free(values);
      return;
   }

   strlcpy(w->info_core, companion_core_current_core_path(w->core),
         sizeof(w->info_core));
   companion_core_core_info_rows(w->info_core, keys, values);

   SendMessageA(w->info, WM_SETREDRAW, FALSE, 0);
   SendMessageA(w->info, LVM_DELETEALLITEMS, 0, 0);
   for (i = 0; i < keys->size; i++)
   {
      memset(&item, 0, sizeof(item));
      item.mask     = LVIF_TEXT;
      item.iItem    = (int)i;
      item.pszText  = (LPSTR)(keys->elems[i].data ? keys->elems[i].data : "");
      SendMessageA(w->info, LVM_INSERTITEMA, 0, (LPARAM)&item);
      item.iSubItem = 1;
      item.pszText  = (LPSTR)(values->elems[i].data ? values->elems[i].data : "");
      SendMessageA(w->info, LVM_SETITEMA, 0, (LPARAM)&item);
   }
   SendMessageA(w->info, WM_SETREDRAW, TRUE, 0);

   string_list_free(keys);
   string_list_free(values);
}

static void cw_info_toggle(ui_companion_win32_wimp_t *w)
{
   if (!w || !w->info)
      return;
   w->info_visible = !w->info_visible;
   ShowWindow(w->info, w->info_visible ? SW_SHOW : SW_HIDE);
   cw_layout(w);
   cw_info_fill(w);
}

/* Append one log line to the EDIT: move the caret to the end and
 * replace the (empty) selection, which is the only O(line) append the
 * control offers. Newlines become CRLF as the control wants. */
static void cw_log_append(ui_companion_win32_wimp_t *w, const char *msg)
{
   char line[1024 + 2];
   size_t i, j;
   LRESULT len;

   if (!w || !w->log || !msg)
      return;

   for (i = 0, j = 0; msg[i] && j < sizeof(line) - 3; i++)
   {
      if (msg[i] == '\n')
      {
         line[j++] = '\r';
         line[j++] = '\n';
      }
      else if (msg[i] != '\r')
         line[j++] = msg[i];
   }
   line[j] = '\0';

   len = SendMessageA(w->log, WM_GETTEXTLENGTH, 0, 0);
   if (len > COMPANION_WIN32_LOG_MAX)
   {
      /* Drop the oldest half in one replacement. */
      SendMessageA(w->log, EM_SETSEL, 0, len / 2);
      SendMessageA(w->log, EM_REPLACESEL, FALSE, (LPARAM)"");
      len = SendMessageA(w->log, WM_GETTEXTLENGTH, 0, 0);
   }
   SendMessageA(w->log, EM_SETSEL, len, len);
   SendMessageA(w->log, EM_REPLACESEL, FALSE, (LPARAM)line);
}

static void cw_log_toggle(ui_companion_win32_wimp_t *w)
{
   if (!w || !w->log)
      return;
   w->log_visible = !w->log_visible;
   ShowWindow(w->log, w->log_visible ? SW_SHOW : SW_HIDE);
   cw_layout(w);
}

/* The only client area not covered by a child control is the splitter
 * gap (and the status bar), so a mouse message reaching the frame is on
 * the splitter. */
static bool cw_on_splitter(ui_companion_win32_wimp_t *w, int x, int y)
{
   RECT rc, sb;
   int status_h = 0;
   GetClientRect(w->hwnd, &rc);
   if (w->status)
   {
      GetWindowRect(w->status, &sb);
      status_h = sb.bottom - sb.top;
   }
   return x >= w->pane_w && x < w->pane_w + COMPANION_WIN32_SPLIT_W
      && y >= 0 && y < rc.bottom - status_h;
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

static void cw_on_scan_finished(void *ud)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)ud;
   if (!w)
      return;
   companion_core_refresh_playlists(w->core);
   cw_status_set(w, "Scan finished.");
}

static const companion_callbacks_t cw_callbacks = {
   cw_on_playlists_changed,
   cw_on_playlist_changed,
   cw_on_status_message,
   NULL, /* on_log_message */
   cw_on_notify_refresh,
   cw_on_scan_finished,
   NULL, /* on_thumbnail_downloaded */
   NULL  /* on_thumbnail_pack_finished */
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

static LRESULT cw_selected_entry(ui_companion_win32_wimp_t *w)
{
   return SendMessageA(w->entries, LVM_GETNEXTITEM,
         (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
}

static void cw_run_selected(ui_companion_win32_wimp_t *w)
{
   LRESULT idx = cw_selected_entry(w);
   if (idx < 0)
      return;
   if (companion_core_request_load_entry(w->core, (size_t)idx))
      ShowWindow(w->hwnd, SW_HIDE);
}

/* Reload the selected playlist after an edit (the core keeps its own
 * parsed copy; the edit went to disk / the menu's cached object). */
static void cw_reload_selected_playlist(ui_companion_win32_wimp_t *w)
{
   size_t sel = companion_core_selected_playlist(w->core);
   if (sel != (size_t)-1)
      companion_core_select_playlist(w->core, sel);
}

static void cw_delete_selected(ui_companion_win32_wimp_t *w)
{
   size_t sel  = companion_core_selected_playlist(w->core);
   LRESULT idx = cw_selected_entry(w);
   const char *path;

   if (idx < 0 || sel == (size_t)-1)
      return;
   if (MessageBoxA(w->hwnd, "Delete this playlist entry?", COMPANION_WIN32_TITLE,
            MB_YESNO | MB_ICONQUESTION) != IDYES)
      return;

   path = companion_core_playlist_path(w->core, sel);
   if (companion_core_playlist_delete_entry(w->core, path, (size_t)idx))
      cw_reload_selected_playlist(w);
}

static void cw_associate_core(ui_companion_win32_wimp_t *w, UINT id)
{
   size_t sel = w->ctx_playlist;
   const char *core_path = NULL;

   if (sel == (size_t)-1)
      sel = companion_core_selected_playlist(w->core);
   if (sel == (size_t)-1)
      return;
   if (id != IDM_CW_ASSOC_DETECT)
      core_path = companion_core_installed_core_path(w->core,
            (size_t)(id - IDM_CW_ASSOC_BASE));

   companion_core_playlist_set_default_core(w->core,
         companion_core_playlist_path(w->core, sel), core_path);
}

/* --- Load Core window -------------------------------------------------- */

static void cw_cores_fill(ui_companion_win32_wimp_t *w)
{
   size_t i, n;
   LVITEMA item;

   SendMessageA(w->cores_list, LVM_DELETEALLITEMS, 0, 0);
   SendMessageA(w->cores_list, WM_SETREDRAW, FALSE, 0);

   n = companion_core_installed_core_count(w->core);
   for (i = 0; i < n; i++)
   {
      const char *name    = companion_core_installed_core_name(w->core, i);
      const char *version = companion_core_installed_core_version(w->core, i);

      memset(&item, 0, sizeof(item));
      item.mask     = LVIF_TEXT | LVIF_PARAM;
      item.iItem    = (int)i;
      item.lParam   = (LPARAM)i;
      item.pszText  = (LPSTR)(name ? name : "");
      SendMessageA(w->cores_list, LVM_INSERTITEMA, 0, (LPARAM)&item);

      item.mask     = LVIF_TEXT;
      item.iSubItem = 1;
      item.pszText  = (LPSTR)(version ? version : "");
      SendMessageA(w->cores_list, LVM_SETITEMA, 0, (LPARAM)&item);
   }

   SendMessageA(w->cores_list, WM_SETREDRAW, TRUE, 0);
   if (n)
      ListView_SetItemState(w->cores_list, 0,
            LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

static void cw_cores_load_selected(ui_companion_win32_wimp_t *w)
{
   LVITEMA item;
   LRESULT idx = SendMessageA(w->cores_list, LVM_GETNEXTITEM,
         (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
   const char *path;

   if (idx < 0)
      return;

   /* The list may be sorted by the user; the core index rides in lParam. */
   memset(&item, 0, sizeof(item));
   item.mask  = LVIF_PARAM;
   item.iItem = (int)idx;
   if (!SendMessageA(w->cores_list, LVM_GETITEMA, 0, (LPARAM)&item))
      return;

   path = companion_core_installed_core_path(w->core, (size_t)item.lParam);
   ShowWindow(w->cores_hwnd, SW_HIDE);
   if (companion_core_load_core(w->core, path))
      cw_status_set(w, "Core loaded.");
   else
      cw_status_set(w, "Failed to load the core.");
}

static LRESULT CALLBACK cw_cores_wndproc(HWND hwnd, UINT msg,
      WPARAM wparam, LPARAM lparam)
{
   ui_companion_win32_wimp_t *w = g_win32_wimp;

   switch (msg)
   {
      case WM_SIZE:
         if (w && w->cores_list)
         {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(w->cores_list, 0, 0, rc.right, rc.bottom - 34, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CW_CORES_OK),
                  rc.right - 170, rc.bottom - 29, 80, 24, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CW_CORES_CANCEL),
                  rc.right - 85, rc.bottom - 29, 80, 24, TRUE);
         }
         return 0;
      case WM_CLOSE:
         ShowWindow(hwnd, SW_HIDE);
         return 0;
      case WM_DESTROY:
         win32_modal_window_destroyed(hwnd);
         break;
      case WM_ENTERSIZEMOVE:
      case WM_ENTERMENULOOP:
         win32_modal_enter(hwnd);
         break;
      case WM_EXITSIZEMOVE:
      case WM_EXITMENULOOP:
         win32_modal_exit(hwnd);
         break;
      case WM_RA_MODAL_TICK:
         win32_modal_tick(hwnd);
         return 0;
      case WM_TIMER:
         if (wparam == WIN32_MODAL_TIMER_ID)
         {
            win32_modal_tick(hwnd);
            return 0;
         }
         break;
      case WM_COMMAND:
         if (!w)
            break;
         switch (LOWORD(wparam))
         {
            case IDC_CW_CORES_OK:
            case IDOK:
               cw_cores_load_selected(w);
               return 0;
            case IDC_CW_CORES_CANCEL:
            case IDCANCEL:
               ShowWindow(hwnd, SW_HIDE);
               return 0;
            default:
               break;
         }
         break;
      case WM_NOTIFY:
         if (w && ((NMHDR*)lparam)->idFrom == IDC_CW_CORES)
         {
            switch (((NMHDR*)lparam)->code)
            {
               case NM_DBLCLK:
               case NM_RETURN:
                  cw_cores_load_selected(w);
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

static bool cw_cores_create(ui_companion_win32_wimp_t *w)
{
   WNDCLASSA wc;
   LVCOLUMNA col;
   HINSTANCE inst = GetModuleHandleA(NULL);

   if (w->cores_hwnd)
      return true;

   memset(&wc, 0, sizeof(wc));
   wc.lpfnWndProc   = cw_cores_wndproc;
   wc.hInstance     = inst;
   wc.hCursor       = LoadCursorA(NULL, MAKEINTRESOURCEA(32512));
   wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
   wc.lpszClassName = COMPANION_WIN32_CORES_CLASS;
   wc.hIcon         = LoadIconA(inst, MAKEINTRESOURCEA(IDI_ICON));
   if (!RegisterClassA(&wc))
      return false;
   w->cores_class_registered = true;

   /* Owned by the companion window so it stays above it and hides with
    * it; WS_EX_TOOLWINDOW keeps it off the taskbar. */
   w->cores_hwnd = CreateWindowExA(WS_EX_TOOLWINDOW, COMPANION_WIN32_CORES_CLASS,
         "Load Core", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
         CW_USEDEFAULT, CW_USEDEFAULT, 420, 400,
         w->hwnd, NULL, inst, NULL);
   if (!w->cores_hwnd)
      return false;

   w->cores_list = CreateWindowExA(WS_EX_CLIENTEDGE, "SysListView32", "",
         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL
         | LVS_SHOWSELALWAYS | LVS_SORTASCENDING,
         0, 0, 0, 0, w->cores_hwnd, (HMENU)IDC_CW_CORES, inst, NULL);
   CreateWindowExA(0, "BUTTON", "&Load",
         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
         0, 0, 0, 0, w->cores_hwnd, (HMENU)IDC_CW_CORES_OK, inst, NULL);
   CreateWindowExA(0, "BUTTON", "Cancel",
         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
         0, 0, 0, 0, w->cores_hwnd, (HMENU)IDC_CW_CORES_CANCEL, inst, NULL);
   if (!w->cores_list)
      return false;

   SendMessageA(w->cores_list, LVM_SETEXTENDEDLISTVIEWSTYLE,
         LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

   memset(&col, 0, sizeof(col));
   col.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
   col.pszText  = (LPSTR)"Name";
   col.cx       = 280;
   col.iSubItem = 0;
   SendMessageA(w->cores_list, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
   col.pszText  = (LPSTR)"Version";
   col.cx       = 110;
   col.iSubItem = 1;
   SendMessageA(w->cores_list, LVM_INSERTCOLUMNA, 1, (LPARAM)&col);

   SendMessageA(w->cores_hwnd, WM_SIZE, 0, 0);
   return true;
}

static void cw_cores_show(ui_companion_win32_wimp_t *w)
{
   if (!cw_cores_create(w))
   {
      cw_status_set(w, "Could not open the core list.");
      return;
   }
   cw_cores_fill(w);
   ShowWindow(w->cores_hwnd, SW_SHOW);
   SetForegroundWindow(w->cores_hwnd);
   SetFocus(w->cores_list);
}

/* SHBrowseForFolder is in shell32 on Windows 95 with the desktop update
 * and in every later release; ANSI entry point, no BIF_NEWDIALOGSTYLE. */
static void cw_scan_directory(ui_companion_win32_wimp_t *w)
{
   BROWSEINFOA bi;
   LPITEMIDLIST pidl;
   char dir[MAX_PATH];

   memset(&bi, 0, sizeof(bi));
   bi.hwndOwner = w->hwnd;
   bi.lpszTitle = "Select a directory to scan for content";
   bi.ulFlags   = BIF_RETURNONLYFSDIRS;

   pidl = SHBrowseForFolderA(&bi);
   if (!pidl)
      return;

   dir[0] = '\0';
   if (SHGetPathFromIDListA(pidl, dir) && dir[0])
   {
      if (companion_core_request_scan(w->core, dir, true,
               config_get_ptr()->bools.show_hidden_files))
         cw_status_set(w, "Scanning...");
      else
         cw_status_set(w, "Scanning is not available in this build.");
   }
   CoTaskMemFree(pidl);
}

static void cw_context_menu(ui_companion_win32_wimp_t *w, HWND from,
      int x, int y)
{
   HMENU menu = CreatePopupMenu();
   POINT pt;

   if (!menu)
      return;

   /* Keyboard-invoked (x,y == -1): anchor at the control. */
   if (x == -1 && y == -1)
   {
      RECT rc;
      GetWindowRect(from, &rc);
      x = rc.left + 8;
      y = rc.top  + 8;
   }
   pt.x = x;
   pt.y = y;

   if (from == w->entries)
   {
      AppendMenuA(menu, MF_STRING, IDM_CW_RUN,          "&Run");
      AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
      AppendMenuA(menu, MF_STRING, IDM_CW_DELETE_ENTRY, "&Delete Entry");
   }
   else if (from == w->playlists)
   {
      HMENU assoc = CreatePopupMenu();
      POINT cl    = pt;
      LRESULT hit;

      ScreenToClient(w->playlists, &cl);
      hit = SendMessageA(w->playlists, LB_ITEMFROMPOINT, 0,
            MAKELPARAM(cl.x, cl.y));
      /* HIWORD is non-zero when the point is outside any item. */
      w->ctx_playlist = HIWORD(hit) ? (size_t)-1 : (size_t)LOWORD(hit);

      if (assoc)
      {
         size_t i, n = companion_core_installed_core_count(w->core);
         if (n > (size_t)(IDM_CW_ASSOC_MAX - IDM_CW_ASSOC_BASE))
            n = (size_t)(IDM_CW_ASSOC_MAX - IDM_CW_ASSOC_BASE);

         AppendMenuA(assoc, MF_STRING, IDM_CW_ASSOC_DETECT, "<Detect>");
         if (n)
            AppendMenuA(assoc, MF_SEPARATOR, 0, NULL);
         for (i = 0; i < n; i++)
         {
            const char *name = companion_core_installed_core_name(w->core, i);
            AppendMenuA(assoc, MF_STRING, IDM_CW_ASSOC_BASE + (UINT)i,
                  name ? name : "");
         }
         AppendMenuA(menu, MF_POPUP, (UINT_PTR_COMPAT)assoc,
               "&Associate Core");
      }
      AppendMenuA(menu, MF_STRING, IDM_CW_REFRESH, "Re&fresh Playlists");
   }

   TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
         pt.x, pt.y, 0, w->hwnd, NULL);
   DestroyMenu(menu); /* destroys the submenu too */
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

      case WM_DESTROY:
         win32_modal_window_destroyed(hwnd);
         break;

      /* Dragging or sizing this window, or browsing its menu bar, runs a
       * modal loop inside DefWindowProc on the main thread. Clock the
       * run loop through it exactly as the main window does, or
       * RetroArch's video stops for the duration. */
      case WM_ENTERSIZEMOVE:
      case WM_ENTERMENULOOP:
         win32_modal_enter(hwnd);
         break;
      case WM_EXITSIZEMOVE:
      case WM_EXITMENULOOP:
         win32_modal_exit(hwnd);
         break;
      case WM_RA_MODAL_TICK:
         win32_modal_tick(hwnd);
         return 0;
      case WM_TIMER:
         if (wparam == WIN32_MODAL_TIMER_ID)
         {
            win32_modal_tick(hwnd);
            return 0;
         }
         break;

      /* Splitter */
      case WM_SETCURSOR:
         if (w && (HWND)wparam == hwnd && LOWORD(lparam) == HTCLIENT)
         {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            if (w->splitting || cw_on_splitter(w, pt.x, pt.y))
            {
               SetCursor(LoadCursorA(NULL, MAKEINTRESOURCEA(32644))); /* IDC_SIZEWE */
               return TRUE;
            }
         }
         break;
      case WM_LBUTTONDOWN:
         if (w && cw_on_splitter(w, (int)(short)LOWORD(lparam),
                  (int)(short)HIWORD(lparam)))
         {
            w->splitting = true;
            SetCapture(hwnd);
            return 0;
         }
         break;
      case WM_MOUSEMOVE:
         if (w && w->splitting)
         {
            w->pane_w = (int)(short)LOWORD(lparam) - COMPANION_WIN32_SPLIT_W / 2;
            cw_layout(w);
            return 0;
         }
         break;
      case WM_LBUTTONUP:
         if (w && w->splitting)
         {
            w->splitting = false;
            ReleaseCapture();
            return 0;
         }
         break;
      case WM_CAPTURECHANGED:
         if (w)
            w->splitting = false;
         break;

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
               /* The companion's own picker (installed cores by name /
                * version), like the Qt Load Core window. */
               cw_cores_show(w);
               return 0;
            case IDM_CW_LOAD_CONTENT:
               /* Reuse the platform driver's dialog flow exactly as the
                * main window menu does. */
               win32_menu_loop(main_window.hwnd, ID_M_LOAD_CONTENT);
               return 0;
            case IDM_CW_START_CORE:
               if (!companion_core_start_core(w->core))
                  cw_status_set(w, "Failed to start the core.");
               return 0;
            case IDM_CW_RUN:
               cw_run_selected(w);
               return 0;
            case IDM_CW_DELETE_ENTRY:
               cw_delete_selected(w);
               return 0;
            case IDM_CW_ASSOC_DETECT:
               cw_associate_core(w, IDM_CW_ASSOC_DETECT);
               return 0;
            case IDM_CW_REFRESH:
               companion_core_refresh_playlists(w->core);
               return 0;
            case IDM_CW_SCAN_DIR:
               cw_scan_directory(w);
               return 0;
            case IDM_CW_TOGGLE_LOG:
               cw_log_toggle(w);
               return 0;
            case IDM_CW_TOGGLE_INFO:
               cw_info_toggle(w);
               return 0;
            case IDM_CW_VIEW_LIST:
               cw_set_icon_view(w, false);
               return 0;
            case IDM_CW_VIEW_ICONS:
               cw_set_icon_view(w, true);
               return 0;
            case IDM_CW_CLOSE:
               ShowWindow(hwnd, SW_HIDE);
               return 0;
            case IDM_CW_QUIT:
               companion_core_event_command(w->core, CMD_EVENT_QUIT);
               return 0;
            default:
               if (     LOWORD(wparam) >= IDM_CW_ASSOC_BASE
                     && LOWORD(wparam) <= IDM_CW_ASSOC_MAX)
               {
                  cw_associate_core(w, LOWORD(wparam));
                  return 0;
               }
               break;
         }
         break;

      case WM_CONTEXTMENU:
         if (w && ((HWND)wparam == w->entries || (HWND)wparam == w->playlists))
         {
            cw_context_menu(w, (HWND)wparam,
                  (int)(short)LOWORD(lparam), (int)(short)HIWORD(lparam));
            return 0;
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
   AppendMenuA(file, MF_STRING, IDM_CW_SCAN_DIR,     "Scan &Directory...");
   AppendMenuA(file, MF_SEPARATOR, 0, NULL);
   AppendMenuA(file, MF_STRING, IDM_CW_CLOSE,        "&Close Window");
   AppendMenuA(file, MF_STRING, IDM_CW_QUIT,         "E&xit RetroArch");

   AppendMenuA(view, MF_STRING, IDM_CW_VIEW_LIST,    "&List");
   AppendMenuA(view, MF_STRING, IDM_CW_VIEW_ICONS,   "&Icons");
   AppendMenuA(view, MF_SEPARATOR, 0, NULL);
   AppendMenuA(view, MF_STRING, IDM_CW_RUN,          "&Run Selected\tEnter");
   AppendMenuA(view, MF_STRING, IDM_CW_REFRESH,      "Re&fresh Playlists\tF5");
   AppendMenuA(view, MF_SEPARATOR, 0, NULL);
   AppendMenuA(view, MF_STRING, IDM_CW_TOGGLE_LOG,   "&Log");
   AppendMenuA(view, MF_STRING, IDM_CW_TOGGLE_INFO,  "Core &Information");

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

   /* Hidden until View > Log; ES_READONLY keeps the user out, the
    * companion appends through EM_REPLACESEL regardless. */
   w->log = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
         WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY
         | ES_AUTOVSCROLL | ES_LEFT,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_LOG, inst, NULL);

   w->info = CreateWindowExA(WS_EX_CLIENTEDGE, "SysListView32", "",
         WS_CHILD | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_INFO, inst, NULL);

   if (!w->playlists || !w->entries || !w->status || !w->log || !w->info)
      return false;

   SendMessageA(w->info, LVM_SETEXTENDEDLISTVIEWSTYLE,
         LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
   {
      LVCOLUMNA icol;
      memset(&icol, 0, sizeof(icol));
      icol.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
      icol.pszText  = (LPSTR)"";
      icol.cx       = 110;
      icol.iSubItem = 0;
      SendMessageA(w->info, LVM_INSERTCOLUMNA, 0, (LPARAM)&icol);
      icol.pszText  = (LPSTR)"Core Information";
      icol.cx       = 400;
      icol.iSubItem = 1;
      SendMessageA(w->info, LVM_INSERTCOLUMNA, 1, (LPARAM)&icol);
   }

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

   g_win32_wimp    = w;
   w->ctx_playlist = (size_t)-1;
   w->pane_w       = COMPANION_WIN32_PANE_W;
   w->core         = companion_core_new(&cw_callbacks, w);

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
   if (w->thumbs)
      ImageList_Destroy(w->thumbs);
   if (w->cores_hwnd)
      DestroyWindow(w->cores_hwnd);
   if (w->cores_class_registered)
      UnregisterClassA(COMPANION_WIN32_CORES_CLASS, GetModuleHandleA(NULL));
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
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   settings_t *settings         = config_get_ptr();

   if (!w || !w->hwnd)
      return;
   if (!(settings->bools.ui_companion_toggle || force))
      return;

   companion_core_prepare_show_window(w->core);
   ShowWindow(w->hwnd, SW_SHOW);
   SetForegroundWindow(w->hwnd);
}

static void ui_companion_win32_wimp_iterate(void *data)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   if (!w)
      return;
   companion_core_iterate(w->core, COMPANION_WIN32_ITER_US);

   /* One thumbnail decode per frame while the icon view is showing. */
   if (w->icon_view && IsWindowVisible(w->hwnd))
      cw_thumb_step(w);

   /* A short strcmp per frame, only while the pane is shown. */
   if (     w->info_visible
         && strcmp(w->info_core, companion_core_current_core_path(w->core)))
      cw_info_fill(w);
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

static void ui_companion_win32_wimp_log_msg(void *data, const char *msg)
{
   cw_log_append((ui_companion_win32_wimp_t*)data, msg);
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
   ui_companion_win32_wimp_log_msg,
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
