/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* Regression harness for the Win32 companion (ui/drivers/
 * ui_win32_companion.c), built with mingw and run under Wine + Xvfb by
 * tools/companion_win32_test.sh. The real driver, the real companion
 * core, the core test's stubs and fixtures; RetroArch's window and the
 * modal helpers are stubbed here.
 *
 * It drives the driver the way RetroArch and a user do - init, iterate
 * until the playlist lands, then the messages the widgets produce
 * (the View combo's CBN_SELCHANGE, the tab's TCN_SELCHANGE, the
 * menu's WM_COMMAND ids, list selections) - and asserts on the real
 * controls: the entries list's count and view style, the playlist
 * list's items and text, the boxart pane's image, the secondary
 * windows' rows, the status bar. Wine's comctl32 is not Microsoft's,
 * but the object graph, the messages and their side effects are the
 * same, which is where the bugs have been. */

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../../../configuration.h"
#include "../../../runloop.h"
#include "../../../core_option_manager.h"
#include "../../../ui/ui_companion_driver.h"
#include "../../../ui/companion/companion_core.h"
#include "../../../ui/drivers/ui_win32.h"

/* --- stubs for what the driver reaches into RetroArch for --------------- */
ui_window_win32_t main_window;
static int stub_menu_loop_calls;
LRESULT win32_menu_loop(HWND hwnd, WPARAM wparam) { (void)hwnd; (void)wparam; stub_menu_loop_calls++; return 0; }

extern settings_t test_settings;
extern runloop_state_t test_runloop;
extern int stub_calls_command;
extern int stub_calls_shader_apply;
extern size_t stub_core_count;
extern ui_companion_driver_t ui_companion_wimp_win32;
extern void companion_test_setup_fixtures(char *root, size_t len);
extern void companion_test_teardown_fixtures(const char *root);

/* The driver's private struct: its first two fields are stable. */
struct wimp_peek { companion_core_t *core; HWND hwnd; };

#include "../../../ui/drivers/ui_win32_companion_ids.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } else { printf("[ok] "); printf(__VA_ARGS__); printf("\n"); } fflush(stdout); } while (0)

/* Pump messages and the driver for a while. */
static void pump(void *data, int ms)
{
   DWORD end = GetTickCount() + (DWORD)ms;
   do
   {
      MSG msg;
      ui_companion_wimp_win32.iterate(data);
      while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
      {
         TranslateMessage(&msg);
         DispatchMessageA(&msg);
      }
      Sleep(5);
   } while (GetTickCount() < end);
}

static void lv_text(HWND lv, int item, int sub, char *buf, int len)
{
   LVITEMA it;
   memset(&it, 0, sizeof(it));
   it.iSubItem   = sub;
   it.pszText    = buf;
   it.cchTextMax = len;
   buf[0] = '\0';
   SendMessageA(lv, LVM_GETITEMTEXTA, (WPARAM)item, (LPARAM)&it);
}

static void send_command(HWND hwnd, int id) { SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(id, 0), 0); }

/* Find a top-level window created by the driver with the given class. */
static HWND find_class_window(const char *cls)
{
   return FindWindowA(cls, NULL);
}

int main(void)
{
   char root[512];
   void *data;
   struct wimp_peek *peek;
   HWND hwnd, entries, playlists, tabs, view, status;
   char buf[512];
   INITCOMMONCONTROLSEX ic = { sizeof(ic), ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES };
   InitCommonControlsEx(&ic);

   companion_test_setup_fixtures(root, sizeof(root));
   test_settings.bools.ui_companion_toggle    = true;
   test_settings.uints.desktop_menu_view_type = 0;

   data = ui_companion_wimp_win32.init();
   CHECK(data != NULL, "driver init (window built)");
   if (!data)
      return 1;
   peek      = (struct wimp_peek*)data;
   hwnd      = peek->hwnd;
   entries   = GetDlgItem(hwnd, IDC_CW_ENTRIES);
   playlists = GetDlgItem(hwnd, IDC_CW_PLAYLISTS);
   tabs      = GetDlgItem(hwnd, IDC_CW_TABS);
   view      = GetDlgItem(hwnd, IDC_CW_VIEW_COMBO);
   status    = GetDlgItem(hwnd, IDC_CW_STATUS);
   CHECK(hwnd && entries && playlists && tabs && view, "controls found (hwnd=%p entries=%p playlists=%p tabs=%p view=%p)", (void*)hwnd, (void*)entries, (void*)playlists, (void*)tabs, (void*)view);
   ui_companion_wimp_win32.toggle(data, true);
   pump(data, 400);

   /* playlists listed, one selected, its entries land through iterate */
   CHECK(SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0) == 4, "playlist list has 4 rows (%d)", (int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0));
   lv_text(playlists, 2, 0, buf, sizeof(buf));
   CHECK(string_is_equal(buf, "Nintendo - Nintendo Entertainment System"), "row 2 text (got %s)", buf);
   ListView_SetItemState(playlists, 2, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
   pump(data, 600);
   CHECK(companion_core_entry_count(peek->core) == 3, "NES entries landed (%u)", (unsigned)companion_core_entry_count(peek->core));
   CHECK(SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0) == 3, "entries list shows 3 (%d)", (int)SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0));
   lv_text(entries, 0, 0, buf, sizeof(buf));
   CHECK(strlen(buf) > 0, "entry 0 has text (%s)", buf);

   /* Icons view through the combo (what a click sends) */
   SendMessageA(view, CB_SETCURSEL, 1, 0);
   SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(IDC_CW_VIEW_COMBO, CBN_SELCHANGE), (LPARAM)view);
   pump(data, 300);
   {
      LONG_PTR st = GetWindowLongPtrA(entries, GWL_STYLE);
      CHECK((st & LVS_TYPEMASK) == LVS_ICON, "entries list is in icon view (style 0x%lx)", (long)(st & LVS_TYPEMASK));
      CHECK(SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0) == 3, "icon view keeps the 3 items");
   }
   send_command(hwnd, IDM_CW_VIEW_LIST);
   pump(data, 200);
   CHECK((GetWindowLongPtrA(entries, GWL_STYLE) & LVS_TYPEMASK) == LVS_REPORT, "back to list view");

   /* search filters */
   SetWindowTextA(GetDlgItem(hwnd, IDC_CW_SEARCH), "metroid");
   pump(data, 200);
   CHECK(SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0) == 1, "search 'metroid' leaves 1 (%d)", (int)SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0));
   send_command(hwnd, IDC_CW_CLEAR);
   pump(data, 200);
   CHECK(SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0) == 3, "clear restores 3");

   /* File Browser tab: the listing lands async; folders in the left list,
    * the whole listing in the content list with 4 columns */
   {
      NMHDR nm;
      TabCtrl_SetCurSel(tabs, 1);
      nm.hwndFrom = tabs; nm.idFrom = IDC_CW_TABS; nm.code = TCN_SELCHANGE;
      SendMessageA(hwnd, WM_NOTIFY, IDC_CW_TABS, (LPARAM)&nm);
   }
   pump(data, 900);
   CHECK(companion_core_browse_count(peek->core) >= 5, "browse listing landed (%u)", (unsigned)companion_core_browse_count(peek->core));
   CHECK(SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0) == (LRESULT)companion_core_browse_dir_count(peek->core), "folder pane rows = dir count (%d)", (int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0));
   lv_text(playlists, 0, 0, buf, sizeof(buf));
   CHECK(string_is_equal(buf, ".."), "folder row 0 is '..' (got %s)", buf);
   CHECK(SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0) == (LRESULT)companion_core_browse_count(peek->core), "content rows = browse count (%d)", (int)SendMessageA(entries, LVM_GETITEMCOUNT, 0, 0));
   CHECK(Header_GetItemCount(ListView_GetHeader(entries)) == 4, "Name / Size / Type / Date columns (%d)", Header_GetItemCount(ListView_GetHeader(entries)));
   lv_text(entries, 2, 2, buf, sizeof(buf));
   CHECK(strstr(buf, "File") != NULL, "Type column text for a file (%s)", buf);
   {
      HBITMAP bm = (HBITMAP)SendMessageA(GetDlgItem(hwnd, IDC_CW_BOXART), STM_GETIMAGE, IMAGE_BITMAP, 0);
      CHECK(bm == NULL, "boxart pane empty in the browser");
   }
   /* header click sorts by Type */
   {
      NMLISTVIEW nl;
      memset(&nl, 0, sizeof(nl));
      nl.hdr.hwndFrom = entries; nl.hdr.idFrom = IDC_CW_ENTRIES; nl.hdr.code = LVN_COLUMNCLICK; nl.iSubItem = 2;
      SendMessageA(hwnd, WM_NOTIFY, IDC_CW_ENTRIES, (LPARAM)&nl);
      pump(data, 200);
      CHECK(companion_core_browse_sort_column(peek->core) == COMPANION_BROWSE_SORT_TYPE, "column click sorts by Type");
      SendMessageA(hwnd, WM_NOTIFY, IDC_CW_ENTRIES, (LPARAM)&nl);
      pump(data, 200);
      CHECK(!companion_core_browse_sort_ascending(peek->core), "same column again flips to descending");
   }
   /* Up / Start Directory */
   send_command(hwnd, IDC_CW_BR_UP);
   pump(data, 900);
   CHECK(!string_is_equal(companion_core_browse_dir(peek->core), ""), "Up navigated (dir=%s)", companion_core_browse_dir(peek->core));
   send_command(hwnd, IDC_CW_BR_START);
   pump(data, 900);
   CHECK(strstr(companion_core_browse_dir(peek->core), "content") != NULL, "Start Directory returns to content (%s)", companion_core_browse_dir(peek->core));

   /* back to playlists */
   {
      NMHDR nm;
      TabCtrl_SetCurSel(tabs, 0);
      nm.hwndFrom = tabs; nm.idFrom = IDC_CW_TABS; nm.code = TCN_SELCHANGE;
      SendMessageA(hwnd, WM_NOTIFY, IDC_CW_TABS, (LPARAM)&nm);
   }
   pump(data, 400);
   CHECK(SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0) == 4, "playlist list back to 4 rows");

   /* menus: Stop / Unload reach the core; the three windows open with rows */
   {
      int before = stub_calls_command;
      send_command(hwnd, IDC_CW_STOP_BTN);
      CHECK(stub_calls_command > before, "Stop reaches the core");
   }
   {
      static struct retro_core_option_v2_definition defs[3];
      struct retro_core_options_v2 v2;
      char cfg[600];
      HWND ow, ol;
      memset(defs, 0, sizeof(defs));
      defs[0].key = "test_speed"; defs[0].desc = "Speed";
      defs[0].values[0].value = "slow"; defs[0].values[0].label = "Slow";
      defs[0].values[1].value = "fast"; defs[0].values[1].label = "Fast";
      defs[0].default_value = "fast";
      defs[1].key = "test_color"; defs[1].desc = "Colour";
      defs[1].values[0].value = "rgb"; defs[1].values[1].value = "mono";
      defs[1].default_value = "rgb";
      v2.categories = NULL; v2.definitions = defs;
      snprintf(cfg, sizeof(cfg), "%s/core.opt", root);
      test_runloop.core_options = core_option_manager_new(cfg, NULL, &v2, false);
      send_command(hwnd, IDM_CW_CORE_OPTIONS);
      pump(data, 300);
      ow = find_class_window("RetroArchCompanionCoreOptions");
      CHECK(ow && IsWindowVisible(ow), "Core Options window shown");
      ol = ow ? GetDlgItem(ow, 50101 + 0) : NULL; /* not a dialog id: find the list by class */
      (void)ol;
      ol = ow ? FindWindowExA(ow, NULL, "SysListView32", NULL) : NULL;
      CHECK(ol && SendMessageA(ol, LVM_GETITEMCOUNT, 0, 0) == 2, "2 options listed");
      if (ol)
      {
         lv_text(ol, 0, 1, buf, sizeof(buf));
         CHECK(string_is_equal(buf, "Fast"), "value label (got %s)", buf);
         ListView_SetItemState(ol, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
         {
            NMHDR nm; nm.hwndFrom = ol; nm.idFrom = GetDlgCtrlID(ol); nm.code = NM_DBLCLK;
            SendMessageA(ow, WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
         }
         lv_text(ol, 0, 1, buf, sizeof(buf));
         CHECK(string_is_equal(buf, "Slow"), "double-click cycles (got %s)", buf);
      }
      if (ow) ShowWindow(ow, SW_HIDE);
      send_command(hwnd, IDM_CW_SHADER_PARAMS);
      pump(data, 300);
      ow = find_class_window("RetroArchCompanionShaderParams");
      CHECK(ow && IsWindowVisible(ow), "Shader Parameters window shown");
      ol = ow ? FindWindowExA(ow, NULL, "SysListView32", NULL) : NULL;
      CHECK(ol && SendMessageA(ol, LVM_GETITEMCOUNT, 0, 0) == 2, "2 parameters listed");
      if (ol)
      {
         lv_text(ol, 0, 2, buf, sizeof(buf));
         CHECK(string_is_equal(buf, "0 .. 1 (step 0.05)"), "range column (got %s)", buf);
      }
      if (ow) ShowWindow(ow, SW_HIDE);
      send_command(hwnd, IDM_CW_OPTIONS);
      pump(data, 300);
      ow = find_class_window("RetroArchCompanionOptions");
      CHECK(ow && IsWindowVisible(ow), "Options window shown");
      ol = ow ? FindWindowExA(ow, NULL, "SysListView32", NULL) : NULL;
      CHECK(ol && SendMessageA(ol, LVM_GETITEMCOUNT, 0, 0) == 13, "13 settings listed");
      if (ow) ShowWindow(ow, SW_HIDE);
      core_option_manager_free(test_runloop.core_options);
      test_runloop.core_options = NULL;
   }

   /* --- hide / unhide, new / delete playlists (the context menu) --- */
   {
      int before = (int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0);
      char keep[512];
      strlcpy(keep, companion_core_playlist_path(peek->core, 3), sizeof(keep));
      ListView_SetItemState(playlists, 3, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
      send_command(hwnd, IDM_CW_HIDE_PLAYLIST);
      pump(data, 300);
      CHECK((int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0) == before - 1, "Hide drops the row (%d -> %d)", before, (int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0));
      CHECK(companion_core_hidden_count(peek->core) == 1, "one hidden");
      /* the Hidden Playlists submenu sends IDM_CW_UNHIDE_FIRST + index */
      send_command(hwnd, IDM_CW_UNHIDE_FIRST);
      pump(data, 300);
      CHECK((int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0) == before, "unhidden: the row is back");
      CHECK(string_is_equal(companion_core_playlist_path(peek->core, 3), keep), "and in its place");
      /* New Playlist: created and selected for an in-place rename */
      send_command(hwnd, IDM_CW_NEW_PLAYLIST);
      pump(data, 300);
      CHECK((int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0) == before + 1, "New Playlist adds a row (%d)", (int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0));
      {
         size_t i, n = companion_core_playlist_count(peek->core);
         const char *np = NULL;
         for (i = 0; i < n; i++)
            if (strstr(companion_core_playlist_name(peek->core, i), "New Playlist"))
               np = companion_core_playlist_path(peek->core, i);
         CHECK(np != NULL, "the new playlist is listed");
         if (np)
         {
            CHECK(companion_core_playlist_delete(peek->core, np), "delete it again");
            pump(data, 300);
            CHECK((int)SendMessageA(playlists, LVM_GETITEMCOUNT, 0, 0) == before, "listing back to %d", before);
         }
      }
   }

   /* Dock rows: the layout on screen is written back in the shared
    * format (with "Save Dock Positions" on), and a second driver built
    * from a saved layout - the one Tatsuya79 arranged in Qt: Screenshots
    * above Title Screen, Core Info hidden, a wider left column, the log
    * shown - opens to it: the thumbnail pane on the Screenshots tab,
    * Core Info hidden, the log up, the columns at the saved widths. */
   {
      RECT ri, rb;
      HWND info = GetDlgItem(hwnd, IDC_CW_INFO), boxart = GetDlgItem(hwnd, IDC_CW_BOXART);
      test_settings.bools.desktop_menu_save_dock_positions = true;
      SendMessageA(hwnd, WM_SIZE, 0, 0);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,1,", 8) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,0") != NULL,
            "Core Info row: shown, right, slot 0 (%s)", test_settings.arrays.desktop_menu_dock_core_info);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_boxart, "right,1,", 8) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_boxart, ",-,1,1") != NULL,
            "boxart row: raised tab, slot 1 (%s)", test_settings.arrays.desktop_menu_dock_boxart);
      CHECK(strstr(test_settings.arrays.desktop_menu_dock_title, ",boxart,0,1") != NULL,
            "title row tabbed onto boxart (%s)", test_settings.arrays.desktop_menu_dock_title);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_log, "bottom,0,", 9) == 0,
            "log row hidden (%s)", test_settings.arrays.desktop_menu_dock_log);
      GetWindowRect(info, &ri); GetWindowRect(boxart, &rb);
      CHECK(ri.bottom <= rb.top, "Core Info above the thumbnails by default");
   }
   /* The Core combo's "Load Core..." item is an action, as in Qt: picking
    * it opens the Load Core window, and dismissing that puts the combo
    * back on its last real pick. */
   {
      HWND combo = GetDlgItem(hwnd, IDC_CW_CORE_COMBO);
      LRESULT n  = SendMessageA(combo, CB_GETCOUNT, 0, 0);
      HWND cores;
      stub_core_count = 12; /* a dozen fixture cores to size the picker to */
      CHECK(n >= 2 && SendMessageA(combo, CB_GETITEMDATA, (WPARAM)(n - 1), 0) == COMPANION_LAUNCH_LOAD_CORE,
            "combo's last item is Load Core... (%ld items)", (long)n);
      SendMessageA(combo, CB_SETCURSEL, (WPARAM)(n - 2), 0);
      SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(IDC_CW_CORE_COMBO, CBN_SELCHANGE), (LPARAM)combo);
      SendMessageA(combo, CB_SETCURSEL, (WPARAM)(n - 1), 0);
      SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(IDC_CW_CORE_COMBO, CBN_SELCHANGE), (LPARAM)combo);
      pump(data, 200);
      cores = FindWindowA(NULL, "Load Core");
      CHECK(cores && IsWindowVisible(cores), "picking Load Core... opens the Load Core window");
      if (cores)
      {
         /* Sized to its list and kept on the work area: no smaller than
          * the 420x400 floor, inside the screen, every row visible with
          * no vertical scrollbar, no horizontal one either, and centred
          * over the companion. */
         RECT rc, wa, ow, lr;
         HWND list = GetDlgItem(cores, IDC_CW_CORES);
         LRESULT rows = SendMessageA(list, LVM_GETITEMCOUNT, 0, 0);
         LONG style;
         GetWindowRect(cores, &rc); GetWindowRect(hwnd, &ow);
         SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
         CHECK(rc.right - rc.left >= 420 && rc.bottom - rc.top >= 400,
               "picker no smaller than 420x400 (%ldx%ld)", (long)(rc.right - rc.left), (long)(rc.bottom - rc.top));
         CHECK(rc.left >= wa.left && rc.top >= wa.top && rc.right <= wa.right && rc.bottom <= wa.bottom,
               "picker inside the work area (%ld,%ld-%ld,%ld in %ld,%ld-%ld,%ld)",
               (long)rc.left, (long)rc.top, (long)rc.right, (long)rc.bottom, (long)wa.left, (long)wa.top, (long)wa.right, (long)wa.bottom);
         pump(data, 100);
         style = GetWindowLongA(list, GWL_STYLE);
         CHECK(rows > 0 && !(style & WS_VSCROLL) && !(style & WS_HSCROLL),
               "%ld rows shown with no scrollbar (style 0x%lx)", (long)rows, (long)style);
         GetClientRect(list, &lr);
         CHECK(SendMessageA(list, LVM_GETCOLUMNWIDTH, 0, 0) + SendMessageA(list, LVM_GETCOLUMNWIDTH, 1, 0) <= lr.right,
               "columns fit the list (%ld + %ld in %ld)", (long)SendMessageA(list, LVM_GETCOLUMNWIDTH, 0, 0), (long)SendMessageA(list, LVM_GETCOLUMNWIDTH, 1, 0), (long)lr.right);
         CHECK(abs(((int)rc.left + (int)rc.right) / 2 - ((int)ow.left + (int)ow.right) / 2) <= 8
               || rc.left == wa.left + 16 || rc.right == wa.right - 16,
               "picker centred over the companion (picker mid %ld, companion mid %ld)",
               (long)((rc.left + rc.right) / 2), (long)((ow.left + ow.right) / 2));
         SendMessageA(cores, WM_COMMAND, IDC_CW_CORES_CANCEL, 0);
         pump(data, 100);
         CHECK(!IsWindowVisible(cores), "Cancel hides it");
      }
      CHECK(SendMessageA(combo, CB_GETCURSEL, 0, 0) == n - 2, "combo back on its last pick (%ld)", (long)SendMessageA(combo, CB_GETCURSEL, 0, 0));
      /* Eighty cores on an 800-tall screen: the picker is clamped to the
       * work area and the list scrolls rather than the window running
       * off the screen. */
      stub_core_count = 80;
      SendMessageA(hwnd, WM_COMMAND, IDM_CW_LOAD_CORE, 0);
      pump(data, 200);
      if (cores)
      {
         RECT rc, wa;
         HWND list = GetDlgItem(cores, IDC_CW_CORES);
         GetWindowRect(cores, &rc);
         SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
         CHECK(IsWindowVisible(cores) && rc.top >= wa.top && rc.bottom <= wa.bottom,
               "80 rows: picker clamped to the work area (%ld-%ld in %ld-%ld)", (long)rc.top, (long)rc.bottom, (long)wa.top, (long)wa.bottom);
         CHECK(SendMessageA(list, LVM_GETITEMCOUNT, 0, 0) == 80 && (GetWindowLongA(list, GWL_STYLE) & WS_VSCROLL),
               "80 rows: the list scrolls instead");
         SendMessageA(cores, WM_COMMAND, IDC_CW_CORES_CANCEL, 0);
         pump(data, 50);
      }
      stub_core_count = 0;
   }

   /* close: the window hides */
   SendMessageA(hwnd, WM_CLOSE, 0, 0);
   pump(data, 200);
   CHECK(!IsWindowVisible(hwnd), "companion window hidden after close");
   ui_companion_wimp_win32.deinit(data);

   {
      void *d2;
      struct wimp_peek *p2;
      HWND h2, info, boxart, log, btabs, pl;
      RECT rb, rl, rp;
      strlcpy(test_settings.arrays.desktop_menu_dock_search,     "left,1,340,60,-,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_playlists,  "left,1,340,500,-,0,1", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_core,       "left,1,340,40,-,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_boxart,     "right,0,0,0,-,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_title,      "right,1,300,210,-,0,1", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_screenshot, "right,1,300,420,-,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_logo,       "right,0,0,0,boxart,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_core_info,  "right,0,0,0,-,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_log,        "bottom,1,0,150,-,0,0", 64);
      test_settings.bools.desktop_menu_save_geometry     = true;
      test_settings.uints.desktop_menu_window_x          = 20;
      test_settings.uints.desktop_menu_window_y          = 30;
      test_settings.uints.desktop_menu_window_width      = 1000;
      test_settings.uints.desktop_menu_window_height     = 600;
      d2 = ui_companion_wimp_win32.init();
      CHECK(d2 != NULL, "second driver init from a saved layout");
      if (d2)
      {
         p2     = (struct wimp_peek*)d2;
         h2     = p2->hwnd;
         info   = GetDlgItem(h2, IDC_CW_INFO);
         boxart = GetDlgItem(h2, IDC_CW_BOXART);
         log    = GetDlgItem(h2, IDC_CW_LOG);
         btabs  = GetDlgItem(h2, IDC_CW_BOXART_TABS);
         pl     = GetDlgItem(h2, IDC_CW_PLAYLISTS);
         ui_companion_wimp_win32.toggle(d2, true);
         pump(d2, 400);
         CHECK(!IsWindowVisible(info), "Core Info hidden as its row says");
         CHECK(IsWindowVisible(boxart) && IsWindowVisible(log), "thumbnails and log shown as their rows say");
         CHECK(SendMessageA(btabs, TCM_GETCURSEL, 0, 0) == 2, "Screenshots tab raised: the topmost split-out dock (%d)", (int)SendMessageA(btabs, TCM_GETCURSEL, 0, 0));
         GetWindowRect(h2, &rp);
         CHECK(rp.right - rp.left == 1000 && rp.bottom - rp.top == 600 && rp.left == 20 && rp.top == 30,
               "window geometry restored (%ld,%ld %ldx%ld)", (long)rp.left, (long)rp.top, (long)(rp.right - rp.left), (long)(rp.bottom - rp.top));
         GetWindowRect(pl, &rp); GetWindowRect(boxart, &rb); GetWindowRect(log, &rl);
         CHECK(rp.right - rp.left > 300, "left column at the saved 340 (playlists %ld wide)", (long)(rp.right - rp.left));
         CHECK(rb.right - rb.left >= 280 && rb.right - rb.left <= 300, "right column at the saved 300 (thumbnails %ld wide)", (long)(rb.right - rb.left));
         CHECK(rl.bottom - rl.top >= 140 && rl.bottom - rl.top <= 150, "log at the saved 150 (%ld)", (long)(rl.bottom - rl.top));
         /* Re-saved as shown: nothing lost in the round trip. */
         SendMessageA(h2, WM_SIZE, 0, 0);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_screenshot, "right,1,", 8) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",boxart,1,0") != NULL,
               "screenshot row re-saved raised in slot 0 (%s)", test_settings.arrays.desktop_menu_dock_screenshot);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,0,", 8) == 0,
               "Core Info row re-saved hidden (%s)", test_settings.arrays.desktop_menu_dock_core_info);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_log, "bottom,1,0,150,", 15) == 0,
               "log row re-saved shown at 150 (%s)", test_settings.arrays.desktop_menu_dock_log);
         CHECK(test_settings.uints.desktop_menu_window_width == 1000 && test_settings.uints.desktop_menu_window_x == 20,
               "window geometry re-saved (%u,%u %ux%u)", test_settings.uints.desktop_menu_window_x, test_settings.uints.desktop_menu_window_y,
               test_settings.uints.desktop_menu_window_width, test_settings.uints.desktop_menu_window_height);
         /* Core Info back on via the View menu: below the thumbnails,
          * where the rows put it. */
         SendMessageA(h2, WM_COMMAND, IDM_CW_TOGGLE_INFO, 0);
         pump(d2, 100);
         GetWindowRect(info, &rp); GetWindowRect(boxart, &rb);
         CHECK(IsWindowVisible(info) && rb.bottom <= rp.top, "Core Info shown below the thumbnails (thumbs bottom %ld, info top %ld)", (long)rb.bottom, (long)rp.top);
         CHECK(strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,1") != NULL
               && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",boxart,1,0") != NULL,
               "rows: Core Info slot 1, thumbnails slot 0 (%s / %s)", test_settings.arrays.desktop_menu_dock_core_info, test_settings.arrays.desktop_menu_dock_screenshot);
         /* Splitters: a drag on the gap after the left column widens
          * it and the rows follow; a drag on the gap between the two
          * right panes moves their split. */
         {
            RECT rb2;
            int gx, gy, before;
            POINT pt;
            GetWindowRect(pl, &rp);
            before = rp.right - rp.left;
            gx = before + 2 * 4 + 2; /* just past the column: the gap */
            gy = 50;
            SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(gx, gy));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(gx + 60, gy));
            SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(gx + 60, gy));
            pump(d2, 50);
            GetWindowRect(pl, &rp);
            CHECK(rp.right - rp.left >= before + 50, "left splitter drag widened the column (%d -> %ld)", before, (long)(rp.right - rp.left));
            CHECK(strncmp(test_settings.arrays.desktop_menu_dock_playlists, "left,1,4", 8) == 0,
                  "playlists row follows the drag (%s)", test_settings.arrays.desktop_menu_dock_playlists);
            /* Both right panes up, thumbnails on top: the gap sits just
             * under the thumbnail image. */
            GetWindowRect(boxart, &rb2);
            pt.x = rb2.left + 10; pt.y = rb2.bottom + 2;
            ScreenToClient(h2, &pt);
            before = rb2.bottom - rb2.top;
            SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(pt.x, pt.y + 80));
            SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(pt.x, pt.y + 80));
            pump(d2, 50);
            GetWindowRect(boxart, &rb2);
            CHECK(rb2.bottom - rb2.top >= before + 60, "pane splitter drag grew the top pane (%d -> %ld)", before, (long)(rb2.bottom - rb2.top));
         }
         ui_companion_wimp_win32.deinit(d2);
      }
   }

   companion_test_teardown_fixtures(root);
   if (fails)
   {
      printf("companion_win32_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("companion_win32_test: OK\n");
   return 0;
}
