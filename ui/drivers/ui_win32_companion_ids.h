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

/* The Win32 companion's control and menu ids. Private to the driver,
 * shared with its regression harness (ui/companion/test/
 * companion_win32_test.c) so the harness sends exactly the commands the
 * widgets send. */

#ifndef __UI_WIN32_COMPANION_IDS_H
#define __UI_WIN32_COMPANION_IDS_H

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
   IDC_CW_BR_UP,      /* file browser: Up / Start Directory / Downloads */
   IDC_CW_BR_START,
   IDC_CW_BR_DOWNLOADS,
   IDC_CW_CORE_LABEL,
   IDC_CW_CORE_COMBO, /* launch-with core selection */
   IDC_CW_CORE_INFO_BTN,
   IDC_CW_RUN_BTN,
   IDC_CW_STOP_BTN,
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
   IDC_CW_OPTS_LIST,      /* Core Options window */
   IDC_CW_OPTS_RESET,
   IDC_CW_OPTS_RESET_ALL,
   IDC_CW_OPTS_CLOSE,
   IDC_CW_SHP_LIST,       /* Shader Parameters window */
   IDC_CW_SHP_EDIT,
   IDC_CW_SHP_APPLY,
   IDC_CW_SHP_RESET,
   IDC_CW_SHP_CLOSE,
   IDC_CW_SET_LIST,       /* Options window */
   IDC_CW_SET_EDIT,
   IDC_CW_SET_APPLY,
   IDC_CW_SET_CLOSE,
   IDM_CW_LOAD_CORE  = 50101,
   IDM_CW_LOAD_CONTENT,
   IDM_CW_REFRESH,
   IDM_CW_RENAME_PLAYLIST,
   IDM_CW_ADD_FILES,
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
   IDM_CW_CORE_OPTIONS,
   IDM_CW_SHADER_PARAMS,
   IDM_CW_OPTIONS,
   IDM_CW_HELP_CONTRIBUTORS,
   IDM_CW_UNLOAD_CORE,
   IDM_CW_LOAD_CUSTOM_CORE,
   /* IDM_CW_ASSOC_BASE + i selects installed core i as the playlist's
    * default core; keep a wide gap after it. */
   IDM_CW_ASSOC_BASE = 51000,
   IDM_CW_ASSOC_MAX  = 59999
};

#endif
