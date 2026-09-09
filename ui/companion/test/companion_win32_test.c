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
#include "../../../ui/companion/companion_dock.h"
#include "../../../ui/drivers/ui_win32.h"
#include "../../../version.h"

/* --- stubs for what the driver reaches into RetroArch for --------------- */
ui_window_win32_t main_window;
static int stub_menu_loop_calls;
LRESULT win32_menu_loop(HWND hwnd, WPARAM wparam) { (void)hwnd; (void)wparam; stub_menu_loop_calls++; return 0; }
void win32_sizemove_enter(HWND h) { (void)h; }
void win32_sizemove_exit(HWND h) { (void)h; }
void win32_sizemove_tick(void) { }
void win32_sizemove_abort(void) { }

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

   /* Docks: the panes are docks as Qt's are - the shared model laid out
    * by the driver. The default layout is written back as the rows Qt
    * writes for its default (thumbnails tabbed above Core Info on the
    * right, Boxart raised, the log hidden); and a second driver built
    * from a saved layout - the one Tatsuya79 arranged in Qt: Screenshots
    * split out above Title Screen split out, Boxart and Core Info
    * hidden, a wider left column, the log shown, a saved window frame -
    * opens to exactly it. */
   {
      RECT ri, rb;
      HWND info = GetDlgItem(hwnd, IDC_CW_INFO), boxart = GetDlgItem(hwnd, IDC_CW_BOXART);
      test_settings.bools.desktop_menu_save_dock_positions = true;
      SendMessageA(hwnd, WM_SIZE, 0, 0);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_boxart, "right,1,", 8) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_boxart, ",-,1,0") != NULL,
            "boxart row: raised tab, slot 0 (%s)", test_settings.arrays.desktop_menu_dock_boxart);
      CHECK(strstr(test_settings.arrays.desktop_menu_dock_title, ",boxart,0,0") != NULL
            && strncmp(test_settings.arrays.desktop_menu_dock_title, "right,1,0,0,", 12) == 0,
            "title row tabbed onto boxart, carrying no size (%s)", test_settings.arrays.desktop_menu_dock_title);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,1,", 8) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,1") != NULL,
            "Core Info row: shown, right, slot 1 (%s)", test_settings.arrays.desktop_menu_dock_core_info);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_log, "bottom,0,", 9) == 0,
            "log row hidden (%s)", test_settings.arrays.desktop_menu_dock_log);
      CHECK(strncmp(test_settings.arrays.desktop_menu_dock_playlists, "left,1,280,", 11) == 0
            && strstr(test_settings.arrays.desktop_menu_dock_playlists, ",-,0,1") != NULL,
            "playlists row: left, 280 wide, slot 1 (%s)", test_settings.arrays.desktop_menu_dock_playlists);
      GetWindowRect(info, &ri); GetWindowRect(boxart, &rb);
      CHECK(IsWindowVisible(boxart) && IsWindowVisible(info) && rb.bottom <= ri.top,
            "thumbnails above Core Info by default, as Qt's");
      CHECK(!IsWindowVisible(GetDlgItem(hwnd, IDC_CW_BOXART + 1)), "Title Screen is a tab behind Boxart: not shown");
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
         /* Qt's chrome: no Load / Cancel buttons, a "Load Custom
          * Core..." button and a "<version> - <core>" status bar; and
          * every row carries its version (the list sorts on insert,
          * so the version has to land on the row the insert reports). */
         {
            char buf[128];
            LVITEMA it;
            HWND custom = GetDlgItem(cores, IDC_CW_CORES_CUSTOM);
            HWND sb     = GetDlgItem(cores, IDC_CW_CORES_STATUS);
            CHECK(custom && IsWindowVisible(custom), "Load Custom Core... button present");
            CHECK(!FindWindowExA(cores, NULL, "BUTTON", "&Load") && !FindWindowExA(cores, NULL, "BUTTON", "Cancel"),
                  "no Load / Cancel buttons");
            buf[0] = '\0';
            if (sb)
               SendMessageA(sb, SB_GETTEXTA, 0, (LPARAM)buf);
            CHECK(sb && strstr(buf, PACKAGE_VERSION " - ") != NULL, "status bar shows '<version> - <core>' (%s)", buf);
            memset(&it, 0, sizeof(it));
            it.iSubItem = 1; it.pszText = buf; it.cchTextMax = sizeof(buf);
            buf[0] = '\0';
            SendMessageA(list, LVM_GETITEMTEXTA, (WPARAM)(rows - 1), (LPARAM)&it);
            CHECK(strcmp(buf, "1.0") == 0, "last row carries its version (got '%s')", buf);
         }
         CHECK(abs(((int)rc.left + (int)rc.right) / 2 - ((int)ow.left + (int)ow.right) / 2) <= 8
               || rc.left == wa.left + 16 || rc.right == wa.right - 16,
               "picker centred over the companion (picker mid %ld, companion mid %ld)",
               (long)((rc.left + rc.right) / 2), (long)((ow.left + ow.right) / 2));
         SendMessageA(cores, WM_COMMAND, IDCANCEL, 0);
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
         SendMessageA(cores, WM_COMMAND, IDCANCEL, 0);
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
      HWND h2, info, boxart, title, shot, log, pl;
      RECT rb, rl, rp, rt;
      strlcpy(test_settings.arrays.desktop_menu_dock_search,     "left,1,340,60,-,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_playlists,  "left,1,340,500,-,0,1", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_core,       "left,1,340,40,-,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_boxart,     "right,0,0,0,-,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_title,      "right,1,300,210,-,0,1", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_screenshot, "right,1,300,420,-,0,0", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_logo,       "right,0,0,0,boxart,0,2", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_core_info,  "right,0,0,0,-,0,3", 64);
      strlcpy(test_settings.arrays.desktop_menu_dock_log,        "bottom,1,0,150,-,0,0", 64);
      test_settings.bools.desktop_menu_save_geometry = true;
      test_settings.uints.desktop_menu_window_x      = 20;
      test_settings.uints.desktop_menu_window_y      = 30;
      test_settings.uints.desktop_menu_window_width  = 1000;
      test_settings.uints.desktop_menu_window_height = 700;
      d2 = ui_companion_wimp_win32.init();
      CHECK(d2 != NULL, "second driver init from a saved layout");
      if (d2)
      {
         p2     = (struct wimp_peek*)d2;
         h2     = p2->hwnd;
         info   = GetDlgItem(h2, IDC_CW_INFO);
         boxart = GetDlgItem(h2, IDC_CW_BOXART);
         title  = GetDlgItem(h2, IDC_CW_BOXART + 1);
         shot   = GetDlgItem(h2, IDC_CW_BOXART + 2);
         log    = GetDlgItem(h2, IDC_CW_LOG);
         pl     = GetDlgItem(h2, IDC_CW_PLAYLISTS);
         ui_companion_wimp_win32.toggle(d2, true);
         pump(d2, 400);
         CHECK(!IsWindowVisible(info) && !IsWindowVisible(boxart), "Core Info and Boxart hidden as their rows say");
         CHECK(IsWindowVisible(shot) && IsWindowVisible(title) && IsWindowVisible(log),
               "Screenshots, Title Screen and the log shown as their rows say");
         GetWindowRect(shot, &rb); GetWindowRect(title, &rt);
         CHECK(rb.bottom <= rt.top, "Screenshots above Title Screen, each its own pane (%ld <= %ld)", (long)rb.bottom, (long)rt.top);
         GetWindowRect(h2, &rp);
         CHECK(rp.right - rp.left == 1000 && rp.bottom - rp.top == 700 && rp.left == 20 && rp.top == 30,
               "window geometry restored (%ld,%ld %ldx%ld)", (long)rp.left, (long)rp.top, (long)(rp.right - rp.left), (long)(rp.bottom - rp.top));
         GetWindowRect(pl, &rp); GetWindowRect(log, &rl);
         CHECK(rp.right - rp.left > 300, "left column at the saved 340 (playlists %ld wide)", (long)(rp.right - rp.left));
         CHECK(rb.right - rb.left >= 280 && rb.right - rb.left <= 300, "right column at the saved 300 (Screenshots %ld wide)", (long)(rb.right - rb.left));
         CHECK(rl.bottom - rl.top >= 100 && rl.bottom - rl.top <= 150, "log inside the saved 150 (%ld)", (long)(rl.bottom - rl.top));
         /* Re-saved as shown: nothing lost in the round trip. */
         SendMessageA(h2, WM_SIZE, 0, 0);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_screenshot, "right,1,300,", 12) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",-,0,0") != NULL,
               "screenshot row re-saved standalone in slot 0 (%s)", test_settings.arrays.desktop_menu_dock_screenshot);
         CHECK(strstr(test_settings.arrays.desktop_menu_dock_title, ",-,0,1") != NULL,
               "title row re-saved standalone in slot 1 (%s)", test_settings.arrays.desktop_menu_dock_title);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_logo, "right,0,", 8) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_logo, ",boxart,0,2") != NULL,
               "logo row re-saved hidden, tabbed onto boxart in slot 2 (%s)", test_settings.arrays.desktop_menu_dock_logo);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,0,", 8) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,3") != NULL,
               "Core Info row re-saved hidden in slot 3 (%s)", test_settings.arrays.desktop_menu_dock_core_info);
         CHECK(strncmp(test_settings.arrays.desktop_menu_dock_log, "bottom,1,", 9) == 0
               && strstr(test_settings.arrays.desktop_menu_dock_log, ",150,-,0,0") != NULL,
               "log row re-saved shown at 150 (%s)", test_settings.arrays.desktop_menu_dock_log);
         CHECK(test_settings.uints.desktop_menu_window_width == 1000 && test_settings.uints.desktop_menu_window_x == 20,
               "window geometry re-saved (%u,%u %ux%u)", test_settings.uints.desktop_menu_window_x, test_settings.uints.desktop_menu_window_y,
               test_settings.uints.desktop_menu_window_width, test_settings.uints.desktop_menu_window_height);
         /* Core Info back on: below the thumbnails, in the slot its
          * row kept for it. */
         SendMessageA(h2, WM_COMMAND, IDM_CW_TOGGLE_INFO, 0);
         pump(d2, 100);
         GetWindowRect(info, &rp); GetWindowRect(title, &rt);
         CHECK(IsWindowVisible(info) && rt.bottom <= rp.top, "Core Info shown below Title Screen (title bottom %ld, info top %ld)", (long)rt.bottom, (long)rp.top);
         CHECK(strstr(test_settings.arrays.desktop_menu_dock_core_info, ",-,0,3") != NULL
               && strncmp(test_settings.arrays.desktop_menu_dock_core_info, "right,1,", 8) == 0,
               "rows: Core Info shown, still slot 3 (%s)", test_settings.arrays.desktop_menu_dock_core_info);
         /* Gaps: a drag on the gap after the left column widens it and
          * the rows follow; a drag on the gap between the two right
          * panes moves their split. Gaps sit just past a pane's
          * controls (the pane's padding, then the gap). */
         {
            RECT rb2;
            POINT pt;
            int before;
            GetWindowRect(pl, &rp);
            before = rp.right - rp.left;
            pt.x = rp.right + 4 + 2; pt.y = rp.top + 20;
            ScreenToClient(h2, &pt);
            SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(pt.x + 60, pt.y));
            SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(pt.x + 60, pt.y));
            pump(d2, 50);
            GetWindowRect(pl, &rp);
            CHECK(rp.right - rp.left >= before + 50, "left gap drag widened the column (%d -> %ld)", before, (long)(rp.right - rp.left));
            CHECK(strncmp(test_settings.arrays.desktop_menu_dock_playlists, "left,1,4", 8) == 0,
                  "playlists row follows the drag (%s)", test_settings.arrays.desktop_menu_dock_playlists);
            GetWindowRect(shot, &rb2);
            pt.x = rb2.left + 10; pt.y = rb2.bottom + 4 + 2;
            ScreenToClient(h2, &pt);
            before = rb2.bottom - rb2.top;
            SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(pt.x, pt.y + 50));
            SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(pt.x, pt.y + 50));
            pump(d2, 50);
            GetWindowRect(shot, &rb2);
            CHECK(rb2.bottom - rb2.top >= before + 40, "pane gap drag grew Screenshots (%d -> %ld)", before, (long)(rb2.bottom - rb2.top));
         }
         /* Drag-and-drop of a strip: Title Screen's strip (just above
          * its control) dragged onto the middle of Screenshots tabs it
          * there; dragged out to the content it floats in a window of
          * its own; a double-click on that window's caption re-docks
          * it; the strip's close glyph hides it and View > Closed Docks
          * brings it back. */
         {
            RECT rs, rt2, fw;
            POINT a, b;
            HWND fl;
            GetWindowRect(title, &rt2); GetWindowRect(shot, &rs);
            a.x = rt2.left + 10; a.y = rt2.top - 4 - 8;   /* the strip above the control */
            b.x = rs.left + (rs.right - rs.left) / 2; b.y = rs.top + (rs.bottom - rs.top) / 2;
            ScreenToClient(h2, &a); ScreenToClient(h2, &b);
            SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(a.x, a.y));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(a.x + 20, a.y + 20));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(b.x, b.y));
            SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(b.x, b.y));
            pump(d2, 100);
            CHECK(strstr(test_settings.arrays.desktop_menu_dock_title, ",-,1,0") != NULL
                  && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",title,0,0") != NULL,
                  "strip dropped mid-pane: Title Screen tabbed with Screenshots and raised (%s / %s)",
                  test_settings.arrays.desktop_menu_dock_title, test_settings.arrays.desktop_menu_dock_screenshot);
            CHECK(IsWindowVisible(title) && !IsWindowVisible(shot), "the raised tab shows, the other hides");
            /* Out to the content: floats. */
            GetWindowRect(title, &rt2);
            a.x = rt2.left + 10; a.y = rt2.top - 4 - 8;
            b.x = 600; b.y = 300;
            ScreenToClient(h2, &a);
            SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(a.x, a.y));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(a.x + 20, a.y + 20));
            SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(b.x, b.y));
            SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(b.x, b.y));
            pump(d2, 100);
            fl = GetParent(title);
            CHECK(fl && fl != h2 && IsWindowVisible(fl) && IsWindowVisible(title),
                  "strip dropped on the content: Title Screen floats in its own window");
            CHECK(strncmp(test_settings.arrays.desktop_menu_dock_title, "float,1,", 8) == 0,
                  "floating row (%s)", test_settings.arrays.desktop_menu_dock_title);
            CHECK(IsWindowVisible(shot), "Screenshots shows again once its tab partner left");
            if (fl && fl != h2)
            {
               /* Dragged by its caption over Screenshots' middle, the
                * float docks as a tab there (WM_MOVING tracks the
                * pointer, WM_EXITSIZEMOVE drops), as Qt's floats do. */
               POINT c;
               GetWindowRect(shot, &rs);
               c.x = rs.left + (rs.right - rs.left) / 2; c.y = rs.top + (rs.bottom - rs.top) / 2;
               SetCursorPos(c.x, c.y);
               GetWindowRect(fl, &fw);
               SendMessageA(fl, WM_ENTERSIZEMOVE, 0, 0);
               SendMessageA(fl, WM_MOVING, WMSZ_LEFT, (LPARAM)&fw);
               SendMessageA(fl, WM_EXITSIZEMOVE, 0, 0);
               pump(d2, 100);
               CHECK(GetParent(title) == h2 && !IsWindowVisible(fl)
                     && strstr(test_settings.arrays.desktop_menu_dock_screenshot, ",title,0,0") != NULL,
                     "float dragged over a pane docks there as a tab (%s)", test_settings.arrays.desktop_menu_dock_screenshot);
               /* Out again, then the caption double-click re-docks it
                * on its side's end. */
               GetWindowRect(title, &rt2);
               a.x = rt2.left + 10; a.y = rt2.top - 4 - 8;
               ScreenToClient(h2, &a);
               SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(a.x, a.y));
               SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(a.x + 20, a.y + 20));
               SendMessageA(h2, WM_MOUSEMOVE,   MK_LBUTTON, MAKELPARAM(b.x, b.y));
               SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(b.x, b.y));
               pump(d2, 100);
               CHECK(GetParent(title) == fl && IsWindowVisible(fl), "floated again");
               GetWindowRect(fl, &fw);
               SendMessageA(fl, WM_NCLBUTTONDBLCLK, HTCAPTION, MAKELPARAM(fw.left + 20, fw.top + 5));
               pump(d2, 100);
               CHECK(GetParent(title) == h2 && !IsWindowVisible(fl), "caption double-click re-docks it");
               CHECK(strncmp(test_settings.arrays.desktop_menu_dock_title, "right,1,", 8) == 0,
                     "re-docked row (%s)", test_settings.arrays.desktop_menu_dock_title);
            }
            /* The close glyph, then Closed Docks. */
            GetWindowRect(title, &rt2);
            a.x = rt2.right - 4 - 6; a.y = rt2.top - 4 - 8;
            ScreenToClient(h2, &a);
            SendMessageA(h2, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(a.x, a.y));
            SendMessageA(h2, WM_LBUTTONUP,   0,          MAKELPARAM(a.x, a.y));
            pump(d2, 50);
            CHECK(!IsWindowVisible(title) && strncmp(test_settings.arrays.desktop_menu_dock_title, "right,0,", 8) == 0,
                  "close glyph hides the pane (%s)", test_settings.arrays.desktop_menu_dock_title);
            SendMessageA(h2, WM_COMMAND, IDM_CW_DOCK_FIRST + COMPANION_DOCK_TITLE, 0);
            pump(d2, 50);
            CHECK(IsWindowVisible(title) && strncmp(test_settings.arrays.desktop_menu_dock_title, "right,1,", 8) == 0,
                  "Closed Docks shows it again (%s)", test_settings.arrays.desktop_menu_dock_title);
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
