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
 * and manifest resources that were in media/rarch.rc.
 *
 * The icon resource remains in rarch.rc (compiled by windres/rc)
 * so that the .exe has an embedded icon visible in Explorer.
 * Everything else is created at runtime via Win32 API calls. */

#ifndef WIN32_RESOURCES_H__
#define WIN32_RESOURCES_H__

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)

#include <windows.h>

#include "../../ui/drivers/ui_win32_resource.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Call once before creating any windows.
 * Applies DPI awareness and creates the accelerator table. */
void win32_resources_init(void);

/* Release resources created by win32_resources_init(). */
void win32_resources_free(void);

/* Build the application menu bar programmatically
 * (replaces LoadMenu + MAKEINTRESOURCE(IDR_MENU)).
 * Returns a fresh HMENU each call — caller owns it. */
HMENU win32_resources_create_menu(void);

/* Return the accelerator table
 * (replaces LoadAccelerators + MAKEINTRESOURCE(IDR_ACCELERATOR1)). */
HACCEL win32_resources_get_accelerator(void);

/* Show the "Pick Core" dialog built in memory
 * (replaces DialogBoxParam + MAKEINTRESOURCE(IDD_PICKCORE)).
 * Returns IDOK or IDCANCEL. */
int win32_resources_pick_core_dialog(HWND parent, DLGPROC dlg_proc);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 && !_XBOX && !__WINRT__ */
#endif /* WIN32_RESOURCES_H__ */
