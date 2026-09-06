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
#include <ctype.h>
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
#include <shellapi.h>
#include <shlobj.h>

#include <compat/strl.h>
#include <features/features_cpu.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
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
#include "../../msg_hash.h"
#include "../../version.h"
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
/* Startup window size, matching the Qt companion, clamped to the work
 * area with a floor so it stays usable on sub-720p displays (a 640x480
 * or 800x600 CRT on the 9x baseline included). */
#define COMPANION_WIN32_INIT_W     1280
#define COMPANION_WIN32_INIT_H     720
#define COMPANION_WIN32_ITER_US    2000
#define COMPANION_WIN32_PANE_MIN   100
#define COMPANION_WIN32_SPLIT_W    5     /* draggable gap between panes */
#define COMPANION_WIN32_MIN_W      480
#define COMPANION_WIN32_MIN_H      320
#define COMPANION_WIN32_LOG_H      120   /* log pane height when shown */
#define COMPANION_WIN32_INFO_W     280   /* core-info pane width when shown */
#define COMPANION_WIN32_SEARCH_H   24    /* search strip above the entries */
#define COMPANION_WIN32_LABEL_H    18    /* section caption ("Search", "Core"...) */
#define COMPANION_WIN32_TAB_H      24    /* Playlists / File Browser tab strip */
#define COMPANION_WIN32_CTRL_H     24    /* combo / button row */
#define COMPANION_WIN32_FOOTER_H   28    /* "N items" + View combo under the list */
#define COMPANION_WIN32_PL_ICON    32    /* playlist icon; sets the row height, as Qt's */
#define COMPANION_WIN32_DOCS_URL   "https://docs.libretro.com/"

/* Every size above is in 96-dpi logical pixels, as Qt's are; Qt renders
 * them at the monitor's DPI, so at 200% its 1280x720 window fills a
 * 2560x1440 screen. Scale ours the same way or the companion comes up
 * half the size with a font that does not fit its rows. */
#define CW_S(w, x) MulDiv((x), (w)->dpi, 96)

/* Thumbnail pipeline lock (a no-op without decode threads). */
#define CW_LOCK(w)   do { if ((w)->lock) slock_lock((w)->lock); } while (0)
#define CW_UNLOCK(w) do { if ((w)->lock) slock_unlock((w)->lock); } while (0)
#ifndef HAVE_THREADS
#undef  CW_LOCK
#undef  CW_UNLOCK
#define CW_LOCK(w)   ((void)0)
#define CW_UNLOCK(w) ((void)0)
#endif

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
   IDC_CW_SEARCH,     /* search box (left column, top) */
   IDC_CW_BOXART,     /* boxart preview (right column, below info) */
   IDC_CW_SEARCH_LABEL,
   IDC_CW_CLEAR,      /* "Clear" next to the search box */
   IDC_CW_BROWSER_LABEL,
   IDC_CW_TABS,       /* Playlists / File Browser */
   IDC_CW_CORE_LABEL,
   IDC_CW_CORE_COMBO, /* launch-with core selection */
   IDC_CW_CORE_INFO_BTN,
   IDC_CW_RUN_BTN,
   IDC_CW_ITEMS_LABEL,/* "N items" footer */
   IDC_CW_VIEW_LABEL,
   IDC_CW_VIEW_COMBO, /* List / Icons */
   IDC_CW_ZOOM_LABEL,
   IDC_CW_ZOOM,       /* icon-view zoom trackbar */
   IDC_CW_THUMB_COMBO,/* boxart / screenshot / title / logo for the grid */
   IDC_CW_BOXART_TABS,/* the same four, for the boxart pane */
   IDC_CW_INFO_LABEL,
   IDC_CW_BOXART_LABEL,
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
   IDM_CW_TOGGLE_BOXART,
   IDM_CW_VIEW_LIST,
   IDM_CW_VIEW_ICONS,
   IDM_CW_BROWSE_FILES,
   IDM_CW_FIND,
   IDM_CW_HELP_DOCS,
   IDM_CW_HELP_ABOUT,
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
   HWND search;      /* EDIT above the entries; substring filter */
   char filter[128]; /* lower-cased search text, "" = show all */
   HWND search_label, clear_btn;
   HWND browser_label, tabs;
   HWND core_label, core_combo, core_info_btn, run_btn;
   HWND items_label, view_label, view_combo;
   HWND info_label, boxart_label;
   HFONT font;              /* the system message font at this DPI */
   int dpi;                 /* LOGPIXELSX; all layout sizes scale by it */
   int text_h;              /* font height in px, drives row heights */
   HIMAGELIST pl_icons;     /* folder icon for the playlist list */
   HWND zoom_label, zoom, thumb_combo, boxart_tabs;
   int thumb_px;            /* icon-view thumbnail edge in px, from zoom */
   const char *boxart_subdir; /* boxart pane's type (its own tabs) */
   HWND boxart;      /* STATIC (SS_BITMAP): selected entry's boxart */
   HBITMAP boxart_bmp;
   bool boxart_visible;
   long boxart_entry; /* entry index shown, -1 = none */
   /* Core the info pane currently describes; the pane follows the
    * running core from the iterate hook (only the shader commands are
    * forwarded to companions, so a load/unload is not an event here). */
   char info_core[PATH_MAX_LENGTH];
   /* Files browser: the entries list shows the filesystem instead of the
    * selected playlist; a directory descends, a file loads. */
   bool browse_mode;
   /* Icon (grid) view: thumbnails in a 32-bit image list, index 0 the
    * placeholder; pending items decode one per frame while the view
    * is showing. */
   HIMAGELIST thumbs;
   /* The entry list is a virtual (LVS_OWNERDATA) list view: the control
    * asks for each row's text and image as it draws (LVN_GETDISPINFO),
    * so populating it costs one LVM_SETITEMCOUNT however large the
    * playlist, and thumbnails are decoded only for rows the control has
    * actually asked to draw - the model the Qt view uses. */
   size_t *rows;           /* visible row -> entry (or browse) index */
   size_t row_count;
   int *thumb_idx;         /* per row: 0 none/placeholder, -1 queued, >0 image */
   /* Rows whose thumbnail the control wants, oldest first. Each job
    * carries the resolved file path so the decode needs nothing from
    * the (single-threaded) companion core. */
   struct cw_thumb_job { size_t row; unsigned gen; char path[PATH_MAX_LENGTH]; } *req;
   size_t req_cap, req_head, req_len;
   /* Finished decodes, thumbnail-sized ARGB, waiting for the UI thread
    * to blit them into the image list. */
   struct cw_thumb_done { size_t row; unsigned gen; uint32_t *bits; int edge; } *ready;
   size_t ready_cap, ready_len;
   unsigned gen;            /* bumped whenever the list or size changes */
#ifdef HAVE_THREADS
   /* Decode pool: like the Qt companion's ThumbnailLoader thread, but
    * several of them, and they scale to thumbnail size themselves so
    * the UI thread never touches the full-size image. */
   slock_t *lock;
   scond_t *cond;
   sthread_t *workers[4];
   unsigned nworkers;
   bool quit;
#endif
   /* Image-list slots recycle in a ring so memory is bounded: slot_row
    * remembers which row owns a slot so it can be evicted. */
   size_t *slot_row;
   size_t slot_cap, slot_used, slot_next;
   bool icon_view;
   /* Which repository subdirectory the icon view and boxart pane show;
    * from desktop_menu_thumbnail_type, boxart default. */
   const char *thumb_subdir;
   bool started;   /* initial_playlist applied once after the first list */
   /* Load Core window (non-modal: a DialogBox would run its own loop). */
   HWND cores_hwnd;
   HWND cores_list;
   bool cores_class_registered;
   /* When the Load Core window was opened to run a specific content
    * item (Run on an entry with no core), the content to launch with
    * the picked core; empty when it is a plain "load a core" request. */
   char cores_content[PATH_MAX_LENGTH];
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

/* Case-insensitive substring test against the current filter; empty
 * filter matches everything. */
static bool cw_filter_match(ui_companion_win32_wimp_t *w, const char *s)
{
   char low[PATH_MAX_LENGTH];
   size_t i;
   if (!w->filter[0])
      return true;
   if (!s)
      return false;
   for (i = 0; s[i] && i < sizeof(low) - 1; i++)
      low[i] = (char)tolower((unsigned char)s[i]);
   low[i] = '\0';
   return strstr(low, w->filter) != NULL;
}

static HBITMAP cw_boxart_scale(const struct texture_image *img,
      int maxw, int maxh, uint32_t bg);
static uint32_t cw_sys_color_argb(int index);
static const char *cw_combo_core_path(ui_companion_win32_wimp_t *w);
static void cw_status_default(ui_companion_win32_wimp_t *w);

static void cw_playlists_rebuild(ui_companion_win32_wimp_t *w)
{
   size_t i, n;
   if (!w || !w->playlists)
      return;

   SendMessageA(w->playlists, LVM_DELETEALLITEMS, 0, 0);
   SendMessageA(w->playlists, WM_SETREDRAW, FALSE, 0);
   /* Drop last time's asset icons; index 0 (the shell folder) stays. */
   if (w->pl_icons)
      while (ImageList_GetImageCount(w->pl_icons) > 1)
         ImageList_Remove(w->pl_icons, 1);
   n = companion_core_playlist_count(w->core);
   for (i = 0; i < n; i++)
   {
      LVITEMA item;
      char icon[PATH_MAX_LENGTH];
      const char *name = companion_core_playlist_name(w->core, i);
      int img          = 0;

      /* Qt's per-system XMB dot-art icon, when the asset exists. */
      if (w->pl_icons && companion_core_playlist_icon_path(w->core, i,
               icon, sizeof(icon)))
      {
         struct texture_image ti;
         memset(&ti, 0, sizeof(ti));
         if (image_texture_load(&ti, icon))
         {
            HBITMAP bmp = cw_boxart_scale(&ti,
                  CW_S(w, COMPANION_WIN32_PL_ICON),
                  CW_S(w, COMPANION_WIN32_PL_ICON),
                  cw_sys_color_argb(COLOR_WINDOW));
            image_texture_free(&ti);
            if (bmp)
            {
               int idx = ImageList_Add(w->pl_icons, bmp, NULL);
               DeleteObject(bmp);
               if (idx > 0)
                  img = idx;
            }
         }
      }

      memset(&item, 0, sizeof(item));
      item.mask    = LVIF_TEXT | LVIF_IMAGE;
      item.iItem   = (int)i;
      item.iImage  = img;
      item.pszText = (LPSTR)(name ? name : "");
      SendMessageA(w->playlists, LVM_INSERTITEMA, 0, (LPARAM)&item);
   }
   SendMessageA(w->playlists, WM_SETREDRAW, TRUE, 0);
   /* One column, sized to the pane. */
   SendMessageA(w->playlists, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE_USEHEADER);
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
/* Icon-view thumbnail edge for a zoom of 0..100: 64..320 logical px,
 * scaled to the display, like Qt's zoom slider range. */
static int cw_thumb_edge(ui_companion_win32_wimp_t *w)
{
   unsigned z = companion_core_pref_icon_view_zoom(w->core);
   return CW_S(w, 64 + (int)(z * 256 / 100));
}

/* Icon-view cell spacing for the current thumbnail edge: the thumbnail
 * plus room for a two-line label, like Qt's grid cells. */
static void cw_icon_spacing_apply(ui_companion_win32_wimp_t *w)
{
   int T = cw_thumb_edge(w);
   w->thumb_px = T;
   SendMessageA(w->entries, LVM_SETICONSPACING, 0,
         MAKELPARAM(T + CW_S(w, 24), T + CW_S(w, 40)));
}

/* Letterbox @img into a T x T ARGB buffer (caller frees). Pure: safe on
 * a worker thread. Nearest-neighbour is plenty at thumbnail size. */
static uint32_t *cw_thumb_pixels(int T, const struct texture_image *img,
      uint32_t bg)
{
   uint32_t *buf;
   int sw, sh, ox, oy, x, y;

   if (!img->pixels || !img->width || !img->height || T < 1)
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
   return buf;
}

/* Decode @path and scale it to @T: the work a decode thread does. */
static uint32_t *cw_thumb_decode(const char *path, int T, uint32_t bg)
{
   struct texture_image img;
   uint32_t *bits = NULL;
   memset(&img, 0, sizeof(img));
   if (image_texture_load(&img, path))
   {
      bits = cw_thumb_pixels(T, &img, bg);
      image_texture_free(&img);
   }
   return bits;
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
   const int T = cw_thumb_edge(w);
   CW_LOCK(w);
   w->thumb_px = T;
   CW_UNLOCK(w);

   /* A new size means new cell spacing too, or the grid lays items out
    * for the old (or, at startup, zero) thumbnail size. */
   if (w->icon_view)
      cw_icon_spacing_apply(w);

   if (w->thumbs)
   {
      SendMessageA(w->entries, LVM_SETIMAGELIST, LVSIL_NORMAL, (LPARAM)NULL);
      ImageList_Destroy(w->thumbs);
      w->thumbs = NULL;
   }
   /* Forget every decoded / queued thumbnail: the list or its size
    * changed, so the slots are rebuilt on demand. Results already
    * decoding carry the old generation and are dropped when they land. */
   CW_LOCK(w);
   w->gen++;
   if (w->thumb_idx)
      memset(w->thumb_idx, 0, w->row_count * sizeof(*w->thumb_idx));
   w->req_len   = w->req_head = 0;
   while (w->ready_len)
      free(w->ready[--w->ready_len].bits);
   w->slot_used = w->slot_next = 0;
   CW_UNLOCK(w);
   (void)count;

   /* Bounded: at most slot_cap decoded thumbnails live in the list at a
    * time (a few screens' worth); older ones are recycled. ILC_COLOR32 is
    * honoured from comctl32 4.71; older ones pick a lower depth. */
   if (!w->slot_cap)
   {
      w->slot_cap  = 512;
      w->slot_row  = (size_t*)calloc(w->slot_cap, sizeof(*w->slot_row));
      w->req_cap   = w->slot_cap;
      w->req       = (struct cw_thumb_job*)calloc(w->req_cap, sizeof(*w->req));
      w->ready_cap = 256;
      w->ready     = (struct cw_thumb_done*)calloc(w->ready_cap, sizeof(*w->ready));
   }
   w->thumbs = ImageList_Create(T, T, ILC_COLOR32, (int)w->slot_cap + 1, 0);
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

/* --- Thumbnail pipeline ------------------------------------------------ */
/* UI thread: LVN_GETDISPINFO -> cw_thumb_request (resolve the file path,
 * queue a job). Decode threads: pop newest job, decode + scale, push a
 * ready result. UI thread, per frame: cw_thumb_collect blits ready
 * results into the image list and redraws their rows. Without
 * HAVE_THREADS the UI thread runs the decode itself under a budget. */

/* Queue a row's thumbnail (from LVN_GETDISPINFO when the control draws a
 * row that has no image yet). The path is resolved here, on the UI
 * thread, because the companion core is not thread-safe; the decode is
 * not. A full queue drops the oldest job - it was scrolled past. */
static void cw_thumb_request(ui_companion_win32_wimp_t *w, size_t row)
{
   char db_name[NAME_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   const struct playlist_entry *e;
   struct cw_thumb_job *job;
   size_t at;

   if (!w->req || row >= w->row_count || w->thumb_idx[row] != 0)
      return;
   e = w->browse_mode ? NULL : companion_core_entry(w->core, w->rows[row]);
   if (!e)
      return;
   strlcpy(db_name, e->db_name ? e->db_name : "", sizeof(db_name));
   path_remove_extension(db_name);
   if (!companion_core_thumbnail_path(w->core, db_name,
            w->thumb_subdir ? w->thumb_subdir : COMPANION_THUMB_BOXART,
            !string_is_empty(e->label) ? e->label : path_basename(e->path),
            e->path, path, sizeof(path)))
   {
      w->thumb_idx[row] = 0; /* no file: the placeholder is final */
      return;
   }

   CW_LOCK(w);
   w->thumb_idx[row] = -1;
   if (w->req_len == w->req_cap)
   {
      size_t old = w->req[w->req_head].row;
      if (old < w->row_count && w->thumb_idx[old] == -1)
         w->thumb_idx[old] = 0;
      w->req_head = (w->req_head + 1) % w->req_cap;
      w->req_len--;
   }
   at        = (w->req_head + w->req_len) % w->req_cap;
   job       = &w->req[at];
   job->row  = row;
   job->gen  = w->gen;
   strlcpy(job->path, path, sizeof(job->path));
   w->req_len++;
#ifdef HAVE_THREADS
   if (w->cond)
      scond_signal(w->cond);
#endif
   CW_UNLOCK(w);
}

/* Take the newest job (the row most likely still on screen). Lock held. */
static bool cw_thumb_pop(ui_companion_win32_wimp_t *w, struct cw_thumb_job *out)
{
   if (!w->req_len)
      return false;
   *out = w->req[(w->req_head + w->req_len - 1) % w->req_cap];
   w->req_len--;
   return true;
}

/* Park a finished decode for the UI thread. Lock held. Drops the result
 * when the ring is full (the row simply re-queues when drawn again). */
static void cw_thumb_push_ready(ui_companion_win32_wimp_t *w, size_t row,
      unsigned gen, uint32_t *bits, int edge)
{
   struct cw_thumb_done *d;
   if (w->ready_len == w->ready_cap)
   {
      free(bits);
      return;
   }
   d       = &w->ready[w->ready_len++];
   d->row  = row;
   d->gen  = gen;
   d->bits = bits;
   d->edge = edge;
}

#ifdef HAVE_THREADS
static void cw_thumb_worker(void *ud)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)ud;
   for (;;)
   {
      struct cw_thumb_job job;
      uint32_t *bits;
      int T;
      slock_lock(w->lock);
      while (!w->quit && (!w->req_len || w->ready_len == w->ready_cap))
         scond_wait(w->cond, w->lock);
      if (w->quit)
      {
         slock_unlock(w->lock);
         return;
      }
      cw_thumb_pop(w, &job);
      T = w->thumb_px;
      slock_unlock(w->lock);

      bits = cw_thumb_decode(job.path, T, cw_sys_color_argb(COLOR_WINDOW));

      slock_lock(w->lock);
      if (bits)
         cw_thumb_push_ready(w, job.row, job.gen, bits, T);
      else if (job.gen == w->gen && job.row < w->row_count
            && w->thumb_idx[job.row] == -1)
         w->thumb_idx[job.row] = 0; /* undecodable: placeholder is final */
      slock_unlock(w->lock);
   }
}

static void cw_thumb_pool_start(ui_companion_win32_wimp_t *w)
{
   unsigned n = cpu_features_get_core_amount();
   unsigned i;
   if (w->lock)
      return;
   w->lock = slock_new();
   w->cond = scond_new();
   if (!w->lock || !w->cond)
      return;
   /* Leave a core for RetroArch itself; one worker at least, four at most. */
   n = (n > 1) ? n - 1 : 1;
   if (n > 4)
      n = 4;
   for (i = 0; i < n; i++)
   {
      w->workers[i] = sthread_create(cw_thumb_worker, w);
      if (!w->workers[i])
         break;
      w->nworkers++;
   }
}

static void cw_thumb_pool_stop(ui_companion_win32_wimp_t *w)
{
   unsigned i;
   if (!w->lock)
      return;
   slock_lock(w->lock);
   w->quit = true;
   scond_signal(w->cond);
   slock_unlock(w->lock);
   for (i = 0; i < w->nworkers; i++)
   {
      /* one signal per waiter would need a broadcast; nudge each */
      slock_lock(w->lock);
      scond_signal(w->cond);
      slock_unlock(w->lock);
      sthread_join(w->workers[i]);
   }
   w->nworkers = 0;
   scond_free(w->cond);
   slock_free(w->lock);
   w->cond = NULL;
   w->lock = NULL;
}
#endif

/* UI thread: install one finished thumbnail into a (recycled) image-list
 * slot and redraw its row. Index 0 is the placeholder, so slot s lives
 * at image index s + 1. */
static void cw_thumb_install(ui_companion_win32_wimp_t *w, size_t row,
      uint32_t *bits, int edge)
{
   HBITMAP bmp;
   int slot;

   if (row >= w->row_count || w->thumb_idx[row] != -1 || edge != w->thumb_px)
      return; /* stale: list or zoom changed while it was decoding */
   bmp = cw_dib_from_argb(bits, edge, edge);
   if (!bmp)
   {
      CW_LOCK(w);
      w->thumb_idx[row] = 0;
      CW_UNLOCK(w);
      return;
   }
   if (w->slot_used < w->slot_cap)
   {
      slot = (int)w->slot_used++;
      ImageList_Add(w->thumbs, bmp, NULL);
   }
   else
   {
      size_t victim;
      slot         = (int)w->slot_next;
      w->slot_next = (w->slot_next + 1) % w->slot_cap;
      victim       = w->slot_row[slot];
      if (victim < w->row_count && w->thumb_idx[victim] == slot + 1)
      {
         CW_LOCK(w);
         w->thumb_idx[victim] = 0;
         CW_UNLOCK(w);
         SendMessageA(w->entries, LVM_REDRAWITEMS, victim, victim);
      }
      ImageList_Replace(w->thumbs, slot + 1, bmp, NULL);
   }
   DeleteObject(bmp);
   w->slot_row[slot] = row;
   CW_LOCK(w);
   w->thumb_idx[row] = slot + 1;
   CW_UNLOCK(w);
   SendMessageA(w->entries, LVM_REDRAWITEMS, row, row);
}

/* UI thread, per frame: blit every finished decode (cheap - a DIB and an
 * image-list replace each) and, without threads, do a little decoding. */
static void cw_thumb_collect(ui_companion_win32_wimp_t *w)
{
   size_t i, n;
   struct cw_thumb_done batch[32];

   if (!w->thumbs)
      return;

#ifndef HAVE_THREADS
   /* No decode threads: decode on the UI thread within a small budget. */
   {
      retro_time_t end = cpu_features_get_time_usec() + 4000;
      struct cw_thumb_job job;
      while (w->req_len && cpu_features_get_time_usec() < end
            && cw_thumb_pop(w, &job))
      {
         uint32_t *bits = cw_thumb_decode(job.path, w->thumb_px,
               cw_sys_color_argb(COLOR_WINDOW));
         if (bits)
            cw_thumb_push_ready(w, job.row, job.gen, bits, w->thumb_px);
         else if (job.row < w->row_count && w->thumb_idx[job.row] == -1)
            w->thumb_idx[job.row] = 0;
      }
   }
#endif

   /* Take a batch out from under the lock, then do the GDI work. */
   CW_LOCK(w);
   n = w->ready_len < 32 ? w->ready_len : 32;
   memcpy(batch, w->ready, n * sizeof(*batch));
   if (n < w->ready_len)
      memmove(w->ready, w->ready + n, (w->ready_len - n) * sizeof(*w->ready));
   w->ready_len -= n;
#ifdef HAVE_THREADS
   if (n && w->cond)
      scond_signal(w->cond); /* room in the ring again */
#endif
   CW_UNLOCK(w);

   for (i = 0; i < n; i++)
   {
      if (batch[i].gen == w->gen)
         cw_thumb_install(w, batch[i].row, batch[i].bits, batch[i].edge);
      free(batch[i].bits);
   }
}

static void cw_set_icon_view(ui_companion_win32_wimp_t *w, bool icons)
{
   LONG style;
   if (!w || !w->entries)
      return;
   if (w->icon_view != icons && w->started)
      companion_core_pref_set_icon_view(w->core, icons);
   w->icon_view = icons;
   style  = GetWindowLongA(w->entries, GWL_STYLE);
   style &= ~(LVS_TYPEMASK);
   style |= icons ? LVS_ICON : LVS_REPORT;
   SetWindowLongA(w->entries, GWL_STYLE, style);
   if (icons)
   {
      cw_icon_spacing_apply(w);
      SendMessageA(w->entries, LVM_ARRANGE, LVA_DEFAULT, 0);
   }
   InvalidateRect(w->entries, NULL, TRUE);
}

/* Size the row map and per-row thumbnail table for @n rows. */
static bool cw_rows_alloc(ui_companion_win32_wimp_t *w, size_t n)
{
   size_t *r, *rr;
   int *t;
   bool ok;
   /* Workers index thumb_idx by row and check row_count: swap the
    * arrays under the lock, and make any job in flight stale. */
   CW_LOCK(w);
   w->gen++;
   w->row_count = 0;
   r  = (size_t*)realloc(w->rows, (n ? n : 1) * sizeof(*r));
   t  = (int*)realloc(w->thumb_idx, (n ? n : 1) * sizeof(*t));
   if (r) w->rows      = r;
   if (t) w->thumb_idx = t;
   ok = (r && t);
   if (ok)
      memset(w->thumb_idx, 0, (n ? n : 1) * sizeof(*t));
   rr = r; (void)rr;
   CW_UNLOCK(w);
   return ok;
}

/* Hand the control its new row count; it draws through LVN_GETDISPINFO. */
static void cw_rows_commit(ui_companion_win32_wimp_t *w, size_t n)
{
   CW_LOCK(w);
   w->row_count = n;
   CW_UNLOCK(w);
   SendMessageA(w->entries, LVM_SETITEMCOUNT, (WPARAM)n,
         LVSICF_NOSCROLL);
   InvalidateRect(w->entries, NULL, TRUE);
}

static void cw_browse_rebuild(ui_companion_win32_wimp_t *w)
{
   size_t i, n = companion_core_browse_count(w->core);

   if (!cw_rows_alloc(w, n))
      n = 0;
   for (i = 0; i < n; i++)
      w->rows[i] = i;            /* browse rows are never filtered */
   cw_rows_commit(w, n);
   cw_thumbs_reset(w, n);        /* browse rows show the placeholder */
   cw_status_set(w, companion_core_browse_dir(w->core));
}

static void cw_entries_rebuild(ui_companion_win32_wimp_t *w)
{
   size_t i, n, row;
   char buf[64];

   if (!w || !w->entries)
      return;

   if (w->browse_mode)
   {
      cw_browse_rebuild(w);
      return;
   }

   n = companion_core_entry_count(w->core);

   /* Visible rows: every entry, or those matching the search filter.
    * That is the whole cost of populating the view - no per-item
    * inserts; the control draws rows through LVN_GETDISPINFO. */
   if (!cw_rows_alloc(w, n))
      n = 0;
   for (i = 0, row = 0; i < n; i++)
   {
      const struct playlist_entry *e = companion_core_entry(w->core, i);
      const char *label;
      if (!e)
         continue;
      if (w->filter[0])
      {
         label = !string_is_empty(e->label) ? e->label
               : (e->path ? e->path : "");
         if (!cw_filter_match(w, label))
            continue;
      }
      w->rows[row++] = i;
   }
   cw_rows_commit(w, row);
   cw_thumbs_reset(w, row);

   /* Qt selects the first entry of a freshly loaded playlist, so the
    * boxart pane and Core section show something at once. */
   if (row > 0)
      ListView_SetItemState(w->entries, 0,
            LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

   /* Qt's footer: "%1 items". */
   {
      /* The string is Qt-style "%1 items"; swap the placeholder for %u. */
      const char *fmt = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT);
      const char *p1  = strstr(fmt, "%1");
      if (p1)
         snprintf(buf, sizeof(buf), "%.*s%u%s", (int)(p1 - fmt), fmt,
               (unsigned)row, p1 + 2);
      else
         snprintf(buf, sizeof(buf), "%u", (unsigned)row);
   }
   if (w->items_label)
      SetWindowTextA(w->items_label, buf);
   /* The playlist has loaded; the status bar goes back to Qt's
    * "<version> - <core>" rather than staying on "Loading playlist...". */
   cw_status_default(w);
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
   {
      int pane_min = CW_S(w, COMPANION_WIN32_PANE_MIN);
      int split_w  = CW_S(w, COMPANION_WIN32_SPLIT_W);
      if (w->pane_w > rc.right - pane_min - split_w)
         w->pane_w = rc.right - pane_min - split_w;
      if (w->pane_w < pane_min)
         w->pane_w = pane_min;
   }

   {
      int log_h    = (w->log_visible && w->log) ? CW_S(w, COMPANION_WIN32_LOG_H) : 0;
      bool r_info  = (w->info_visible && w->info);
      bool r_box   = (w->boxart_visible && w->boxart);
      int right_w  = (r_info || r_box) ? CW_S(w, COMPANION_WIN32_INFO_W) : 0;
      int list_h   = rc.bottom - status_h - log_h;
      int entry_x  = w->pane_w + CW_S(w, COMPANION_WIN32_SPLIT_W);
      int entry_w  = rc.right - entry_x - right_w;
      /* Rows follow the font: a caption is one line, a control a line
       * plus button chrome. */
      const int L  = w->text_h + CW_S(w, 4);
      const int C  = w->text_h + CW_S(w, 10);
      const int P  = CW_S(w, 4); /* padding */
      const int TAB_H = w->text_h + CW_S(w, 10);
      int y;
      if (list_h < 0)
         list_h = 0;
      if (entry_w < CW_S(w, COMPANION_WIN32_PANE_MIN))
      {
         right_w = 0;
         r_info  = r_box = false;
         entry_w = rc.right - entry_x;
      }

      /* Left column, laid out like the Qt companion's docks:
       *   Search        [ edit ][Clear]
       *   Content Browser
       *   [Playlists | File Browser]
       *   [ playlist list ...                ]
       *   Core
       *   [ launch-with combo ][Info][Run]  */
      y = P;
      {
         int btn_w = CW_S(w, 56); /* "Clear" */
         int sm_w  = CW_S(w, 48); /* "Info" / "Run" */
         MoveWindow(w->search_label, P, y, w->pane_w - 2 * P, L, TRUE);
         y += L;
         MoveWindow(w->search, P, y, w->pane_w - 3 * P - btn_w, C, TRUE);
         MoveWindow(w->clear_btn, w->pane_w - P - btn_w, y, btn_w, C, TRUE);
         y += C + P;
         MoveWindow(w->browser_label, P, y, w->pane_w - 2 * P, L, TRUE);
         y += L;
         MoveWindow(w->tabs, P, y, w->pane_w - 2 * P, TAB_H, TRUE);
         y += TAB_H;
         {
            int core_h = P + L + C + P;           /* the Core section */
            int pl_h   = list_h - y - core_h;
            if (pl_h < 0)
               pl_h = 0;
            MoveWindow(w->playlists, P, y, w->pane_w - 2 * P, pl_h, TRUE);
            /* One column, the width of the list less the icon. */
            SendMessageA(w->playlists, LVM_SETCOLUMNWIDTH, 0,
                  w->pane_w - 2 * P - CW_S(w, COMPANION_WIN32_PL_ICON) - CW_S(w, 24));
            y += pl_h + P;
            MoveWindow(w->core_label, P, y, w->pane_w - 2 * P, L, TRUE);
            y += L;
            /* A COMBOBOX's height is its dropped-list height; the closed
             * control stays one row tall regardless. */
            MoveWindow(w->core_combo, P, y, w->pane_w - 4 * P - 2 * sm_w,
                  C + 10 * (w->text_h + CW_S(w, 4)), TRUE);
            MoveWindow(w->core_info_btn, w->pane_w - 2 * P - 2 * sm_w, y, sm_w, C, TRUE);
            MoveWindow(w->run_btn, w->pane_w - P - sm_w, y, sm_w, C, TRUE);
         }
      }

      /* Centre: the content view with Qt's footer ("N items" left,
       * View combo right). */
      {
         int fh   = C + 2 * P;
         int eh   = list_h - fh;
         int cb_w = CW_S(w, 110);   /* View combo */
         int lb_w = CW_S(w, 44);    /* "View" caption */
         if (eh < 0)
            eh = 0;
         int list_room = C + 5 * (w->text_h + CW_S(w, 4)); /* combo drop room */
         int tb_w = CW_S(w, 130);   /* Thumbnail type combo */
         int zs_w = CW_S(w, 140);   /* zoom slider */
         int x    = entry_x + entry_w - P;
         MoveWindow(w->entries, entry_x, 0, entry_w, eh, TRUE);
         MoveWindow(w->items_label, entry_x + P, eh + P + (C - L) / 2,
               CW_S(w, 160), L, TRUE);
         /* Right-aligned run, as Qt's footer: View, Thumbnail, Zoom. */
         x -= cb_w;
         MoveWindow(w->view_combo, x, eh + P, cb_w, list_room, TRUE);
         x -= P + tb_w;
         MoveWindow(w->thumb_combo, x, eh + P, tb_w, list_room, TRUE);
         x -= P + zs_w;
         MoveWindow(w->zoom, x, eh + P, zs_w, C, TRUE);
         x -= P + lb_w;
         MoveWindow(w->zoom_label, x, eh + P + (C - L) / 2, lb_w, L, TRUE);
         MoveWindow(w->view_label, 0, 0, 0, 0, TRUE); /* Qt shows none */
         /* Name / Core columns share the list width (Qt: name wider). */
         SendMessageA(w->entries, LVM_SETCOLUMNWIDTH, 0, (entry_w * 2) / 3 - CW_S(w, 8));
         SendMessageA(w->entries, LVM_SETCOLUMNWIDTH, 1, entry_w / 3 - CW_S(w, 24));
      }

      /* Right column: "Core Info" caption + list on top, "Boxart" caption
       * + image below; each takes the full column when alone. */
      {
         int rx     = entry_x + entry_w;
         /* Qt's Core Info and Boxart docks share the column equally. */
         int box_h  = r_box ? (r_info ? list_h / 2 : list_h) : 0;
         int info_h = r_info ? list_h - box_h : 0;
         int iw     = right_w - P;
         if (r_info)
         {
            MoveWindow(w->info_label, rx + P, P, iw - P, L, TRUE);
            MoveWindow(w->info, rx + P, P + L, iw - P, info_h - L - P, TRUE);
            /* One text column; wide enough that long firmware lines
             * scroll horizontally rather than truncate, as Qt's do. */
            SendMessageA(w->info, LVM_SETCOLUMNWIDTH, 0, CW_S(w, 800));
         }
         else
         {
            MoveWindow(w->info_label, rx, 0, 0, 0, TRUE);
            MoveWindow(w->info, rx, 0, 0, 0, TRUE);
         }
         if (r_box)
         {
            int tab_h = w->text_h + CW_S(w, 10);
            MoveWindow(w->boxart_label, rx + P, info_h, iw - P, L, TRUE);
            MoveWindow(w->boxart_tabs, rx + P, info_h + L, iw - P, tab_h, TRUE);
            MoveWindow(w->boxart, rx + P, info_h + L + tab_h, iw - P,
                  box_h - L - tab_h, TRUE);
         }
         else
         {
            MoveWindow(w->boxart_label, rx, 0, 0, 0, TRUE);
            MoveWindow(w->boxart_tabs, rx, 0, 0, 0, TRUE);
            MoveWindow(w->boxart, rx, 0, 0, 0, TRUE);
         }
         ShowWindow(w->info_label, r_info ? SW_SHOW : SW_HIDE);
         ShowWindow(w->boxart_label, r_box ? SW_SHOW : SW_HIDE);
         ShowWindow(w->boxart_tabs, r_box ? SW_SHOW : SW_HIDE);
      }

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

   /* Qt shows the core picked in the Core combo (the entry's own or the
    * playlist default), not only the running one. */
   strlcpy(w->info_core, cw_combo_core_path(w), sizeof(w->info_core));
   companion_core_core_info_rows(w->info_core, keys, values);

   SendMessageA(w->info, WM_SETREDRAW, FALSE, 0);
   SendMessageA(w->info, LVM_DELETEALLITEMS, 0, 0);
   for (i = 0; i < keys->size; i++)
   {
      /* Qt renders each row as "Key: value" on one line; do the same in
       * a single column so nothing is cut at a column boundary. */
      char line[1024];
      const char *k = keys->elems[i].data   ? keys->elems[i].data   : "";
      const char *v = values->elems[i].data ? values->elems[i].data : "";
      if (*k && *v)
         snprintf(line, sizeof(line), "%s %s", k, v);
      else
         strlcpy(line, *k ? k : v, sizeof(line));
      memset(&item, 0, sizeof(item));
      item.mask     = LVIF_TEXT;
      item.iItem    = (int)i;
      item.pszText  = line;
      SendMessageA(w->info, LVM_INSERTITEMA, 0, (LPARAM)&item);
   }
   SendMessageA(w->info, WM_SETREDRAW, TRUE, 0);

   string_list_free(keys);
   string_list_free(values);
}

/* Scale @img to fit @maxw x @maxh preserving aspect, as a 32-bit DIB. */
static HBITMAP cw_boxart_scale(const struct texture_image *img,
      int maxw, int maxh, uint32_t bg)
{
   uint32_t *buf;
   HBITMAP bmp;
   int dw, dh, x, y;

   if (!img->pixels || !img->width || !img->height || maxw < 1 || maxh < 1)
      return NULL;

   dw = maxw;
   dh = (int)((unsigned)maxw * img->height / img->width);
   if (dh > maxh)
   {
      dh = maxh;
      dw = (int)((unsigned)maxh * img->width / img->height);
   }
   if (dw < 1) dw = 1;
   if (dh < 1) dh = 1;

   buf = (uint32_t*)malloc((size_t)dw * dh * sizeof(uint32_t));
   if (!buf)
      return NULL;
   for (y = 0; y < dh; y++)
   {
      const uint32_t *src = img->pixels
         + (size_t)((unsigned)y * img->height / dh) * img->width;
      uint32_t *dst = buf + (size_t)y * dw;
      for (x = 0; x < dw; x++)
      {
         /* Composite the source alpha over @bg so transparent PNGs (the
          * XMB icons) sit on the control colour rather than on black;
          * the result is opaque, which every comctl32 draws the same. */
         uint32_t s = src[(unsigned)x * img->width / dw];
         unsigned a = (s >> 24) & 0xff;
         if (a == 0xff || bg == 0)
            dst[x] = s | 0xff000000u;
         else
         {
            unsigned ia = 255 - a;
            unsigned r  = (((s >> 16) & 0xff) * a + ((bg >> 16) & 0xff) * ia) / 255;
            unsigned g  = (((s >>  8) & 0xff) * a + ((bg >>  8) & 0xff) * ia) / 255;
            unsigned b  = (( s        & 0xff) * a + ( bg        & 0xff) * ia) / 255;
            dst[x] = 0xff000000u | (r << 16) | (g << 8) | b;
         }
      }
   }
   bmp = cw_dib_from_argb(buf, dw, dh);
   free(buf);
   return bmp;
}

/* Show the boxart of entry @entry (index into the playlist, or -1 to
 * clear). No-op if the pane already shows it. */
static void cw_boxart_update(ui_companion_win32_wimp_t *w, long entry)
{
   char path[PATH_MAX_LENGTH];
   char db_name[NAME_MAX_LENGTH];
   struct texture_image img;
   const struct playlist_entry *e;
   RECT rc;
   HBITMAP bmp = NULL;

   if (!w || !w->boxart || !w->boxart_visible)
      return;
   if (entry == w->boxart_entry)
      return;
   w->boxart_entry = entry;

   if (entry >= 0 && (e = companion_core_entry(w->core, (size_t)entry)))
   {
      strlcpy(db_name, e->db_name ? e->db_name : "", sizeof(db_name));
      path_remove_extension(db_name);
      memset(&img, 0, sizeof(img));
      GetClientRect(w->boxart, &rc);
      if (companion_core_thumbnail_path(w->core, db_name,
               w->boxart_subdir ? w->boxart_subdir : COMPANION_THUMB_BOXART,
               !string_is_empty(e->label) ? e->label : path_basename(e->path),
               e->path, path, sizeof(path))
            && image_texture_load(&img, path))
      {
         bmp = cw_boxart_scale(&img, rc.right - 4, rc.bottom - 4,
               cw_sys_color_argb(COLOR_BTNFACE));
         image_texture_free(&img);
      }
   }

   SendMessageA(w->boxart, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)bmp);
   if (w->boxart_bmp)
      DeleteObject(w->boxart_bmp);
   w->boxart_bmp = bmp;
}

/* The playlist-entry index behind the entries' focused row, or -1. */
static long cw_focused_entry(ui_companion_win32_wimp_t *w)
{
   LRESULT row = SendMessageA(w->entries, LVM_GETNEXTITEM,
         (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
   if (row < 0 || w->browse_mode || (size_t)row >= w->row_count)
      return -1;
   return (long)w->rows[row];
}

static void cw_boxart_toggle(ui_companion_win32_wimp_t *w)
{
   if (!w || !w->boxart)
      return;
   w->boxart_visible = !w->boxart_visible;
   ShowWindow(w->boxart, w->boxart_visible ? SW_SHOW : SW_HIDE);
   cw_layout(w);
   if (w->boxart_visible)
   {
      w->boxart_entry = -2; /* force a refresh */
      cw_boxart_update(w, cw_focused_entry(w));
   }
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
   return x >= w->pane_w && x < w->pane_w + CW_S(w, COMPANION_WIN32_SPLIT_W)
      && y >= 0 && y < rc.bottom - status_h;
}

/* --- companion_core -> Win32 callbacks -------------------------------- */

static void cw_on_playlists_changed(void *ud)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)ud;
   cw_playlists_rebuild(w);

   /* Startup: open the playlist Qt would - desktop_menu_initial_playlist,
    * falling back to History. Once. */
   if (w && !w->started)
   {
      char initial[PATH_MAX_LENGTH];
      size_t i, n = companion_core_playlist_count(w->core);
      long pick   = -1;
      w->started  = true;

      strlcpy(initial, companion_core_pref_initial_playlist(w->core),
            sizeof(initial));
      if (initial[0])
      {
         for (i = 0; i < n && pick < 0; i++)
         {
            const char *p_i = companion_core_playlist_path(w->core, i);
            if (p_i && string_is_equal(p_i, initial))
               pick = (long)i;
         }
      }
      /* No (or unknown) start playlist: All Playlists, index 0, as the
       * Qt companion opens. */
      if (pick < 0 && n > 0)
         pick = 0;
      if (pick >= 0 && w->playlists)
      {
         ListView_SetItemState(w->playlists, (int)pick,
               LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
         ListView_EnsureVisible(w->playlists, (int)pick, FALSE);
         /* LVN_ITEMCHANGED from the above selects it through
          * cw_select_playlist. */
      }
   }
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
   LRESULT sel = SendMessageA(w->playlists, LVM_GETNEXTITEM,
         (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
   if (sel < 0)
      return;
   w->browse_mode = false; /* picking a playlist leaves the file browser */
   if (w->tabs)
      SendMessageA(w->tabs, TCM_SETCURSEL, 0, 0);
   if (companion_core_select_playlist(w->core, (size_t)sel))
      cw_status_set(w, "Loading playlist...");
}

static void cw_cores_show(ui_companion_win32_wimp_t *w, const char *content);
static long cw_selected_entry(ui_companion_win32_wimp_t *w);
static void cw_run_selected(ui_companion_win32_wimp_t *w);

/* --- Core section (launch-with combo, like Qt's Core dock) ------------- */

/* Item data in the combo: the companion_launch_selection of that row; the
 * core path for the first three kinds is kept in a parallel table. */
#define CW_COMBO_MAX 8
static char cw_combo_paths[CW_COMBO_MAX][PATH_MAX_LENGTH];

static void cw_core_combo_fill(ui_companion_win32_wimp_t *w, long entry)
{
   companion_launch_option_t opts[CW_COMBO_MAX - 2];
   size_t i, n = 0;
   const struct playlist_entry *e = NULL;
   char pl_name[NAME_MAX_LENGTH];
   LRESULT idx;

   if (!w || !w->core_combo)
      return;
   SendMessageA(w->core_combo, CB_RESETCONTENT, 0, 0);

   pl_name[0] = '\0';
   if (entry >= 0 && !w->browse_mode)
      e = companion_core_entry(w->core, (size_t)entry);
   if (e && e->db_name)
   {
      strlcpy(pl_name, e->db_name, sizeof(pl_name));
      path_remove_extension(pl_name);
   }

   n = companion_core_launch_options(w->core,
         e ? e->core_path : NULL, e ? e->core_name : NULL, pl_name,
         companion_core_pref_suggest_loaded_core_first(w->core),
         opts, sizeof(opts) / sizeof(opts[0]));

   for (i = 0; i < n; i++)
   {
      idx = SendMessageA(w->core_combo, CB_ADDSTRING, 0, (LPARAM)opts[i].name);
      if (idx >= 0 && idx < CW_COMBO_MAX)
      {
         SendMessageA(w->core_combo, CB_SETITEMDATA, (WPARAM)idx,
               (LPARAM)opts[i].selection);
         strlcpy(cw_combo_paths[idx], opts[i].path, PATH_MAX_LENGTH);
      }
   }
   idx = SendMessageA(w->core_combo, CB_ADDSTRING, 0,
         (LPARAM)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK));
   if (idx >= 0)
      SendMessageA(w->core_combo, CB_SETITEMDATA, (WPARAM)idx,
            (LPARAM)COMPANION_LAUNCH_ASK);
   {
      char label[64];
      snprintf(label, sizeof(label), "%s...",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE));
      idx = SendMessageA(w->core_combo, CB_ADDSTRING, 0, (LPARAM)label);
      if (idx >= 0)
         SendMessageA(w->core_combo, CB_SETITEMDATA, (WPARAM)idx,
               (LPARAM)COMPANION_LAUNCH_LOAD_CORE);
   }
   SendMessageA(w->core_combo, CB_SETCURSEL, 0, 0);
}

/* Core path the Core combo currently names: the running / entry / default
 * core for those kinds, else the running core's path. */
static const char *cw_combo_core_path(ui_companion_win32_wimp_t *w)
{
   LRESULT idx, sel;
   if (!w->core_combo)
      return companion_core_current_core_path(w->core);
   idx = SendMessageA(w->core_combo, CB_GETCURSEL, 0, 0);
   if (idx < 0 || idx >= CW_COMBO_MAX)
      return companion_core_current_core_path(w->core);
   sel = SendMessageA(w->core_combo, CB_GETITEMDATA, (WPARAM)idx, 0);
   switch ((enum companion_launch_selection)sel)
   {
      case COMPANION_LAUNCH_CURRENT:
      case COMPANION_LAUNCH_PLAYLIST_SAVED:
      case COMPANION_LAUNCH_PLAYLIST_DEFAULT:
         if (cw_combo_paths[idx][0])
            return cw_combo_paths[idx];
         break;
      default:
         break;
   }
   return companion_core_current_core_path(w->core);
}

/* Run the selected entry with the core chosen in the combo. */
static void cw_run_with_combo(ui_companion_win32_wimp_t *w)
{
   char content[PATH_MAX_LENGTH];
   const struct playlist_entry *e;
   LRESULT idx;
   LRESULT sel;
   long entry = cw_selected_entry(w);

   if (w->browse_mode || entry < 0)
   {
      cw_run_selected(w); /* browse mode / nothing: the default path */
      return;
   }
   e = companion_core_entry(w->core, (size_t)entry);
   if (!e)
      return;

   idx = SendMessageA(w->core_combo, CB_GETCURSEL, 0, 0);
   sel = (idx >= 0) ? SendMessageA(w->core_combo, CB_GETITEMDATA, (WPARAM)idx, 0)
                    : COMPANION_LAUNCH_ASK;

   switch ((enum companion_launch_selection)sel)
   {
      case COMPANION_LAUNCH_CURRENT:
      case COMPANION_LAUNCH_PLAYLIST_SAVED:
      case COMPANION_LAUNCH_PLAYLIST_DEFAULT:
         if (idx < CW_COMBO_MAX && companion_core_request_load_content(
                  w->core, cw_combo_paths[idx], e->path, e->label,
                  e->db_name, e->crc32))
            ShowWindow(w->hwnd, SW_HIDE);
         else
            cw_status_set(w, "Failed to load the content.");
         return;
      case COMPANION_LAUNCH_LOAD_CORE:
         cw_cores_show(w, NULL);
         return;
      case COMPANION_LAUNCH_ASK:
      default:
         strlcpy(content, e->path ? e->path : "", sizeof(content));
         cw_cores_show(w, content);
         return;
   }
}

/* Status-bar default, as Qt shows: "<version> - <core or No Core>". */
static void cw_status_default(ui_companion_win32_wimp_t *w)
{
   char buf[NAME_MAX_LENGTH + 32];
   const char *core = companion_core_current_core_name(w->core);
   snprintf(buf, sizeof(buf), "%s - %s", PACKAGE_VERSION,
         (core && *core) ? core
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE));
   cw_status_set(w, buf);
}

static LRESULT cw_selected_row(ui_companion_win32_wimp_t *w)
{
   return SendMessageA(w->entries, LVM_GETNEXTITEM,
         (WPARAM)-1, MAKELPARAM(LVNI_SELECTED, 0));
}

/* The playlist-entry (or browse) index behind the selected row. Browse
 * rows are never filtered so row == index there; playlist rows carry the
 * entry index in lParam because a filter makes rows non-contiguous.
 * Returns -1 when nothing is selected. */
static long cw_selected_entry(ui_companion_win32_wimp_t *w)
{
   LRESULT row = cw_selected_row(w);
   if (row < 0 || (size_t)row >= w->row_count)
      return -1;
   return (long)w->rows[row]; /* browse rows map 1:1 */
}

static void cw_run_selected(ui_companion_win32_wimp_t *w)
{
   char content[PATH_MAX_LENGTH];
   long idx = cw_selected_entry(w);
   if (idx < 0)
      return;

   if (w->browse_mode)
   {
      bool needs_core = false;
      int r = companion_core_browse_activate(w->core, (size_t)idx,
            NULL, &needs_core, content, sizeof(content));
      if (r == 0)
         cw_browse_rebuild(w);       /* entered a directory */
      else if (r == 1)
         ShowWindow(w->hwnd, SW_HIDE); /* content loaded */
      else if (needs_core)
         cw_cores_show(w, content);  /* pick a core for this file */
      return;
   }

   /* No usable core for this entry: ask, filtered to what runs it. */
   if (companion_core_entry_needs_core(w->core, (size_t)idx,
            content, sizeof(content)))
   {
      cw_cores_show(w, content);
      return;
   }

   if (companion_core_request_load_entry(w->core, (size_t)idx))
      ShowWindow(w->hwnd, SW_HIDE);
}

static void cw_browse_enter(ui_companion_win32_wimp_t *w)
{
   w->browse_mode = true;
   if (w->tabs)
      SendMessageA(w->tabs, TCM_SETCURSEL, 1, 0);
   if (!companion_core_browse_dir(w->core)[0])
      companion_core_browse_open(w->core, NULL);
   cw_browse_rebuild(w);
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
   size_t sel = companion_core_selected_playlist(w->core);
   long idx   = cw_selected_entry(w);
   const char *path;

   if (idx < 0 || sel == (size_t)-1)
      return;
   if (MessageBoxA(w->hwnd, "Delete this playlist entry?", COMPANION_WIN32_TITLE,
            MB_YESNO | MB_ICONQUESTION) != IDYES)
      return;

   /* The entry's own playlist file and its index there - under All
    * Playlists these differ from the selected slot / aggregate index. */
   path = companion_core_entry_playlist_path(w->core, (size_t)idx);
   if (path && companion_core_playlist_delete_entry(w->core, path,
            companion_core_entry_index_in_playlist(w->core, (size_t)idx)))
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

   size_t supported;

   SendMessageA(w->cores_list, LVM_DELETEALLITEMS, 0, 0);
   SendMessageA(w->cores_list, WM_SETREDRAW, FALSE, 0);

   /* When launching specific content, put the cores that can run it
    * first and show only those; otherwise show every installed core. */
   n = companion_core_installed_core_count(w->core);
   if (w->cores_content[0])
   {
      supported = companion_core_installed_cores_supporting(w->core,
            w->cores_content);
      if (supported > 0)
         n = supported;
   }

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

   if (w->cores_content[0])
   {
      /* Run the pending content with the chosen core. */
      if (companion_core_request_load_content(w->core, path,
               w->cores_content, NULL, NULL, NULL))
         ShowWindow(w->hwnd, SW_HIDE);
      else
         cw_status_set(w, "Failed to load the content.");
      w->cores_content[0] = '\0';
   }
   else if (companion_core_load_core(w->core, path))
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

static void cw_cores_show(ui_companion_win32_wimp_t *w, const char *content)
{
   if (!cw_cores_create(w))
   {
      cw_status_set(w, "Could not open the core list.");
      return;
   }
   if (content && *content)
      strlcpy(w->cores_content, content, sizeof(w->cores_content));
   else
      w->cores_content[0] = '\0';
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
               companion_core_pref_show_hidden_files(w->core)))
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

      LVHITTESTINFO ht;
      ScreenToClient(w->playlists, &cl);
      memset(&ht, 0, sizeof(ht));
      ht.pt = cl;
      hit   = SendMessageA(w->playlists, LVM_HITTEST, 0, (LPARAM)&ht);
      w->ctx_playlist = (hit < 0) ? (size_t)-1 : (size_t)hit;

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
            mmi->ptMinTrackSize.x = w ? CW_S(w, COMPANION_WIN32_MIN_W) : COMPANION_WIN32_MIN_W;
            mmi->ptMinTrackSize.y = w ? CW_S(w, COMPANION_WIN32_MIN_H) : COMPANION_WIN32_MIN_H;
         }
         return 0;

      case WM_HSCROLL:
         /* Zoom slider: a released thumb (or keyboard step) re-sizes the
          * icon view's thumbnails; the drag itself is not chased. */
         if (w && (HWND)lparam == w->zoom
               && (LOWORD(wparam) == TB_ENDTRACK || LOWORD(wparam) == TB_LINEUP
                  || LOWORD(wparam) == TB_LINEDOWN || LOWORD(wparam) == TB_PAGEUP
                  || LOWORD(wparam) == TB_PAGEDOWN))
         {
            LRESULT z = SendMessageA(w->zoom, TBM_GETPOS, 0, 0);
            companion_core_pref_set_icon_view_zoom(w->core, (unsigned)z);
            if (w->icon_view)
               cw_entries_rebuild(w);
            else
               w->thumb_px = cw_thumb_edge(w);
            return 0;
         }
         break;

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
            w->pane_w = (int)(short)LOWORD(lparam) - CW_S(w, COMPANION_WIN32_SPLIT_W) / 2;
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
            case IDC_CW_CLEAR:
               SetWindowTextA(w->search, ""); /* EN_CHANGE re-filters */
               return 0;
            case IDC_CW_RUN_BTN:
               cw_run_with_combo(w);
               return 0;
            case IDC_CW_CORE_INFO_BTN:
               cw_info_toggle(w);
               return 0;
            case IDC_CW_CORE_COMBO:
               if (HIWORD(wparam) == CBN_SELCHANGE)
                  cw_info_fill(w);
               return 0;
            case IDC_CW_THUMB_COMBO:
               if (HIWORD(wparam) == CBN_SELCHANGE)
               {
                  LRESULT t = SendMessageA(w->thumb_combo, CB_GETCURSEL, 0, 0);
                  companion_core_pref_set_thumbnail_type(w->core, (unsigned)t);
                  w->thumb_subdir = companion_core_pref_thumbnail_subdir(w->core);
                  cw_entries_rebuild(w); /* new image list, redecode */
               }
               return 0;
            case IDC_CW_VIEW_COMBO:
               if (HIWORD(wparam) == CBN_SELCHANGE)
                  cw_set_icon_view(w,
                        SendMessageA(w->view_combo, CB_GETCURSEL, 0, 0) == 1);
               return 0;
            case IDM_CW_HELP_DOCS:
               ShellExecuteA(hwnd, "open", COMPANION_WIN32_DOCS_URL,
                     NULL, NULL, SW_SHOWNORMAL);
               return 0;
            case IDM_CW_HELP_ABOUT:
               MessageBoxA(hwnd, "RetroArch " PACKAGE_VERSION,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT),
                     MB_OK | MB_ICONINFORMATION);
               return 0;
            case IDC_CW_SEARCH:
               if (HIWORD(wparam) == EN_CHANGE)
               {
                  char raw[128];
                  size_t k;
                  GetWindowTextA(w->search, raw, sizeof(raw));
                  for (k = 0; raw[k]; k++)
                     w->filter[k] = (char)tolower((unsigned char)raw[k]);
                  w->filter[k] = '\0';
                  /* Browse listing is not filtered; re-run the active one. */
                  if (w->browse_mode)
                     cw_browse_rebuild(w);
                  else
                     cw_entries_rebuild(w);
               }
               return 0;
            case IDM_CW_LOAD_CORE:
               /* The companion's own picker (installed cores by name /
                * version), like the Qt Load Core window. */
               cw_cores_show(w, NULL);
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
            case IDM_CW_FIND:
               SetFocus(w->search);
               return 0;
            case IDM_CW_BROWSE_FILES:
               cw_browse_enter(w);
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
            case IDM_CW_TOGGLE_BOXART:
               cw_boxart_toggle(w);
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
         if (!w)
            break;
         {
            NMHDR *hdr = (NMHDR*)lparam;
            if (hdr->idFrom == IDC_CW_ENTRIES)
            {
               switch (hdr->code)
               {
                  case LVN_GETDISPINFOA: /* ANSI control, ANSI notification */
                     {
                        /* Virtual list: text and image for one row, as
                         * the control draws it. */
                        LVITEMA *it = &((NMLVDISPINFOA*)lparam)->item;
                        size_t row  = (size_t)it->iItem;
                        if (row >= w->row_count)
                           return 0;
                        if (it->mask & LVIF_TEXT)
                        {
                           const char *s = "";
                           if (w->browse_mode)
                           {
                              const char *name = companion_core_browse_name(w->core, row);
                              if (it->iSubItem == 0)
                              {
                                 if (companion_core_browse_is_dir(w->core, row)
                                       && name && strcmp(name, ".."))
                                 {
                                    snprintf(it->pszText, (size_t)it->cchTextMax,
                                          "%s\\", name);
                                    s = NULL; /* already written */
                                 }
                                 else
                                    s = name ? name : "";
                              }
                           }
                           else
                           {
                              const struct playlist_entry *e =
                                 companion_core_entry(w->core, w->rows[row]);
                              if (e)
                                 s = (it->iSubItem == 1)
                                    ? (!string_is_empty(e->core_name) ? e->core_name : "")
                                    : (!string_is_empty(e->label) ? e->label
                                          : (e->path ? e->path : ""));
                           }
                           if (s)
                              strlcpy(it->pszText, s, (size_t)it->cchTextMax);
                        }
                        if (it->mask & LVIF_IMAGE)
                        {
                           int t = w->thumb_idx ? w->thumb_idx[row] : 0;
                           it->iImage = t > 0 ? t : 0;
                           /* Ask for it: the control wants this row on
                            * screen and it has no image yet. */
                           if (t == 0 && w->icon_view && !w->browse_mode)
                              cw_thumb_request(w, row);
                        }
                        return 0;
                     }
                  case NM_DBLCLK:
                  case NM_RETURN:
                     cw_run_selected(w);
                     return 0;
                  case LVN_ITEMCHANGED:
                     {
                        NMLISTVIEW *nm = (NMLISTVIEW*)lparam;
                        if ((nm->uChanged & LVIF_STATE)
                              && (nm->uNewState & LVIS_SELECTED))
                        {
                           long e = cw_focused_entry(w);
                           cw_boxart_update(w, e);
                           cw_core_combo_fill(w, e);
                           cw_info_fill(w);
                        }
                     }
                     break;
                  default:
                     break;
               }
            }
            else if (hdr->idFrom == IDC_CW_PLAYLISTS)
            {
               if (hdr->code == LVN_ITEMCHANGED)
               {
                  NMLISTVIEW *nm = (NMLISTVIEW*)lparam;
                  if ((nm->uChanged & LVIF_STATE)
                        && (nm->uNewState & LVIS_SELECTED))
                     cw_select_playlist(w);
               }
            }
            else if (hdr->idFrom == IDC_CW_BOXART_TABS)
            {
               if (hdr->code == TCN_SELCHANGE)
               {
                  switch (SendMessageA(w->boxart_tabs, TCM_GETCURSEL, 0, 0))
                  {
                     case 1:  w->boxart_subdir = COMPANION_THUMB_TITLE;      break;
                     case 2:  w->boxart_subdir = COMPANION_THUMB_SCREENSHOT; break;
                     case 3:  w->boxart_subdir = COMPANION_THUMB_LOGO;       break;
                     default: w->boxart_subdir = COMPANION_THUMB_BOXART;     break;
                  }
                  w->boxart_entry = -2; /* force a refresh */
                  cw_boxart_update(w, cw_focused_entry(w));
                  return 0;
               }
            }
            else if (hdr->idFrom == IDC_CW_TABS)
            {
               if (hdr->code == TCN_SELCHANGE)
               {
                  LRESULT tab = SendMessageA(w->tabs, TCM_GETCURSEL, 0, 0);
                  if (tab == 1)
                     cw_browse_enter(w);
                  else
                  {
                     w->browse_mode = false;
                     cw_entries_rebuild(w);
                  }
                  companion_core_pref_set_last_tab(w->core, (int)tab);
                  return 0;
               }
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
   AppendMenuA(file, MF_STRING, IDM_CW_BROWSE_FILES, "&Browse Files");
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
   AppendMenuA(view, MF_STRING, IDM_CW_TOGGLE_BOXART, "&Boxart");

   {
      /* Same top-level titles as the Qt menubar, from the same strings. */
      HMENU edit = CreatePopupMenu();
      HMENU help = CreatePopupMenu();
      char find[64];
      snprintf(find, sizeof(find), "%s\tCtrl+F",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH));
      AppendMenuA(edit, MF_STRING, IDM_CW_FIND, find);
      AppendMenuA(help, MF_STRING, IDM_CW_HELP_DOCS,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION));
      AppendMenuA(help, MF_STRING, IDM_CW_HELP_ABOUT,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT));
      AppendMenuA(bar, MF_POPUP, (UINT_PTR_COMPAT)file,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_FILE));
      AppendMenuA(bar, MF_POPUP, (UINT_PTR_COMPAT)edit,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT));
      AppendMenuA(bar, MF_POPUP, (UINT_PTR_COMPAT)view,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW));
      AppendMenuA(bar, MF_POPUP, (UINT_PTR_COMPAT)help,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_MENU_HELP));
   }
   return bar;
}

/* A plain child control: STATIC / BUTTON / COMBOBOX, in the GUI font. */
static HWND cw_make(ui_companion_win32_wimp_t *w, const char *cls,
      const char *text, DWORD style, int id)
{
   HWND h = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style,
         0, 0, 0, 0, w->hwnd, (HMENU)(UINT_PTR_COMPAT)id,
         GetModuleHandleA(NULL), NULL);
   if (h && w->font)
      SendMessageA(h, WM_SETFONT, (WPARAM)w->font, TRUE);
   return h;
}

/* Apply the GUI font to every child; the default is the bold, bitmapped
 * System font, which is most of why the window looked nothing like Qt. */
static BOOL CALLBACK cw_setfont_cb(HWND h, LPARAM lp)
{
   SendMessageA(h, WM_SETFONT, (WPARAM)lp, TRUE);
   return TRUE;
}

static bool cw_create_window(ui_companion_win32_wimp_t *w)
{
   WNDCLASSA wc;
   LVCOLUMNA col;
   HINSTANCE inst = GetModuleHandleA(NULL);

   InitCommonControls();

   /* DPI and the system message font at that DPI, before any control is
    * created. lfMessageFont scales with the display; DEFAULT_GUI_FONT
    * is a fixed 8pt bitmap that does not. */
   {
      HDC hdc = GetDC(NULL);
      NONCLIENTMETRICSA ncm;
      TEXTMETRICA tm;
      w->dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
      if (w->dpi <= 0)
         w->dpi = 96;
      memset(&ncm, 0, sizeof(ncm));
      ncm.cbSize = sizeof(ncm);
      if (SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
         w->font = CreateFontIndirectA(&ncm.lfMessageFont);
      if (!w->font)
         w->font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
      w->text_h = CW_S(w, 16);
      if (hdc)
      {
         HFONT old = (HFONT)SelectObject(hdc, w->font);
         if (GetTextMetricsA(hdc, &tm))
            w->text_h = tm.tmHeight;
         SelectObject(hdc, old);
         ReleaseDC(NULL, hdc);
      }
      /* Qt's left dock is about 280 logical px wide. */
      w->pane_w = CW_S(w, 280);
   }


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

   {
      /* Work area (screen minus taskbar); fall back to the full screen
       * if the call fails, as it can on 9x. */
      RECT wa;
      int sw, sh, ww, wh, wx, wy;
      if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0))
      {
         wa.left = wa.top = 0;
         wa.right  = GetSystemMetrics(SM_CXSCREEN);
         wa.bottom = GetSystemMetrics(SM_CYSCREEN);
      }
      {
         /* Qt's 1280x720 is logical; at this DPI it is this many pixels. */
         int iw = CW_S(w, COMPANION_WIN32_INIT_W);
         int ih = CW_S(w, COMPANION_WIN32_INIT_H);
         int mw = CW_S(w, COMPANION_WIN32_MIN_W);
         int mh = CW_S(w, COMPANION_WIN32_MIN_H);
         sw = wa.right  - wa.left;
         sh = wa.bottom - wa.top;
         ww = (iw < sw) ? iw : sw;
         wh = (ih < sh) ? ih : sh;
         if (ww < mw && mw < sw)
            ww = mw;
         if (wh < mh && mh < sh)
            wh = mh;
      }
      wx = wa.left + (sw - ww) / 2;
      wy = wa.top  + (sh - wh) / 2;

      w->hwnd = CreateWindowExA(0, COMPANION_WIN32_CLASS,
            COMPANION_WIN32_TITLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            wx, wy, ww, wh, NULL, cw_build_menu(), inst, NULL);
   }
   if (!w->hwnd)
      return false;

   /* Playlist list: a list view with a folder icon per row, like Qt's. */
   w->playlists = CreateWindowExA(WS_EX_CLIENTEDGE, "SysListView32", "",
         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER
         | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_PLAYLISTS, inst, NULL);
   if (w->playlists)
   {
      SHFILEINFOA sfi;
      LVCOLUMNA c;
      w->pl_icons = ImageList_Create(CW_S(w, COMPANION_WIN32_PL_ICON),
            CW_S(w, COMPANION_WIN32_PL_ICON), ILC_COLOR32 | ILC_MASK, 1, 1);
      memset(&sfi, 0, sizeof(sfi));
      /* The shell's folder icon, without touching the filesystem. */
      if (w->pl_icons && SHGetFileInfoA("folder", FILE_ATTRIBUTE_DIRECTORY,
               &sfi, sizeof(sfi),
               SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES)
            && sfi.hIcon)
      {
         ImageList_AddIcon(w->pl_icons, sfi.hIcon);
         DestroyIcon(sfi.hIcon);
      }
      if (w->pl_icons)
         SendMessageA(w->playlists, LVM_SETIMAGELIST, LVSIL_SMALL,
               (LPARAM)w->pl_icons);
      memset(&c, 0, sizeof(c));
      c.mask = LVCF_WIDTH;
      c.cx   = 180;
      SendMessageA(w->playlists, LVM_INSERTCOLUMNA, 0, (LPARAM)&c);
      SendMessageA(w->playlists, LVM_SETEXTENDEDLISTVIEWSTYLE,
            LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
   }

   /* Section captions and controls that make up Qt's docks. */
   w->search_label  = cw_make(w, "STATIC", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH), SS_LEFT, IDC_CW_SEARCH_LABEL);
   w->clear_btn     = cw_make(w, "BUTTON", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR), BS_PUSHBUTTON, IDC_CW_CLEAR);
   w->browser_label = cw_make(w, "STATIC", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER), SS_LEFT,
            IDC_CW_BROWSER_LABEL);
   w->tabs          = cw_make(w, "SysTabControl32", "", WS_CLIPSIBLINGS, IDC_CW_TABS);
   if (w->tabs)
   {
      TCITEMA ti;
      memset(&ti, 0, sizeof(ti));
      ti.mask    = TCIF_TEXT;
      ti.pszText = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS);
      SendMessageA(w->tabs, TCM_INSERTITEMA, 0, (LPARAM)&ti);
      ti.pszText = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER);
      SendMessageA(w->tabs, TCM_INSERTITEMA, 1, (LPARAM)&ti);
   }
   w->core_label    = cw_make(w, "STATIC", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_CORE), SS_LEFT, IDC_CW_CORE_LABEL);
   w->core_combo    = cw_make(w, "COMBOBOX", "",
         CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CW_CORE_COMBO);
   w->core_info_btn = cw_make(w, "BUTTON", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_INFO), BS_PUSHBUTTON, IDC_CW_CORE_INFO_BTN);
   w->run_btn       = cw_make(w, "BUTTON", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_RUN), BS_PUSHBUTTON, IDC_CW_RUN_BTN);
   w->items_label   = cw_make(w, "STATIC", "", SS_LEFT, IDC_CW_ITEMS_LABEL);
   w->view_label    = cw_make(w, "STATIC", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_VIEW), SS_RIGHT, IDC_CW_VIEW_LABEL);
   w->view_combo    = cw_make(w, "COMBOBOX", "", CBS_DROPDOWNLIST, IDC_CW_VIEW_COMBO);
   if (w->view_combo)
   {
      SendMessageA(w->view_combo, CB_ADDSTRING, 0,
            (LPARAM)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST));
      SendMessageA(w->view_combo, CB_ADDSTRING, 0,
            (LPARAM)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS));
      SendMessageA(w->view_combo, CB_SETCURSEL, 0, 0);
   }
   /* Qt's footer also has a Zoom slider and a Thumbnail type combo. */
   w->zoom_label  = cw_make(w, "STATIC", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_ZOOM), SS_RIGHT, IDC_CW_ZOOM_LABEL);
   w->zoom        = cw_make(w, "msctls_trackbar32", "",
         TBS_HORZ | TBS_NOTICKS | WS_TABSTOP, IDC_CW_ZOOM);
   if (w->zoom)
   {
      SendMessageA(w->zoom, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
      SendMessageA(w->zoom, TBM_SETPOS, TRUE,
            (LPARAM)companion_core_pref_icon_view_zoom(w->core));
   }
   w->thumb_combo = cw_make(w, "COMBOBOX", "", CBS_DROPDOWNLIST, IDC_CW_THUMB_COMBO);
   if (w->thumb_combo)
   {
      SendMessageA(w->thumb_combo, CB_ADDSTRING, 0,
            (LPARAM)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART));
      SendMessageA(w->thumb_combo, CB_ADDSTRING, 0,
            (LPARAM)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT));
      SendMessageA(w->thumb_combo, CB_ADDSTRING, 0,
            (LPARAM)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN));
      SendMessageA(w->thumb_combo, CB_ADDSTRING, 0,
            (LPARAM)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO));
      SendMessageA(w->thumb_combo, CB_SETCURSEL,
            (WPARAM)companion_core_pref_thumbnail_type(w->core), 0);
   }
   /* Qt's boxart dock has tabs for the four thumbnail types. */
   w->boxart_tabs = cw_make(w, "SysTabControl32", "", WS_CLIPSIBLINGS, IDC_CW_BOXART_TABS);
   if (w->boxart_tabs)
   {
      TCITEMA ti;
      memset(&ti, 0, sizeof(ti));
      ti.mask    = TCIF_TEXT;
      ti.pszText = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART);
      SendMessageA(w->boxart_tabs, TCM_INSERTITEMA, 0, (LPARAM)&ti);
      ti.pszText = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN);
      SendMessageA(w->boxart_tabs, TCM_INSERTITEMA, 1, (LPARAM)&ti);
      ti.pszText = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT);
      SendMessageA(w->boxart_tabs, TCM_INSERTITEMA, 2, (LPARAM)&ti);
      ti.pszText = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO);
      SendMessageA(w->boxart_tabs, TCM_INSERTITEMA, 3, (LPARAM)&ti);
   }
   w->boxart_subdir = COMPANION_THUMB_BOXART;

   w->info_label    = cw_make(w, "STATIC", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_CORE_INFO), SS_LEFT, IDC_CW_INFO_LABEL);
   w->boxart_label  = cw_make(w, "STATIC", msg_hash_to_str(
            MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART), SS_LEFT, IDC_CW_BOXART_LABEL);

   /* Qt shows the Core Info and Boxart docks by default. */
   w->info_visible   = true;
   w->boxart_visible = true;

   w->entries = CreateWindowExA(WS_EX_CLIENTEDGE, "SysListView32", "",
         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS
         | LVS_OWNERDATA,
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

   w->search = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
         WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_SEARCH, inst, NULL);

   w->boxart = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "",
         WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_CENTERIMAGE,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_BOXART, inst, NULL);
   w->boxart_entry = -1;

   w->info = CreateWindowExA(WS_EX_CLIENTEDGE, "SysListView32", "",
         WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
         0, 0, 0, 0, w->hwnd, (HMENU)IDC_CW_INFO, inst, NULL);

   if (!w->playlists || !w->entries || !w->status || !w->log || !w->info
         || !w->search || !w->boxart)
      return false;

   SendMessageA(w->info, LVM_SETEXTENDEDLISTVIEWSTYLE,
         LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
   {
      LVCOLUMNA icol;
      memset(&icol, 0, sizeof(icol));
      icol.mask     = LVCF_WIDTH | LVCF_SUBITEM;
      icol.cx       = 800;
      icol.iSubItem = 0;
      SendMessageA(w->info, LVM_INSERTCOLUMNA, 0, (LPARAM)&icol);
   }

   /* Full-row select is an IE3+ extended style; harmless where absent. */
   SendMessageA(w->entries, LVM_SETEXTENDEDLISTVIEWSTYLE,
         LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

   memset(&col, 0, sizeof(col));
   col.mask     = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
   col.pszText  = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_NAME);
   col.cx       = 380;
   col.iSubItem = 0;
   SendMessageA(w->entries, LVM_INSERTCOLUMNA, 0, (LPARAM)&col);
   col.pszText  = (LPSTR)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QT_CORE);
   col.cx       = 180;
   col.iSubItem = 1;
   SendMessageA(w->entries, LVM_INSERTCOLUMNA, 1, (LPARAM)&col);

   /* Every control in the GUI font (the ones created before the font was
    * chosen included). */
   if (w->font)
      EnumChildWindows(w->hwnd, cw_setfont_cb, (LPARAM)w->font);

   cw_layout(w);
   cw_status_default(w);
   cw_info_fill(w);
   cw_core_combo_fill(w, -1);
#ifdef HAVE_THREADS
   cw_thumb_pool_start(w);
#endif
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

   /* Apply the shared companion settings (retroarch.cfg) the Qt
    * companion also honours. */
   if (companion_core_pref_icon_view(w->core))
      cw_set_icon_view(w, true);
   if (w->view_combo)
      SendMessageA(w->view_combo, CB_SETCURSEL, w->icon_view ? 1 : 0, 0);
   w->thumb_subdir = companion_core_pref_thumbnail_subdir(w->core);
   if (companion_core_pref_last_tab(w->core) == 1)
      cw_browse_enter(w);

   companion_core_refresh_playlists(w->core);
   return w;
}

static void ui_companion_win32_wimp_deinit(void *data)
{
   ui_companion_win32_wimp_t *w = (ui_companion_win32_wimp_t*)data;
   if (!w)
      return;
   if (w->boxart_bmp)
      DeleteObject(w->boxart_bmp);
   if (w->pl_icons)
      ImageList_Destroy(w->pl_icons);
   if (w->font && w->font != (HFONT)GetStockObject(DEFAULT_GUI_FONT))
      DeleteObject(w->font);
#ifdef HAVE_THREADS
   cw_thumb_pool_stop(w);
#endif
   if (w->thumbs)
      ImageList_Destroy(w->thumbs);
   while (w->ready && w->ready_len)
      free(w->ready[--w->ready_len].bits);
   free(w->rows);
   free(w->thumb_idx);
   free(w->req);
   free(w->ready);
   free(w->slot_row);
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

   /* Thumbnails decoded by the pool land here, a DIB blit each. */
   if (w->icon_view && IsWindowVisible(w->hwnd))
      cw_thumb_collect(w);

   /* A short strcmp per frame: the info pane and the "<version> - <core>"
    * status follow the running core. */
   if (strcmp(w->info_core, cw_combo_core_path(w)))
   {
      if (w->info_visible)
         cw_info_fill(w);
      else
         strlcpy(w->info_core, cw_combo_core_path(w), sizeof(w->info_core));
      cw_status_default(w);
   }
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
