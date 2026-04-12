/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 - RetroArch contributors
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

/* Programmatic replacement for the menu, dialog, accelerator,
 * and manifest resources formerly in media/rarch.rc and
 * media/rarch_ja.rc.
 *
 * The icon resource remains in rarch.rc so the executable has
 * an embedded icon visible in Explorer / taskbar / Alt+Tab.
 *
 * This file creates:
 *   IDR_MENU          → win32_resources_create_menu()
 *   IDR_ACCELERATOR1  → win32_resources_get_accelerator()
 *   IDD_PICKCORE      → win32_resources_pick_core_dialog()
 *   rarch.manifest    → apply_dpi_awareness()  (called from _init)
 */

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <string.h>
#include <wchar.h>

#include "win32_resources.h"
#include "../../ui/drivers/ui_win32_resource.h"

/* ----------------------------------------------------------------
 * Internal state
 * ---------------------------------------------------------------- */
static HACCEL s_accel_table = NULL;

/* ----------------------------------------------------------------
 * DPI AWARENESS  (replaces media/rarch.manifest)
 *
 * The manifest contained:
 *   <dpiAware xmlns="...">true</dpiAware>
 *
 * We call the equivalent API at runtime.
 * ---------------------------------------------------------------- */
typedef HRESULT (WINAPI *pfn_SetProcessDpiAwareness)(int);

static void apply_dpi_awareness(void)
{
   HMODULE shcore = LoadLibraryW(L"shcore.dll");
   if (shcore)
   {
      pfn_SetProcessDpiAwareness fn =
         (pfn_SetProcessDpiAwareness)(void*)GetProcAddress(
               shcore, "SetProcessDpiAwareness");
      if (fn)
      {
         /* PROCESS_SYSTEM_DPI_AWARE = 1 */
         fn(1);
         FreeLibrary(shcore);
         return;
      }
      FreeLibrary(shcore);
   }

   /* Fallback for Vista / Win7 without shcore */
   SetProcessDPIAware();
}

/* ----------------------------------------------------------------
 * ACCELERATOR TABLE  (replaces IDR_ACCELERATOR1)
 *
 *   Ctrl+O     → ID_M_LOAD_CONTENT
 *   Alt+Enter  → ID_M_FULL_SCREEN
 * ---------------------------------------------------------------- */
static HACCEL create_accelerator_table(void)
{
   ACCEL accel[2];

   accel[0].fVirt = FCONTROL | FVIRTKEY | FNOINVERT;
   accel[0].key   = 'O';
   accel[0].cmd   = ID_M_LOAD_CONTENT;

   accel[1].fVirt = FALT | FVIRTKEY | FNOINVERT;
   accel[1].key   = VK_RETURN;
   accel[1].cmd   = ID_M_FULL_SCREEN;

   return CreateAcceleratorTableW(accel, 2);
}

/* ----------------------------------------------------------------
 * MENU BAR  (replaces IDR_MENU from rarch.rc)
 *
 * The menu is always created with English labels; RetroArch's
 * existing win32_localize_menu() in win32_common.c re-labels
 * every item with the localized string for the active language,
 * so there is no need for a separate Japanese menu.
 * ---------------------------------------------------------------- */
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

   /* Audio Options */
   audio_menu = CreatePopupMenu();
   AppendMenuA(audio_menu, MF_STRING, ID_M_MUTE_TOGGLE, "Audio Mute Toggle");
   AppendMenuA(command_menu, MF_POPUP, (UINT_PTR)audio_menu, "Audio Options");

   /* Disk Options */
   disk_menu = CreatePopupMenu();
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_CYCLE, "Disk Eject Toggle");
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_PREV,  "Disk Previous");
   AppendMenuA(disk_menu, MF_STRING, ID_M_DISK_NEXT,  "Disk Next");
   AppendMenuA(command_menu, MF_POPUP, (UINT_PTR)disk_menu, "Disk Options");

   /* Save State Options */
   savestate_menu = CreatePopupMenu();

   /* State Index sub-menu */
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

   /* Windowed Scale sub-menu */
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

/* ----------------------------------------------------------------
 * "PICK CORE" DIALOG  (replaces IDD_PICKCORE)
 *
 * Builds DLGTEMPLATE + DLGITEMTEMPLATE structs in memory and calls
 * DialogBoxIndirectParam, so no compiled resource is needed.
 * ---------------------------------------------------------------- */

/* Align pointer up to DWORD boundary */
static LPWORD align_dword(LPWORD ptr)
{
   ULONG_PTR ul = (ULONG_PTR)ptr;
   ul  = (ul + 3) & ~(ULONG_PTR)3;
   return (LPWORD)ul;
}

/* Append a wide string (including NUL) into template buffer */
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

   /* Dialog header */
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

/* ----------------------------------------------------------------
 * PUBLIC INIT / FREE
 * ---------------------------------------------------------------- */
void win32_resources_init(void)
{
   apply_dpi_awareness();
   s_accel_table = create_accelerator_table();
}

void win32_resources_free(void)
{
   if (s_accel_table)
   {
      DestroyAcceleratorTable(s_accel_table);
      s_accel_table = NULL;
   }
}

HACCEL win32_resources_get_accelerator(void)
{
   return s_accel_table;
}

#endif /* _WIN32 && !_XBOX && !__WINRT__ */
