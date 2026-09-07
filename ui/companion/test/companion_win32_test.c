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
void win32_modal_enter(HWND h) { (void)h; }
void win32_modal_exit(HWND h) { (void)h; }
void win32_modal_tick(HWND h) { (void)h; }
void win32_modal_window_destroyed(HWND h) { (void)h; }

extern settings_t test_settings;
extern runloop_state_t test_runloop;
extern int stub_calls_command;
extern int stub_calls_shader_apply;
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

   /* close: the window hides */
   SendMessageA(hwnd, WM_CLOSE, 0, 0);
   pump(data, 200);
   CHECK(!IsWindowVisible(hwnd), "companion window hidden after close");

   ui_companion_wimp_win32.deinit(data);
   companion_test_teardown_fixtures(root);
   if (fails)
   {
      printf("companion_win32_test: %d failure(s)\n", fails);
      return 1;
   }
   printf("companion_win32_test: OK\n");
   return 0;
}
